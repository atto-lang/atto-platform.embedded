"""Host log parser controls; synthetic logs are not proof of real QEMU execution."""
import unittest
from scripts.verify_qemu_output import EXPECTED, validate

BOOT = "*** Booting Zephyr OS build v3.7.1 ***"


def guest_log(markers=EXPECTED, *, serial=False, newline="\n"):
    prefix = "[serial] " if serial else ""
    return (newline.join([prefix + BOOT] + [prefix + value for value in markers])
            + newline).encode("utf-8")


class GuestReceiptAdmission(unittest.TestCase):
    def test_exact_ordered_guest_lines_accept_serial_wrapper_and_timeout(self):
        for code in (0, 124):
            for serial in (False, True):
                for newline in ("\n", "\r\n"):
                    with self.subTest(code=code, serial=serial, newline=repr(newline)):
                        validate(guest_log(serial=serial, newline=newline), code)

    def test_preboot_markers_or_shell_echoes_cannot_promote_host_output(self):
        for data in (
            "\n".join(EXPECTED).encode("utf-8"),
            ("\n".join(EXPECTED) + "\n" + BOOT).encode("utf-8"),
            (BOOT + "\n" + "\n".join("echo " + m for m in EXPECTED)).encode("utf-8"),
        ):
            with self.subTest(log=data), self.assertRaisesRegex(ValueError,
                    "no Zephyr guest boot banner|missing guest markers"):
                validate(data, 0)

    def test_reordered_duplicate_or_second_boot_is_denied(self):
        cases = (
            guest_log((EXPECTED[1], EXPECTED[0], EXPECTED[2])),
            guest_log((EXPECTED[0], EXPECTED[0], EXPECTED[1], EXPECTED[2])),
            (guest_log() + (BOOT + "\n").encode("utf-8")),
        )
        for data in cases:
            with self.subTest(log=data), self.assertRaisesRegex(
                    ValueError, "out of order|duplicate guest marker|booted multiple times"):
                validate(data, 0)

    def test_ansi_decoration_does_not_suppress_postboot_faults(self):
        control = guest_log(serial=True)
        colorized = b"\x1b[32m" + control.replace(b"\n", b"\x1b[0m\r\n")
        validate(colorized, 0)
        for fault in (b"FATAL ERROR", b"HARD FAULT",
                      b"ORESLANG_CORTEXM_ACTORS_FAIL"):
            raw = control + b"\x1b[31m" + fault + b"\x1b[0m\n"
            with self.subTest(fault=fault), self.assertRaisesRegex(
                    ValueError, "guest fault/failure"):
                validate(raw, 0)

    def test_guest_markers_do_not_override_abnormal_process_exit(self):
        for code in (-9, 1, 2, 137):
            with self.subTest(exit_code=code), self.assertRaisesRegex(
                    ValueError, "QEMU/west exited unexpectedly"):
                validate(guest_log(), code)


if __name__ == "__main__":
    unittest.main()
