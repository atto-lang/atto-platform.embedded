# OresEmbedded

Embedded and RTOS integration for **Oreslang**, initially targeting Cortex-M and RISC-V.

> **Status: experimental scalar Cortex-M AOT demonstrated in Zephyr/QEMU.** The platform now compiles a limited real `.ores` function to ARM code and links it into Zephyr firmware, alongside a deterministic fixed-capacity C mailbox reference. **Oreslang actors, source-driven mailboxes and hardware isolation are not implemented.**

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

## Native Oreslang firmware smoke

`zephyr/ores/firmware.ores` compiles via the LLVM compiler's
`--target=thumbv7m-none-eabi` profile; Clang builds the ARM object, which
Zephyr links into firmware. `zephyr/src/main.c` invokes the exported guest
function, checks that the result is 42, and emits the pass marker under QEMU.

[Verified Zephyr 4.3.0 QEMU run](https://github.com/canonical-cloud/canonical-docs/actions/runs/38646752678) |
[Evidence and limitations](verification/2026-10-14-cortexm-aot.md) |
[Build instructions](zephyr/README.md).

The test proves native execution of an intentionally narrow arithmetic subset,
not source-level actor/runtime or device parity.


## Experimental source actor firmware

A restricted real Oreslang `.ores` program now defines two scalar actor
types and invokes source `spawn`/`send`/`stop`. The C adapter uses eight
static actor slots, generation-tagged refs and fixed FIFO queues, and links
compiler-generated actor handlers into Zephyr/QEMU. Host sanitizer tests
cover mailbox overflow and stale capabilities. Read
[static source actor contract](docs/STATIC_SOURCE_ACTORS.md).

The current proof is limited to source **bootstrap-to-actor** messaging and
handler execution; actor-to-actor sends from inside handlers, await,
supervision, and hardware isolation are not implemented.
