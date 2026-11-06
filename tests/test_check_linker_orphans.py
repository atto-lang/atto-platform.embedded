#!/usr/bin/env python3
"""Positive and negative cases for the bounded Zephyr linker-log gate."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from check_linker_orphans import MAX_LOG_BYTES, check_file, validate


class LinkerLogGateTests(unittest.TestCase):
    def test_successful_build(self):
        validate(b"[100%] Linking C executable zephyr.elf\nMemory region Used Size\n")

    def test_gnu_ld_arm_extab_warning(self):
        with self.assertRaisesRegex(ValueError, "orphan-section diagnostic"):
            validate(b"/usr/bin/ld: warning: orphan section `.ARM.extab' from `libgcc.a' being placed\n")

    def test_other_orphan_section(self):
        with self.assertRaisesRegex(ValueError, "orphan-section diagnostic"):
            validate(b"ld.lld: warning: orphan section .my_new_section is being placed in .rodata\n")

    def test_reversed_diagnostic(self):
        with self.assertRaisesRegex(ValueError, "orphan-section diagnostic"):
            validate(b"linker: warning: section '.ARM.extab' is an orphan\n")

    def test_ansi_and_crlf(self):
        with self.assertRaisesRegex(ValueError, "orphan-section diagnostic"):
            validate(b"\x1b[1;33mld: warning: orphan section .ARM.extab\x1b[0m\r\n")

    def test_malformed_utf8(self):
        with self.assertRaisesRegex(ValueError, "orphan-section diagnostic"):
            validate(b"ld: warning: orphan \xff section .ARM.extab\n")

    def test_unrecognized_control_bytes_cannot_split_orphan_warning(self):
        for middle in (b"\x08", b"\x00", b"\x7f", b"\xc2\x9b"):
            warning = b"ld: warning: or" + middle + b"phan section .ARM.extab\n"
            with self.subTest(middle=middle):
                with self.assertRaisesRegex(ValueError, "unsafe control"):
                    validate(warning)

    def test_invisible_unicode_and_line_separator_cannot_hide_warning(self):
        for middle in ("\u200b", "\u200d", "\ufeff", "\u2028", "\u2029"):
            raw = ("ld: warning: or" + middle + "phan section .ARM.extab\n").encode("utf-8")
            with self.subTest(middle=repr(middle)):
                with self.assertRaisesRegex(ValueError, "unsafe control"):
                    validate(raw)

    def test_malformed_utf8_between_warning_letters_is_not_clean(self):
        with self.assertRaisesRegex(ValueError, "invalid UTF-8"):
            validate(b"ld: warning: or\xffphan section .ARM.extab\n")

    def test_recognized_ansi_and_valid_printable_utf8_remain_accepted(self):
        validate(b"\x1b]0;orphan section in terminal title\x07"
                 b"[100%] Linking firmware\r\n")
        validate("✓ Linked firmware café\n".encode("utf-8"))

    def test_empty_log(self):
        with self.assertRaisesRegex(ValueError, "empty"):
            validate(b"")

    def test_oversized_log(self):
        with self.assertRaisesRegex(ValueError, "exceeds"):
            validate(b"a" * (MAX_LOG_BYTES + 1))

    def test_exact_limit(self):
        validate(b"a" * MAX_LOG_BYTES)

    def test_file_missing(self):
        with tempfile.TemporaryDirectory() as temp:
            with self.assertRaises(FileNotFoundError):
                check_file(Path(temp) / "nonexistent.log")

    def test_file_symlink_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            file = Path(temp) / "source"
            file.write_text("good\n")
            link = Path(temp) / "alias"
            link.symlink_to(file)
            with self.assertRaises(OSError):
                check_file(link)

    def test_command_exit_statuses(self):
        script = Path(__file__).resolve().parents[1] / "scripts" / "check_linker_orphans.py"
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "build.log"
            path.write_bytes(b"Linking zephyr.elf\n")
            result = subprocess.run([sys.executable, "-I", str(script), "--log", str(path)], capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            path.write_bytes(b"ld: warning: orphan section `.ARM.extab'\n")
            result = subprocess.run([sys.executable, "-I", str(script), "--log", str(path)], capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b"FAIL", result.stderr)


if __name__ == "__main__":
    unittest.main()
