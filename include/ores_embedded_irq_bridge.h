#ifndef ORES_EMBEDDED_IRQ_BRIDGE_H
#define ORES_EMBEDDED_IRQ_BRIDGE_H

#include <stdatomic.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* One single, non-reentrant ISR producer and one foreground consumer.
 * The ISR must not call any actor runtime API; only enqueue scalar events.
 * Do not call init while either endpoint can run. No DMA access to this ring.
 * Nested or multi-IRQ producers require external serialization and are NOT
 * supported by this contract. */
#define ORES_IRQ_BRIDGE_CAPACITY 8u

typedef enum {
    ORES_IRQ_OK = 0,
    ORES_IRQ_FULL = 1,
    ORES_IRQ_EMPTY = 2,
    ORES_IRQ_INVALID = 3
} ores_irq_status;

typedef struct {
    uint32_t source;
    uint32_t value;
} ores_irq_event;

typedef struct {
    _Atomic unsigned int head; /* 32-bit; only foreground task writes. */
    _Atomic unsigned int tail; /* 32-bit; only designated ISR writes. */
    ores_irq_event slots[ORES_IRQ_BRIDGE_CAPACITY];
} ores_irq_bridge;

/* Boot/quiescent-only initialization; never called in an ISR. */
ores_irq_status ores_irq_bridge_init(ores_irq_bridge *bridge);
/* Bounded, allocation-free, lock-free ISR operation. */
ores_irq_status ores_irq_bridge_isr_push(
    ores_irq_bridge *bridge, uint32_t source, uint32_t value);
/* Foreground only: caller routes event through checked actor send API.
 * Event contains no actor heap pointers or capabilities. The consumed slot's
 * complete object representation is erased via volatile character stores
 * before its release publication to the producer (CPU-side erasure only). */
/* Foreground-only, non-consuming inspection of the oldest published event.
 * Uses acquire on the ISR tail; the consumer must serialize peek and pop.
 * Rejection/EMPTY do not modify *out. Aliases into queue storage are denied.
 * No allocation, waiting, ISR callbacks or actor runtime calls. */
ores_irq_status ores_irq_bridge_task_peek(
    const ores_irq_bridge *bridge, ores_irq_event *out);

ores_irq_status ores_irq_bridge_task_pop(
    ores_irq_bridge *bridge, ores_irq_event *out);

#ifdef __cplusplus
}
#endif
#endif

