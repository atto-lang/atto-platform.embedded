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
import unicodedata

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
    # The normal linker warning must be found even when its ASCII letters are
    # separated by adversarial controls. Recognized ANSI escapes are removed
    # first; unrecognized C0/C1, invisible Unicode format characters, and
    # invalid UTF-8 cannot be accepted as a clean transcript.
    if "\ufffd" in cleaned:
        raise ValueError("invalid UTF-8 or replacement character in linker log")
    for character in cleaned:
        if character in "\r\n\t":
            continue
        if unicodedata.category(character) in ("Cc", "Cf", "Cs"):
            raise ValueError("unsafe control or invisible format in linker log")


def check_file(path: Path) -> None:
    # A FIFO can block in open() before fstat() ever rejects it. Require
    # O_NONBLOCK and O_NOFOLLOW so neither special files nor symlinks can
    # stall the provenance gate or redirect it to a different file.
    if not hasattr(os, "O_NOFOLLOW") or not hasattr(os, "O_NONBLOCK"):
        raise ValueError("safe linker-log admission unsupported on this host")
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    with os.fdopen(fd, "rb") as handle:
        before = os.fstat(handle.fileno())
        if not stat.S_ISREG(before.st_mode):
            raise ValueError("linker log must be a regular file")
        if before.st_size > MAX_LOG_BYTES:
            raise ValueError(f"linker build log exceeds {MAX_LOG_BYTES} bytes")
        raw = handle.read(MAX_LOG_BYTES + 1)
        after = os.fstat(handle.fileno())
        # Reject visibly moving/truncated log files before parsing. This
        # cannot authenticate a producer or detect every same-size rewrite.
        if (len(raw) != before.st_size or
                (before.st_dev, before.st_ino, before.st_size,
                 before.st_mtime_ns, before.st_ctime_ns) !=
                (after.st_dev, after.st_ino, after.st_size,
                 after.st_mtime_ns, after.st_ctime_ns)):
            raise ValueError("linker log changed during bounded read")
        validate(raw)


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
