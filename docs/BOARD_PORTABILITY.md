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
| Raspberry Pi Pico 2 | `rpi_pico2/rp2350a/m33` | ARMv8-M.main Cortex-M33 | 520 KiB RAM / 4 MiB flash | **Zephyr 4.3.0 firmware cross-link PASSED**; no physical execution yet |
| Raspberry Pi Pico | `rpi_pico/rp2040` | ARMv6-M Cortex-M0+ | 264 KiB RAM / 2 MiB flash | **Not supported**: needs explicit Thumb-1 ARMv6-M compiler backend |
| Raspberry Pi 4/5 | n/a for this Cortex-M application | AArch64 Cortex-A | varies | Separate native ARM64/Linux project; not Cortex-M firmware |
| Pico 2 Hazard3 | `rpi_pico2/rp2350a/hazard3` | RISC-V | 520 KiB RAM / 4 MiB flash | **Not supported** by this ARM Cortex-M output |

Pico 2 and UNO R4 board names are pinned to Zephyr **v4.3.0**. The qualifiers
are not guaranteed stable across later Zephyr releases.

## Raspberry Pi Pico 2 source-derived firmware cross-build — PASSED

[Private cross-build run 38808266845](https://github.com/benefactor-cc/backend.rs/actions/runs/38808266845)
successfully compiled and linked the real compiler-generated `.ores` actor
object into **Zephyr 4.3.0 for `rpi_pico2/rp2350a/m33`**. The final ARM ELF
contained `oreslang_cortexm_bootstrap` and
`oreslang_actor_Producer_on_message`, verified using `arm-none-eabi-nm`.

The linker reported **24,324 B FLASH (0.58% of 4 MiB)** and
**7,904 B RAM (1.48% of 520 KiB)**. An ARMv7-M/soft-ABI guest subset
linked into the Cortex-M33 target, but this is **not** native ARMv8-M
instruction lowering and does not prove the image boots on an RP2350.
No physical execution, GPIO, ISR, stack-high-water, or MPU security test
has been performed. GNU libgcc emitted the existing `.ARM.extab` and
GNU-stack metadata linker warnings; review remains necessary.

The overall Actions workflow was marked failed because its subsequent
Arduino UNO R4 build failed. The **Pico 2 sub-build passed**. Treat these
as distinct results.

## Arduino UNO R4 initial cross-build failure (2026-10-15)

[Exact-source private Actions run 38807500395](https://github.com/benefactor-cc/backend.rs/actions/runs/38807500395)
compiled the .ores actor object and the Zephyr 4.3.0 UNO R4 Minima
application, but **failed at final link**. The board enables ARMv7E-M
hard-float VFP register calling conventions; the current guest object is
ARMv7-M soft-float. The linker rejected mixing those ABIs:

```
zephyr_pre0.elf uses VFP register arguments,
app/libapp.a(oreslang_cortexm_probe.o) does not
```

The same link exposed an unrelated orphan `.ARM.extab` placement
into `OFS_OSIS_MEMORY` (24-byte region overflow). The solution needs
**both** verified ABI agreement and correct Zephyr/Renesas section
placement. It is not safe to disable the link checks or relabel a
Cortex-M3 object as Cortex-M4 hard-float merely to force a pass.
A separate private probe will evaluate a Zephyr soft-ABI configuration
and explicit exception-table placement; it is not currently qualified.
Pico 2 is probed independently because the first run stopped on UNO R4.

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
