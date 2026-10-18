#!/usr/bin/env python3
"""Fail-closed *pre-link* screening of Cortex-M Oreslang guest IR and object symbols.

This is NOT a complete firmware, compiler, toolchain, FFI or linker audit.
Cortex-M guest IR is deliberately a freestanding, no-allocator scalar subset.
"""
import os
import re
import stat
import sys
from pathlib import Path

MAX_IR_BYTES = 2 * 1024 * 1024
MAX_NM_BYTES = 2 * 1024 * 1024
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
# Named direct calls only: an indirect native function pointer can call
# an allocator without mentioning its name in the LLVM module.
CALL_OP = re.compile(r'^\s*(?:%(?:[\w.$-]+|"[^"]+")\s*=\s*)?(?:(?:tail|musttail|notail)\s+)?(?:call|invoke|callbr)\b')
# First actual named function call token controls invocation: a later @safe()
# in an operand bundle must not mask an earlier indirect %fn() call.
CALLEE_TOKEN = re.compile(r'([%@])(?:"(?:[^"\\]|\\.)+"|[A-Za-z_$._][\w$._]*)\s*\(')
# LLVM byte escapes can spell a forbidden function without matching its
# lexical identifier. Reject escaped symbols until an LLVM parser decodes them.
ESCAPED_QUOTED_SYMBOL = re.compile(r'@"(?:[^"\\]|\\.)*\\[0-9A-Fa-f]{2}')

NM_LINE = re.compile(r'^\s*(?:(?:[0-9a-fA-F]+)\s+)?([A-Za-z?])\s+(\S+)\s*$')


def strip_llvm_comment(line: str) -> str:
    """Remove LLVM comments without stripping semicolons inside quoted names."""
    quoted = False
    escaped = False
    for pos, char in enumerate(line):
        if escaped:
            escaped = False
        elif char == "\\" and quoted:
            escaped = True
        elif char == '"':
            quoted = not quoted
        elif char == ";" and not quoted:
            return line[:pos]
    return line


def audit_ir(text: str) -> None:
    if len(text.encode("utf-8")) > MAX_IR_BYTES:
        raise ValueError("IR exceeds 2 MiB memory-profile admission limit")
    for number, raw_line in enumerate(text.splitlines(), 1):
        # Ignore *inline* comments. A trailing '; @allowed()' must not
        # launder an indirect call as a named call.
        line = strip_llvm_comment(raw_line)
        if not line.strip():
            continue
        if ESCAPED_QUOTED_SYMBOL.search(line):
            raise ValueError(f"line {number}: escaped LLVM symbol needs semantic proof")
        if GC_STRATEGY.search(line):
            raise ValueError(f"line {number}: tracing GC strategy forbidden")
        # Inline/module assembly hides its actual calls from both the
        # LLVM symbol regex and ordinary no-heap IR analysis. Strict
        # freestanding mode cannot admit that uninspectable execution.
        if INLINE_ASM.search(line):
            raise ValueError(f"line {number}: unverifiable inline assembly forbidden")
        call = CALL_OP.match(line)
        if call is not None:
            first_target = CALLEE_TOKEN.search(line, call.end())
            if first_target is None or first_target.group(1) != "@":
                raise ValueError(f"line {number}: indirect or unresolved call forbidden")
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
    if not isinstance(nm_output, str) or len(nm_output.encode("utf-8")) > MAX_NM_BYTES:
        raise ValueError("nm output exceeds bounded symbol-table admission limit")
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


def read_bounded_regular(path: Path, limit: int, description: str) -> str:
    """Reject symlinks, FIFOs and oversized/racing files before full decoding."""
    flags = os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_NONBLOCK", 0)
    fd = os.open(path, flags)
    try:
        metadata = os.fstat(fd)
        if not stat.S_ISREG(metadata.st_mode):
            raise ValueError(f"{description} must be a regular non-symlink file")
        if metadata.st_size > limit:
            raise ValueError(f"{description} exceeds bounded admission limit ({limit} bytes)")
        # Re-check the actual bytes, not merely the pre-read stat size:
        # a racing writer must never force an unbounded allocation.
        with os.fdopen(fd, "rb", closefd=False) as file:
            contents = file.read(limit + 1)
        if len(contents) > limit:
            raise ValueError(f"{description} exceeds bounded admission limit ({limit} bytes)")
        return contents.decode("utf-8")
    finally:
        os.close(fd)


def main(argv=None):
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--ir", type=Path, required=True)
    parser.add_argument("--nm-output", type=Path)
    args = parser.parse_args(argv)
    audit_ir(read_bounded_regular(args.ir, MAX_IR_BYTES, "IR"))
    if args.nm_output:
        audit_external_symbols(read_bounded_regular(args.nm_output, MAX_NM_BYTES, "nm output"))
    print("PRELINK_NO_GC_SCREEN_OK (not firmware/linked-image certification)")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, UnicodeError) as error:
        print("PRELINK_NO_GC_DENIED: " + str(error), file=sys.stderr)
        raise SystemExit(1)
