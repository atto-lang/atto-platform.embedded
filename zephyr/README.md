# Zephyr reference smoke

This builds a **real Zephyr firmware image** containing the portable C mailbox and a tiny on-boot test. It is **not** an Oreslang compiler/actor integration.

With Zephyr SDK, Python dependencies and `west` configured:

```sh
west build -b qemu_cortex_m3 zephyr -d build-zephyr
west build -t run -d build-zephyr
```

Board availability and naming depend on Zephyr release. Choose an installed Cortex-M board if `qemu_cortex_m3` is unavailable. The source prints `OresEmbedded mailbox reference PASS` when the smoke succeeds.

Next: Zephyr syscalls/HAL capability bindings, cross-target compiler smoke, bounded actor scheduling, MPU constraints and target evidence.
