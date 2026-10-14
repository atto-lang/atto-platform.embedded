# Embedded platform architecture

## Purpose

Provide target profiles, bounded actors, capability-oriented hardware APIs and board-specific build tooling on top of Oreslang's eventual native-AOT toolchain. The current fixed-capacity **C mailbox is a reference component**, not the Oreslang actor runtime.

## Target plan

- Tiny: bare-metal/RTOS ARM Cortex-M and RV32, fixed memory budgets, no runtime JIT, no GC.
- Future HAL: GPIO, UART, SPI, I2C, timers, DMA, interrupt routing and watchdog, each declared as an explicit capability.
- Actor admission: statically bound the number of actors, per-actor mailbox, stack, heap/arena and CPU quotas; reject programs that exceed limits.
- Cooperation: explicit compiler safepoints and `rt cooperate` integration only after verified lowering.
- Supervision: bounded restarts with failure policy; do not assume unlimited resources.

## Mailbox v0

- Eight fixed slots, 64 payload bytes per slot, zero dynamic allocation.
- Single-thread/cooperative event-loop use only. Not safe for concurrent ISR/CPU access without an external critical section or atomics.
- FIFO, non-destructive failure on too-small output buffers, scrub slot bytes on pop.
- Neither MPU isolation nor real-time latency guarantees are implied.

## Build and verification milestones

1. Compiler-emitted Cortex-M/RV32 object with correct calling conventions and linker scripts.
2. Boot on QEMU/board and run bounded mailbox fixture via Oreslang source.
3. Prove/measure allocator, stack, scheduling and interrupt interactions.
4. Perform hardware fault-injection; verify MPU region configuration where available.
5. Distinguish **worst-case response time evidence** from ordinary functional correctness.

Current CTest cases run on Linux host only.
