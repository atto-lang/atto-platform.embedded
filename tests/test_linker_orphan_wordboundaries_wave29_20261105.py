"""Host-only linker-log negative and positive controls; no firmware claim."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from check_linker_orphans import validate


class OrphanDiagnosticWordBoundaryTests(unittest.TestCase):
    def test_ansi_osc_prefix_cannot_conceal_diagnostic(self):
        with self.assertRaisesRegex(ValueError, "orphan-section diagnostic"):
            validate(b"\x1b]0;build\x07ld: warning: orphan section .ARM.extab\n")

    def test_input_section_plural_and_case_variants(self):
        for raw in (b"ld: ORPHANED input sections .foo\n",
                    b"ld: orphan-input section .bar\n",
                    b"ld: section .baz is ORPHANED\n"):
            with self.subTest(raw=raw), self.assertRaisesRegex(ValueError, "orphan-section diagnostic"):
                validate(raw)

    def test_similar_words_do_not_trigger_false_positive(self):
        for raw in (b"orphanage relocation map generated\n",
                    b"unrelated sections linked successfully\n",
                    b"Linking output.elf\n"):
            with self.subTest(raw=raw):
                validate(raw)

    def test_empty_log_is_not_success(self):
        with self.assertRaisesRegex(ValueError, "empty linker build log"):
            validate(b"")


if __name__ == "__main__":
    unittest.main()
