"""Guards the flash-size Kconfig choice the ESP32 MMU page size is derived from.

CONFIG_ESPTOOLPY_FLASHSIZE_* looks like it only reaches esptool, so removing it
as redundant with the devicetree's zephyr,flash node seems safe.  It is not: on
the SoCs whose Kconfig.mmu keys off that choice, the value picks MMU_PAGE_SIZE.
With the choice removed, it falls back to its 2 MB default and moves those parts
to the 32 KB page size -- a silent layout change nobody asked for.

On every other ESP32 the choice is inert, so a board that states it for the MMU
alone is not a defect.  The only requirement is that a board running on an
MMU-dependent SoC states a size at all.
"""

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ZEPHCORE = ROOT / "zephcore"
HAL = ROOT / "modules" / "hal" / "espressif" / "components" / "soc"

FLASH_CHOICE = re.compile(r"^CONFIG_ESPTOOLPY_FLASHSIZE_(\w+)=y", re.MULTILINE)
MMU_DEPENDENCY = re.compile(r"^Kconfig\.mmu$", re.MULTILINE)


def mmu_socs():
    """SoCs whose page size comes from the flash-size choice."""
    return sorted(p.parent.name for p in HAL.glob("*/Kconfig.mmu") if p.is_file())


def esp32_boards():
    """Every ESP32 board directory under the tree."""
    return sorted(p for p in (ZEPHCORE / "boards" / "esp32").iterdir() if p.is_dir())


def target_socs(board):
    """The SoC names in a board manifest's target line, e.g. xiao/esp32c6/hpcore.

    Read with a regex rather than a YAML parser: the host runner takes no third
    party dependency, and `target:` is one line by construction.
    """
    manifest = board / "zephcore.yml"
    if not manifest.is_file():
        return set()
    match = re.search(r"^target:\s*(\S+)", manifest.read_text(errors="ignore"), re.MULTILINE)
    if not match:
        return set()
    return {part for part in match.group(1).split("/") if part}


def confs(state):
    return [p for p in state.rglob("*.conf") if p.is_file()]


def states_flash_size(conf):
    return bool(FLASH_CHOICE.search(conf.read_text(errors="ignore")))


class FlashSizeChoiceTests(unittest.TestCase):
    def test_the_mmu_dependency_is_where_we_think_it_is(self):
        """If upstream moves the dependency, the rule below stops meaning anything."""
        socs = mmu_socs()
        self.assertTrue(socs, "no Kconfig.mmu found under modules/hal/espressif")
        # The scan is only useful if the MMU really reads the choice.
        text = (HAL / socs[0] / "Kconfig.mmu").read_text(errors="ignore")
        self.assertRegex(
            text,
            r"MMU_PAGE_SIZE|ESPTOOLPY_FLASHSIZE",
            f"{socs[0]}/Kconfig.mmu no longer relates the page size to the flash size",
        )

    def test_every_board_on_an_mmu_dependent_soc_states_its_flash_size(self):
        dependent = [s for s in mmu_socs() if self._keys_off_flash_size(s)]
        if not dependent:
            self.skipTest("no SoC derives MMU_PAGE_SIZE from the flash-size choice")
        unsized = []
        scanned = 0
        for board in esp32_boards():
            soci = target_socs(board)
            if not soci:
                continue
            scanned += 1
            if not soci & set(dependent):
                continue
            # The board's own confs plus the shared ESP32 common one, which it
            # always includes before its own -- either may state the size.
            candidates = confs(board) + [ZEPHCORE / "boards" / "common" / "esp32_common.conf"]
            if not any(states_flash_size(c) for c in candidates if c.is_file()):
                unsized.append(board.name)
        self.assertTrue(scanned, "no ESP32 board manifest was read: the scan is wrong")
        self.assertEqual(
            unsized,
            [],
            f"these boards run on {dependent}, whose MMU_PAGE_SIZE is derived from "
            f"CONFIG_ESPTOOLPY_FLASHSIZE_*: with no size stated the choice falls "
            f"back to 2 MB and the page size silently changes:\n  "
            + "\n  ".join(unsized),
        )

    @staticmethod
    def _keys_off_flash_size(soc):
        text = (HAL / soc / "Kconfig.mmu").read_text(errors="ignore")
        return "ESPTOOLPY_FLASHSIZE" in text


if __name__ == "__main__":
    unittest.main()
