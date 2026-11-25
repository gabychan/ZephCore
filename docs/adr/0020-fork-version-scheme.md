# ADR 0020 — Fork version scheme (`<upstream-base>-ztc.<n>`)

Status: accepted (2026-11-25).

## Context

This fork (`Zephtacore`) is based on upstream ZephCore `1.18.2-zephcore` (`origin/master`), while upstream `origin/dev`
already uses `1.18.3-zephcore` for unrelated work. Leaving `ZEPHCORE_FIRMWARE_VERSION` at `1.18.2-zephcore` caused
devices to report the exact same string in `ver`, BLE DIS, `z.info`, and `PACKET_DEVICE_INFO` as upstream builds
without the Meshtastic bridge, and caused `.github/workflows/build.yml` to skip the fork's release notes (which had been
written under `RELEASE_NOTES_1.18.3-zephcore.md`). Bumping to `1.18.3-zephcore` would collide with upstream and misstate
the base version.

In addition, `PACKET_DEVICE_INFO` in `zephcore/app/CompanionMesh.cpp` copies `FIRMWARE_VERSION` into `static const
uint8_t version[20]`, bounding the version string to 19 characters plus the NUL terminator.

## Decision

- The fork uses `<upstream-base>-ztc.<n>`, starting at `1.18.2-ztc.1` (12 characters + NUL = 13 bytes, leaving 7 bytes of
  headroom in `version[20]`).
- `<upstream-base>` always matches the upstream ZephCore release version merged into the fork (e.g. `1.18.2`, then
  `1.18.3` once `origin/dev` / `1.18.3-zephcore` is merged); `<n>` starts at `1` and increments for each fork release on
  that base.
- `ZEPHCORE_FIRMWARE_VERSION` in `zephcore/CMakeLists.txt` remains the single source of truth, and release notes live at
  `releasenotes/RELEASE_NOTES_<version>.md`, which `.github/workflows/build.yml` reads verbatim.

## Consequences

- Devices cleanly distinguish fork releases from upstream ZephCore in `ver`, BLE Device Information Service, `get z.info`,
  and `PACKET_DEVICE_INFO`.
- In `custom_builder_web/js/flasher-logic.js`, `newestVersion()` (`localeCompare(..., { numeric: true })`) sorts
  `1.18.2-ztc.1` above `1.18.2-zephcore` (`'t' > 'e'`), and `1.18.2-ztc.10` above `1.18.2-ztc.9`.
- Release notes must live at `releasenotes/RELEASE_NOTES_<version>.md` whenever `ZEPHCORE_FIRMWARE_VERSION` is bumped.

## Alternatives considered

- **`1.18.3-zephcore`:** Rejected because the fork is still based on `1.18.2-zephcore` and upstream already claims
  `1.18.3-zephcore`.
- **`1.18.2-zephtacore.1`:** Rejected because it is 19 characters long — hitting the 20-byte `PACKET_DEVICE_INFO` wire
  ceiling on `.1`–`.9` and failing to compile at `.10`.
