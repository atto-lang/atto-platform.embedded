#include "ores_embedded_static_actors.h"

/* All actor storage and message queues are static. No allocation, libc,
 * OS calls or threads. One actor turn is entered at a time. */
typedef struct {
    uint32_t generation;
    uint8_t slot;
    uint8_t valid;
    uint16_t reserved;
} ActorCapability;
_Static_assert(sizeof(ActorCapability) == 8u,
               "static actor capability token must remain 8 bytes");
typedef struct {
    uint64_t domain, epoch;
    uint32_t generation;
    uint8_t active, head, tail, count, fields, phase, capability_count;
    uint8_t owned_child_mask;
    ActorCapability capabilities[ORES_STATIC_ACTOR_CAPABILITY_SLOTS];
    uint8_t mutable_field[ORES_STATIC_ACTOR_FIELDS];
    uint8_t initialized_field[ORES_STATIC_ACTOR_FIELDS];
    int64_t state[ORES_STATIC_ACTOR_FIELDS];
    int64_t mailbox[ORES_STATIC_MAILBOX_CAPACITY];
    int64_t last;
    unsigned deliveries;
    void (*init)(void*, uint64_t, uint64_t);
    int64_t (*handler)(void*, uint64_t, uint64_t, int64_t);
} ActorSlot;
typedef struct {
    uint32_t generation;
    uint8_t slot;
    uint8_t issued;
    unsigned deliveries;
    int64_t last;
} ActorRef;

static ActorSlot slots[ORES_STATIC_ACTOR_LIMIT];
static ActorRef refs[ORES_STATIC_ACTOR_LIMIT];
static ActorSlot *current;
static unsigned issued, active_count;
static int fault;

/* Include every adapter-owned persistent variable in the admission check.
 * This is a compile-time bound, not a claim about the whole Zephyr image,
 * linker alignment, stack, or other system allocations. */
_Static_assert(sizeof(slots) + sizeof(refs) + sizeof(current) +
               sizeof(issued) + sizeof(active_count) + sizeof(fault) <=
               ORES_STATIC_ACTOR_STORAGE_BUDGET_BYTES,
               "static actor adapter exceeds 3 KiB storage budget");

/* A sticky fault is fatal for the whole static actor session. Outside a
 * callback, retire every active slot immediately so private state and queued
 * payloads cannot survive a failed send, invalid capability or reset. During
 * a callback, defer cleanup until the trusted callback boundary returns. */
static void retire_slot(ActorSlot *slot, ActorRef *ref);
static void fail(void) {
    unsigned i;
    fault = 1;
    if (current) return;
    for (i=0; i<issued; ++i) {
        ActorRef *ref=&refs[i];
        if (!ref->issued || ref->slot >= ORES_STATIC_ACTOR_LIMIT) continue;
        ActorSlot *slot=&slots[ref->slot];
        if (slot->active && slot->generation == ref->generation)
            retire_slot(slot,ref);
    }
}
void ores_embedded_actors_reset(void) {
    unsigned i, j;
    /* Address-valued capabilities cannot be safely reissued after reset.
     * Reject all attempts after the first spawn, even if actors are stopped
     * or a sticky failure occurred. Only a device reboot may start anew. */
    if (current || issued != 0 || fault) { fail(); return; }
    for (i=0; i<ORES_STATIC_ACTOR_LIMIT; ++i) {
        slots[i].active = 0;
        slots[i].head = slots[i].tail = slots[i].count = slots[i].fields = 0;
        slots[i].phase = 0;
        slots[i].capability_count = 0;
        slots[i].owned_child_mask = 0;
        slots[i].deliveries = 0;
        slots[i].last = 0;
        slots[i].init = 0; slots[i].handler = 0;
        refs[i].issued = 0;
        refs[i].deliveries = 0;
        refs[i].last = 0;
        for (j=0; j<ORES_STATIC_ACTOR_FIELDS; ++j) {
            slots[i].state[j] = 0;
            slots[i].mutable_field[j] = 0;
            slots[i].initialized_field[j] = 0;
        }
        for (j=0; j<ORES_STATIC_MAILBOX_CAPACITY; ++j)
            slots[i].mailbox[j] = 0;
    }
    issued = 0; active_count = 0; fault = 0;
}
/* Global diagnostics are host-only. Guests must neither inspect the
 * population of other actors nor probe whether an unrelated turn faulted. */
