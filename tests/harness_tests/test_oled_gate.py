"""Guards the devicetree switch that turns the panel sleep command on.

Zephyr spells the per-compatible test DT_HAS_COMPAT_STATUS_OKAY(<compat>), built
on the DT_COMPAT_HAS_OKAY_<compat> macros it generates.  It does not generate
DT_HAS_<COMPAT>_ENABLED: that is an undefined identifier, so `#if` reads it as 0
and the guarded code silently compiles out.  A switch written that way fails
open -- the build passes, the symbol exists as an empty function, and nothing
happens at runtime.

This caught a real regression: oled_sleep() was empty on every board and no
panel was ever put to sleep.
"""

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ZEPHCORE = ROOT / "zephcore"

# An undefined macro used in #if is the whole bug: point at the mistake itself.
BOGUS_GATE = re.compile(r"\bDT_HAS_([A-Za-z0-9_]+?)_ENABLED\b")
CORRECT_GATE = re.compile(r"\bDT_HAS_COMPAT_STATUS_OKAY\(([a-z0-9_]+)\)")
# Zephyr also generates CONFIG_DT_HAS_<COMPAT>_ENABLED for Kconfig, which is
# legitimate and must be asked for through IS_ENABLED().
KCONFIG_GATE = re.compile(r"IS_ENABLED\(CONFIG_DT_HAS_([A-Za-z0-9_]+)_ENABLED\)")

PANELS = {"sinowealth_sh1106", "solomon_ssd1306"}


def source_files():
    for path in ZEPHCORE.rglob("*"):
        if path.is_file() and path.suffix in (".c", ".h", ".cpp"):
            yield path


class OledGateTests(unittest.TestCase):
    def test_no_gate_on_a_macro_zephyr_does_not_generate(self):
        offenders = []
        for path in source_files():
            for lineno, line in enumerate(path.read_text(errors="ignore").splitlines(), 1):
                if "DT_HAS_COMPAT_STATUS_OKAY" in line or "CONFIG_DT_HAS_" in line:
                    continue
                if BOGUS_GATE.search(line):
                    offenders.append(f"{path.relative_to(ROOT)}:{lineno}: {line.strip()}")
        self.assertEqual(
            offenders,
            [],
            "DT_HAS_<COMPAT>_ENABLED is undefined, so #if reads it as 0 and the "
            "guarded code compiles out silently. Use "
            "DT_HAS_COMPAT_STATUS_OKAY(<vendor_model>):\n" + "\n".join(offenders),
        )

    def test_the_panel_gate_names_a_real_panel_binding(self):
        gates = CORRECT_GATE.findall((ZEPHCORE / "helpers" / "oled_power.c").read_text())

        self.assertTrue(gates, "oled_power.c no longer gates on a compatible")
        for compatible in gates:
            with self.subTest(compatible=compatible):
                self.assertIn(
                    compatible,
                    PANELS,
                    f"'{compatible}' is not a panel binding Zephyr declares, so the "
                    f"gate is permanently false and the panel is never put to sleep",
                )

    def test_the_sleep_command_reaches_the_bus(self):
        """The switch being present is not enough: the command must reach a device.

        This asserts the shape that emits code -- a devicetree spec, an I2C
        write, and a bus check -- so a switch that silently becomes false, or a
        panel on another bus, still fails here.
        """
        source = (ZEPHCORE / "helpers" / "oled_power.c").read_text()
        self.assertRegex(source, r"I2C_DT_SPEC_GET\(")
        self.assertRegex(source, r"i2c_write_dt\(")
        self.assertIn("DT_ON_BUS(", source, "a panel on another bus must not compile in")


if __name__ == "__main__":
    unittest.main()
