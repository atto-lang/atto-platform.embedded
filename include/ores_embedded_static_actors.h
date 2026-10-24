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
/* Strict send: bounded enqueue. Host/bootstrap sends and handler-to-other-actor
 * sends are admitted; self-send from a handler and constructor sends fail closed.
 * Mailbox overflow is a sticky session fault. */
void oreslang_source_actor_send(void* actor, int64_t payload);
/* Backpressure primitive: same capability/turn rules as strict send, but a full
 * live target returns 0 without faulting so generated code can yield/retry.
 * Returns 1 only when the payload was enqueued. */
int oreslang_source_actor_try_send(void* actor, int64_t payload);
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