int ores_embedded_actors_faulted(void) {
    if (current) { fail(); return 0; }
    return fault;
}
unsigned ores_embedded_actors_spawns(void) {
    if (current) { fail(); return 0; }
    return issued;
}
unsigned ores_embedded_actors_active(void) {
    if (current) { fail(); return 0; }
    return active_count;
}
unsigned ores_embedded_actors_deliveries(unsigned n) {
    /* Host-only telemetry: a guest turn may only access its own heap. */
    if (current) { fail(); return 0; }
    if (n>=issued || !refs[n].issued) { fail(); return 0; }
    const ActorRef *ref=&refs[n];
    if (ref->slot >= ORES_STATIC_ACTOR_LIMIT) { fail(); return 0; }
    const ActorSlot *slot=&slots[ref->slot];
    return slot->active && slot->generation == ref->generation
        ? slot->deliveries : ref->deliveries;
}
int64_t ores_embedded_actors_last(unsigned n) {
    /* Do not expose other actors' completed values to guest callbacks. */
    if (current) { fail(); return 0; }
    if (n>=issued || !refs[n].issued) { fail(); return 0; }
    const ActorRef *ref=&refs[n];
    if (ref->slot >= ORES_STATIC_ACTOR_LIMIT) { fail(); return 0; }
    const ActorSlot *slot=&slots[ref->slot];
    return slot->active && slot->generation == ref->generation
        ? slot->last : ref->last;
}
static ActorRef* lookup(void* ptr) {
    unsigned i;
    if (!ptr) { fail(); return 0; }
    for (i=0; i<issued; ++i)
        if (ptr == &refs[i] && refs[i].issued) return &refs[i];
    fail(); return 0;
}
static ActorSlot* lookup_live(void* ptr) {
    ActorRef* ref=lookup(ptr);
    ActorSlot* slot;
    if (!ref) return 0;
    if (ref->slot >= ORES_STATIC_ACTOR_LIMIT) { fail(); return 0; }
    slot=&slots[ref->slot];
    if (!slot->active || slot->generation != ref->generation) {
        fail(); return 0;
    }
    return slot;
}
static ActorSlot* checked(void* ptr, uint64_t domain, uint64_t epoch,
                          uint64_t field) {
    ActorSlot *slot=(ActorSlot*)ptr;
    if (fault || !slot || slot != current || !slot->active ||
        domain != slot->domain || epoch != slot->epoch ||
        field >= slot->fields) {
        fail(); return 0;
    }
    return slot;
}
void ores_actor_i64_heap_field_init(void* heap, uint64_t domain,
        uint64_t epoch, uint64_t index, int64_t value) {
    ActorSlot* a=checked(heap,domain,epoch,index);
    if (!a) return;
    if (a->phase != 1 || a->initialized_field[index]) { fail(); return; }
    a->initialized_field[index]=1;
    a->state[index]=value;
}
void ores_actor_i64_heap_field_init_mutable(void* heap, uint64_t domain,
        uint64_t epoch, uint64_t index, int64_t value) {
    ActorSlot* a=checked(heap,domain,epoch,index);
    if (!a) return;
    if (a->phase != 1 || a->initialized_field[index]) { fail(); return; }
    a->initialized_field[index]=1;
    a->mutable_field[index]=1;
    a->state[index]=value;
}
int64_t ores_actor_i64_heap_field_read(void* heap, uint64_t domain,
        uint64_t epoch, uint64_t index) {
    ActorSlot* a=checked(heap,domain,epoch,index);
    if (!a) return 0;
    if (a->phase != 2 || !a->initialized_field[index]) { fail(); return 0; }
    return a->state[index];
}
void ores_actor_i64_heap_field_write(void* heap, uint64_t domain,
        uint64_t epoch, uint64_t index, int64_t value) {
    ActorSlot* a=checked(heap,domain,epoch,index);
    if (!a) return;
    if (a->phase != 2 || !a->initialized_field[index] ||
        !a->mutable_field[index]) { fail(); return; }
    a->state[index]=value;
}
/* Volatile byte stores are observable side effects in the C abstract
 * machine: unlike ordinary dead stores, they cannot simply be discarded
 * when the compiler sees the next actor constructor overwrite the slot.
 * This is a single-threaded, CPU-side scrub, NOT protection from DMA,
 * physical probing, platform caches, a compromised kernel or an ISR. */
