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
