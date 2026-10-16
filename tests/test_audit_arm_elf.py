#!/usr/bin/env python3
"""Synthetic ELF32 negative tests for ARM firmware memory layout."""
import struct
import unittest
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from audit_arm_elf import check_elf


def fixture(section_flags=2, load_flags=5, stack_flags=6, addr=0x08000100):
    labels = b"\x00.shstrtab\x00.ARM.extab\x00"
    data = bytearray(0x400)
    data[:16] = b"\x7fELF\x01\x01\x01" + b"\0" * 9
    phnum = 1 + (stack_flags is not None)
    struct.pack_into("<HHIIIIIHHHHHH", data, 16,
                     2, 40, 1, 0x08000000, 52, 0x200, 0, 52, 32, phnum, 40, 3, 1)
    struct.pack_into("<IIIIIIII", data, 52,
                     1, 0, 0x08000000, 0x08000000, 0x180, 0x180, load_flags, 0x1000)
    if stack_flags is not None:
        struct.pack_into("<IIIIIIII", data, 84,
                         0x6474E551, 0, 0, 0, 0, 0, stack_flags, 4)
    data[0x80:0x80+len(labels)] = labels
    struct.pack_into("<IIIIIIIIII", data, 0x200+40,
                     1, 3, 0, 0, 0x80, len(labels), 0, 0, 1, 0)
    struct.pack_into("<IIIIIIIIII", data, 0x200+80,
                     labels.index(b".ARM.extab"), 1, section_flags,
                     addr, 0x100, 8, 0, 0, 4, 0)
    return bytes(data)


class LinkerLayoutTests(unittest.TestCase):
    def test_read_only_load_and_nx_stack(self):
        self.assertEqual(check_elf(fixture(stack_flags=6)), 1)

    def test_reject_write_flag_on_unwind(self):
        with self.assertRaisesRegex(ValueError, "writable"):
            check_elf(fixture(section_flags=3))

    def test_reject_writable_load_mapping(self):
        with self.assertRaisesRegex(ValueError, "mapped writable"):
            check_elf(fixture(load_flags=7))

    def test_reject_unmapped_unwind(self):
        with self.assertRaisesRegex(ValueError, "not contained"):
            check_elf(fixture(addr=0x20000000))

    def test_reject_absent_gnu_stack(self):
        with self.assertRaisesRegex(ValueError, "exactly one explicit GNU_STACK"):
            check_elf(fixture(stack_flags=None))

    def test_reject_load_file_larger_than_memory(self):
        data = bytearray(fixture())
        struct.pack_into("<I", data, 52+16, 0x190)
        with self.assertRaisesRegex(ValueError, "file size exceeds memory size"):
            check_elf(bytes(data))

    def test_reject_load_file_range_outside_elf(self):
        data = bytearray(fixture())
        struct.pack_into("<I", data, 52+4, len(data)-1)
        with self.assertRaisesRegex(ValueError, "file range outside ELF"):
            check_elf(bytes(data))

    def test_reject_executable_stack(self):
        with self.assertRaisesRegex(ValueError, "executable GNU_STACK"):
            check_elf(fixture(stack_flags=7))

    def test_reject_unwind_without_file_storage(self):
        data = bytearray(fixture())
        struct.pack_into("<I", data, 0x200 + 80 + 4, 8)  # SHT_NOBITS
        with self.assertRaisesRegex(ValueError, "file-backed bytes"):
            check_elf(bytes(data))

    def test_reject_section_outside_elf_file(self):
        data = bytearray(fixture())
        struct.pack_into("<I", data, 0x200 + 80 + 16, len(data) - 1)
        with self.assertRaisesRegex(ValueError, "file range exceeds"):
            check_elf(bytes(data))

    def test_reject_section_outside_load_file_range(self):
        data = bytearray(fixture())
        struct.pack_into("<I", data, 52 + 16, 0x100)  # p_filesz
        with self.assertRaisesRegex(ValueError, "outside PT_LOAD file-backed"):
            check_elf(bytes(data))

    def test_reject_inconsistent_section_file_offset(self):
        data = bytearray(fixture())
        struct.pack_into("<I", data, 0x200 + 80 + 16, 0x108)
        with self.assertRaisesRegex(ValueError, "file offset does not match"):
            check_elf(bytes(data))

    def test_accept_nonzero_load_file_offset_when_mapped(self):
        data = bytearray(fixture())
        struct.pack_into("<I", data, 52 + 4, 0x20)  # p_offset
        struct.pack_into("<I", data, 0x200 + 80 + 16, 0x120)  # sh_offset
        self.assertEqual(check_elf(bytes(data)), 1)

    def test_reject_duplicate_gnu_stack_headers(self):
        data = bytearray(fixture())
        struct.pack_into("<H", data, 44, 3)  # e_phnum
        struct.pack_into("<IIIIIIII", data, 52 + 2 * 32,
                         0x6474E551, 0, 0, 0, 0, 0, 6, 4)
        with self.assertRaisesRegex(ValueError, "exactly one explicit GNU_STACK"):
            check_elf(bytes(data))

    def test_reject_truncated_elf(self):
        with self.assertRaisesRegex(ValueError, "expected ELF32"):
            check_elf(b"not an elf")


if __name__ == "__main__":
    unittest.main()
