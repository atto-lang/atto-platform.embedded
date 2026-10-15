#!/usr/bin/env python3
"""Verify real Zephyr/QEMU guest output, not shell command echoes.

The west/QEMU console may use carriage returns, ANSI SGR controls and
line prefixes. A timeout is expected for interactive QEMU only after
all three unique ordered guest markers have been observed.
"""
import argparse
from collections import Counter
from pathlib import Path
import re
import sys

EXPECTED = (
    "ORESLANG_CORTEXM_MAILBOX_INIT_PASS",
    "ORESLANG_CORTEXM_MAILBOX_ALIAS_PASS",
    "ORESLANG_CORTEXM_ACTORS_PASS count=2 delivery=4 allocation=static",
)
# Only accept raw guest serial lines or the known [serial] transport wrapper.
# A shell echo, source snippet, or CI command line is NOT a boot or a marker.
SERIAL = r"(?:\\[serial\\] )?"
BOOT = re.compile(r"^" + SERIAL + r"\\*{3} Booting Zephyr OS build v[^\\s]+ \\*{3}$")
ANSI = re.compile(r"\x1b(?:\[[0-?]*[ -/]*[@-~]|\][^\x07]*(?:\x07|\x1b\\))")
FAIL = re.compile(r"\bORESLANG_CORTEXM_[A-Z0-9_]*FAIL\b")
FATAL = re.compile(r"\\b(?:FATAL ERROR|ASSERTION FAIL(?:ED)?|HARD FAULT|BUS FAULT|USAGE FAULT|MPU FAULT|KERNEL OOPS|KERNEL PANIC)\\b", re.I)
# A guest marker may be prefixed by terminal console text, but not
# embedded in another identifier or followed by a shell quote.
MARKERS = tuple(re.compile(r"^" + SERIAL + re.escape(s) + r"$") for s in EXPECTED)


def validate(raw: bytes, exit_code: int) -> None:
    if exit_code not in (0, 124):
        raise ValueError(f"QEMU/west exited unexpectedly: {exit_code} (wanted 0 or timeout 124)")
    clean = ANSI.sub("", raw.decode("utf-8", "replace")).replace("\x00", "")
    boot_count = 0
    found = []
    for line_number, line in enumerate(clean.splitlines(), 1):
        line = line.strip()
        if BOOT.fullmatch(line):
            boot_count += 1
            if boot_count != 1:
                raise ValueError(f"QEMU guest booted multiple times (line {line_number})")
        if boot_count == 0:
            continue
        if FAIL.search(line) or FATAL.search(line):
            raise ValueError(f"guest fault/failure at line {line_number}: {line[:200]}")
        for index, marker in enumerate(MARKERS):
            if marker.fullmatch(line):
                found.append(index)
                if found.count(index) > 1:
                    raise ValueError(f"duplicate guest marker: {EXPECTED[index]}")
                if index != len(found) - 1:
                    raise ValueError(f"guest markers out of order at line {line_number}: {EXPECTED[index]}")
                break
    if boot_count == 0:
        raise ValueError("no Zephyr guest boot banner (host/compiler logs are not guest execution)")
    expected_indices = list(range(len(EXPECTED)))
    if found != expected_indices:
        missing = [name for i, name in enumerate(EXPECTED) if i not in found]
        raise ValueError(f"missing guest markers: {missing}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", type=Path, required=True)
    parser.add_argument("--exit-code", type=int, required=True)
    args = parser.parse_args()
    try:
        validate(args.log.read_bytes(), args.exit_code)
    except (ValueError, OSError) as error:
        print(f"FAIL QEMU guest-output verification: {error}", file=sys.stderr)
        return 1
    print("PASS QEMU guest boot and 3 unique ordered mailbox/actor markers")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
