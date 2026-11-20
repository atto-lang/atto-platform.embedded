#!/usr/bin/env python3
"""Bounded compiler execution: total stream budget, timeouts and descendants."""
import io
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock
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

    def test_limits_must_be_finite_and_integer_bounded_before_spawn(self):
        for timeout in (float('nan'), float('inf'), -float('inf'),
                        True, '10', 3601, 10**1000):
            with self.subTest(timeout=repr(timeout)):
                with self.assertRaises(ValueError):
                    self.run_python("print('should not run')", timeout=timeout)
        for budget in (float('nan'), float('inf'), 1.5, True,
                       '1024', -1, 16 * 1024 * 1024 + 1):
            with self.subTest(max_bytes=repr(budget)):
                with self.assertRaises(ValueError):
                    self.run_python("print('should not run')", max_bytes=budget)
        self.assertIn('ok', self.run_python("print('ok')",
                      max_bytes=16 * 1024 * 1024, timeout=1).stdout)

    def test_reader_failure_cannot_return_success(self):
        class BadReader:
            def read(self, size):
                raise OSError('injected pipe read fault')
            def close(self):
                pass

        class FakeProcess:
            stdout = BadReader()
            stderr = io.BytesIO(b'')
            pid = 99999999
            returncode = 0
            def wait(self, timeout=None):
                return 0
            def kill(self):
                pass

        with mock.patch('bounded_process.subprocess.Popen', return_value=FakeProcess()):
            with mock.patch('bounded_process.os.killpg', create=True):
                with self.assertRaisesRegex(RuntimeError, 'compiler output reader failed'):
                    run_bounded(['fake-compiler'])

    def test_pipe_close_failure_cannot_return_success(self):
        class BadCloser:
            def read(self, size):
                return b''
            def close(self):
                raise OSError('injected pipe close fault')

        class FakeProcess:
            stdout = BadCloser()
            stderr = io.BytesIO(b'')
            pid = 99999999
            returncode = 0
            def wait(self, timeout=None):
                return 0
            def kill(self):
                pass

        with mock.patch('bounded_process.subprocess.Popen', return_value=FakeProcess()):
            with mock.patch('bounded_process.os.killpg', create=True):
                with self.assertRaisesRegex(RuntimeError, 'compiler output reader failed'):
                    run_bounded(['fake-compiler'])

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


    def test_exact_shared_stdout_stderr_budget_is_admitted(self):
        code = "import os; os.write(1, b'a'*5); os.write(2, b'b'*6)"
        result = self.run_python(code, max_bytes=11, timeout=3)
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, 'a'*5)
        self.assertEqual(result.stderr, 'b'*6)

    def test_shared_stream_budget_rejects_single_byte_overrun(self):
        code = "import os; os.write(1, b'a'*5); os.write(2, b'b'*6)"
        with self.assertRaisesRegex(RuntimeError, "compiler output budget exceeded"):
            self.run_python(code, max_bytes=10, timeout=3)

    def test_invalid_compiler_encoding_must_not_return_success(self):
        # Captured compiler diagnostics must not be silently lossy-decoded.
        code = "import os; os.write(2, bytes([255]))"
        with self.assertRaises(UnicodeDecodeError):
            self.run_python(code, timeout=3)

    def test_invalid_budgets_are_rejected_before_process_creation(self):
        with mock.patch('bounded_process.subprocess.Popen') as spawn:
            for options in ({'timeout': float('nan')},
                            {'timeout': True}, {'max_bytes': float('inf')},
                            {'max_bytes': True}, {'max_bytes': 16*1024*1024+1}):
                with self.subTest(options=options):
                    with self.assertRaises(ValueError):
                        run_bounded(['must-never-run'], **options)
            spawn.assert_not_called()


if __name__ == "__main__":
    unittest.main()