static void scrub_bytes(void *address, size_t length) {
    volatile unsigned char *dst=(volatile unsigned char*)address;
    size_t i;
    for (i=0; i<length; ++i) dst[i]=0;
}
/* Retire only after a guest callback has returned. Scrub even struct
 * padding, function pointers and queued payloads; retain generation
 * across slot reuse, and snapshot only completed delivery telemetry. */
static void retire_slot(ActorSlot *slot, ActorRef *ref) {
    const uint32_t generation=slot->generation;
    ref->last=slot->last;
    ref->deliveries=slot->deliveries;
    slot->active=0;
    active_count--;
    scrub_bytes(slot,sizeof(*slot));
    slot->generation=generation;
}
static ActorRef* spawn_actor_internal(
        void (*init)(void*, uint64_t, uint64_t),
        int64_t (*handler)(void*, uint64_t, uint64_t, int64_t),
        uint64_t fields,
        const ActorCapability *copied,
        uint64_t actor_count,
        int constructor_child) {
    unsigned i;
    ActorSlot *slot=0;
    ActorRef *ref;
    ActorSlot *parent=current;
    if (fault || !init || !handler || fields == 0 ||
        fields > ORES_STATIC_ACTOR_FIELDS || issued >= ORES_STATIC_ACTOR_LIMIT ||
        actor_count > ORES_STATIC_ACTOR_CAPABILITY_SLOTS ||
        (actor_count != 0 && !copied) ||
        (!constructor_child && current) ||
        (constructor_child && (!current || current->phase != 1))) {
        fail(); return 0;
    }
    for (i=0; i<ORES_STATIC_ACTOR_LIMIT; ++i) {
        if (!slots[i].active) { slot=&slots[i]; break; }
    }
    if (!slot) { fail(); return 0; }
    if (slot->generation == UINT32_MAX) { fail(); return 0; }
    slot->generation++;
    slot->active=1;
    slot->head=slot->tail=slot->count=0;
    slot->phase=0;
    slot->fields=(uint8_t)fields;
    slot->capability_count=(uint8_t)actor_count;
    slot->owned_child_mask=0;
    slot->domain=1; slot->epoch=slot->generation;
    slot->deliveries=0; slot->last=0;
    slot->init=init; slot->handler=handler;
    for (i=0; i<ORES_STATIC_ACTOR_CAPABILITY_SLOTS; ++i) {
        slot->capabilities[i].generation=0;
        slot->capabilities[i].slot=0;
        slot->capabilities[i].valid=0;
        slot->capabilities[i].reserved=0;
    }
    for (i=0; i<actor_count; ++i)
        slot->capabilities[i]=copied[i];
    for (i=0; i<ORES_STATIC_ACTOR_FIELDS; ++i) {
        slot->state[i]=0;
        slot->mutable_field[i]=0;
        slot->initialized_field[i]=0;
    }
    for (i=0; i<ORES_STATIC_MAILBOX_CAPACITY; ++i)
        slot->mailbox[i]=0;
    ref=&refs[issued++];
    ref->generation=slot->generation;
    ref->slot=(uint8_t)(slot-slots);
    ref->issued=1;
    ref->deliveries=0;
    ref->last=0;
    active_count++;

    current=slot;
    slot->phase=1;
    slot->init(slot,slot->domain,slot->epoch);
    slot->phase=0;
    current=parent;
    for (i=0; i<fields; ++i)
        if (!slot->initialized_field[i]) fail();
    if (fault) return 0;
    return ref;
}

