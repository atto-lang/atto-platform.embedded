#!/usr/bin/env python3
"""Bounded compiler execution: total stream budget, timeouts and descendants."""
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from bounded_process import run_bounded


class BoundedProcessTests(unittest.TestCase):
    def run_python(self, source, **options):
        return run_bounded([sys.executable, "-c", source], **options)

    def test_captures_both_streams_and_exit_status(self):
        script = "import sys; print('hello'); print('warning', file=sys.stderr); sys.exit(3)"
        result = self.run_python(script)
        self.assertEqual(result.returncode, 3)
        self.assertEqual(result.stdout.strip(), "hello")
        self.assertEqual(result.stderr.strip(), "warning")

    def test_combined_output_budget_rejected(self):
        script = ("import os; os.write(1, b'a'*32768); "
                  "os.write(2, b'b'*32768)")
        with self.assertRaisesRegex(RuntimeError, "compiler output budget exceeded"):
            self.run_python(script, max_bytes=32768, timeout=4)

    def test_invalid_budgets(self):
        with self.assertRaises(ValueError):
            self.run_python("pass", max_bytes=0)
        with self.assertRaises(ValueError):
            self.run_python("pass", timeout=0)

    @unittest.skipUnless(sys.platform.startswith("linux"), "process group semantics tested on Linux")
    def test_descendant_holding_stdout_causes_failure(self):
        # Parent exits normally, but a subprocess inherits the output pipe.
        code = ("import subprocess,sys; "
                "subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(15)']); "
                "print('done',flush=True)")
        started = time.monotonic()
        with self.assertRaisesRegex(RuntimeError, "descendants held captured output"):
            self.run_python(code, timeout=5)
        self.assertLess(time.monotonic() - started, 4)

    @unittest.skipUnless(sys.platform.startswith("linux"), "process group semantics tested on Linux")
    def test_timeout_kills_child_group(self):
        with tempfile.TemporaryDirectory() as d:
            marker = Path(d) / "orphan-written"
            child = "import time; from pathlib import Path; time.sleep(1.2); Path(" + repr(str(marker)) + ").write_text('leaked')"
            code = ("import subprocess,sys,time; "
                    "subprocess.Popen([sys.executable, '-c', " + repr(child) + "]); "
                    "time.sleep(12)")
            with self.assertRaises(subprocess.TimeoutExpired):
                self.run_python(code, timeout=0.6)
            time.sleep(1)
            self.assertFalse(marker.exists(), "child survived compiler timeout")


if __name__ == "__main__":
    unittest.main()
