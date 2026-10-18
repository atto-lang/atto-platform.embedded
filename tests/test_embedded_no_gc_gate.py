#!/usr/bin/env python3
"""Source-free adversarial profile gate tests; not a real ARM firmware proof."""
import unittest
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from audit_no_gc_guest_ir import audit_ir, audit_external_symbols


class NoCollectorPrelinkTest(unittest.TestCase):
    def test_scalar_source_generated_ir_shape_admitted(self):
        audit_ir('''target triple = "thumbv7m-none-eabi"
define i64 @oreslang_cortexm_bootstrap() {
entry:
  ret i64 42
}
''')
        audit_external_symbols("00000000 T oreslang_cortexm_bootstrap\n"
                               "         U __aeabi_lmul\n")

    def test_comments_and_numeric_literals_are_not_gc_calls(self):
        audit_ir('''; declare ptr @GC_malloc(i64)
define i64 @oreslang_cortexm_bootstrap() {
entry:
  ret i64 112835
}
''')

    def test_known_llvm_tracing_gc_directions_rejected(self):
        for fragment in (
            'define void @f() gc "shadow-stack" { ret void }',
            'declare void @llvm.gcroot(ptr, ptr)',
            'declare token @llvm.experimental.gc.statepoint(i64)',
            'declare ptr @GC_malloc(i64)',
            'declare void @gc_collect()',
            'declare ptr @malloc(i32)',
            'declare void @_sbrk(i32)',
            'declare void @"llvm.gc.relocate.p0"(ptr)',
        ):
            with self.subTest(fragment=fragment):
                with self.assertRaisesRegex(ValueError, "forbidden"):
                    audit_ir(fragment)

    def test_actual_symbol_table_admission_rejects_heap_and_gc(self):
        for symbol in ("GC_malloc", "gc_collect", "mono_gc_collect",
                       "jl_gc_collect", "malloc", "free", "_sbrk"):
            with self.subTest(symbol=symbol):
                for kind in ("U", "T"):
                    with self.assertRaisesRegex(ValueError, "forbidden native symbol"):
                        audit_external_symbols(f"         {kind} {symbol}\n")

    def test_wrapped_allocators_and_inline_assembly_not_laundered(self):
        aliases = (
            "__libc_malloc", "__wrap_malloc", "__real_free",
            "__rust_alloc", "__rust_alloc_zeroed", "__rust_realloc",
            "__rust_dealloc", "mi_malloc", "je_free", "tc_realloc",
            "_Znwm", "_Znam", "_ZdlPv", "_ZdaPv",
        )
        for symbol in aliases:
            with self.subTest(symbol=symbol):
                with self.assertRaisesRegex(ValueError, "forbidden"):
                    audit_ir(f"declare ptr @{symbol}(i64)\n")
                with self.assertRaisesRegex(ValueError, "forbidden"):
                    audit_external_symbols(f"         U {symbol}\n")
        for instruction in (
            'module asm "bl malloc"',
            'call void asm sideeffect "bl malloc", ""()',
            'invoke void asm "bl malloc", ""() to label %ok unwind label %fail',
        ):
            with self.subTest(instruction=instruction):
                with self.assertRaisesRegex(ValueError, "inline assembly forbidden"):
                    audit_ir(instruction)

    def test_indirect_calls_cannot_launder_hidden_heap_functions(self):
        for ir in (
            '  %a = call ptr %fn(i64 42)',
            '  call void %callback()',
            '  invoke void %fn() to label %ok unwind label %failure',
            '  callbr void %fn() to label %ok [label %other]',
            '  %v = call i64 inttoptr (i64 999 to ptr)()',
            '  call void %fn() ; @permitted()',
            '  call void %fn() [ "deopt"(ptr @permitted()) ]',
            '  %result = call ptr %fn(ptr @safe())',
            '  invoke void %fn() [ "deopt"(ptr @safe()) ] to label %ok unwind label %bad',
        ):
            with self.subTest(ir=ir):
                with self.assertRaisesRegex(ValueError, "indirect or unresolved call forbidden"):
                    audit_ir(ir)
        audit_ir('source_filename = "call.ores"')
        audit_ir('declare void @llvm.lifetime.start.p0(i64, ptr)\n'
                 'call void @llvm.lifetime.start.p0(i64 8, ptr %slot)')

    def test_escaped_quoted_symbol_cannot_hide_allocator_name(self):
        for fragment in (
            r'declare ptr @"GC_\6dalloc"(i64)',
            r'declare void @"llvm.\67croot"(ptr, ptr)',
        ):
            with self.subTest(fragment=fragment):
                with self.assertRaisesRegex(ValueError, "escaped LLVM symbol"):
                    audit_ir(fragment)
        audit_ir('declare void @"safe;identifier"(i64)')

    def test_nm_output_cannot_hide_malformed_entries(self):
        with self.assertRaisesRegex(ValueError, "unrecognized nm"):
            audit_external_symbols("nm utility output not a symbol listing")
        with self.assertRaisesRegex(ValueError, "unrecognized nm"):
            audit_external_symbols("U\n")
        with self.assertRaisesRegex(ValueError, "unrecognized nm"):
            audit_external_symbols("junk\n         U GC_malloc")

    def test_oversized_external_nm_symbols_fail_closed(self):
        with self.assertRaisesRegex(ValueError, "bounded symbol-table"):
            audit_external_symbols("U a\\n" * (2 * 1024 * 1024 // 4 + 1))

    def test_cli_rejects_oversized_proof_files_before_read(self):
        import subprocess
        import tempfile
        with tempfile.TemporaryDirectory() as directory:
            ir = Path(directory) / "guest.ll"
            nm = Path(directory) / "guest.nm"
            script = Path(__file__).resolve().parents[1] / "scripts" / "audit_no_gc_guest_ir.py"
            ir.write_text('define void @f() { ret void }\\n')
            nm.write_bytes(b"U a\\n" * (2 * 1024 * 1024 // 4 + 1))
            p = subprocess.run([sys.executable, str(script), "--ir", str(ir),
                                "--nm-output", str(nm)],
                               capture_output=True, text=True, timeout=10)
            self.assertEqual(p.returncode, 1, p.stdout)
            self.assertIn("bounded symbol-table", p.stderr)
            ir.write_bytes(b";" * (2 * 1024 * 1024 + 1))
            p = subprocess.run([sys.executable, str(script), "--ir", str(ir)],
                               capture_output=True, text=True, timeout=10)
            self.assertEqual(p.returncode, 1, p.stdout)
            self.assertIn("2 MiB", p.stderr)

    def test_cli_rejects_symlink_and_fifo_without_blocking(self):
        import os
        import subprocess
        import tempfile
        if os.name != "posix":
            self.skipTest("Linux/POSIX O_NOFOLLOW and O_NONBLOCK test")
        with tempfile.TemporaryDirectory() as directory:
            original = Path(directory) / "original.ll"
            alias = Path(directory) / "alias.ll"
            pipe = Path(directory) / "blocked.ll"
            script = Path(__file__).resolve().parents[1] / "scripts" / "audit_no_gc_guest_ir.py"
            original.write_text('define void @f() { ret void }\\n')
            alias.symlink_to(original)
            os.mkfifo(pipe)
            for hostile in (alias, pipe):
                p = subprocess.run([sys.executable, str(script), "--ir", str(hostile)],
                                   capture_output=True, text=True, timeout=5)
                self.assertEqual(p.returncode, 1, p.stdout)
                self.assertIn("PRELINK_NO_GC_DENIED", p.stderr)

    def test_no_broad_claim_of_firmware_qualification(self):
        # This gate screens the compiler-produced IR and relocatable object.
        # It does not see linked libraries, linker scripts, dynamic plugins,
        # runtime allocation counts or hardware-dependent ISR behavior.
        audit_external_symbols("00000000 T oreslang_cortexm_bootstrap\n")


if __name__ == "__main__":
    unittest.main()
