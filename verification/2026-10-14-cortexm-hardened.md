# Embedded Zephyr native ABI hardening — 2026-10-14

**Verification: PASS.**
[Private exact-source cross-organization CI run](https://github.com/canonical-cloud/canonical-docs/actions/runs/38653462399)
built Oreslang compiler, ran its Cortex-M positive/negative admission suite,
then cross-compiled an actual `.ores` source into ARM ELF, linked Zephyr 4.3.0
firmware and booted `qemu_cortex_m3`. Console evidence:

```text
Cortex-M scalar ABI, ARM ELF, recursion and bounded-admission tests PASS
*** Booting Zephyr OS build v4.3.0 ***
ORESLANG_CORTEXM_AOT_PASS value=42 signed64=PASS
```

Compiled `firmware.ores` now accepts two i64 inputs, returns i64 and
exercises signed negative, positive and above-32-bit cases in QEMU firmware.
The object builder rejects destructive output paths, oversize IR/object,
incorrect ELF type/machine and missing exported ARM guest symbol; a failed
build cannot atomically publish a partial object.

Embedded program/builder source audited: `7613d5ba7972b5dc9973b8393be791361a46212c`
(README correction was a later documentation-only commit).
The C mailbox remains a reference; no source-driven actor runtime, hardware
MPU isolation, precise stack/WCET bound or physical board execution is claimed.

The private mirror uses exact-source blobs from source branches in
`canonical-cloud/canonical-docs` branch `ci/ores-cortexm-aot-20261014`.
Do not treat this as a source-repository Actions green check or proof of
backend parity outside the admitted scalar Cortex-M profile.