void* oreslang_source_actor_spawn_with_capabilities(
        void (*init)(void*, uint64_t, uint64_t),
        int64_t (*handler)(void*, uint64_t, uint64_t, int64_t),
        uint64_t fields, void* const* actors, uint64_t actor_count) {
    unsigned i;
    ActorCapability copied[ORES_STATIC_ACTOR_CAPABILITY_SLOTS]={0};
    if (fault || current || actor_count > ORES_STATIC_ACTOR_CAPABILITY_SLOTS ||
        (actor_count != 0 && !actors)) {
        fail(); return 0;
    }
    for (i=0; i<actor_count; ++i) {
        ActorRef *target=lookup(actors[i]);
        ActorSlot *target_slot;
        if (!target || target->slot >= ORES_STATIC_ACTOR_LIMIT) {
            fail(); return 0;
        }
        target_slot=&slots[target->slot];
        if (!target_slot->active ||
            target_slot->generation != target->generation) {
            fail(); return 0;
        }
        copied[i].generation=target->generation;
        copied[i].slot=target->slot;
        copied[i].valid=1;
    }
    return spawn_actor_internal(
        init,handler,fields,copied,actor_count,0);
}
void* oreslang_source_actor_spawn(void (*init)(void*, uint64_t, uint64_t),
        int64_t (*handler)(void*, uint64_t, uint64_t, int64_t), uint64_t fields) {
    return oreslang_source_actor_spawn_with_capabilities(
        init,handler,fields,0,0);
}

void oreslang_source_actor_spawn_child_capability(
        void* heap, uint64_t capability,
        void (*init)(void*, uint64_t, uint64_t),
        int64_t (*handler)(void*, uint64_t, uint64_t, int64_t),
        uint64_t fields) {
    ActorSlot *parent=(ActorSlot*)heap;
    ActorRef *child;
    if (fault || !current || parent != current || parent->phase != 1 ||
        capability != parent->capability_count ||
        capability >= ORES_STATIC_ACTOR_CAPABILITY_SLOTS) {
        fail(); return;
    }
    child=spawn_actor_internal(init,handler,fields,0,0,1);
    if (!child || fault) return;
    parent->capabilities[capability].generation=child->generation;
    parent->capabilities[capability].slot=child->slot;
    parent->capabilities[capability].valid=1;
    parent->capabilities[capability].reserved=0;
    parent->owned_child_mask=(uint8_t)(
        parent->owned_child_mask | (uint8_t)(1u << capability));
    parent->capability_count++;
}

/* Enqueue never executes the target actor inline. During a guest turn only a
 * message handler may enqueue, and only to a different live actor. Self-send
 * is rejected because a stop-drain loop plus unbounded self-refill would defeat
 * the finite-turn/WCET contract of this constrained profile. Constructors
 * likewise cannot publish messages before successful initialization.
 *
 * fail_on_full=0 is the cooperative backpressure probe used by generated code:
 * a full, otherwise-valid target returns 0 without poisoning the session.
 * Invalid/stale capabilities and forbidden turn contexts still fail closed. */
