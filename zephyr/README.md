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

The following are candidate Zephyr 4.3.0 board selectors for build-only
qualification. These do **not** imply that the existing Cortex-M3 object is
a native or fully qualified port for their CPUs.

```sh
west build -b 'arduino_uno_r4@minima' /absolute/path/to/oreslang-platform.embedded/zephyr \
  -d build-oreslang-uno-r4 -- \
  -DORESLANG_LLVMC=/absolute/path/to/oreslang-source.llvm/build/oreslang-llvmc

west build -b 'rpi_pico2/rp2350a/m33' /absolute/path/to/oreslang-platform.embedded/zephyr \
  -d build-oreslang-pico2 -- \
  -DORESLANG_LLVMC=/absolute/path/to/oreslang-source.llvm/build/oreslang-llvmc
```

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
