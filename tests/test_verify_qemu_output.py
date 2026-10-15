#!/usr/bin/env python3
"""Host-only negative/positive tests of the Zephyr/QEMU output proof gate."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from verify_qemu_output import EXPECTED, validate


def console(*lines):
    return ("*** Booting Zephyr OS build v4.3.0 ***\n" + "\n".join(lines) + "\n").encode()


class QemuMarkerVerifierTests(unittest.TestCase):
    def test_normal_timeout(self):
        validate(console(*EXPECTED), 124)

    def test_normal_clean_exit(self):
        validate(console(*EXPECTED), 0)

    def test_crlf_and_ansi_terminal(self):
        lines = [f"\x1b[1;32m{marker}\x1b[0m\r" for marker in EXPECTED]
        validate(console(*lines).replace(b"\n", b"\r\n"), 124)

    def test_console_prefixes(self):
        validate(console(*["[serial] " + x for x in EXPECTED]), 124)

    def test_binary_nul_console(self):
        validate(console(*EXPECTED).replace(b"\n", b"\x00\n"), 124)

    def test_no_guest_boot(self):
        with self.assertRaisesRegex(ValueError, "no Zephyr guest boot"):
            validate(("\n".join(EXPECTED) + "\n").encode(), 124)

    def test_missing_marker(self):
        with self.assertRaisesRegex(ValueError, "missing guest markers"):
            validate(console(EXPECTED[0]), 124)

    def test_duplicate_actor_marker(self):
        with self.assertRaisesRegex(ValueError, "duplicate guest marker"):
            validate(console(*EXPECTED, EXPECTED[2]), 124)

    def test_out_of_order(self):
        with self.assertRaisesRegex(ValueError, "out of order"):
            validate(console(EXPECTED[1], EXPECTED[0], EXPECTED[2]), 124)

    def test_reboot_is_not_one_successful_boot(self):
        with self.assertRaisesRegex(ValueError, "booted multiple times"):
            validate(console(*EXPECTED, "*** Booting Zephyr OS build v4.3.0 ***"), 124)

    def test_guest_failure_after_success(self):
        with self.assertRaisesRegex(ValueError, "guest fault/failure"):
            validate(console(*EXPECTED, "ORESLANG_CORTEXM_ACTORS_FAIL"), 124)

    def test_arm_hardfault_after_success(self):
        with self.assertRaisesRegex(ValueError, "guest fault/failure"):
            validate(console(*EXPECTED, "***** HARD FAULT *****"), 124)

    def test_bad_exit_even_if_guest_markers_present(self):
        with self.assertRaisesRegex(ValueError, "exited unexpectedly"):
            validate(console(*EXPECTED), 1)

    def test_partial_identifier_not_a_marker(self):
        with self.assertRaisesRegex(ValueError, "missing guest markers"):
            validate(console(*EXPECTED[:2], "FAKE_" + EXPECTED[2]), 124)

    def test_quoted_shell_echo_not_a_marker(self):
        with self.assertRaisesRegex(ValueError, "missing guest markers"):
            validate(console(*EXPECTED[:2], "echo '" + EXPECTED[2] + "'"), 124)

    def test_shell_command_echo_cannot_supply_missing_marker(self):
        with self.assertRaisesRegex(ValueError, "missing guest markers"):
            validate(console(EXPECTED[0], EXPECTED[1],
                             "echo " + EXPECTED[2]), 124)

    def test_untrusted_log_prefix_cannot_supply_missing_marker(self):
        with self.assertRaisesRegex(ValueError, "missing guest markers"):
            validate(console(EXPECTED[0], EXPECTED[1],
                             "[host] " + EXPECTED[2]), 124)

    def test_echoed_guest_boot_banner_is_not_a_boot(self):
        raw = ("echo '*** Booting Zephyr OS build v4.3.0 ***'\n"
               + "\n".join(EXPECTED) + "\n").encode()
        with self.assertRaisesRegex(ValueError, "no Zephyr guest boot"):
            validate(raw, 124)

    def test_mpu_fault_after_success_rejected(self):
        with self.assertRaisesRegex(ValueError, "guest fault/failure"):
            validate(console(*EXPECTED, "***** MPU FAULT *****"), 124)

    def test_kernel_panic_after_success_rejected(self):
        with self.assertRaisesRegex(ValueError, "guest fault/failure"):
            validate(console(*EXPECTED, "KERNEL PANIC"), 124)

    def test_markers_before_guest_boot_do_not_count(self):
        raw = ("\n".join(EXPECTED) + "\n").encode() + console(*EXPECTED)
        validate(raw, 124)


if __name__ == "__main__":
    unittest.main()
