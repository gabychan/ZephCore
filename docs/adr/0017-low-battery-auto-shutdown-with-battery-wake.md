# ADR 0017 — Low-battery auto-shutdown with a battery wake

Status: accepted (2026-11-19)

## Context

A node on a cell with no battery protection board is damaged if it discharges to the end. The companion has cut
itself off below a threshold since before this fork, and then stays off until the button. A solar repeater or room
server has no one at hand, so it needs to start again by itself when the battery recovers. The nRF52840 LPCOMP keeps
running through System OFF and its crossing event wakes the SoC.

## Decision

- One policy, `PowerPolicy`: three low readings 30 s apart, never on external power, never under 2000 mV. The
  companion runs it as before; a repeater or room server runs it only on a board with a battery wake
  (`CONFIG_ZEPHCORE_BATTERY_WAKE`: an LPCOMP node and a measured `battery-wake-mv` in `zephyr,user`).
- The threshold is `prefs.auto_shutdown_mv`: 0 (off) or 2900 mV up to a ceiling, 4200 mV or 300 mV under the
  board's `battery-wake-mv`. The CLI and `sanitizeNodePrefs()` share the range.
- Defaults: a companion boots with `CONFIG_ZEPHCORE_AUTO_SHUTDOWN_MILLIVOLTS` (3200 on nRF52, as before); a repeater
  or room server boots with it off, on every board. The owner turns it on with `set autoshutdown <mV>`.
- Two power-off entries: `zephcore_power_off()` suspends the comparator and stays off until the button;
  `zephcore_power_off_low_battery()` keeps it, so the node wakes when the battery rises past `battery-wake-mv`.
  Every new power-off caller picks one.
- A server stores the threshold in `prefs.json` as `zc.auto_off_mv`, an additive key: a file without it keeps the
  default, and a build that does not know it ignores it. The legacy binary prefs are not touched.
- If the comparator cannot be set up, or its input is already above the reference, the node logs it and still powers
  off: the cell comes first.
- The wake voltage is a fraction of VDD and varies between units (3.60 to 3.87 V on the ProMicro SX1262), so it is
  measured per board. A board with a power latch needs the battery divider outside the latched rail, or nothing
  stays up to sense the recovery.

## Consequences

- Only the ProMicro SX1262 and the GAT562 30S have a battery wake today. Other boards keep the old behaviour until a wake voltage is
  measured on each; they carry no code for it.
- A companion on the ProMicro SX1262 now also wakes by itself after a low-battery power-off.
- A stored companion threshold outside the range goes back to the default at boot.
- With the threshold at 0 a server takes no battery reading and adds no wake.
