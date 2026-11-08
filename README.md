# OresEmbedded

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

## Repository ownership and migration (2026-11-08)

**Current home:** `ores-truffle-oreslang/oreslang-platform.embedded` (legacy Oreslang); public language name **AttoLang**. Retain existing `.ores` files and `oreslang-*` dependencies/CLI compatibility; don't infer backend or device capabilities from branding.

- **Owns:** embedded target constraints, small-footprint runtime integration and native hardware receipts.
- **Excludes:** full desktop/mobile runtime promises or managed cloud operations.
- **Organizations:** [atto-lang](https://github.com/atto-lang) owns compiler/runtime target adapters, language portability, stdlib and conformance. [atto-co](https://github.com/atto-co) owns hosted control planes, deployment, tenancy, billing and managed infrastructure. A similarly named destination repo is not an executed transfer or equivalent runtime.
- **Proposed initial migration:** approved *ownership transfer* of this repository into `atto-lang/oreslang-platform.embedded` **without renaming**. Preserve the numeric GitHub ID and Git tree. No direct `atto-co` migration is authorized in the current wave; later commercial carve-outs require a separate review.

**Per-repo safety gates:** claim in [coordination issue #26](https://github.com/ores-truffle-oreslang/.github/issues/26), follow private [migration PR #25's runbook](https://github.com/ores-truffle-oreslang/.github/pull/25); record exact head SHA/tree, numeric repository ID, independent security + both admin approvals, target branch protection/rulesets and access, permissions for Actions/secrets/apps/deploy keys, PRs, releases, packages, Pages and dependencies; approve rollback. Never mirror private code without authorization. After a real GitHub transfer, recheck unchanged ID/tree/history, re-establish permissions, update consumer references and rerun exact-head executable tests. `steps=[]` or `null` is `NOT_EXECUTED`; host-only checks do not demonstrate language guest parity, zero-GC, AOT or safe production use.
