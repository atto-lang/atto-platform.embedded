"""Real-filesystem regression gates for bounded Zephyr linker diagnostics.

Host-only: these tests do not assert a firmware/guest run.
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import check_linker_orphans as gate


class LinkerFileAdmissionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / "firmware-build.log"

    def test_normal_regular_file_is_accepted(self):
        self.path.write_bytes(b"[100%] Linking zephyr.elf\n")
        gate.check_file(self.path)

    @unittest.skipUnless(hasattr(os, "mkfifo") and os.name == "posix",
                         "requires POSIX nonblocking FIFO")
    def test_fifo_is_refused_without_waiting_for_a_writer(self):
        os.mkfifo(self.path)
        script = Path(gate.__file__).resolve()
        result = subprocess.run(
            [sys.executable, "-I", str(script), "--log", str(self.path)],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=4)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b"regular file", result.stderr)

    def test_symlink_does_not_redirect_to_existing_regular_log(self):
        original = Path(self.temp.name) / "original.log"
        original.write_bytes(b"Linking zephyr.elf\n")
        self.path.symlink_to(original)
        with self.assertRaises(OSError):
            gate.check_file(self.path)

    def test_truncated_read_is_never_treated_as_clean_log(self):
        self.path.write_bytes(b"Linking zephyr.elf\n")
        with patch.object(gate.os, "read", return_value=b""):
            # fdopen().read() does not call os.read(), so use a mock handle
            # that returns early EOF for the same inode and size.
            pass
        original_open = gate.os.fdopen
        class ShortFile:
            def __init__(self, real):
                self.real = real
            def __enter__(self):
                self.real.__enter__()
                return self
            def __exit__(self, *args):
                return self.real.__exit__(*args)
            def fileno(self):
                return self.real.fileno()
            def read(self, maximum):
                return b""
        def short_file(fd, mode):
            return ShortFile(original_open(fd, mode))
        with patch.object(gate.os, "fdopen", side_effect=short_file):
            with self.assertRaisesRegex(ValueError, "changed during bounded read"):
                gate.check_file(self.path)

    def test_same_size_rewrite_detected_by_stat_metadata(self):
        self.path.write_bytes(b"Linking zephyr.elf\n")
        original = os.fstat
        calls = [0]
        def changed_stat(fd):
            value = original(fd)
            calls[0] += 1
            if calls[0] != 2:
                return value
            return SimpleNamespace(
                st_dev=value.st_dev, st_ino=value.st_ino,
                st_size=value.st_size, st_mtime_ns=value.st_mtime_ns + 1,
                st_ctime_ns=value.st_ctime_ns, st_mode=value.st_mode)
        with patch.object(gate.os, "fstat", side_effect=changed_stat):
            with self.assertRaisesRegex(ValueError, "changed during bounded read"):
                gate.check_file(self.path)
        self.assertEqual(calls[0], 2)

    def test_admitted_log_still_rejects_a_real_orphan_diagnostic(self):
        self.path.write_bytes(b"ld.lld: warning: orphan section .ARM.extab\n")
        with self.assertRaisesRegex(ValueError, "orphan-section diagnostic"):
            gate.check_file(self.path)


if __name__ == "__main__":
    unittest.main()
