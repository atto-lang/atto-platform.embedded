#include "ores_embedded_irq_bridge.h"
#include <stddef.h>
#include <stdint.h>

_Static_assert(sizeof(unsigned int) == sizeof(uint32_t),
               "IRQ bridge requires 32-bit unsigned int");
_Static_assert(ATOMIC_INT_LOCK_FREE == 2,
               "ISR bridge requires always-lock-free 32-bit atomics");
_Static_assert(ORES_IRQ_BRIDGE_CAPACITY > 0 &&
               ORES_IRQ_BRIDGE_CAPACITY < UINT32_MAX / 2u,
               "invalid bounded ring capacity");

/* Erase the complete event representation, including any padding bytes.
 * Volatile character stores avoid whole-struct alias assumptions and do not
 * permit dead-store elimination on supported C11 compilers. */
static void scrub_slot(ores_irq_event *slot) {
    volatile unsigned char *p = (volatile unsigned char *)(void *)slot;
    size_t i;
    for (i = 0; i < sizeof(*slot); ++i) p[i] = 0;
}

/* The queue itself is intentionally static/caller-owned, with no malloc,
 * calls into the actor runtime, syscalls, mutexes, or retry loops.
 * A producer cannot reuse a slot until the consumer release-publishes head.
 * On ARM, C11 acquire/release maps to target-specific memory barriers. */
ores_irq_status ores_irq_bridge_init(ores_irq_bridge *bridge) {
    unsigned i;
    if (!bridge) return ORES_IRQ_INVALID;
    for (i = 0; i < ORES_IRQ_BRIDGE_CAPACITY; ++i)
        scrub_slot(&bridge->slots[i]);
    atomic_init(&bridge->head, 0);
    atomic_init(&bridge->tail, 0);
    return ORES_IRQ_OK;
}

ores_irq_status ores_irq_bridge_isr_push(
    ores_irq_bridge *bridge, uint32_t source, uint32_t value) {
    uint32_t tail, head, used;
    if (!bridge) return ORES_IRQ_INVALID;
    tail = atomic_load_explicit(&bridge->tail, memory_order_relaxed);
    head = atomic_load_explicit(&bridge->head, memory_order_acquire);
    used = tail - head; /* Defined wraparound arithmetic. */
    if (used > ORES_IRQ_BRIDGE_CAPACITY) return ORES_IRQ_INVALID;
    if (used == ORES_IRQ_BRIDGE_CAPACITY) return ORES_IRQ_FULL;
    bridge->slots[tail % ORES_IRQ_BRIDGE_CAPACITY].source = source;
    bridge->slots[tail % ORES_IRQ_BRIDGE_CAPACITY].value = value;
    atomic_store_explicit(&bridge->tail, tail + 1u, memory_order_release);
    return ORES_IRQ_OK;
}

static int overlaps_bridge(const ores_irq_bridge *bridge, const void *p) {
    uintptr_t a = (uintptr_t)p;
    uintptr_t b = (uintptr_t)bridge;
    return (a >= b) ? (a - b < sizeof(*bridge)) :
                     (b - a < sizeof(ores_irq_event));
}

ores_irq_status ores_irq_bridge_task_peek(
    const ores_irq_bridge *bridge, ores_irq_event *out) {
    uint32_t head, tail, used;
    if (!bridge || !out || overlaps_bridge(bridge, out))
        return ORES_IRQ_INVALID;
    head = atomic_load_explicit(&bridge->head, memory_order_relaxed);
    tail = atomic_load_explicit(&bridge->tail, memory_order_acquire);
    used = tail - head;
    if (used > ORES_IRQ_BRIDGE_CAPACITY) return ORES_IRQ_INVALID;
    if (used == 0u) return ORES_IRQ_EMPTY;
    *out = bridge->slots[head % ORES_IRQ_BRIDGE_CAPACITY];
    return ORES_IRQ_OK;
}

ores_irq_status ores_irq_bridge_task_pop(
    ores_irq_bridge *bridge, ores_irq_event *out) {
    uint32_t head, tail, used, index;
    if (!bridge || !out || overlaps_bridge(bridge, out))
        return ORES_IRQ_INVALID;
    head = atomic_load_explicit(&bridge->head, memory_order_relaxed);
    tail = atomic_load_explicit(&bridge->tail, memory_order_acquire);
    used = tail - head;
    if (used > ORES_IRQ_BRIDGE_CAPACITY) return ORES_IRQ_INVALID;
    if (used == 0) return ORES_IRQ_EMPTY;
    index = head % ORES_IRQ_BRIDGE_CAPACITY;
    *out = bridge->slots[index];
    /* Scrub before publishing the slot to the producer. CPU-side only;
     * does not establish DMA, cache, MPU, or interrupt priority isolation. */
    scrub_slot(&bridge->slots[index]);
    atomic_store_explicit(&bridge->head, head + 1u, memory_order_release);
    return ORES_IRQ_OK;
}

