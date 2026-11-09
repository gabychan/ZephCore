# Zephtacore

ZephCore with a Meshtastic bridge: one SX126x hears and speaks MeshCore and Meshtastic. The MeshCore app
is the client. Status: **MVP, not yet verified on air**. See [Verification](#verification).

## What works

| Feature | How |
| --- | --- |
| Meshtastic channels in the MeshCore app | A group channel named `MT:<name>` with the Meshtastic PSK as its 16 byte secret. |
| Receive text | Text heard on an `MT:` channel reaches the app as a channel message, `<sender>: <text>`. |
| Send text | A message the app sends on an `MT:` channel leaves as a Meshtastic broadcast. |
| Repeats heard | When a Meshtastic node repeats a text we sent, the app gets the same message as a MeshCore group packet, which it counts as a heard repeat. The packet carries the repeater's `relay_node` byte as a 1-byte path, so each repeater is one heard (up to 8 per text). Incoming texts carry their real hop count (`hop_start - hop_limit`). The last 3 sent texts are kept for this. |
| Replies | A reply written in the app (`@[name] text`) leaves as a Meshtastic text with `reply_id` set to the newest text by that name, tag dropped. A Meshtastic reply or reaction reaches the app as `@[name] text`, `name` being the author of the answered text. Authors are known for the last 8 texts on `MT:` channels (ours included); older targets lose the tag on the way in and leave the reply unlinked on the way out. |
| Direct messages | A message to a Meshtastic contact leaves as a Meshtastic PKI message (X25519 with the node's key, AES-256-CCM, channel 0), as Meshtastic 2.5+ nodes expect. It needs the node's public key, learned from its NodeInfo; without it the app gets an error. A PKI message addressed to us reaches the app as a message of that contact, and is answered with an ACK when it asks for one. Our messages ask for an ACK: when the node answers (by PKI or on a shared channel, from that node only), the app gets the confirmation, and times out after 20 s otherwise. |
| Node keys | The public key of a node is kept in the `mtkeys` file (36 bytes per node, next to `contacts3`: `/ext` when there is external flash, `/lfs` otherwise), written once per node. A node that later sends another key keeps the first, as Meshtastic does; removing the contact in the app drops its key. Our own key is derived from the node identity and goes out in our NodeInfo. |
| Exchanging keys | Marking a Meshtastic contact as favourite, when it still has no name or no key, sends it our NodeInfo with `want_response`, once. We also send our NodeInfo, at most once per node per hour: when a node asks for ours (a unicast NodeInfo with `want_response`), when a node answers `PKI_UNKNOWN_PUBKEY`, and (asking for theirs too) when a PKI message for us arrives from a node whose key we do not hold. |
| Nodes in the contact list | NodeInfo creates a contact `<long name> (MT)`. |
| Nodes on the map | Position packets set the contact's coordinates. |
| Our name on Meshtastic | A NodeInfo is broadcast 30 s after boot, then every 12 h, on the first `MT:` channel. |
| Repeat Meshtastic floods | Optional, `CONFIG_ZEPHTACORE_MT_RELAY`: repeats frames of our `MT:` channels with `hop_limit` lowered. |

MeshCore behaves as before. A frame that does not decrypt on one of our `MT:` channels is handled as MeshCore.

## Build

Companion role, SX126x, LR1110 and LR2021 boards. The feature is `CONFIG_ZEPHTACORE_MESHTASTIC`, set by a config fragment:

```bash
west build -b wio_tracker_l1 zephcore --pristine -- -DEXTRA_CONF_FILE="boards/common/zephtacore.conf"
```

Add `CONFIG_ZEPHTACORE_MT_RELAY=y` to the fragment to repeat Meshtastic floods. `CONFIG_ZEPHTACORE_MT_HOP_LIMIT`
(default 7) is the hop limit of the frames we originate; Meshtastic's own default is 3.

SX127x boards build, but cannot pick the sync word per frame, so Meshtastic is neither heard nor sent there.
On LR1110 and LR2021 the sync word is the 8-bit LoRa word: MeshCore `0x12`, Meshtastic `0x2B`, hybrid receive `0x1B`
(high nibble of one, low nibble of the other, as in meshtacore). The driver applies it at the next RX or TX start.
The LR2021 side detectors (other SFs) use the same word as the main detector.

## Defaults

With `CONFIG_ZEPHTACORE_MESHTASTIC` a fresh device starts on meshtacore's `custom_sfnarrow_default` preset:
869.618 MHz, BW 62.5 kHz, SF 7, CR 4/6, 22 dBm (CR 4/5 without the feature; the rest is ZephCore's own default).
On the first boot of a firmware with a new defaults revision (`CompanionMesh::MT_DEFAULTS_REV`, kept in the
data store under `zt_ch_rev`) it adds the channels of meshtacore's community matrix that are missing, by name:

| Channel | Key |
| --- | --- |
| `Public` | MeshCore public key (as in ZephCore) |
| `MT:SFNarrow`, `MT:Catalunya`, `MT:Barcelona` | Meshtastic default key (PSK `AQ==`) |
| `MT:Test`, `MT:Bots` | default key + 1 (PSK `Ag==`) |
| `#test`, `#bot` | MeshCore hashtag channels (first 16 bytes of SHA256 of the name) |

meshtacore's `MC:Public`, `MC:#test` and `MC:#bot` are bridged names there; here those are native MeshCore
channels. A channel you delete does not come back on later boots or on a reflash of the same revision; it comes back
only when a firmware raises the revision. Existing channels and radio settings are never changed.

## Use

1. Check the node's radio (frequency, bandwidth, SF, CR): it must have the **same values the Meshtastic network uses**.
   The sync word is the only thing that separates the two networks; everything else must match.
   For SFNarrow (as in meshtacore's `custom_sfnarrow_default`): 869.618 MHz, BW 62.5 kHz, SF 7, CR 4/6.
   Any SF works: Meshtastic frames carry 16 preamble symbols, so with the bridge on, the RX duty-cycle window is
   sized for 16 (at SF 8 and below that leaves no sleep budget, so the radio listens continuously).
2. In the MeshCore app add a channel. Name: `MT:SFNarrow` (the part after `MT:` is the Meshtastic channel
   name; the channel hash, 0x20, comes from name and key). Secret, for the default key (PSK `AQ==`):
   `d4f1bb3a20290759f0bcffabcf4e6901`. A channel with PSK `Ag==` (default key index 2, e.g. `Test`) has the same
   key with the last byte `02`: `d4f1bb3a20290759f0bcffabcf4e6902`. For a private channel, the 16 byte key in
   hex. 32 byte (AES-256) keys are not supported.
3. Send and receive on that channel. Meshtastic nodes appear as contacts after they send NodeInfo or Position.

## Limits

- Channels use the AES-128 channel cipher only: no AEAD channels, no AES-256 channel keys. Direct messages use PKI.
- A PKI message from a node whose key we do not hold is not opened (it is lost; our NodeInfo request gets the key for the next one). No `PKI_UNKNOWN_PUBKEY` reply, no ACK proof, no admin messages. An ACK is trusted by packet id and sender, without Meshtastic's ACK proof.
- PKI is used for text only (not NodeInfo, Position or routing, as in Meshtastic), except our ACKs, which are PKI too.
- A frame we send or repeat is at most 184 bytes (MeshCore's payload limit), about 164 characters of text.
- Our position is not broadcast.
- Text sent from the device UI (joystick) still goes out as MeshCore; only app messages are bridged.
- A repeated frame waits as a Meshtastic client does: 16 slots left to routers, then a random slot in a window of
  2^3 to 2^8 slots chosen by the SNR it was heard at (fainter: smaller window). A duplicate heard meanwhile pushes it
  back by the reactive backoff (`backoff.multiplier` x airtime, capped as for MeshCore floods) and never cancels it.
  Why: [CAD.md](CAD.md).
- Frames we send to Meshtastic carry Meshtastic's 16-symbol preamble whatever the SF (SX126x, LR11xx, LR20xx).
- The contact list holds Meshtastic nodes as ordinary contacts, so they count against `CONFIG_ZEPHCORE_MAX_CONTACTS`.

## How it works

```
radio RX (hybrid sync 0x14B4) -> Dispatcher::checkRecv
    -> CompanionMesh::onRecvForeign -> mt::Bridge::handleFrame
          decrypts on our MT: channels (hash, AES-CTR, protobuf Data)
          yes: text / NodeInfo / Position -> app, optional relay, frame consumed
          no : MeshCore parser, as before
          channel 0 to us: PKI, opened with the sender's key (mtkeys) -> direct text to the app, ACK if asked
app message on MT: channel -> mt::Bridge::buildText -> Dispatcher::sendForeign
app message to a Meshtastic contact -> mt::Bridge::buildDirectText (PKI) -> same TX queue
    -> normal TX queue (LBT, duty cycle) -> LoRaRadio writes sync 0x24B4 for the frame
```

| Piece | File |
| --- | --- |
| Frame header, AES-CTR, protobuf Data/Position/User | `zephcore/helpers/meshtastic/MtWire.*` |
| Channels, dedupe, delivery, relay, frame builders | `zephcore/helpers/meshtastic/MtBridge.*` |
| PKI: X25519, shared key, AES-CCM seal/open | `zephcore/helpers/meshtastic/MtPki.*` |
| Companion glue (channels, contacts, keys, app commands) | `zephcore/app/CompanionMeshtastic.cpp` |
| Node key file | `ZephyrDataStore::getMtKey/putMtKey/removeMtKey` (`zephcore/adapters/datastore`) |
| RX hook and foreign TX frames | `Dispatcher::onRecvForeign`, `Dispatcher::sendForeign`, `Packet::isForeign` |
| Sync word per frame | `LoRaRadio::applySyncWord`, `sx126x_set_sync_word_reg` in `patches/zephyr/0003-lora-sx126x-native.patch` |

SX126x sync word registers: Meshtastic `0x2B` is `0x24B4`, MeshCore `0x12` is `0x1424`, hybrid receive `0x14B4`
(first nibble from one, second from the other). The value is taken from Meshtacore, which measured it on a
ThinkNode M1; it is a bench value, not a datasheet guarantee.

A Meshtastic node's contact key is the Ed25519 public key of a keypair seeded from `SHA256("zt-mtnode" || node)`.
The node number is kept in the contact's `sync_since` field, and a contact counts as Meshtastic only if the key
derived from it matches.

## Repeater

The repeater role builds with the same `CONFIG_ZEPHTACORE_MESHTASTIC` (`boards/common/repeater.conf;boards/common/zephtacore.conf`).
It holds one channel, the primary (default `LongFast`), because Meshtastic sends NodeInfo, position and telemetry on it.
Settings live under `mt.` in the CLI and in `prefs.json` `mt{}` (see `docs/CLI_commands.md`).

| Piece | Where |
| --- | --- |
| Repeater glue (RX hook, relay queue, NodeInfo, `mt.` commands) | `zephcore/app/RepeaterMeshtastic.cpp` |
| Settings, command parser, defaults | `zephcore/helpers/meshtastic/MtConfig.h/.cpp` |
| What gives way to an echo | `mt::relayClassOf` in `MtPolicy.h` |
| MeshCore or Meshtastic when no key opens a frame | `mt::looksLikeMeshCore` in `MtDiscriminate.h` |
| Shared foreign-frame relay | `Mesh::relayForeign`, `foreignDupeHeard`, `cancelForeignRelay` |

The receiver is hybrid and does not report the sync word a frame came in on. A frame that none of our channels opens is told
from MeshCore by its shape: it must parse as MeshCore, its payload must be whole 16-byte AES blocks after the fixed part,
and its Meshtastic `to`/`from` must not be our node number (the check meshtacore uses). A real MeshCore frame always passes;
about one Meshtastic frame in fifty does by chance and is handled as MeshCore, as it was before this feature.

Timing follows Meshtastic's `getTxDelayMsecWeighted`: `client` and `repeater` wait 16 slots plus a random slot in a window set
by the SNR; `router` waits a random slot of the first 2 x cw, before any client. Text, direct messages, ACKs, NodeInfo and
traceroute are never dropped when another node repeats them first; the rest is (Meshtastic's client behaviour), unless the
role is `router` or `router_late`.

Next hop (Meshtastic 2.6+): a frame is repeated when it names no next hop or names this node (the last byte of its node
number), has hops left, and is neither ours nor for us alone. The repeat clears `next_hop` (this node keeps no routes, so it
floods on) and sets `relay_node`. A frame first heard while it named another node is repeated if it comes again as a flood
fallback, and the originator sending a frame again (`hop_start == hop_limit`, nobody ACKed) is repeated again, once per retry.
A repeat heard from another repeater is not dropped if that repeater named this node.

Our NodeInfo, and our fixed position and device metrics when enabled, go out on the primary channel only.

Size: the repeater grows by 12.2 KB of flash and 1.2 KB of RAM (`wio_tracker_l1`, 289392 vs 277168 B and 85148 vs 83932 B);
the companion by 0.8 KB of flash and 0.3 KB of RAM (settings in `NodePrefs`, the dedup table's relayed flag).
Not run on air.

## Verification

| Check | Result |
| --- | --- |
| Host tests (`python3 tests/run.py run --profile quick`) | 144 pass: AES against FIPS-197, channel hash (LongFast = 0x08, SFNarrow = 0x20), CTR keystream layout, protobuf golden bytes, bridge round trips, relay, rebroadcast timing, relay hash, false-positive bound, Dispatcher RX hook and foreign TX |
| Field numbers and nonce layout | Read against `mesh.proto` and `CryptoEngine.cpp` of the Meshtastic firmware |
| Firmware build `wio_tracker_l1` (SX1262) | OK. Flash +4.6 KB (476724 vs 472084 B), RAM +1.3 KB (227168 vs 225824 B) |
| Firmware build `t1000_e` (LR1110) | OK. Flash 377388 B |
| Firmware build `promicro_lr2021` (LR2021) | OK. Flash 383532 B |
| Firmware build `wio_tracker_l1` repeater, feature off | OK |
| SX126x patch | Applies to the pinned Zephyr (`0534e997e`) |
| **On air** | **Not done**, on any radio family. No frame has been sent to or received from a Meshtastic node, and the hybrid sync word has not been measured for packet loss on this code |

First on-air checks, in order: (1) a Meshtastic node on the same frequency, bandwidth, SF and CR sends text on
`SFNarrow`: it must show in the app; (2) the app sends on `MT:SFNarrow`: the Meshtastic node must show it;
(3) MeshCore traffic still arrives with the hybrid register on; (4) NodeInfo and Position make contacts and map
pins; (5) loss of MeshCore and Meshtastic frames against a build without the feature.
