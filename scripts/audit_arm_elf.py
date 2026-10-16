#!/usr/bin/env python3
"""Check ARM ELF unwind tables are read-only, loaded, and non-stack-executable.

This is a linker placement check; it does not establish MPU configuration,
a secure bare-metal stack, interrupt isolation or physical-board correctness.
"""
from pathlib import Path
import argparse
import struct

PT_LOAD = 1
PT_GNU_STACK = 0x6474E551
PF_X = 1
PF_W = 2
SHF_WRITE = 1
SHF_ALLOC = 2


def require(ok, message):
    if not ok:
        raise ValueError(message)


def check_elf(data: bytes) -> int:
    require(len(data) >= 52 and data[:6] == b"\x7fELF\x01\x01",
            "expected ELF32 little-endian")
    header = struct.unpack_from("<HHIIIIIHHHHHH", data, 16)
    kind, machine, version = header[:3]
    phoff, shoff, phentsize, phnum, shentsize, shnum, shstrndx = (
        header[4], header[5], header[8], header[9], header[10], header[11], header[12]
    )
    require(kind == 2 and machine == 40 and version == 1,
            "expected ARM executable ELF")
    require(phnum > 0 and phentsize >= 32 and
            phoff + phnum * phentsize <= len(data), "invalid program-header table")
    require(shnum > 0 and shentsize >= 40 and shstrndx < shnum and
            shoff + shnum * shentsize <= len(data), "invalid section-header table")
    ph = [struct.unpack_from("<IIIIIIII", data, phoff + i * phentsize)
          for i in range(phnum)]
    sh = [struct.unpack_from("<IIIIIIIIII", data, shoff + i * shentsize)
          for i in range(shnum)]
    names_table = sh[shstrndx]
    offset, size = names_table[4:6]
    require(offset + size <= len(data), "invalid section-name table")
    names = data[offset:offset + size]

    def get_name(index):
        require(index < len(names), "invalid section name offset")
        end = names.find(b"\0", index)
        require(end >= 0, "unterminated section name")
        return names[index:end].decode("ascii")

    # An absent GNU_STACK header is NOT proof of a non-executable stack.
    # The Zephyr linker is explicitly passed -z noexecstack; fail closed
    # if its final firmware does not retain the corresponding program header.
    stack_headers = [p for p in ph if p[0] == PT_GNU_STACK]
    require(len(stack_headers) == 1,
            "expected exactly one explicit GNU_STACK program header")
    require(not stack_headers[0][6] & PF_X,
            "executable GNU_STACK segment")
    for segment in ph:
        if segment[0] != PT_LOAD:
            continue
        file_offset, file_size, mem_size = segment[1], segment[4], segment[5]
        require(file_size <= mem_size, "PT_LOAD file size exceeds memory size")
        require(file_offset + file_size <= len(data),
                "PT_LOAD file range outside ELF")
    loads = [p for p in ph if p[0] == PT_LOAD]
    require(bool(loads), "no loadable segments")
    checked = 0
    for section in sh:
        name = get_name(section[0])
        if name not in (".ARM.extab", ".ARM.exidx") and not name.startswith(
                (".ARM.extab.", ".ARM.exidx.")):
            continue
        kind, flags, address, file_offset, length = (
            section[1], section[2], section[3], section[4], section[5]
        )
        if length == 0:
            continue
        require(kind != 8, f"{name} is SHT_NOBITS, not file-backed")
        require(file_offset + length <= len(data),
                f"{name} section bytes outside ELF file")
        require(flags & SHF_ALLOC and not flags & SHF_WRITE,
                f"{name} is writable or not allocated")
        require(address + length <= 0x100000000, f"{name} address wraps ELF32")
        owners = [p for p in loads
                  if address >= p[2] and address + length <= p[2] + p[5]]
        require(bool(owners), f"{name} not contained in a PT_LOAD segment")
        require(all(not (p[6] & PF_W) for p in owners),
                f"{name} mapped writable by PT_LOAD")
        # A section inside PT_LOAD.p_memsz can still point into zero-filled
        # memory or unrelated on-disk bytes. Require exact PT_LOAD.p_filesz
        # coverage and its p_offset + (sh_addr - p_vaddr) file mapping.
        backed = [p for p in owners
                  if address + length <= p[2] + p[4]]
        require(bool(backed), f"{name} not backed by PT_LOAD file bytes")
        require(any(file_offset == p[1] + address - p[2] for p in backed),
                f"{name} file offset differs from PT_LOAD mapping")
        checked += 1
    require(checked > 0, "missing .ARM exception-index/table sections")
    return checked


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--elf", type=Path, required=True)
    args = parser.parse_args()
    count = check_elf(args.elf.read_bytes())
    print(f"PASS {count} read-only load-mapped ARM unwind sections and non-executable GNU_STACK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
