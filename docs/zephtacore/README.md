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
| Direct messages | A message to a Meshtastic contact leaves as a channel-encrypted text addressed to that node, on the first `MT:` channel. No ACK is tracked. |
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

- Channel encryption only: no Meshtastic PKI direct messages, no AEAD channels, no AES-256 keys.
- A frame we send or repeat is at most 184 bytes (MeshCore's payload limit), about 164 characters of text.
- Our position is not broadcast.
- Text sent from the device UI (joystick) still goes out as MeshCore; only app messages are bridged.
- A repeated frame is sent after a random 50-400 ms delay; a duplicate heard meanwhile does not cancel it.
- The contact list holds Meshtastic nodes as ordinary contacts, so they count against `CONFIG_ZEPHCORE_MAX_CONTACTS`.

## How it works

```
radio RX (hybrid sync 0x14B4) -> Dispatcher::checkRecv
    -> CompanionMesh::onRecvForeign -> mt::Bridge::handleFrame
          decrypts on our MT: channels (hash, AES-CTR, protobuf Data)
          yes: text / NodeInfo / Position -> app, optional relay, frame consumed
          no : MeshCore parser, as before
app message on MT: channel -> mt::Bridge::buildText -> Dispatcher::sendForeign
    -> normal TX queue (LBT, duty cycle) -> LoRaRadio writes sync 0x24B4 for the frame
```

| Piece | File |
| --- | --- |
| Frame header, AES-CTR, protobuf Data/Position/User | `zephcore/helpers/meshtastic/MtWire.*` |
| Channels, dedupe, delivery, relay, frame builders | `zephcore/helpers/meshtastic/MtBridge.*` |
| Companion glue (channels, contacts, app commands) | `zephcore/app/CompanionMeshtastic.cpp` |
| RX hook and foreign TX frames | `Dispatcher::onRecvForeign`, `Dispatcher::sendForeign`, `Packet::isForeign` |
| Sync word per frame | `LoRaRadio::applySyncWord`, `sx126x_set_sync_word_reg` in `patches/zephyr/0003-lora-sx126x-native.patch` |

SX126x sync word registers: Meshtastic `0x2B` is `0x24B4`, MeshCore `0x12` is `0x1424`, hybrid receive `0x14B4`
(first nibble from one, second from the other). The value is taken from Meshtacore, which measured it on a
ThinkNode M1; it is a bench value, not a datasheet guarantee.

A Meshtastic node's contact key is the Ed25519 public key of a keypair seeded from `SHA256("zt-mtnode" || node)`.
The node number is kept in the contact's `sync_since` field, and a contact counts as Meshtastic only if the key
derived from it matches.

## Verification

| Check | Result |
| --- | --- |
| Host tests (`python3 tests/run.py run --profile quick`) | 142 pass: AES against FIPS-197, channel hash (LongFast = 0x08, SFNarrow = 0x20), CTR keystream layout, protobuf golden bytes, bridge round trips, relay, false-positive bound, Dispatcher RX hook and foreign TX |
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