static int enqueue_slot(ActorSlot* slot, int64_t payload, int fail_on_full) {
    if (!slot || fault || !slot->active) {
        fail(); return 0;
    }
    if (current && (current->phase != 2 || slot == current)) {
        fail(); return 0;
    }
    if (slot->count >= ORES_STATIC_MAILBOX_CAPACITY) {
        if (fail_on_full) fail();
        return 0;
    }
    slot->mailbox[slot->tail]=payload;
    slot->tail=(uint8_t)((slot->tail+1u)%ORES_STATIC_MAILBOX_CAPACITY);
    slot->count++;
    return 1;
}
static int enqueue_actor_message(void* actor, int64_t payload, int fail_on_full) {
    ActorSlot* slot=lookup_live(actor);
    if (!slot || fault) return 0;
    return enqueue_slot(slot,payload,fail_on_full);
}
static ActorSlot* capability_target(void* heap, uint64_t capability) {
    ActorSlot *owner=(ActorSlot*)heap;
    ActorCapability cap;
    ActorSlot *target;
    if (fault || !current || owner != current || current->phase != 2 ||
        capability >= current->capability_count) {
        fail(); return 0;
    }
    cap=current->capabilities[capability];
    if (!cap.valid || cap.slot >= ORES_STATIC_ACTOR_LIMIT) {
        fail(); return 0;
    }
    target=&slots[cap.slot];
    if (!target->active || target->generation != cap.generation ||
        target == current) {
        fail(); return 0;
    }
    return target;
}
void oreslang_source_actor_send(void* actor, int64_t payload) {
    (void)enqueue_actor_message(actor,payload,1);
}
int oreslang_source_actor_try_send(void* actor, int64_t payload) {
    return enqueue_actor_message(actor,payload,0);
}
void oreslang_source_actor_send_capability(
        void* heap, uint64_t capability, int64_t payload) {
    ActorSlot *target=capability_target(heap,capability);
    if (target) (void)enqueue_slot(target,payload,1);
}
int oreslang_source_actor_try_send_capability(
        void* heap, uint64_t capability, int64_t payload) {
    ActorSlot *target=capability_target(heap,capability);
    return target ? enqueue_slot(target,payload,0) : 0;
}
static ActorRef* owned_child_ref(ActorCapability capability) {
    unsigned i;
    if (!capability.valid || capability.slot >= ORES_STATIC_ACTOR_LIMIT) {
        fail(); return 0;
    }
    for (i=0; i<issued; ++i) {
        if (refs[i].issued &&
            refs[i].slot == capability.slot &&
            refs[i].generation == capability.generation)
            return &refs[i];
    }
    fail(); return 0;
}
void oreslang_source_actor_stop(void* actor) {
    ActorSlot* slot=lookup_live(actor);
    if (!slot || fault || current) { fail(); return; }
    while (slot->count && !fault) {
        int64_t value=slot->mailbox[slot->head];
        scrub_bytes(&slot->mailbox[slot->head],
                    sizeof(slot->mailbox[slot->head]));
        slot->head=(uint8_t)((slot->head+1u)%ORES_STATIC_MAILBOX_CAPACITY);
        slot->count--;
        current=slot;
        slot->phase=2;
        int64_t result=slot->handler(slot,slot->domain,slot->epoch,value);
        slot->phase=0;
        current=0;
        if (!fault) {
            slot->last=result;
            slot->deliveries++;
        }
    }
    if (fault) {
        /* A failed handler cannot leave *other* actors active. The callback
         * has returned, so this is a safe global retire/scrub boundary. */
        fail();
        return;
    }
    {
        uint8_t owned=slot->owned_child_mask;
        uint64_t capability;
        for (capability=0; capability<slot->capability_count; ++capability) {
            if ((owned & (uint8_t)(1u << capability)) != 0) {
                ActorRef *child=owned_child_ref(slot->capabilities[capability]);
                if (!child) return;
                oreslang_source_actor_stop(child);
                if (fault) { fail(); return; }
            }
        }
    }
    ActorRef *ref=lookup(actor);
    if (!ref) return;
    retire_slot(slot,ref);
}
