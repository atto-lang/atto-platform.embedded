import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from check_freestanding_symbols import InvalidFirmware, check_undefined_symbols


class FreestandingSymbolTests(unittest.TestCase):
    def test_no_undefined_symbols_passes_preflight(self):
        self.assertTrue(check_undefined_symbols(""))
        self.assertTrue(check_undefined_symbols("\n\nfirmware.elf:\n"))

    def test_libc_and_pthread_imports_fail(self):
        for nm in (" U malloc", "                 U calloc",
                   "                 U pthread_create", " w _sbrk",
                   " U _Znwm", " U __libc_init_array"):
            with self.subTest(nm=nm), self.assertRaises(InvalidFirmware):
                check_undefined_symbols(nm+"\n")

    def test_unknown_imports_fail_not_just_denylist(self):
        with self.assertRaises(InvalidFirmware):
            check_undefined_symbols("  U mysterious_allocator\n")

    def test_malformed_or_oversized_output_fails(self):
        for sample in (None, b" U malloc", "x" * 131073):
            with self.subTest(sample=str(sample)[:15]), self.assertRaises(InvalidFirmware):
                check_undefined_symbols(sample)


if __name__ == "__main__":
    unittest.main()
