# Oreslang native actors in Zephyr (experimental)

The application compiles the real `zephyr/ores/firmware.ores` source using
`oreslang-llvmc` and links that generated ARM ELF object with the allocation-free
static actor adapter into Zephyr. It is **not** a C imitation of the Oreslang
guest. The host `zephyr/src/main.c` validates the compiled actors and mailbox
contract before printing the success marker.

## Verified reference target: QEMU Cortex-M3

Using Zephyr 4.3.0 in a west workspace with LLVM 18, Clang (ARM enabled),
GNU Arm tools and QEMU installed:

```sh
west build -b qemu_cortex_m3 /absolute/path/to/oreslang-platform.embedded/zephyr \
  -d build-oreslang-qemu -- \
  -DORESLANG_LLVMC=/absolute/path/to/oreslang-source.llvm/build/oreslang-llvmc
west build -t run -d build-oreslang-qemu
```

The emulator should print both
`ORESLANG_CORTEXM_MAILBOX_ALIAS_PASS` and
`ORESLANG_CORTEXM_ACTORS_PASS count=2 delivery=4 allocation=static`.
The board run does not terminate QEMU automatically. CI may use a bounded
emulator timeout plus an *exact* success-marker assertion; a timed-out or
otherwise terminated QEMU command alone is not evidence of guest success.

The guest is compiled for `thumbv7m-none-eabi`/`cortex-m3`, checked for the
correct ELF32 ARM machine and the exported `oreslang_cortexm_bootstrap` symbol,
then linked into the final Zephyr image. A failure to compile, verify, or link
the real guest is fatal; there is no host-only source fallback.

## Board cross-build probes (not hardware execution)

The following Zephyr 4.3.0 board selectors have **passed firmware cross-link**
with the current integer-only Oreslang guest subset on a private runner.
The UNO R4 requires the explicitly opt-in `uno_r4_compat.conf` overlay;
without it, the linker rejects a hard/soft-float ABI mismatch and places
`.ARM.extab` in the wrong Renesas option-setting memory region. These do **not** imply that the existing Cortex-M3 object is
a native or fully qualified port for their CPUs.

```sh
west build -b 'arduino_uno_r4@minima' /absolute/path/to/oreslang-platform.embedded/zephyr \
  -d build-oreslang-uno-r4 -- \
  -DORESLANG_LLVMC=/absolute/path/to/oreslang-source.llvm/build/oreslang-llvmc \
  -DEXTRA_CONF_FILE=/absolute/path/to/oreslang-platform.embedded/zephyr/uno_r4_compat.conf

west build -b 'rpi_pico2/rp2350a/m33' /absolute/path/to/oreslang-platform.embedded/zephyr \
  -d build-oreslang-pico2 -- \
  -DORESLANG_LLVMC=/absolute/path/to/oreslang-source.llvm/build/oreslang-llvmc
```

Pico 2: [successful source-derived cross-link](https://github.com/benefactor-cc/backend.rs/actions/runs/38808266845),
24,324 B flash / 7,904 B RAM. UNO R4:
[successful experimental soft-ABI + CPP profile](https://github.com/benefactor-cc/backend.rs/actions/runs/38808558925),
28,928 B flash / 8,584 B RAM; the soft-ABI-only experiment failed
on the exception-table linker region. See
[LLVM target ABI issue #423](https://github.com/ores-truffle-oreslang/oreslang-source.llvm/issues/423)
for the eventual native ARMv7E-M hard-float implementation.

Even a successful link only establishes compatibility for the emitted
instruction subset and linkage in that configuration. Flashing, serial output,
GPIO behavior, IRQ safety, stack high-water, MPU/TrustZone, and multicore
semantics require further **physical-board** tests.

**Do not use the Pico (Cortex-M0+) or Pico 2 Hazard3 (RISC-V) with the current
ARMv7-M target**. Those require separate compiler target admission/lowering;
the full-size Raspberry Pi 4/5 belongs to the native ARM64/Linux backend.

See [the board portability gates](../docs/BOARD_PORTABILITY.md) and
[target acceptance](../docs/TARGET_ACCEPTANCE.md) before claiming hardware
support. Previous exact-source Cortex-M3 QEMU success is recorded in
[run 38767714704](https://github.com/benefactor-cc/backend.rs/actions/runs/38767714704).
