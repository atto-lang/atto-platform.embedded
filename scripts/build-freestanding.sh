#!/usr/bin/env bash
set -euo pipefail

# Compile the freestanding, zero-libc C mailbox for the caller's toolchain.
# CC may be e.g. arm-none-eabi-gcc or riscv32-unknown-elf-gcc.
cd "$(dirname "$0")/.."
: "${CC:=cc}"
: "${OUT:=build-freestanding/mailbox.o}"
mkdir -p "$(dirname "$OUT")"
"$CC" -std=c11 -ffreestanding -fno-builtin -Os -Wall -Wextra -Werror \
   -Iinclude -c src/ores_embedded_mailbox.c -o "$OUT"
echo "Created $OUT (object only; firmware linkage not performed)"
