# Exact-source Cortex-M static actor audit — 2026-10-14

## Source provenance

Each C source file and the reproducible verification script were fetched
through the GitHub connector from this draft branch. Git blob SHA-1 values
were compared to the bytes actually compiled and run in the local container.

| File | Git blob SHA-1 |
|---|---|
| `src/ores_embedded_static_actors.c` | `aceb3c47b6bb748f1a788d0a67182df7394dacb0` |
| `include/ores_embedded_static_actors.h` | `9644dc7f58a66d4cc8d8dbc4798b7e028bbf3b94` |
| `tests/test_embedded_static_actors.c` | `74a69b9b2ff080e2b895f17a9a68cfb5b95161fc` |
| `scripts/verify-static-actors.sh` | `242191486bc7cea81d9e28009f83fe58389189cd` |
| `tests/cortexm_static_actor_link_smoke.c` | `6741ab5d8fc4b2d02e8bd93f7f34f4466b1d1afd` |
| `tests/host_static_actor_abi_driver.c` | `f9ac87afc6703d540c079a6b18113a6bc0124051` |

## Host and ARM results

**PASS: 38/38 independent GCC and Clang sanitizer-backed executions.**
The suite contains 18 runtime/lifecycle scenarios per compiler, plus one
two-actor guest ABI stand-in execution per compiler. It uses
`-std=c11 -Wall -Wextra -Wpedantic -Werror -O1 -g`,
`-fsanitize=address,undefined -fno-omit-frame-pointer -UNDEBUG`,
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1`.

The lifecycle cases cover: normal delivery, stale references, mailbox
overflow, *global cleanup after overflow*, invalid telemetry lookup,
all eight actor slots carrying sixteen messages each, actor-slot
exhaustion, illegal reset, sticky fault before spawn, invalid/missing
constructor state, handler corruption, partial-delivery telemetry,
fault in one actor with another live, cross-actor heap read,
reentrant send, immutable write and double stop.

**PASS: ARM Cortex-M3 freestanding object compilation**, Clang 17
`--target=thumbv7m-none-eabi -mcpu=cortex-m3 -mthumb -O2`.
Measured persistent adapter `.bss` is 2,324 bytes versus a 3,072-byte
compile-time storage ceiling. A negative build with a deliberately reduced
1,024-byte limit fails with the expected static assertion.

**PASS: Freestanding Cortex-M3 executable link** using LLVM lld and a
two-actor ABI stand-in compiled from C, with no unresolved symbols.
This validates C linker and ABI compatibility only. The stand-in is
**not** an Oreslang compiler-generated source program and the ARM
executable was not run under QEMU.

**PASS:** GCC `-fanalyzer` and Clang `--analyze` of the actor runtime
translation unit with warnings promoted to errors.

Run all reproducible checks locally using
`bash scripts/verify-static-actors.sh`.

## CI / merge gate

Current private exact-source CI mirror:
https://github.com/canonical-cloud/canonical-docs/tree/ci/ores-cortexm-actor-hardening-20261014

The GitHub Actions jobs in the private mirror continue to fail before any
runner steps execute. That is an infrastructure/startup failure, not a
compiler or firmware test result. The exact-source LLVM compiler regression
suite and Zephyr 4.3.0 Cortex-M3 QEMU firmware execution remain **unverified
for this hardening revision**.

Keep Embedded PR #5 and LLVM PR #388 **draft and unmerged** until those
checks pass. There is no formal bound on Zephyr stack/WCET or proof of
MPU-backed isolation or whole-system allocation-free operation.
