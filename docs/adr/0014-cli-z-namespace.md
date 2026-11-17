# ADR 0014 — Zephtacore CLI under `z.`

Status: accepted (2026-11-10)

## Context

Zephtacore added commands to a CLI that upstream MeshCore owns: `mt.*` (the Meshtastic side), `get tx.stalls`, and then the
ping bot. Names with no prefix can collide with a later upstream command, and nothing told an operator which commands are
ours. Some are common to both protocols (`tx.stalls`, the ping SNR and RSSI switches) and some belong to one (`mt.*`, the
ping enable and command).

## Decision

- Everything this fork adds hangs from `z.`. What belongs to a protocol goes under `z.mt.` or `z.mc.`; what is common to both
  goes directly under `z.`: `z.mt.*`, `z.mc.ping.*`, `z.mt.ping.*`, `z.ping.*`, `z.tx.stalls`.
- The old names (`mt.*`, `tx.stalls`, `ping.*`) are removed, with no alias. They existed only on the fork's branch and in no
  release, and the settings in `prefs.json` keep their keys (`mt{}`), so nothing stored changes.
- Upstream commands and the ones ZephCore already carried (`cad.*`, `dc.restarts`, ...) keep their names.

## Consequences

- Scripts written for the old names get an unknown-command reply.
- One namespace to grep and to document; a new Zephtacore command has an obvious home.
