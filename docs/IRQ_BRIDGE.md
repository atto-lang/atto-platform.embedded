# Cortex-M IRQ-to-actor event bridge — experimental

`ores_embedded_irq_bridge` is a standalone **single-producer/single-consumer**
bounded queue of scalar events (`uint32_t source,value`). The **designated,
non-reentrant ISR** only calls `ores_irq_bridge_isr_push()`. One foreground
consumer calls `ores_irq_bridge_task_pop()` and, after returning to normal
task context, checks the routing policy and invokes the existing actor
`oreslang_source_actor_send()` outside any callback. It never treats a scalar
event as an actor capability or heap pointer.

The bridge does not modify the existing event-loop mailbox or actor ABI.
It uses always-lock-free C11 32-bit atomics and acquire/release publication,
no dynamic allocation, no spin retry in ISR, and a deterministic FULL return.
Initialize once while interrupts are disabled and producer/consumer quiescent.
After bootstrap, no reset/reinit during active traffic. Failed full pushes
do not overwrite queued data. Successful pops scrub the dequeued slot.

**Not supported:** nested/multiple ISR producers, multiple task consumers,
direct actor runtime calls from ISR, user-chosen pointer messages, DMA
access, actor access while a callback runs, interrupt masking guarantees,
peripheral interrupt ack, fault recovery, stack/WCET bounds, MPU isolation,
physical device boot, or hard real-time certification. A board adapter must
provide interrupt priority and dispatch policy and demonstrate its actual
atomic implementation. An ARM cross-build and hardware-in-loop test are
mandatory before making any physical ISR-safety claim.
