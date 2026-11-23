# ADR 0019 — A packet held by a busy channel draws its wait again

Status: accepted (2026-11-23). Amends the dupe handling of [ADR 0016](0016-meshtastic-zero-hop-and-roles.md) for Meshtastic
frames. Not yet measured on the air.

## Context

The windows of MeshCore and Meshtastic spread the nodes that heard the same frame only while the channel is free. A packet
whose turn fell inside another node's frame went out as soon as that frame ended, and a dupe moved every due slot to the
same `now + delay`. Every node held by one frame then ran CAD at the same moment, found the channel free and transmitted.

Upstream MeshCore waits a random retry after a busy reading. Meshtastic restarts the delay of its queued frame after every
RX and TX (`RadioLibInterface::onNotify`, `setTransmitDelay`).

## Decision

- One rule for both protocols, in `Dispatcher::recontend()`: when the channel comes free, or after a frame heard or sent, a
  due packet draws its wait again. It does not transmit with the end of the frame.
- Each protocol keeps its own windows (`getContentionDelay`):
  - MeshCore: a relayed flood, or a bot's reply, from the adaptive flood window; our own flood and direct packets from the
    short direct window. A slot still ahead keeps its place and a dupe still pushes it back by the reactive backoff.
  - Meshtastic: a queued frame keeps its window and SNR (`mt::TxWindow`) and draws again after **every** frame, due or not,
    so routers keep going before clients. This replaces the reactive backoff for Meshtastic frames.
- `router_late`, on hearing a copy, moves past the worst case of the client window, as `clampToLateRebroadcastWindow` does.
- A bot's reply uses the window of a repeat, because several bots answer the same frame at the same moment.
- An echo still never cancels a repeat.

## Consequences

- The last repeat of a flood goes out later. A model of five nodes that all hear each other (SF7, 62.5 kHz, no capture)
  went from 58 % of frames collided to 6 % in MeshCore, 66 % to 8 % in Meshtastic and 85 % to 10 % with both, with the last
  repeat ending at about 2.7 s instead of 1.4 s in MeshCore.
- On a saturated channel a Meshtastic frame can keep redrawing, as it does in Meshtastic. At most 8 wait; past that a repeat
  is shed.
- Left out: the window of our own Meshtastic frames stays 2^3 (Meshtastic scales it with channel utilisation), `client_base`
  has no late clamp for favourites, and the slot time covers sub-GHz only.
