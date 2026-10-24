#ifndef ORES_EMBEDDED_STATIC_ACTORS_H
#define ORES_EMBEDDED_STATIC_ACTORS_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define ORES_STATIC_ACTOR_LIMIT 8u
#define ORES_STATIC_ACTOR_FIELDS 8u
#define ORES_STATIC_MAILBOX_CAPACITY 16u
#define ORES_STATIC_ACTOR_CAPABILITY_SLOTS 8u
/* Adapter static variables must remain below this admission ceiling. */
#define ORES_STATIC_ACTOR_STORAGE_BUDGET_BYTES 3072u
/* Single-threaded reference only: NOT reentrant, ISR-safe, or parallel.
 * Boot-only initializer; after any actor has been spawned, a subsequent
 * reset is rejected and sets a sticky fault. Never reuse address capabilities
 * across sessions without device reboot. Tests must use fresh processes.
 * Field init is permitted only during generated actor constructors.
 * Handler reads/writes are turn-scoped and fail on immutable writes.
 */
void ores_embedded_actors_reset(void);
int ores_embedded_actors_faulted(void);
unsigned ores_embedded_actors_spawns(void);
unsigned ores_embedded_actors_deliveries(unsigned index);
int64_t ores_embedded_actors_last(unsigned index);
unsigned ores_embedded_actors_active(void);
/* Runtime imports required by actual compiler-generated guest actors. */
void* oreslang_source_actor_spawn(void (*init)(void*, uint64_t, uint64_t),
    int64_t (*handler)(void*, uint64_t, uint64_t, int64_t), uint64_t fields);
/* Runtime-only capability-slot prerequisite. The supplied live ActorRef values
 * are validated at spawn and copied as compact slot+generation capabilities;
 * their pointer addresses are never retained by the actor. */
void* oreslang_source_actor_spawn_with_capabilities(
    void (*init)(void*, uint64_t, uint64_t),
    int64_t (*handler)(void*, uint64_t, uint64_t, int64_t),
    uint64_t fields, void* const* actors, uint64_t actor_count);
/* Strict send: bounded enqueue. Host/bootstrap sends and handler-to-other-actor
 * sends are admitted; self-send from a handler and constructor sends fail closed.
 * Mailbox overflow is a sticky session fault. */
void oreslang_source_actor_send(void* actor, int64_t payload);
/* Backpressure primitive: same capability/turn rules as strict send, but a full
 * live target returns 0 without faulting so generated code can yield/retry.
 * Returns 1 only when the payload was enqueued. */
int oreslang_source_actor_try_send(void* actor, int64_t payload);
/* Handler-only capability send. heap must be the current actor slot; capability
 * indices are bounded and generation-fenced. try_send returns 0 only when the
 * live target mailbox is full; stale/out-of-range use fails closed. */
void oreslang_source_actor_send_capability(
    void* heap, uint64_t capability, int64_t payload);
int oreslang_source_actor_try_send_capability(
    void* heap, uint64_t capability, int64_t payload);
void oreslang_source_actor_stop(void* actor);
void ores_actor_i64_heap_field_init(void* heap, uint64_t domain,
    uint64_t epoch, uint64_t index, int64_t value);
void ores_actor_i64_heap_field_init_mutable(void* heap, uint64_t domain,
    uint64_t epoch, uint64_t index, int64_t value);
int64_t ores_actor_i64_heap_field_read(void* heap, uint64_t domain,
    uint64_t epoch, uint64_t index);
void ores_actor_i64_heap_field_write(void* heap, uint64_t domain,
    uint64_t epoch, uint64_t index, int64_t value);
#ifdef __cplusplus
}
#endif
#endif
