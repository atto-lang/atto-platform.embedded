#!/usr/bin/env python3
"""Supplementary unresolved-symbol admission for a freestanding Cortex-M ELF.

A clean undefined-symbol table is necessary, NOT sufficient, for proving
no heap allocation, libc, POSIX, or tracing GC exists in statically linked code.
"""
import argparse
from pathlib import Path
import subprocess
import sys


class InvalidFirmware(ValueError):
    pass


def check_undefined_symbols(text):
    if type(text) is not str or len(text) > 131072:
        raise InvalidFirmware("unbounded or malformed nm output")
    # llvm-nm --undefined-only prints one unresolved symbol per nonblank line;
    # fail closed instead of trusting symbol-name deny lists or nm's formatting.
    unresolved = []
    for line in text.splitlines():
        line = line.strip()
        if not line:
            continue
        # Some nm variants emit an object header before unresolved symbols.
        if line.endswith(":") and len(line) < 1024 and not any(c.isspace() for c in line):
            continue
        unresolved.append(line)
        if len(unresolved) >= 4:
            break
    if unresolved:
        raise InvalidFirmware("undefined symbols in freestanding ELF: " +
                              "; ".join(unresolved))
    return True


def audit(elf, nm="llvm-nm"):
    path = Path(elf)
    if not path.is_file() or path.stat().st_size > 128 * 1024 * 1024:
        raise InvalidFirmware("invalid or oversized ELF")
    try:
        result = subprocess.run([nm, "--undefined-only", str(path)],
                                capture_output=True, timeout=20, check=False)
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise InvalidFirmware("nm execution unavailable or timed out") from exc
    if result.returncode != 0:
        raise InvalidFirmware("nm could not inspect ELF")
    try:
        output = result.stdout.decode("utf-8", errors="strict")
    except UnicodeError as exc:
        raise InvalidFirmware("invalid nm output encoding") from exc
    return check_undefined_symbols(output)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--nm", default="llvm-nm")
    args = parser.parse_args()
    try:
        audit(args.elf, args.nm)
        print("PASS: no unresolved ELF symbols (does not prove zero allocations)")
        return 0
    except (OSError, InvalidFirmware) as exc:
        print("REJECTED: " + str(exc), file=sys.stderr)
        return 65


if __name__ == "__main__":
    raise SystemExit(main())
