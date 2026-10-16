#include "ores_embedded_irq_bridge.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdatomic.h>
#include <sched.h>
#include <string.h>

#define TOTAL 40000u
static ores_irq_bridge q;
static _Atomic unsigned failures;

static void *producer(void *unused) {
    unsigned i;
    (void)unused;
    for (i=0; i<TOTAL; ++i) {
        ores_irq_status status;
        do {
            status = ores_irq_bridge_isr_push(&q, i % 17u, i ^ UINT32_C(0xdeadbeef));
            if (status == ORES_IRQ_FULL) sched_yield();
        } while (status == ORES_IRQ_FULL);
        if (status != ORES_IRQ_OK) atomic_fetch_add(&failures, 1u);
    }
    return NULL;
}
static void *consumer(void *unused) {
    unsigned i;
    (void)unused;
    for (i=0; i<TOTAL; ++i) {
        ores_irq_event event = { UINT32_MAX, UINT32_MAX };
        ores_irq_status status;
        do {
            status = ores_irq_bridge_task_pop(&q, &event);
            if (status == ORES_IRQ_EMPTY) sched_yield();
        } while (status == ORES_IRQ_EMPTY);
        if (status != ORES_IRQ_OK || event.source != i % 17u ||
            event.value != (i ^ UINT32_C(0xdeadbeef)))
            atomic_fetch_add(&failures, 1u);
    }
    return NULL;
}

int main(void) {
    ores_irq_event out = {123, 456};
    pthread_t p, c;
    unsigned i;
    assert(ores_irq_bridge_init(NULL) == ORES_IRQ_INVALID);
    assert(ores_irq_bridge_isr_push(NULL, 1, 2) == ORES_IRQ_INVALID);
    assert(ores_irq_bridge_task_pop(NULL, &out) == ORES_IRQ_INVALID);
    assert(ores_irq_bridge_init(&q) == ORES_IRQ_OK);
    assert(ores_irq_bridge_task_pop(&q, NULL) == ORES_IRQ_INVALID);
    assert(ores_irq_bridge_task_pop(&q, &q.slots[0]) == ORES_IRQ_INVALID);
    assert(ores_irq_bridge_task_pop(&q, &out) == ORES_IRQ_EMPTY);
    assert(out.source == 123 && out.value == 456);
    for (i=0; i<ORES_IRQ_BRIDGE_CAPACITY; ++i)
        assert(ores_irq_bridge_isr_push(&q, i, i+42u) == ORES_IRQ_OK);
    assert(ores_irq_bridge_isr_push(&q, 99u, 99u) == ORES_IRQ_FULL);
    for (i=0; i<ORES_IRQ_BRIDGE_CAPACITY; ++i) {
        assert(ores_irq_bridge_task_pop(&q, &out) == ORES_IRQ_OK);
        assert(out.source == i && out.value == i+42u);
        assert(q.slots[i].source == 0 && q.slots[i].value == 0);
    }
    assert(ores_irq_bridge_task_pop(&q, &out) == ORES_IRQ_EMPTY);
    /* Overflow of monotonic 32-bit indices must not corrupt FIFO order. */
    atomic_store(&q.head, UINT32_MAX - 3u);
    atomic_store(&q.tail, UINT32_MAX - 3u);
    for (i=0; i<ORES_IRQ_BRIDGE_CAPACITY; ++i)
        assert(ores_irq_bridge_isr_push(&q, i, i+100u) == ORES_IRQ_OK);
    for (i=0; i<ORES_IRQ_BRIDGE_CAPACITY; ++i) {
        assert(ores_irq_bridge_task_pop(&q, &out) == ORES_IRQ_OK);
        assert(out.source == i && out.value == i+100u);
    }
    assert(ores_irq_bridge_init(&q) == ORES_IRQ_OK);
    atomic_init(&failures, 0u);
    assert(pthread_create(&p, NULL, producer, NULL) == 0);
    assert(pthread_create(&c, NULL, consumer, NULL) == 0);
    assert(pthread_join(p, NULL) == 0);
    assert(pthread_join(c, NULL) == 0);
    assert(atomic_load(&failures) == 0u);
    assert(ores_irq_bridge_task_pop(&q, &out) == ORES_IRQ_EMPTY);
    assert(atomic_load(&q.head) == TOTAL && atomic_load(&q.tail) == TOTAL);
    return 0;
}
