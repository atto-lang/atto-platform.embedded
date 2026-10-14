# Native embedded target acceptance

Use this checklist to claim a working Oreslang target only when **all** items hold:

- Exact target triple, board, RTOS/SDK/toolchain versions pinned and published.
- An actual `.ores` actor fixture compiles AOT and links with the runtime.
- Compile-time sendability and borrow checks reject unsafe actor cross-boundary values.
- Statically bounded code, stack, heap/arena and mailbox budgets are enforced.
- Scheduler operations, supervision, error propagation and cancellation have target tests.
- Interrupt handlers pass events through an ISR-safe queue (the current reference mailbox is not one).
- Capability-checked HAL operations replace unrestricted native access.
- On-board or QEMU integration run proves correctness, bounded resource behavior and failure policy.
- MPU/MMU isolation only claimed when enabled, tested and hardware-supported.
- Worst-case latency or hard-real-time claims require separate measurement/formal analysis.

The host CI, `build-freestanding.sh` and Zephyr C example do **not** by themselves satisfy actor or Oreslang compiler acceptance.
