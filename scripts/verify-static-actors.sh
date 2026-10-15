#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
scenarios=(basic stale overflow reset bad-initializer missing-initializer bad-handler immutable-field double-stop)
for cc in gcc clang; do
  if ! command -v "$cc" >/dev/null 2>&1; then
    echo "Missing required C compiler: $cc" >&2
    exit 1
  fi
  "$cc" -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG \
    -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"$root/include" "$root/src/ores_embedded_static_actors.c" \
    "$root/tests/test_embedded_static_actors.c" -o "$tmp/actor-$cc"
  for scenario in "${scenarios[@]}"; do
    ASAN_OPTIONS="detect_leaks=1:halt_on_error=1" \
    UBSAN_OPTIONS="halt_on_error=1" "$tmp/actor-$cc" "$scenario"
    echo "PASS [$cc] $scenario"
  done
done
clang --target=thumbv7m-none-eabi -mcpu=cortex-m3 -mthumb \
  -ffreestanding -fno-builtin -fno-stack-protector -std=c11 -O2 \
  -Wall -Wextra -Wpedantic -Werror -I"$root/include" \
  -c "$root/src/ores_embedded_static_actors.c" -o "$tmp/actor-arm.o"
if command -v readelf >/dev/null 2>&1; then
  readelf -h "$tmp/actor-arm.o" | grep -Eq 'Machine: *ARM'
  readelf -h "$tmp/actor-arm.o" | grep -Eq 'Class: *ELF32'
  if readelf -Ws "$tmp/actor-arm.o" | grep ' UND ' | grep -v 'NOTYPE  LOCAL  DEFAULT  UND' | grep -qE '[[:alpha:]_]'; then
    echo 'Unexpected unresolved symbols in freestanding Cortex-M runtime' >&2
    exit 1
  fi
fi
echo 'PASS ARM Cortex-M3 freestanding runtime object (ELF32 ARM)'
