# Isolation, interrupts and timing

## No accidental hard-real-time claims

The reference mailbox has a fixed-size loop and fixed memory budget, but this alone does **not** make the runtime hard real-time. Scheduler, interrupt priority, WCET, stack usage, shared resources and compiler-generated code all contribute to end-to-end latency.

## Isolation levels

1. Language-level: ownership/borrowing and explicit sendability, subject to compiler soundness.
2. Runtime-level: bounded arenas and capability checks, subject to native implementation.
3. Hardware-level: MPU/MMU-backed protection only on capable SoCs, with verified privilege configuration.

A Cortex-M without configured MPU domains cannot provide per-actor hardware address-space isolation. Do not describe cooperative actors as OS processes.

## Interrupt handling

Interrupt service routines should enqueue fixed-size events through a proven ISR-safe queue; the reference mailbox is **not** ISR-safe. No allocation, blocking wait, hot code loading or arbitrary host callbacks from ISRs. Backpressure and overrun policies must be explicit.

## Firmware integrity

Signed firmware, verified boot when supported, rollback protection and fail-safe update policies belong in board-specific integration work. They are not included in the reference mailbox.
