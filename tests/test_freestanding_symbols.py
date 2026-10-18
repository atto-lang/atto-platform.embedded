import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from check_freestanding_symbols import InvalidFirmware, check_undefined_symbols, audit


class FreestandingSymbolTests(unittest.TestCase):
    def test_no_undefined_symbols_passes_preflight(self):
        self.assertTrue(check_undefined_symbols(""))
        self.assertTrue(check_undefined_symbols("\n  \n"))
        with self.assertRaises(InvalidFirmware):
            check_undefined_symbols("firmware.elf:\n")

    def test_libc_and_pthread_imports_fail(self):
        for nm in (" U malloc", "                 U calloc",
                   "                 U pthread_create", " w _sbrk",
                   " U _Znwm", " U __libc_init_array"):
            with self.subTest(nm=nm), self.assertRaises(InvalidFirmware):
                check_undefined_symbols(nm+"\n")

    def test_unknown_imports_fail_not_just_denylist(self):
        with self.assertRaises(InvalidFirmware):
            check_undefined_symbols("  U mysterious_allocator\n")

    def test_malformed_arm_elf_and_symlinks_rejected_before_nm(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "firmware.elf"
            path.write_bytes(b"not an ELF" + bytes(20))
            with self.assertRaisesRegex(InvalidFirmware, "ARM ELF32"):
                audit(path, "this-program-does-not-exist")
            alias = path.parent / "alias.elf"
            alias.symlink_to(path)
            with self.assertRaises(OSError):
                audit(alias, "this-program-does-not-exist")

    def test_relocatable_arm_elf_is_not_firmware(self):
        # Valid ELF identification + EM_ARM is insufficient: require ET_EXEC.
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "relocatable.o"
            header = bytearray(52)
            header[:7] = bytes((127, 69, 76, 70, 1, 1, 1))
            header[16:18] = bytes((1, 0))  # ET_REL
            header[18:20] = bytes((40, 0))  # EM_ARM
            path.write_bytes(header)
            with self.assertRaisesRegex(InvalidFirmware, "executable"):
                audit(path, "nonexistent-nm")

    def test_non_arm_executable_is_not_firmware(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "x86.elf"
            header = bytearray(52)
            header[:7] = bytes((127, 69, 76, 70, 1, 1, 1))
            header[16:18] = bytes((2, 0))
            header[18:20] = bytes((62, 0))  # x86_64
            path.write_bytes(header)
            with self.assertRaisesRegex(InvalidFirmware, "ARM ELF32"):
                audit(path, "nonexistent-nm")

    def test_fifo_and_empty_files_rejected_without_blocking(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "firmware.elf"
            path.write_bytes(b"")
            with self.assertRaises(InvalidFirmware):
                audit(path)
            path.unlink()
            if hasattr(os, "mkfifo"):
                os.mkfifo(path)
                with self.assertRaises(InvalidFirmware):
                    audit(path)

    def test_malformed_or_oversized_output_fails(self):
        for sample in (None, b" U malloc", "x" * 131073):
            with self.subTest(sample=str(sample)[:15]), self.assertRaises(InvalidFirmware):
                check_undefined_symbols(sample)


if __name__ == "__main__":
    unittest.main()
