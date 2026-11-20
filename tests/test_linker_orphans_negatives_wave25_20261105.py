"""Host linker-log admission tests; not firmware ELF, QEMU or hardware evidence."""
import importlib.util
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location(
    "orphan_gate", Path(__file__).resolve().parents[1] / "scripts/check_linker_orphans.py")
gate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)

class LinkerOrphanAdmission(unittest.TestCase):
    def test_exact_log_size_ceiling_and_empty_input(self):
        with patch.object(gate, "MAX_LOG_BYTES", 128):
            gate.validate(b"x" * 128)
            with self.assertRaisesRegex(ValueError, "exceeds"):
                gate.validate(b"x" * 129)
            with self.assertRaisesRegex(ValueError, "empty"):
                gate.validate(b"")

    def test_ansi_color_cannot_hide_an_orphan_input_section(self):
        raw = (b"ld: normal\nld: no warnings\n"
               b"\x1b[31mld: orphan section .unexpected from foo.o\x1b[0m\n")
        with self.assertRaisesRegex(ValueError, "orphan-section diagnostic at line 3"):
            gate.validate(raw)
        with self.assertRaisesRegex(ValueError, "orphan-section diagnostic"):
            gate.validate(b"ld: section .foo has been orphaned\n")

    def test_unrelated_orphan_words_are_not_fabricated_linker_sections(self):
        gate.validate(b"worker orphan process was reaped\n"
                      b"all explicit linker sections accounted for\n")
        gate.validate(b"ld: linked image successfully\n")

    def test_symlink_and_nonregular_log_targets_are_denied(self):
        if not hasattr(os, "O_NOFOLLOW"):
            self.skipTest("requires O_NOFOLLOW")
        with tempfile.TemporaryDirectory() as root:
            d = Path(root)
            valid = d / "build.log"
            valid.write_bytes(b"ld: final image linked\n")
            gate.check_file(valid)
            alias = d / "alias.log"
            alias.symlink_to(valid)
            with self.assertRaises((OSError, ValueError)):
                gate.check_file(alias)
            with self.assertRaises((OSError, ValueError)):
                gate.check_file(d)

if __name__ == "__main__":
    unittest.main()
