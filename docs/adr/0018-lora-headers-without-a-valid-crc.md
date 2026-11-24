# ADR 0018 — A LoRa header whose CRC fails is not received

Status: accepted (2026-11-24, recorded from behaviour shipped with the SX126x, LR1110 and LR2021 drivers). Not
measured on the air.

## Context

A LoRa reception reports two independent verdicts: whether the header arrived and its CRC matched, and whether the
payload arrived and its CRC matched. The radio can hand over a packet whose payload CRC passes while the header CRC
failed, and some reference drivers accept it. The header is what carries the payload length and the coding rate, so a
header that does not verify means the payload was decoded on assumptions the air never confirmed: a wrong length can
truncate a good frame or pad a short one, and a wrong coding rate is what a corrupt header usually shows as.

`MeshCore`'s Arduino firmware accepts such a frame. Meshtastic's RadioLib path rejects it. A wrong-length packet
reaches the mesh as a duplicate of nothing, is forwarded once and pollutes every neighbour's duplicate table.

## Decision

- The three drivers of `zephcore/patches/zephyr-new/drivers/lora/` treat a failed header CRC as a rejected reception:
  the frame is not delivered to `lora_recv` and no `RX_DONE` is raised for it. The sites are `sx126x.c` (the
  `SX126X_IRQ_HEADER_ERR` arm of the DIO1 handler), `lr11xx_lora.c` and `lr20xx_lora.c` (the same arm of their IRQ
  mask).
- A header that cannot be read is treated as rejected too, not as "nothing happened": the frame is not delivered.
  The drivers differ in how they express that (a `return` per driver), and none of them retries the read.
- Nothing else changes: a payload whose CRC fails was already rejected, and MeshCore's byte compatibility on the air
  is unaffected because a node that rejects these frames only stops relaying a corrupt one.

## Consequences

- `SX127x` and `loramac-node` boards keep their chip's own behaviour: the header check is not applied there, and this
  ADR does not claim it is. It is a gap per radio, not a per-board decision: the check belongs to the driver that
  owns the IRQ mask, and the SX127x path has no equivalent hook.
- On a weak link the node now loses a frame it could have decoded by luck. That is the intended trade: a frame with a
  bad header is not the frame that was sent.
- The behaviour is pinned by the driver sources only; no host test can reach them. A board build is the check that it
  compiles, and air time on a weak channel is where it would show.

## Alternatives considered

- Accept the frame when the payload CRC passes (upstream MeshCore's behaviour). Rejected: the payload CRC covers the
  payload as decoded, not the length or coding rate the corrupt header claimed, so it can pass on a frame that is not
  what the sender transmitted.
- Reject only when the length field is implausible. Rejected: it needs a plausibility rule per protocol and would
  still accept a wrong coding rate.
