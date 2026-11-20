# Actual Oreslang Cortex-M3 firmware smoke — 2026-10-14

**Result: PASS — scalar subset only.**

- Actual LLVM compiler source: `oreslang-source.llvm@ba8552bc9ca8cc4e9edc3b7f665ed2a1ed9a5b39`.
- Actual Embedded source: `oreslang-platform.embedded@5705a25b0dc5ee9ddb1a7c15ebe747142d51975c`.
- Build/test mirror: private `canonical-cloud/canonical-docs` branch `ci/ores-cortexm-aot-20261014`.
- [Passed workflow #38646752678](https://github.com/canonical-cloud/canonical-docs/actions/runs/38646752678).
- LLVM 18.1.3 and Clang 18; Zephyr v4.3.0; board `qemu_cortex_m3`.
- The source fixture `zephyr/ores/firmware.ores` was passed to `oreslang-llvmc --target=thumbv7m-none-eabi`.
- Clang generated the real 32-bit little-endian ARM ELF object.
- Zephyr CMake built and linked firmware with this guest object.
- QEMU booted Zephyr, and the guest function returned the integer 42.

Exact observed console excerpt:

```text
*** Booting Zephyr OS build v4.3.0 ***
ORESLANG_CORTEXM_AOT_PASS value=42
```

The QEMU process was stopped by an intentional timeout after printing the
marker, because Zephyr's idle loop does not automatically exit. The workflow
checks for the marker, and the entire job completed successfully.

## Not covered

No LLVM source-level actors/mailboxes, no ISR-safe messaging, no MPC/WCET or
hard-real-time guarantee, no hardware MPU or device security claims, no
production IoT networking. CI source repositories may lack GitHub Actions
minutes; this evidence is from an **exact-source private mirror** with real
LLVM and Zephyr toolchains.
