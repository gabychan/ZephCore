# ZephCore 1.18.3-zephcore

Zephtacore, the build that also hears and repeats Meshtastic, repeats less by default and keeps its
default channels and node name in order.

---

## Zephtacore builds in the release

- The `t1000_e` and `wio_tracker_l1` companion and repeater are also published with the Meshtastic bridge
  (`boards/common/zephtacore.conf`), as the `zephtacore` variant of their `zephcore.yml`: files named
  `<board>-<role>-zephtacore-<hash>`, listed in the catalog with the subtitle Zephtacore. The Zephtacore web flasher
  downloads only these. See `docs/PROVIDER_CATALOG.md`.
- A push to a branch other than `master` still builds, but no longer replaces the release or `firmware-dist`.

---

## Reception statistics (repeater)

- `get z.mc.hopstats` and `get z.mt.hopstats` count the frames heard by hops crossed; `get z.mc.relays` and
  `get z.mt.relays` count them by the repeater that delivered them. They help to tune `z.mt.zerohop` and to find
  repeaters that `neighbors` does not list. See `docs/CLI_commands.md`.

---

## Receive limits (repeater)

- `set z.mc.fw.drop <rule> on` drops the flood frames of a payload type (`advert`, `request`, `text`, `group`, `ack`,
  `path`, `trace`). `set z.mc.fw.gap <minutes>` keeps one advert per key per window, and `set z.mc.fw.rate <frames>` caps the flood
  frames a minute accepted from one neighbouring repeater. Both are off by default; `get z.mc.fw.stats` counts what
  they dropped. The gap remembers 48 keys and the rate 16 repeaters; a full table forgets the one written longest ago. A
  1-byte path hash is counted by the rate too, and every repeater with that byte shares its count. The same commands exist for
  Meshtastic as `z.mt.fw.drop`, `.gap` and `.rate` (rules are ports; the gap is per NodeInfo), and a PKI direct text goes
  through them once opened. See `docs/CLI_commands.md`.

---

## Meshtastic (Zephtacore builds)

- Meshtastic channels on the companion are made by command, and the app no longer mixes channels that share a
  key (`Test` and `Bots`, or `SFNarrow`, `Catalunya` and `Barcelona`: a text sent to one showed in the others, and
  one app listed only one of them). `set z.mt.channel.add <name> [<key>]` takes the key as Meshtastic shows it
  (`AQ==`, `Ag==` or 16 bytes in base64), `set z.mt.channel.remove <name>` removes it and `get z.mt.channels` lists
  them. The app lists each as `#mt-<name>`. The default channels are created the same way. A node that held the old
  `MT:<name>` channels needs a factory reset; they are plain private channels now and carry no Meshtastic.
- A Meshtastic text now reaches the channel it was heard on, not the first channel with the same key.

- The repeater starts with `z.mt.relay essential`: text, direct messages, their ACKs, NodeInfo and
  traceroute are repeated. `z.mt.relay all`
  still repeats the rest as before. `essential` also repeats position, so nodes show on the map, and the frames of
  channels the node holds no key for (`z.mt.relay.opaque on`). A repeat is never cancelled when another node sends the frame first: it only waits longer.
  Before, `essential` repeated only the channels the node held a key for.
- The companion repeats nothing unless asked (`set z.mt.relay essential` or `all`), and when asked it
  never drops a repeat because another node sent it first.
- New Meshtastic roles. A repeater now announces `client_base` (a fixed node) and can be set to `client`, `client_base`,
  `router`, `router_late` or `repeater`. A companion announces `client_mute` and can be set to `client_mute` or `client`;
  as `client_mute` it repeats nothing, so a companion that should repeat needs `set z.mt.role client` as well as
  `z.mt.relay`. A node already set up keeps its stored role: on a repeater run `set z.mt.role client_base` to adopt
  the new one.
- New zero hop, off by default: `set z.mt.zerohop auto` repeats a frame without spending a hop when it came through a
  fixed neighbour (heard directly, announcing `router`, `router_late` or `client_base`), so a chain of fixed nodes costs
  one hop. `set z.mt.zerohop.add <node>` lists a neighbour by hand, `get z.mt.zerohop` shows the mode and the list,
  and `get z.mt.stats` counts the frames kept (`kept`).
- The default Meshtastic channels (`CONFIG_ZEPHTACORE_DEFAULT_CHANNELS`, as `#mt-<name>` slots) and `#test` / `#bot` are
  added once per firmware revision, which is kept in `prefs.json` as `mt.dr`. Deleting one in the app sticks.
- A direct message to a Meshtastic node that the app retries keeps the packet id of the first try, so the node's ACK still
  answers it. A PKI message that arrives before we know the sender's key is delivered when it is sent again, and a key that no
  longer opens a frame makes us ask for the node's NodeInfo.
- `get z.mt.stats` counts `long` apart: Meshtastic frames over 184 bytes, which are heard and delivered but not repeated.
- A Meshtastic node heard for the first time is added as a contact only as your auto-add settings
  allow; with manual adding it shows up as a discovered contact instead.
- Renaming the node changes the name in its Meshtastic NodeInfo too, without a restart.
- Settings of the Meshtastic side live in `prefs.json` only in builds that include it.
- New ping bot on the companion and the repeater: it answers a command of your choice on MeshCore `#bot` and on
  Meshtastic `Bots` with the hops, the SNR and the route (all of it in MeshCore, one hop per line with the RSSI/SNR after the last, the
  last relay in Meshtastic). Up to 6 pings a minute are answered per protocol, and the companion shows its answer in the app.
  Off until you pick the command: `set z.mc.ping.enable on /ping`, `set z.mt.ping.enable on /ping`; `get z.mc.ping.status`
  shows the counters.
- New `get z.mc.neighbors` and `get z.mt.neighbors` (alias `vecinos`; `recientes` for `recent`; the page is a plain number) on the companion and the repeater: the nodes heard directly,
  best SNR first, as `name (SNR, age)`, 4 to a page (`get z.mt.neighbors 2`), or the latest heard first with `recent` (`get z.mt.neighbors recent`). MeshCore lists repeaters heard with no hops, Meshtastic nodes heard with no relay
  in between. Kept in RAM, so a reboot empties them.
- New `get z.info` (version, uptime, battery, flash, heap and contacts in use) and `get z.stats` (MeshCore and Meshtastic counters), each in
  one line that fits a radio reply.
- New dashboard page on 200x200 e-paper companions (ThinkNode M1): MeshCore and Meshtastic side by side, with packet counts, nearest
  neighbours and their signal, radio setup, heap, flash and time on air. It is the first page, and redraws when opened, on a button press
  and once an hour.
- Commands of the Meshtastic side and the new ones hang from `z.`: `z.mt.*`, `z.mc.ping.*`, `z.mt.ping.*`, `z.ping.*`;
  `get tx.stalls` is now `get z.tx.stalls`.
- `help` at the end of a `z.` command (`get z.mt.relay help`) tells its usage, and on a group (`get z.mt help`) lists its keys;
  `set` of a key that takes text (a ping command, a channel) sets that text even when it is `help`.
  Every on/off `z.` setting (`z.mt.enable`, `z.mt.relay.opaque`, `z.ping.*`, `z.mc.ping.enable`, `z.mt.ping.enable`) also takes `1` and `0`.

## Radio

- SX126x: changing the sync word between MeshCore and Meshtastic no longer waits for the radio's
  sleep phase when RX duty cycle is on.
