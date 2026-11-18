# OresEmbedded

## AttoLang naming migration and language authority (2026-11-18)

The public language name is **AttoLang** (formerly **Oreslang**). This is a historical `ores-truffle-oreslang/oreslang-platform.embedded` checkout; its current ecosystem routing is **[atto-lang/atto-platform.embedded](https://github.com/atto-lang/atto-platform.embedded)**. Do not claim device/board or hard-real-time execution from host tests or QEMU alone. A similarly named repository is not evidence of identical commits, GitHub issue/PR history or feature status.

**[atto-lang/atto-spec](https://github.com/atto-lang/atto-spec) is the canonical normative language specification.** Use [atto-platform-conformance](https://github.com/atto-lang/atto-platform-conformance) for executable positive/negative guest fixtures, [atto-compilers-parity-testing](https://github.com/atto-lang/atto-compilers-parity-testing) for cross-backend comparisons, and [atto-common-source](https://github.com/atto-lang/atto-common-source) for portable `.atto` core/std code. Modern proposed language rules include `snake_case` callable naming, `borrow!`/`take!`/`share!`/`copy!`/`copy!!`, [default nonprimitive argument borrowing](https://github.com/atto-lang/atto-spec/blob/main/language/implicit-argument-borrow-v1.md), [static `define const` including one-time `os.read_env`](https://github.com/atto-lang/atto-spec/blob/main/language/file-module-const-v1.md), and [`<<<`/`>>>`/`ashr` shifts versus `<<`/`>>` stream I/O](https://github.com/atto-lang/atto-spec/blob/main/language/bitwise-and-stream-operators-v1.md). Contract status and compiler execution evidence take precedence over old README descriptions.

Retain actual legacy `.ores` files, CLI/package/ABI names and reproducible commands below until each replacement is separately implemented and verified. No Python scripts, CI/test support or generators for new AttoLang work; favor Rust support tooling. No feature or parity claim without exact SHA/tree, toolchain/target, real compiled guest execution and negative tests; host-only tests or zero executed CI steps count as **not executed**.

Embedded and RTOS integration for **Oreslang**, initially targeting Cortex-M and RISC-V.

> **Status: experimental scalar Cortex-M AOT and restricted source actors.** The platform compiles a limited real `.ores` function to ARM code and links it into Zephyr firmware. A bounded static actor adapter supports compiler-generated scalar actor handlers, generation-fenced refs/capability slots and fixed mailboxes. Full source-level actor-to-actor capability binding, await/supervision and hardware isolation are not implemented.

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

The runtime ABI now supports bounded **handler-to-other-actor enqueue** without
inline receiver execution, plus a non-fatal full-mailbox `try_send` primitive.
It also has eight generation-fenced capability slots per actor; the ARM C ABI
stand-in forwards through a copied slot rather than a global target pointer.
LLVM PR #943 implements the matching hosted runtime foundation. A real `.ores`
handler still needs an agreed cross-backend source binding for those ActorRef
capabilities before actor-to-actor messaging can be claimed.
Await, supervision, and hardware isolation are also not implemented.
