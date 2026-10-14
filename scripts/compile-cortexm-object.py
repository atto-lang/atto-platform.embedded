#!/usr/bin/env python3
"""Build a genuine Oreslang Cortex-M ELF object with atomic publication.

The caller must pass a trusted compiler and Clang. This is not a security
sandbox for executing a malicious compiler/toolchain binary.
"""
import argparse
import os
import pathlib
import shutil
import subprocess
import tempfile

TARGET = "thumbv7m-none-eabi"
GUEST_SYMBOL = "oreslang_cortexm_bootstrap"
MAX_IR_BYTES = 2 * 1024 * 1024
MAX_OBJECT_BYTES = 4 * 1024 * 1024


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def main() -> int:
    p = argparse.ArgumentParser()
    for name in ("compiler", "clang", "source", "output"):
        p.add_argument("--" + name, required=True)
    a = p.parse_args()
    compiler = pathlib.Path(a.compiler).resolve(strict=True)
    source = pathlib.Path(a.source).resolve(strict=True)
    clang = shutil.which(a.clang) or str(pathlib.Path(a.clang).resolve(strict=True))
    output = pathlib.Path(a.output).resolve()
    require(compiler.is_file() and os.access(compiler, os.X_OK),
            "Oreslang compiler must be an executable file")
    require(source.is_file(), "Oreslang source must be an existing file")
    require(output != source and output != compiler and
            output != pathlib.Path(clang).resolve(),
            "refusing to overwrite compiler, source, or Clang executable")
    require(source.stat().st_size <= 128 * 1024, "Cortex-M source budget exceeded")
    output.parent.mkdir(parents=True, exist_ok=True)
    nm = shutil.which("arm-none-eabi-nm") or shutil.which("llvm-nm") or shutil.which("nm")
    require(nm is not None, "ARM-capable nm required to verify exported guest symbol")

    with tempfile.TemporaryDirectory(prefix=".ores-arm-", dir=output.parent) as tmp:
        ir = pathlib.Path(tmp) / "guest.ll"
        obj = pathlib.Path(tmp) / "guest.o"
        with ir.open("wb") as dest:
            subprocess.run([str(compiler), "--target=" + TARGET, str(source)],
                           stdout=dest, check=True, timeout=90)
        require(ir.stat().st_size <= MAX_IR_BYTES, "compiler IR budget exceeded")
        content = ir.read_text(encoding="utf-8")
        require('target triple = "' + TARGET + '"' in content,
                "compiler failed to mark Cortex-M triple")
        require("@ores_embedded_probe(" in content, "expected guest symbol missing from IR")
        subprocess.run([clang, "--target=" + TARGET, "-mcpu=cortex-m3",
                        "-mthumb", "-O2", "-ffreestanding", "-fno-builtin",
                        "-fno-stack-protector", "-x", "ir", "-c", str(ir),
                        "-o", str(obj)], check=True, timeout=90)
        require(obj.stat().st_size <= MAX_OBJECT_BYTES, "ARM object budget exceeded")
        blob = obj.read_bytes()
        require(len(blob) >= 52 and blob[:4] == b"\x7fELF" and
                blob[4] == 1 and blob[5] == 1, "invalid ELF32 little-endian object")
        require(int.from_bytes(blob[16:18], "little") == 1 and
                int.from_bytes(blob[18:20], "little") == 40,
                "expected relocatable ARM ELF object")
        symbols = subprocess.run([nm, "--defined-only", "--extern-only", str(obj)],
                                 text=True, capture_output=True, check=True, timeout=30)
        require(any(len(row.split()) >= 3 and row.split()[-1] == GUEST_SYMBOL and
                    row.split()[-2].upper() == "T"
                    for row in symbols.stdout.splitlines()),
                "required global Cortex-M guest function missing from object")
        os.replace(obj, output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
