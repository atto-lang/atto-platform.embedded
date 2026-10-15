# Cortex-M static actor security audit — 2026-10-14

## Exact-source provenance

Audited files were retrieved from the **private** Embedded PR #5 branch and
reconstructed byte-identically in the local compiler environment. `git
hash-object` matched these upstream source blobs:

| Path | Git blob SHA-1 |
|---|---|
| `src/ores_embedded_static_actors.c` | `80bfe0959c56a038d0b7be0d14eb8ed16907644f` |
| `include/ores_embedded_static_actors.h` | `9644dc7f58a66d4cc8d8dbc4798b7e028bbf3b94` |
| `tests/test_embedded_static_actors.c` | `392c61f1d1d4a0dbcd5d4cdb61d3b75d227f1c1a` |
| `tests/cortexm_static_actor_link_smoke.c` | `6741ab5d8fc4b2d02e8bd93f7f34f4466b1d1afd` |
| `tests/host_static_actor_abi_driver.c` | `f9ac87afc6703d540c079a6b18113a6bc0124051` |

## Findings and fix

**Fixed:** The global host telemetry getters `ores_embedded_actors_last`
and `ores_embedded_actors_deliveries` previously allowed any actor
constructor or handler to read another actor's completed result/delivery
count, bypassing private actor state boundaries. A pre-fix local test
reproduced this: the getter call inside the handler **did not fault**.

Both functions now fail closed when `current != NULL`. That callback
receives zero and a sticky session fault; the runtime retires/scrubs
active actors at the next safe callback boundary. Trusted host code can
still read completed telemetry outside guest turns.

Three upstream regressions were committed:
`guest-cross-actor-last`, `guest-cross-actor-deliveries`,
`guest-constructor-telemetry`. The prior retirement wipe, capacity,
stale-ref, constructor-validation, reentrancy and fault tests remain.

## Local verification — current runtime revision

**PASS: 96/96 exact-source GCC/Clang ASan+UBSan executions:**

| Toolchain | -O1 | -O2 |
|---|---:|---:|
| GCC | 23 runtime scenarios + 1 ABI fixture | 23 runtime scenarios + 1 ABI fixture |
| Clang | 23 runtime scenarios + 1 ABI fixture | 23 runtime scenarios + 1 ABI fixture |

Each binary uses `-std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG`,
`-g -fsanitize=address,undefined -fno-omit-frame-pointer` and the
corresponding `-O1` or `-O2`, with leak and undefined behavior checking.

**PASS:** GCC `-fanalyzer` and Clang `--analyze` on the updated runtime
translation unit, warnings treated as errors.

**PASS:** Freestanding Clang 17 `--target=thumbv7m-none-eabi`,
`-mcpu=cortex-m3 -mthumb -O2 -ffreestanding`; output is ELF32 ARM.

**PASS:** Linking the exact C two-actor ABI fixture with ARM runtime object,
`ld.lld`, `-nostdlib`, and `--no-undefined` produced an ARM ELF
executable. This does **not** involve an Oreslang-generated guest object.

**PASS:** Measured adapter `.bss` of the ARM object: **2,324 bytes**.
Its persistent adapter cap is **3,072 bytes**. A compile with a header
configured to 1,024 bytes fails with the intended `_Static_assert`.

## Remaining blockers / no merge authorization

These local host and ARM-link results do **not** establish successful
updated `.ores` compiler admission, full LLVM regression testing, or
Zephyr 4.3.0 Cortex-M3/QEMU boot. The private GitHub-hosted CI mirror
continues to fail before job steps; its test jobs have not executed.
The adapted ARM C ABI fixture is a **stand-in**, not generated guest code.

No thread/ISR-safety, MCU MPU memory protection, overall firmware RAM
budget, stack/WCET analysis, cache/DMA wiping, or physical-remanence
guarantee is claimed. Leave Embedded PR #5 and LLVM PR #388 **draft and
unmerged** pending the compiler and QEMU acceptance gates.


## Follow-up hardening — global diagnostic isolation (new revision)

**Current Embedded source head (as of this follow-up):**
`78851fe7b0532835b11803cecd444405c2812d76`.

The previous sections report **96/96 verified host executions on an
earlier revision**, specifically runtime Git blob
`80bfe0959c56a038d0b7be0d14eb8ed16907644f`
and tests Git blob
`392c61f1d1d4a0dbcd5d4cdb61d3b75d227f1c1a`.
Those positive results are **not** test results for the new revision.

### New finding and committed fix

The global functions `ores_embedded_actors_faulted`,
`ores_embedded_actors_spawns`, and `ores_embedded_actors_active` were
still callable during an actor callback, disclosing overall session
state/population. They now return zero, set a sticky session fault, and
cause every active actor to retire at the safe callback boundary,
matching the pre-existing host-only restriction on per-actor telemetry.

Four new negative regression scenarios cover these functions in handler
and constructor contexts. The checked-in `verify-static-actors.sh`
now executes all **27 cases + 1 C ABI fixture**, on both GCC and Clang
at `-O1` and `-O2`: **112 required executions**.

**STATUS: COMMITTED, NOT YET VERIFIED EXECUTING.** Neither the current
112-execution host matrix nor real `.ores` LLVM admission and Zephyr/QEMU
firmware has completed on the updated branch. No success claim is made.
The local environment has Clang 17 and GNU C but lacks the LLVM headers
and CMake package configuration required to build `oreslang-llvmc`,
as well as QEMU and Zephyr/west. The GitHub Actions private mirror runs
are failing before any job steps execute, including the refreshed
revision. No alternative organization was confirmed to have a runnable
private hosted runner.

Merge gates for Embedded #5 and LLVM #388 therefore remain closed.
