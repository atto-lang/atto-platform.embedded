#include "ores_embedded_static_actors.h"

/* All actor storage and message queues are static. No allocation, libc,
 * OS calls or threads. One actor turn is entered at a time. */
typedef struct {
    uint64_t domain, epoch;
    uint32_t generation;
    uint8_t active, head, tail, count, fields;
    uint8_t mutable_field[ORES_STATIC_ACTOR_FIELDS];
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
} ActorRef;

static ActorSlot slots[ORES_STATIC_ACTOR_LIMIT];
static ActorRef refs[ORES_STATIC_ACTOR_LIMIT];
static ActorSlot *current;
static unsigned issued, active_count;
static int fault;

static void fail(void) { fault = 1; }
void ores_embedded_actors_reset(void) {
    unsigned i, j;
    if (current) { fail(); return; }
    /* Do not reuse old address-valued refs across reset for live guests.
       Reset is only legal before guest bootstrap, never during a turn. */
    for (i=0; i<ORES_STATIC_ACTOR_LIMIT; ++i) {
        slots[i].active = 0;
        slots[i].head = slots[i].tail = slots[i].count = slots[i].fields = 0;
        slots[i].deliveries = 0;
        slots[i].last = 0;
        slots[i].init = 0; slots[i].handler = 0;
        refs[i].issued = 0;
        for (j=0; j<ORES_STATIC_ACTOR_FIELDS; ++j) {
            slots[i].state[j] = 0;
            slots[i].mutable_field[j] = 0;
        }
        for (j=0; j<ORES_STATIC_MAILBOX_CAPACITY; ++j)
            slots[i].mailbox[j] = 0;
    }
    issued = 0; active_count = 0; fault = 0;
}
int ores_embedded_actors_faulted(void) { return fault; }
unsigned ores_embedded_actors_spawns(void) { return issued; }
unsigned ores_embedded_actors_active(void) { return active_count; }
unsigned ores_embedded_actors_deliveries(unsigned n) {
    if (n>=issued || !refs[n].issued) { fail(); return 0; }
    return slots[refs[n].slot].deliveries;
}
int64_t ores_embedded_actors_last(unsigned n) {
    if (n>=issued || !refs[n].issued) { fail(); return 0; }
    return slots[refs[n].slot].last;
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
    slot=&slots[ref->slot];
    if (!slot->active || slot->generation != ref->generation) {
        fail(); return 0;
    }
    return slot;
}
static ActorSlot* checked(void* ptr, uint64_t domain, uint64_t epoch,
                          uint64_t field) {
    ActorSlot *slot=(ActorSlot*)ptr;
    if (!slot || slot != current || domain != slot->domain ||
        epoch != slot->epoch || field >= slot->fields) {
        fail(); return 0;
    }
    return slot;
}
void ores_actor_i64_heap_field_init(void* heap, uint64_t domain,
        uint64_t epoch, uint64_t index, int64_t value) {
    ActorSlot* a=checked(heap,domain,epoch,index);
    if (!a) return;
    a->state[index]=value;
}
void ores_actor_i64_heap_field_init_mutable(void* heap, uint64_t domain,
        uint64_t epoch, uint64_t index, int64_t value) {
    ActorSlot* a=checked(heap,domain,epoch,index);
    if (!a) return;
    a->mutable_field[index]=1;
    a->state[index]=value;
}
int64_t ores_actor_i64_heap_field_read(void* heap, uint64_t domain,
        uint64_t epoch, uint64_t index) {
    ActorSlot* a=checked(heap,domain,epoch,index);
    return a ? a->state[index] : 0;
}
void ores_actor_i64_heap_field_write(void* heap, uint64_t domain,
        uint64_t epoch, uint64_t index, int64_t value) {
    ActorSlot* a=checked(heap,domain,epoch,index);
    if (!a) return;
    if (!a->mutable_field[index]) { fail(); return; }
    a->state[index]=value;
}
void* oreslang_source_actor_spawn(void (*init)(void*, uint64_t, uint64_t),
        int64_t (*handler)(void*, uint64_t, uint64_t, int64_t), uint64_t fields) {
    unsigned i;
    ActorSlot *slot=0;
    ActorRef *ref;
    if (fault || current || !init || !handler || fields == 0 ||
        fields > ORES_STATIC_ACTOR_FIELDS || issued >= ORES_STATIC_ACTOR_LIMIT) {
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
    slot->fields=(uint8_t)fields;
    slot->domain=1; slot->epoch=slot->generation;
    slot->deliveries=0; slot->last=0;
    slot->init=init; slot->handler=handler;
    for (i=0; i<fields; ++i) {
        slot->state[i]=0;
        slot->mutable_field[i]=0;
    }
    ref=&refs[issued++];
    ref->generation=slot->generation;
    ref->slot=(uint8_t)(slot-slots);
    ref->issued=1;
    active_count++;
    current=slot;
    slot->init(slot,slot->domain,slot->epoch);
    current=0;
    return ref;
}
void oreslang_source_actor_send(void* actor, int64_t payload) {
    ActorSlot* slot=lookup_live(actor);
    if (!slot || fault || current || slot->count >= ORES_STATIC_MAILBOX_CAPACITY) {
        fail(); return;
    }
    slot->mailbox[slot->tail]=payload;
    slot->tail=(uint8_t)((slot->tail+1u)%ORES_STATIC_MAILBOX_CAPACITY);
    slot->count++;
}
void oreslang_source_actor_stop(void* actor) {
    ActorSlot* slot=lookup_live(actor);
    if (!slot || fault || current) { fail(); return; }
    while (slot->count && !fault) {
        int64_t value=slot->mailbox[slot->head];
        slot->mailbox[slot->head]=0;
        slot->head=(uint8_t)((slot->head+1u)%ORES_STATIC_MAILBOX_CAPACITY);
        slot->count--;
        current=slot;
        slot->last=slot->handler(slot,slot->domain,slot->epoch,value);
        current=0;
        slot->deliveries++;
    }
    if (fault) return;
    slot->active=0;
    active_count--;
}
