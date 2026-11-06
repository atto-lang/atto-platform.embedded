"""Host-only compiler process output budgeting by *raw bytes*, not code points."""
from pathlib import Path
import subprocess
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from bounded_process import run_bounded


class Utf8ByteBudgetWave243(unittest.TestCase):
    def child(self, code, *, budget):
        return run_bounded([sys.executable, "-c", code],
                           max_bytes=budget, timeout=3)

    def test_one_multibyte_character_uses_four_bytes(self):
        code = 'import os; os.write(1, bytes([240,159,154,128]))'
        result = self.child(code, budget=4)
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "🚀")
        self.assertEqual(result.stderr, "")
        with self.assertRaisesRegex(RuntimeError, "compiler output budget exceeded"):
            self.child(code, budget=3)

    def test_combined_stderr_stdout_counts_bytes_not_characters(self):
        code = ('import os; '
                'os.write(1, bytes([240,159,154,128])); '
                'os.write(2, bytes([195,184]))')
        result = self.child(code, budget=6)
        self.assertEqual((result.stdout, result.stderr), ("🚀", "ø"))
        with self.assertRaisesRegex(RuntimeError, "compiler output budget exceeded"):
            self.child(code, budget=5)

    def test_nonzero_compiler_exit_retains_complete_bounded_unicode_diagnostics(self):
        code = ('import os,sys; '
                'os.write(2, bytes([195,184,10])); '
                'sys.exit(17)')
        result = self.child(code, budget=3)
        self.assertEqual(result.returncode, 17)
        self.assertEqual(result.stdout, "")
        self.assertEqual(result.stderr, "ø\n")

    def test_invalid_utf8_never_yields_a_laundered_successful_transcript(self):
        code = 'import os; os.write(2, bytes([195]))'
        with self.assertRaises(UnicodeDecodeError):
            self.child(code, budget=1)


if __name__ == "__main__":
    unittest.main()
