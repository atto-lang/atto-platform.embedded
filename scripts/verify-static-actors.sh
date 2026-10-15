#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
scenarios=(basic stale overflow overflow-scrub invalid-query-scrub capacity-stress exhaustion-scrub reset fault-before-spawn-reset bad-initializer missing-initializer bad-handler fault-after-success handler-fault-scrub-others cross-actor-heap reentrant-send post-retire-wipe fault-retire-wipe guest-cross-actor-last guest-cross-actor-deliveries guest-constructor-telemetry immutable-field double-stop)
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
  "$cc" -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG \
    -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"$root/include" "$root/src/ores_embedded_static_actors.c" \
    "$root/tests/cortexm_static_actor_link_smoke.c" \
    "$root/tests/host_static_actor_abi_driver.c" -o "$tmp/abi-$cc"
  ASAN_OPTIONS="detect_leaks=1:halt_on_error=1" \
  UBSAN_OPTIONS="halt_on_error=1" "$tmp/abi-$cc"
  echo "PASS [$cc] two-actor ABI stand-in execution (not .ores or QEMU)"
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
# Negative proof: a deliberately 1-KiB budget must refuse this exact runtime.
mkdir -p "$tmp/limited-include"
sed 's/#define ORES_STATIC_ACTOR_STORAGE_BUDGET_BYTES 3072u/#define ORES_STATIC_ACTOR_STORAGE_BUDGET_BYTES 1024u/' \
  "$root/include/ores_embedded_static_actors.h" > "$tmp/limited-include/ores_embedded_static_actors.h"
if clang --target=thumbv7m-none-eabi -mcpu=cortex-m3 -mthumb \
  -ffreestanding -std=c11 -O2 -Werror -I"$tmp/limited-include" \
  -c "$root/src/ores_embedded_static_actors.c" -o "$tmp/should-not-compile.o" \
  2> "$tmp/limited.err"; then
  echo 'ERROR: undersized static actor storage was not rejected' >&2
  exit 1
fi
if ! grep -q 'static actor adapter exceeds' "$tmp/limited.err"; then
  cat "$tmp/limited.err" >&2
  echo 'ERROR: expected static budget diagnostic was absent' >&2
  exit 1
fi
echo 'PASS negative 1-KiB static storage budget rejected at compile-time'
# This is an ABI/linker smoke using C stand-ins for generated guest functions,
# not a replacement for running real .ores source through LLVM and Zephyr.
if ! command -v ld.lld >/dev/null 2>&1; then
  echo 'Missing ld.lld; Cortex-M link verification is required' >&2
  exit 1
fi
clang --target=thumbv7m-none-eabi -mcpu=cortex-m3 -mthumb \
  -ffreestanding -fno-builtin -fno-stack-protector -std=c11 -O2 \
  -Wall -Wextra -Wpedantic -Werror -I"$root/include" \
  -c "$root/tests/cortexm_static_actor_link_smoke.c" -o "$tmp/actor-guest.o"
clang --target=thumbv7m-none-eabi -mcpu=cortex-m3 -mthumb -nostdlib \
  -fuse-ld=lld -Wl,-e,cortexm_static_actor_link_smoke \
  -Wl,--no-undefined -Wl,-Ttext=0x10000 \
  "$tmp/actor-arm.o" "$tmp/actor-guest.o" -o "$tmp/actor-linked.elf"
if command -v readelf >/dev/null 2>&1; then
  readelf -h "$tmp/actor-linked.elf" | grep -Eq 'Machine: *ARM'
  readelf -h "$tmp/actor-linked.elf" | grep -Eq 'Type: *EXEC'
fi
echo 'PASS freestanding ARM runtime + two-actor ABI stub link (not QEMU execution)'
