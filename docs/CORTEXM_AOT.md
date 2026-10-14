# Cortex-M3 Oreslang source-to-firmware target

## Scope

The compiler emits LLVM IR with an explicit `thumbv7m-none-eabi` triple and
Cortex-M AAPCS32 datalayout for scalar i64 functions only. It rejects entrypoint
`main` (which would conflict with Zephyr), source actors, heap-backed owners,
suspension, host calls, classes, control flow and local state in this profile.
The C Zephyr host calls exported `ores_embedded_probe() : int` as `int64_t`.

A Python build helper validates target IR and the ARM machine type (EM_ARM=40)
before atomically publishing the object. Zephyr CMake depends on the Oreslang
source and compiler executable, so changes force relinking.

## Acceptance

1. The same `.ores` source must compile under the exact LLVM compiler commit.
2. The output must be ARM ELF 32-bit little-endian, not x86 host ELF.
3. The Zephyr link must include the Oreslang function symbol, no surrogate C implementation.
4. QEMU must print `ORESLANG_CORTEXM_AOT_PASS value=42`.
5. The audit log must capture compiler SHA, Zephyr SDK version and run URL.
6. Refuse to mark this gate verified without a QEMU execution log.

The native mailbox is still a C reference and does not imply LLVM actor
lowering or memory isolation. Actual source actor support is the next milestone.
