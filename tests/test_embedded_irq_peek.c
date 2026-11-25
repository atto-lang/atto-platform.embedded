#include "ores_embedded_irq_bridge.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>

int main(void) {
    ores_irq_bridge q;
    ores_irq_event out = {77u, 88u};
    unsigned i;
    assert(ores_irq_bridge_init(&q) == ORES_IRQ_OK);
    assert(ores_irq_bridge_task_peek(NULL, &out) == ORES_IRQ_INVALID);
    assert(ores_irq_bridge_task_peek(&q, NULL) == ORES_IRQ_INVALID);
    assert(ores_irq_bridge_task_peek(&q, &q.slots[0]) == ORES_IRQ_INVALID);
    assert(ores_irq_bridge_task_peek(&q, &out) == ORES_IRQ_EMPTY);
    assert(out.source == 77u && out.value == 88u);
    assert(ores_irq_bridge_isr_push(&q, 101u, 202u) == ORES_IRQ_OK);
    for (i = 0; i < 3u; ++i) {
        assert(ores_irq_bridge_task_peek(&q, &out) == ORES_IRQ_OK);
        assert(out.source == 101u && out.value == 202u);
        assert(atomic_load(&q.head) == 0u);
    }
    assert(ores_irq_bridge_task_pop(&q, &out) == ORES_IRQ_OK);
    assert(out.source == 101u && out.value == 202u);
    assert(ores_irq_bridge_task_peek(&q, &out) == ORES_IRQ_EMPTY);
    for (i = 0; i < ORES_IRQ_BRIDGE_CAPACITY; ++i)
        assert(ores_irq_bridge_isr_push(&q, i + 1u, 100u + i) == ORES_IRQ_OK);
    assert(ores_irq_bridge_isr_push(&q, 9u, 9u) == ORES_IRQ_FULL);
    for (i = 0; i < ORES_IRQ_BRIDGE_CAPACITY; ++i) {
        assert(ores_irq_bridge_task_peek(&q, &out) == ORES_IRQ_OK);
        assert(out.source == i + 1u && out.value == 100u + i);
        assert(ores_irq_bridge_task_pop(&q, &out) == ORES_IRQ_OK);
    }
    atomic_store(&q.head, 0u);
    atomic_store(&q.tail, ORES_IRQ_BRIDGE_CAPACITY + 1u);
    out.source = 77u; out.value = 88u;
    assert(ores_irq_bridge_task_peek(&q, &out) == ORES_IRQ_INVALID);
    assert(out.source == 77u && out.value == 88u);
    return 0;
}
