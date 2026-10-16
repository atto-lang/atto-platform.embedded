#!/usr/bin/env python3
"""Reject linker orphan-section diagnostics in bounded firmware build logs.

Run after every successful firmware west build, against captured stdout+stderr.
This complements, but does not replace, the final-ELF layout/stack audit.
"""
import argparse
import os
from pathlib import Path
import re
import stat
import sys

MAX_LOG_BYTES = 16 * 1024 * 1024
ANSI = re.compile(r"\x1b(?:\[[0-?]*[ -/]*[@-~]|\][^\x07]*(?:\x07|\x1b\\))")
ORPHAN = re.compile(
    r"\borphan(?:ed)?[\W_]+(?:input[\W_]+)?sections?\b"
    r"|\bsections?\b[^\n]{0,96}\borphan(?:ed)?\b", re.IGNORECASE
)


def validate(raw: bytes) -> None:
    """Reject empty/oversized logs and any orphan-section diagnostic."""
    if not raw:
        raise ValueError("empty linker build log")
    if len(raw) > MAX_LOG_BYTES:
        raise ValueError(f"linker build log exceeds {MAX_LOG_BYTES} bytes")
    cleaned = ANSI.sub("", raw.decode("utf-8", errors="replace"))
    for line_number, line in enumerate(cleaned.splitlines(), 1):
        if ORPHAN.search(line):
            detail = "".join(c if c.isprintable() else "?" for c in line).strip()[:240]
            raise ValueError(f"orphan-section diagnostic at line {line_number}: {detail}")


def check_file(path: Path) -> None:
    # Follow neither a symlink nor an unbounded stream. Detect resize on read.
    fd = os.open(path, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0))
    with os.fdopen(fd, "rb") as handle:
        meta = os.fstat(handle.fileno())
        if not stat.S_ISREG(meta.st_mode):
            raise ValueError("linker log must be a regular file")
        if meta.st_size > MAX_LOG_BYTES:
            raise ValueError(f"linker build log exceeds {MAX_LOG_BYTES} bytes")
        validate(handle.read(MAX_LOG_BYTES + 1))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", type=Path, required=True)
    args = parser.parse_args()
    try:
        check_file(args.log)
    except (OSError, ValueError) as error:
        print(f"FAIL linker orphan-section audit: {error}", file=sys.stderr)
        return 1
    print("PASS linker build log contains no orphan-section diagnostics")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
