# Independent Cortex-M static actor audit — 2026-10-14

## Exact-source verification

The source files in this PR were fetched through the GitHub connector and
checked against the *locally executed bytes* using `git hash-object`.

| File | Git blob SHA-1 |
|---|---|
| `src/ores_embedded_static_actors.c` | `2419c2c9555bf731d34e6e1b0e283006d691145e` |
| `include/ores_embedded_static_actors.h` | `9644dc7f58a66d4cc8d8dbc4798b7e028bbf3b94` |
| `tests/test_embedded_static_actors.c` | `755ae4fb12031dfbba56f278c2472953b440a5be` |
| `scripts/verify-static-actors.sh` | `912fba47c3edd60c1bba3dd4880116bf479167d6` |

**PASS:** 22/22 standalone runtime test executions (11 distinct scenarios
each under GCC and Clang), using `-std=c11 -Wall -Wextra -Wpedantic -Werror
-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -UNDEBUG`
with `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1`.

Scenarios: `basic`, `stale`, `overflow`, `reset`,
`fault-before-spawn-reset`, `bad-initializer`,
`missing-initializer`, `bad-handler`, `fault-after-success`,
`immutable-field`, `double-stop`.

**PASS:** Clang 17 freestanding Cortex-M3 object compilation using
`--target=thumbv7m-none-eabi -mcpu=cortex-m3 -mthumb -O2`.
The resulting ARM EABI object contains no unresolved external function
symbols and has **2,324 bytes of `.bss`**, verified with `size -A`.
A compile-time admission assertion now budgets 3,072 bytes for the
adapter's persistent state. This bound does not include unrelated kernel
state, stack, linker padding or other firmware components.

**PASS:** Clang static analyzer and GCC `-fanalyzer` completed without
warnings under `-Werror` for the same actor runtime translation unit.

## Additional isolation fixes

- Retire and scrub an actor after constructor or handler faults, after the
  generated callback returns. Previously committed per-reference telemetry
  survives; an in-flight failed message does not count as delivered.
- Reset can no longer clear sticky faults before the first spawn.
- Compiler-specific and runtime ABI behavior remain separately gated.

Reproduce with `bash scripts/verify-static-actors.sh`. Native test
execution uses no GitHub Actions billing or secrets.

## What this does not verify

Neither the updated LLVM `.ores` source compiler tests nor the updated
Zephyr 4.3.0/QEMU Cortex-M3 end-to-end actor firmware have passed for
these commits. The [exact-source private CI branch](https://github.com/canonical-cloud/canonical-docs/tree/ci/ores-cortexm-actor-hardening-20261014)
contains the updated source; prior Actions attempts ended before any
runner step. These PRs must stay unmerged until those remaining gates
pass. There is no formal WCET, stack budget, hardware MPU isolation, or
whole-firmware allocation-free proof.
