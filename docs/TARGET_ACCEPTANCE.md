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

## Scalar firmware precursor — verified

An intermediate prerequisite, native scalar guest execution in Zephyr/QEMU,
passed on 2026-10-14; see
[exact-source run #38646752678](https://github.com/canonical-cloud/canonical-docs/actions/runs/38646752678).
The actor-specific acceptance criteria above remain **unmet**.

## Actor firmware precursor — independently verified

The previous scalar-only summary above is historical. On October 15, 2026,
the full `.ores` source-actor firmware (two actors, four messages and static
storage) compiled with the LLVM backend and booted under Zephyr 4.3.0
`qemu_cortex_m3`: [original acceptance
run](https://github.com/benefactor-cc/backend.rs/actions/runs/38757294870).
A later [exact LLVM PR-head
run](https://github.com/benefactor-cc/backend.rs/actions/runs/38767714704)
passed both compiler admission and QEMU jobs. The real guest emitted:
`ORESLANG_CORTEXM_ACTORS_PASS count=2 delivery=4 allocation=static`.

The verified linked image used 16,140 bytes FLASH and 7,440 bytes RAM on
QEMU's Cortex-M3 memory profile. These figures do **not** establish WCET,
worst-case stack usage, physical MPU protection, ISR safety or hardware-board
compatibility. The actor runtime is still a **single-threaded static adapter**,
not complete target supervision and cancellation. The checklist above remains
the bar for graduating from **prototype** to a supported production target.

See [board portability](BOARD_PORTABILITY.md) for ARMv7E-M UNO R4,
ARMv8-M Pico 2 and explicitly excluded ARMv6-M Pico / RISC-V combinations.
