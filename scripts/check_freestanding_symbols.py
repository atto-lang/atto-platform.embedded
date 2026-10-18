#!/usr/bin/env python3
"""Supplementary unresolved-symbol admission for a freestanding Cortex-M ELF.

A clean undefined-symbol table is necessary, NOT sufficient, for proving
no heap allocation, libc, POSIX, or tracing GC exists in statically linked code.
"""
import argparse
from pathlib import Path
import os
import stat
import subprocess
import sys


class InvalidFirmware(ValueError):
    pass


def check_undefined_symbols(text):
    if type(text) is not str or len(text) > 131072:
        raise InvalidFirmware("unbounded or malformed nm output")
    # One exact ELF is passed to llvm-nm; an object header is not expected.
    # Any non-whitespace output means this is NOT proven to have zero imports.
    unresolved = [line.strip() for line in text.splitlines() if line.strip()][:4]
    if unresolved:
        raise InvalidFirmware("undefined symbols in freestanding ELF: " +
                              "; ".join(unresolved))
    return True


def audit(elf, nm="llvm-nm"):
    # Linux-only, fail-closed inode-bound preflight. Avoid a check/use race in
    # which another process swaps the checked path before nm opens it.
    if not all(hasattr(os, key) for key in ("O_NOFOLLOW", "O_NONBLOCK")) or not os.path.isdir("/proc/self/fd"):
        raise InvalidFirmware("secure Linux ELF descriptor admission unavailable")
    fd = os.open(elf, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    try:
        info = os.fstat(fd)
        if not stat.S_ISREG(info.st_mode) or not 20 <= info.st_size <= 128 * 1024 * 1024:
            raise InvalidFirmware("invalid ELF size or input file type")
        header = os.read(fd, 20)
        if (header[:7] != bytes((127, 69, 76, 70, 1, 1, 1)) or
                header[18:20] != bytes((40, 0))):
            raise InvalidFirmware("requires ARM ELF32 little-endian object")
        try:
            result = subprocess.run(
                [nm, "--undefined-only", f"/proc/self/fd/{fd}"],
                capture_output=True, timeout=20, check=False, pass_fds=(fd,)
            )
        except (OSError, subprocess.TimeoutExpired) as exc:
            raise InvalidFirmware("nm execution unavailable or timed out") from exc
        if result.returncode != 0:
            raise InvalidFirmware("nm could not inspect ELF")
        try:
            output = result.stdout.decode("utf-8", errors="strict")
        except UnicodeError as exc:
            raise InvalidFirmware("invalid nm output encoding") from exc
        return check_undefined_symbols(output)
    finally:
        os.close(fd)


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
