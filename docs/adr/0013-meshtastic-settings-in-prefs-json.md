# ADR 0013 — Meshtastic settings in `prefs.json`

Status: accepted (2026-11-09)

## Context

The Meshtastic side of Zephtacore has settings of its own (`z.mt.enable`, `z.mt.role`, `z.mt.relay`, the repeater's
channel and key, the NodeInfo and telemetry intervals). The companion and the repeater share them, with the same
CLI. Settings persist in `prefs.json` only; the legacy binary files are frozen.

## Decision

- The settings are one `mt{}` object in `prefs.json`, for both roles (`en`, `role`, `hops`, `relay`, `opaque`,
  `ni_s`, `tel_s`, `ch`, `psk_len`, `psk`). A file without `mt{}` keeps the defaults; a value out of range is put
  back in range when the file is read.
- Defaults: `z.mt.role client`; `z.mt.relay` `essential` on the repeater and `off` on the companion. The companion
  never gives way to an echo, so what it is asked to repeat it repeats.
- `mt{}` and `NodePrefs::mt` exist only when `CONFIG_ZEPHTACORE_MESHTASTIC` is set. A build without it carries no
  field and writes no `mt{}`; a build with it reads a file written by one without it as "no `mt{}`".

## Consequences

- No change to `PrefsCodec` or to the legacy binary files.
- Going from a Zephtacore build to a plain one drops `mt{}` the next time the file is written.
- Cost of the field when enabled: 44 bytes of RAM in `NodePrefs`.
