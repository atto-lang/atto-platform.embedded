# Independent local audit — Cortex-M actor runtime

Date: 2026-10-14 (America/Lima); test environment: Linux container, GCC 14.2.0, Clang 17.0.0.

## Source provenance

The three C source artifacts were fetched from the private hardening branch
`audit/cortexm-static-actor-isolation-20261014`; their exact Git blob SHA-1
values were cross-checked against the GitHub connector results before testing.

| GitHub path | Git blob SHA-1 |
|---|---|
| `src/ores_embedded_static_actors.c` | `984d59fdcfac42744beeccd48bcf20f0efc79e9e` |
| `include/ores_embedded_static_actors.h` | `8d706a914fe40e8ee3e7d92aa3a8b21e3e6ebfbb` |
| `tests/test_embedded_static_actors.c` | `257addd5afd08eb157619ffc7a37612be348b693` |

## Results

**PASS: 18/18 standalone runtime invocations**, each scenario under
both GCC and Clang with `-std=c11 -Wall -Wextra -Wpedantic -Werror`,
`-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -UNDEBUG`,
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`, and
`UBSAN_OPTIONS=halt_on_error=1`.

Nine scenarios per compiler: `basic`, `stale`, `overflow`, `reset`,
`bad-initializer`, `missing-initializer`, `bad-handler`,
`immutable-field`, `double-stop`.

**PASS:** The same actor-runtime C source cross-compiled with Clang 17
`--target=thumbv7m-none-eabi -mcpu=cortex-m3 -mthumb -O2 -ffreestanding`.
`readelf` identified a little-endian ELF32 relocatable object targeting
ARM EABI5. The object had no unresolved external function symbols.

Repro: `bash scripts/verify-static-actors.sh` on a host with GCC, Clang,
and (preferably) `readelf`. This script performs all the above checks.

## Limits; merge blocked

This is **local runtime verification**, not a green GitHub Actions result.
The cross-compiled C object does **not** prove that an Oreslang actor
`.ores` source was compiled/linked/booted in Zephyr. LLVM compiler source
regression tests and exact-source Zephyr/QEMU execution are still required.
Current private Actions runs fail before runner steps, including retry attempt
2 for [run 38661610241](https://github.com/canonical-cloud/canonical-docs/actions/runs/38661610241).
Keep LLVM PR #388 and Embedded PR #5 as draft until those two remaining
gates pass; no merge or certified memory/WCET guarantee is claimed.
