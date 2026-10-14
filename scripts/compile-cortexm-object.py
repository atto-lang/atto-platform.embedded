#!/usr/bin/env python3
"""Compile an actual admitted Oreslang source to a freestanding ARM ELF object.

Fail closed: no fallback host compile, simulated object, or Oreslang interpreter.
"""
import argparse
import os
import pathlib
import subprocess
import tempfile


def main() -> int:
    p = argparse.ArgumentParser()
    for name in ("compiler", "clang", "source", "output"):
        p.add_argument("--" + name, required=True)
    a = p.parse_args()
    compiler = pathlib.Path(a.compiler).resolve(strict=True)
    source = pathlib.Path(a.source).resolve(strict=True)
    output = pathlib.Path(a.output).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    # Object must be published only after both phases succeed. On errors the
    # existing object is untouched so an interrupted build cannot publish junk.
    with tempfile.TemporaryDirectory(prefix=".ores-arm-", dir=output.parent) as tmp:
        ir = pathlib.Path(tmp) / "guest.ll"
        obj = pathlib.Path(tmp) / "guest.o"
        with ir.open("wb") as dest:
            subprocess.run([str(compiler), "--target=thumbv7m-none-eabi", str(source)],
                           stdout=dest, check=True)
        content = ir.read_text(encoding="utf-8")
        if 'target triple = "thumbv7m-none-eabi"' not in content:
            raise SystemExit("compiler failed to mark target triple")
        subprocess.run([a.clang, "--target=thumbv7m-none-eabi", "-mcpu=cortex-m3",
                        "-mthumb", "-O2", "-ffreestanding", "-fno-builtin",
                        "-fno-stack-protector", "-x", "ir", "-c", str(ir),
                        "-o", str(obj)], check=True)
        blob = obj.read_bytes()
        if len(blob) < 52 or blob[:4] != b"\x7fELF" or blob[4] != 1 or blob[5] != 1 or int.from_bytes(blob[18:20], "little") != 40:
            raise SystemExit("Clang output is not a 32-bit little-endian ARM ELF object")
        os.replace(obj, output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
