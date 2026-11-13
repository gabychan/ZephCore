# ADR 0016 — Zero hop between fixed Meshtastic neighbours, and the roles that go with it

Status: accepted (2026-11-13)

## Context

A Meshtastic frame dies after `hop_limit` repeats, 3 by default. A chain of fixed nodes on hills spends them all before the
frame reaches the valley behind the last one. Upstream Meshtastic keeps the hop when the node that repeated the frame is a
favourite router; meshtacore extends that with a list of relay bytes and with lists learned from the roles heard.

A ZephCore repeater has no node database and no favourites. It does have the table of Meshtastic nodes heard directly, and it
decodes the NodeInfo of the channels it holds a key for. It announced the role `client`, which no node treats as fixed, and a
companion announced `client` too although it repeats nothing by default.

## Decision

- `z.mt.zerohop auto` repeats a frame with the `hop_limit` it came with when the node that repeated it to us is fixed. Fixed is:
  heard directly in the last 72 h, the only neighbour with that relay byte, and announcing `router`, `router_late` or
  `client_base`; or on a list of up to 8 node numbers set by hand. The first hop always counts.
- A neighbour announcing `client` is never taken as fixed automatically: the hop count is what bounds a flood through nodes
  that move. It can be listed by hand.
- Our own role does not gate it. It applies wherever `z.mt.relay` is on.
- Default `off` in both firmwares: it changes what other nodes see on the air and has not run on a mesh yet.
- New roles `client_base` (12) and `client_mute` (1). A repeater defaults to `client_base` and may be `client`, `client_base`,
  `router`, `router_late` or `repeater`. A companion defaults to `client_mute` and may be `client_mute` or `client`.
  `client_mute` repeats nothing whatever `z.mt.relay` says. `client_base` has the timing of `client`: there are no favourites
  to treat as upstream does.

## Consequences

- Two ZephCore repeaters in sight of each other keep hops for each other once both run `z.mt.zerohop auto` with the default role,
  and upstream or meshtacore nodes can treat them as fixed.
- A stored role is kept: a repeater set up before this release still announces `client` until `set z.mt.role client_base`; a
  companion that stored a role it can no longer choose keeps it until it is set again.
- The hop count other nodes show for a frame is lower than the repeats it took.
- Only the last byte of the node number travels in a frame. A node that only repeats, shares the byte of a fixed neighbour and
  was never heard directly gets its hop kept too; the cost is one hop, not a loop: the `(from, id)` table stops a frame
  that comes back.
- A fixed neighbour whose NodeInfo we cannot read is not learned; it goes on the list.
- Left out: per-port switches (position, telemetry) as meshtacore has; a frame of a channel without a key shows no port.
