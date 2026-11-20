"""Host-only bounded linker log file tests: bytes, not character count."""
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from check_linker_orphans import MAX_LOG_BYTES, check_file


class LinkerLogByteBudgetWave244(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / "linker.log"

    def test_exact_maximum_byte_count_is_admitted(self):
        self.path.write_bytes(b"x" * MAX_LOG_BYTES)
        check_file(self.path)

    def test_one_byte_over_fails_before_parsing(self):
        self.path.write_bytes(b"x" * (MAX_LOG_BYTES + 1))
        with self.assertRaisesRegex(ValueError, "exceeds"):
            check_file(self.path)

    def test_utf8_multibyte_content_is_measured_in_bytes(self):
        # Repeating the UTF-8 bytes of é still consumes exactly two bytes.
        self.path.write_bytes("é".encode("utf-8") * (MAX_LOG_BYTES // 2))
        check_file(self.path)
        with self.path.open("ab") as output:
            output.write(b"x")
        with self.assertRaisesRegex(ValueError, "exceeds"):
            check_file(self.path)

    def test_warning_at_end_of_exact_budget_is_not_hidden(self):
        warning = b"ld: warning: orphan section .ARM.exidx\n"
        self.path.write_bytes(b"x" * (MAX_LOG_BYTES - len(warning)) + warning)
        with self.assertRaisesRegex(ValueError, "orphan-section diagnostic"):
            check_file(self.path)

    def test_empty_regular_log_does_not_pass(self):
        self.path.write_bytes(b"")
        with self.assertRaisesRegex(ValueError, "empty linker build log"):
            check_file(self.path)


if __name__ == "__main__":
    unittest.main()
