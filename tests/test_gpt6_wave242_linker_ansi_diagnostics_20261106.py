"""Host-only regression for Zephyr linker warning normalization and line semantics."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from check_linker_orphans import validate


class LinkerAnsiDiagnosticsWave242(unittest.TestCase):
    def test_fragmented_ansi_warning_is_still_rejected(self):
        raw = b"ld.lld: warning: or\x1b[31mphan\x1b[0m section .ARM.extab\r\n"
        with self.assertRaisesRegex(ValueError, "orphan-section diagnostic at line 1"):
            validate(raw)

    def test_osc_terminal_title_does_not_fabricate_orphan_warning(self):
        raw = (b"\x1b]0;orphan section in terminal title\x07"
               b"[100%] Linking zephyr.elf\r\n")
        validate(raw)

    def test_line_number_after_clean_lines_and_carriage_return(self):
        raw = (b"[ 10%] Building objects\r\n"
               b"[ 90%] Linking firmware\r\n"
               b"ld: warning: section .unexpected is orphaned\r\n")
        with self.assertRaisesRegex(ValueError, "at line 3:"):
            validate(raw)

    def test_word_boundaries_do_not_reject_non_diagnostics(self):
        validate(b"building orphanage_section fixture, no linker warnings\n")
        validate(b"review orphanages in unrelated docs; linking succeeded\n")

    def test_reverse_section_warning_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "orphan-section diagnostic"):
            validate(b"ld: warning: section '.ARM.exidx' is orphaned\n")


if __name__ == "__main__":
    unittest.main()
