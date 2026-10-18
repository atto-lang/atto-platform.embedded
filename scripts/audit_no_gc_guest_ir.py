#!/usr/bin/env python3
"""Fail-closed *pre-link* screening of Cortex-M Oreslang guest IR and object symbols.

This is NOT a complete firmware, compiler, toolchain, FFI or linker audit.
Cortex-M guest IR is deliberately a freestanding, no-allocator scalar subset.
"""
import re
import sys
from pathlib import Path

MAX_IR_BYTES = 2 * 1024 * 1024
# Match actual LLVM instructions/declarations, not comments or arbitrary raw
# bytes. Conservative false positives are safer than silently linking GC.
GC_STRATEGY = re.compile(r'\bgc\s+"[^"]*"')
FORBIDDEN_SYMBOL = re.compile(
    r'(?:llvm\.gcroot|llvm\.gcread|llvm\.gcwrite|'
    r'llvm\.gc\.(?:statepoint|relocate|result)(?:\.[\w.]+)?|'
    r'llvm\.experimental\.gc\.[\w.]+|'
    r'GC_(?:malloc|malloc_atomic|realloc|gcollect|enable|disable)|'
    r'mono_gc_[A-Za-z_0-9]*|jl_gc_[A-Za-z_0-9]*|PyGC_Collect|'
    r'gc_(?:alloc|collect|mark|sweep|root)[A-Za-z_0-9]*|'
    r'(?:malloc|calloc|realloc|free|_malloc_r|_free_r|_sbrk)|'
    r'__(?:libc|wrap|real)_(?:malloc|calloc|realloc|free)|'
    r'__(?:rust_alloc|rust_alloc_zeroed|rust_realloc|rust_dealloc)|'
    r'(?:mi_|je_|tc_)(?:malloc|calloc|realloc|free)|'
    r'_Z(?:nw|na|dl|da)[A-Za-z_0-9]*)'
)
LLVM_SYMBOL = re.compile(r'@(?:"([^"]+)"|([A-Za-z_$._][\w$._]*))')
INLINE_ASM = re.compile(r'\b(?:module\s+asm|(?:call|invoke|callbr)\b[^\n]*\basm\b)')
NM_LINE = re.compile(r'^\s*(?:(?:[0-9a-fA-F]+)\s+)?([A-Za-z?])\s+(\S+)\s*$')


def audit_ir(text: str) -> None:
    if len(text.encode("utf-8")) > MAX_IR_BYTES:
        raise ValueError("IR exceeds 2 MiB memory-profile admission limit")
    for number, line in enumerate(text.splitlines(), 1):
        # LLVM comments start with ; outside quoted symbol names. Here we
        # skip whole-line comments but retain the remainder, rejecting any
        # ambiguous quoted text conservatively rather than losing a symbol.
        if line.lstrip().startswith(";"):
            continue
        if GC_STRATEGY.search(line):
            raise ValueError(f"line {number}: tracing GC strategy forbidden")
        # Inline/module assembly hides its actual calls from both the
        # LLVM symbol regex and ordinary no-heap IR analysis. Strict
        # freestanding mode cannot admit that uninspectable execution.
        if INLINE_ASM.search(line):
            raise ValueError(f"line {number}: unverifiable inline assembly forbidden")
        for match in LLVM_SYMBOL.finditer(line):
            symbol = match.group(1) or match.group(2)
            if FORBIDDEN_SYMBOL.fullmatch(symbol):
                raise ValueError(f"line {number}: collector or heap allocator symbol forbidden: {symbol}")


def audit_external_symbols(nm_output: str) -> None:
    """Ban forbidden native collector/heap references in the actual ARM object.

    Undefined allowed compiler ABI helpers (e.g. __aeabi_lmul) are NOT proof
    of a freestanding final executable. The final firmware still needs a
    complete linked-image, symbol, library and runtime-allocation audit.
    """
    for number, line in enumerate(nm_output.splitlines(), 1):
        if not line.strip():
            continue
        match = NM_LINE.fullmatch(line)
        if not match:
            raise ValueError(f"line {number}: unrecognized nm symbol output")
        _kind, symbol = match.groups()
        # Macho/COFF name decor? Current profile is ARM ELF; do not strip
        # arbitrary prefixes that might launder forbidden symbols.
        if FORBIDDEN_SYMBOL.fullmatch(symbol):
            raise ValueError(f"line {number}: forbidden native symbol: {symbol}")


def main(argv=None):
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--ir", type=Path, required=True)
    parser.add_argument("--nm-output", type=Path)
    args = parser.parse_args(argv)
    audit_ir(args.ir.read_text(encoding="utf-8"))
    if args.nm_output:
        audit_external_symbols(args.nm_output.read_text(encoding="utf-8"))
    print("PRELINK_NO_GC_SCREEN_OK (not firmware/linked-image certification)")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, UnicodeError) as error:
        print("PRELINK_NO_GC_DENIED: " + str(error), file=sys.stderr)
        raise SystemExit(1)
