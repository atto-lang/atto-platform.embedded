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
                    audit_ir(f"declare ptr @{symbol}(i64)\\n")
                with self.assertRaisesRegex(ValueError, "forbidden"):
                    audit_external_symbols(f"         U {symbol}\\n")
        for instruction in (
            'module asm "bl malloc"',
            'call void asm sideeffect "bl malloc", ""()',
            'invoke void asm "bl malloc", ""() to label %ok unwind label %fail',
        ):
            with self.subTest(instruction=instruction):
                with self.assertRaisesRegex(ValueError, "inline assembly forbidden"):
                    audit_ir(instruction)

    def test_nm_output_cannot_hide_malformed_entries(self):
        with self.assertRaisesRegex(ValueError, "unrecognized nm"):
            audit_external_symbols("nm utility output not a symbol listing")
        with self.assertRaisesRegex(ValueError, "unrecognized nm"):
            audit_external_symbols("U\n")
        with self.assertRaisesRegex(ValueError, "unrecognized nm"):
            audit_external_symbols("junk\n         U GC_malloc")

    def test_no_broad_claim_of_firmware_qualification(self):
        # This gate screens the compiler-produced IR and relocatable object.
        # It does not see linked libraries, linker scripts, dynamic plugins,
        # runtime allocation counts or hardware-dependent ISR behavior.
        audit_external_symbols("00000000 T oreslang_cortexm_bootstrap\n")


if __name__ == "__main__":
    unittest.main()
