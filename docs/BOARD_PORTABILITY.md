# Oreslang embedded board portability (Zephyr 4.3.0)

This document distinguishes **compiler admission**, **firmware link**, **QEMU execution**,
and **physical board execution**. These are separate evidence levels. In particular,
building a Cortex-M3 guest object does not automatically make the Oreslang compiler
a native Cortex-M4, Cortex-M33, ARMv6-M, or RISC-V backend.

## First hardware candidates

| Target | Zephyr 4.3 board name | CPU | Memory | Current evidence |
|---|---|---|---|---|
| QEMU reference | `qemu_cortex_m3` | ARMv7-M Cortex-M3 | 64 KiB RAM / 256 KiB flash | Real `.ores` actors executed in QEMU |
| Arduino UNO R4 Minima | `arduino_uno_r4@minima` | ARMv7E-M Cortex-M4 | 32 KiB RAM / 256 KiB flash | Candidate for cross-build and on-board tests |
| Raspberry Pi Pico 2 | `rpi_pico2/rp2350a/m33` | ARMv8-M.main Cortex-M33 | 520 KiB RAM / 4 MiB flash | Candidate for cross-build and on-board tests |
| Raspberry Pi Pico | `rpi_pico/rp2040` | ARMv6-M Cortex-M0+ | 264 KiB RAM / 2 MiB flash | **Not supported**: needs explicit Thumb-1 ARMv6-M compiler backend |
| Raspberry Pi 4/5 | n/a for this Cortex-M application | AArch64 Cortex-A | varies | Separate native ARM64/Linux project; not Cortex-M firmware |
| Pico 2 Hazard3 | `rpi_pico2/rp2350a/hazard3` | RISC-V | 520 KiB RAM / 4 MiB flash | **Not supported** by this ARM Cortex-M output |

Pico 2 and UNO R4 board names are pinned to Zephyr **v4.3.0**. The qualifiers
are not guaranteed stable across later Zephyr releases.

## Build qualification (does NOT imply a working physical port)

All builds must use the actual `zephyr/ores/firmware.ores` program and a
source-pinned LLVM compiler, not a C-only surrogate. The current experimental
compiler path emits `thumbv7m-none-eabi` with `-mcpu=cortex-m3`.
A cross-board *link* that accepts this object may establish **binary compatibility
for this limited instruction subset only**. It is NOT native ARMv7E-M or ARMv8-M
codegen, MPU/TrustZone support, or a device boot guarantee.

For every board: run `west build -b <board> zephyr` with a pinned `-DORESLANG_LLVMC`
in an initialized west workspace; fail if the compiler-generated
`oreslang_cortexm_bootstrap` or `oreslang_actor_Producer_on_message`
symbol is absent from the final ELF; record linker memory regions and warnings.
Use the private SHA-matched mirror if source-org Actions are out of minutes.
Never publish private source in a public test repository or use cross-org tokens
as a workaround.

Before calling a board **supported**, additional checks are required:
- Boot the actual board and capture UART/RTT/USB output proving the Oreslang
  actor execution marker; prove that native code is not replaced with a host stub.
- Exercise full/empty mailbox bounds, stale references, guest isolation,
  field initialization/writes, and error/fault paths on-device.
- Verify per-board memory headroom against 32 KiB/520 KiB RAM as applicable;
  measure stack high-water under realistic interrupts and worst-case workloads.
- Use an ISR-safe event bridge; the existing static reference mailbox is
  neither interrupt-safe nor concurrent.
- Add capability-gated GPIO/serial HAL operations. Compiler-generated actor
  callbacks must not receive unrestricted raw memory-mapped I/O pointers.
- Check linking/ABI, CPU features, hard-/soft-float attributes, interrupt
  contexts, stack execution metadata and linker section placement.
- For Pico 2 multicore scheduling or M33 MPU/TrustZone, add separate physical
  hardware tests. Do not claim them from single-core Zephyr/QEMU smoke.

## Prior execution evidence

The Cortex-M3 Zephyr 4.3.0 actor smoke passed on a private runner:
[compiler + QEMU](https://github.com/benefactor-cc/backend.rs/actions/runs/38767714704).
The native ownership/actor tests passed separately:
[7/7 native test suites](https://github.com/benefactor-cc/backend.rs/actions/runs/38769054742).
Both represent the pinned source versions recorded by those runs; do not claim
they verify future branch heads.

## External Zephyr board references

- https://docs.zephyrproject.org/latest/boards/arduino/uno_r4/doc/index.html
- https://docs.zephyrproject.org/latest/boards/raspberrypi/rpi_pico2/doc/index.html
- https://docs.zephyrproject.org/latest/boards/raspberrypi/rpi_pico/doc/index.html
