# OresEmbedded

Embedded and RTOS integration for **Oreslang**, initially targeting Cortex-M and RISC-V.

> **Status: bootstrap.** The first deliverable is a deterministic, fixed-capacity, no-heap C mailbox **reference component** with runnable host tests, plus target profiles. It does **not** yet generate Oreslang firmware, implement Oreslang actors, or provide a hardware sandbox.

## Profiles

- `embedded-tiny`: no OS, freestanding ABI, AOT only, bounded static memory.
- `embedded-rtos`: Zephyr/FreeRTOS integration and capability HAL, after explicit runtime contracts.
- `iot-edge`: larger native Linux deployments belong to existing LLVM backend plus IoT services.

## Quick start

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Host tests validate the mailbox reference, **not** a real microcontroller image or the Oreslang compiler. See `docs/ARCHITECTURE.md`, `docs/ISOLATION_AND_REALTIME.md`, and `profiles/`.

## Upstream

[oreslang-source.llvm](https://github.com/ores-truffle-oreslang/oreslang-source.llvm) provides the intended native AOT code-generation foundation, once the compiler supports the necessary language/runtime subset.
