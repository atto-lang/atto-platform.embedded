# Real Oreslang -> Cortex-M3 Zephyr application

This is an *experimental scalar native-AOT integration* that executes an
Oreslang-defined function on Zephyr under QEMU. It does not implement compiled
actors, actor message transport, interrupts, MPU isolation, or hard real-time.

## Toolchain

- Build `oreslang-llvmc` on the host from the matching LLVM PR.
- Install Zephyr SDK / `west`, QEMU, Python, CMake and `clang` with ARM support.
- Run `west` from the embedded repo root (or use an absolute application path).

```sh
# From oreslang-platform.embedded
west build -b qemu_cortex_m3 zephyr -d build-zephyr -- \
  -DORESLANG_LLVMC=/absolute/path/to/oreslang-source.llvm/build/oreslang-llvmc
west build -t run -d build-zephyr
```

The CMake custom build step invokes `oreslang-llvmc --target=thumbv7m-none-eabi`
on `zephyr/ores/firmware.ores`, then `clang` emits a freestanding ARM ELF
object. Zephyr links this object into its final firmware. A C caller checks
`ores_embedded_probe()` returns 42, printing:

```
ORESLANG_CORTEXM_AOT_PASS value=42
```

A C-only mailbox smoke also runs, but the marker above requires actual Oreslang
native guest code. Failure to compile or link the guest is fatal, with no
host-only fallback. A grep for the pass marker after a QEMU run provides a
minimal end-to-end smoke. A zero exit from the Zephyr main does not terminate
QEMU; end the emulator with Ctrl-A X or automate a timeout and inspect output.

This target only admits simple `int` arithmetic and same-module function calls
until ownership, actors and runtime services are implemented.
