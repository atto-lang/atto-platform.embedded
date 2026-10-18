# Freestanding actor adapter link gate (NOT OTP runtime certification)

The existing static **C reference adapter** for embedded Oreslang actors
must compile and link under an explicit freestanding, no-libc/no-startup
contract. CMake/CTest executes `tests/check_static_actor_freestanding.py`.

This compiles the real `src/ores_embedded_static_actors.c` and header using
`-ffreestanding -fno-builtin -fno-stack-protector`, then links
`-nostdlib -no-pie --no-undefined` with an explicit adapter entry symbol.
It rejects unresolved object or ELF imports via `nm -u`, missing compilers,
missing header and missing symbol tools, rather than declaring a skip pass.

This establishes a bounded **C adapter** link invariant only. It does **not**
prove Oreslang `core` source compiles freestanding, or support dynamic OTP
gen_server, supervisor CONTROL, typed source handler dispatch, hardware actor
isolation, or a live Cortex-M firmware image. All remain independent gates.
