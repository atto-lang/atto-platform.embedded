#!/usr/bin/env python3
"""Prove the existing static C actor ABI object links without hosted libraries.

This is not compilation/execution of an Oreslang guest `core` or OTP module.
"""
import argparse
import pathlib
import subprocess
import tempfile
import shutil

def run(argv):
    completed = subprocess.run(argv, text=True, capture_output=True, timeout=20)
    if completed.returncode != 0:
        raise RuntimeError("command failed: " + repr(argv) + "\n"
                           + completed.stdout + "\n" + completed.stderr)
    return completed.stdout

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--cc", required=True)
    p.add_argument("--source-root", type=pathlib.Path, required=True)
    args = p.parse_args()
    source = args.source_root / "src/ores_embedded_static_actors.c"
    header = args.source_root / "include/ores_embedded_static_actors.h"
    if not source.is_file() or not header.is_file():
        raise RuntimeError("source and header must exist")
    nm = shutil.which("nm")
    if not nm:
        raise RuntimeError("nm required: no silent skip of undefined-symbol gate")
    with tempfile.TemporaryDirectory(prefix="ores-static-freestanding-") as tmp:
        obj = pathlib.Path(tmp) / "actors.o"
        elf = pathlib.Path(tmp) / "actors.elf"
        run([args.cc, "-std=c11", "-O0", "-ffreestanding", "-fno-builtin",
             "-fno-stack-protector", "-fno-pie", "-Wall", "-Wextra",
             "-Wpedantic", "-Werror", "-I", str(args.source_root / "include"),
             "-c", str(source), "-o", str(obj)])
        unresolved_object = run([nm, "-u", str(obj)]).strip()
        if unresolved_object:
            raise RuntimeError("actor object needs external runtime symbols:\n"
                               + unresolved_object)
        run([args.cc, "-nostdlib", "-no-pie", "-Wl,--no-undefined",
             "-Wl,-e,ores_embedded_actors_reset", str(obj), "-o", str(elf)])
        unresolved_elf = run([nm, "-u", str(elf)]).strip()
        if unresolved_elf:
            raise RuntimeError("freestanding actor ELF imports symbols:\n" +
                               unresolved_elf)
        if not elf.is_file() or elf.stat().st_size == 0:
            raise RuntimeError("missing freestanding ELF")
    print("FREESTANDING_C_STATIC_ACTOR_NO_LIBC_LINK_PASS (not Oreslang guest OTP)")

if __name__ == "__main__":
    main()
