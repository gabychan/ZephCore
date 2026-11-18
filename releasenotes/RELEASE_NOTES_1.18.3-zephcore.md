# ZephCore 1.18.3-zephcore

Zephtacore, the build that also hears and repeats Meshtastic, repeats less by default and keeps its
default channels and node name in order.

---

## Zephtacore builds in the release

- Every board of the web builder that CI publishes, 37 of them, carries the companion and the repeater also with the
  Meshtastic bridge (`boards/common/zephtacore.conf`), as the `zephtacore` variant of its `zephcore.yml`: files named
  `<board>-<role>-zephtacore-<hash>`, listed in the catalog with the subtitle Zephtacore. The Zephtacore web flasher
  downloads only these. `lilygo_tlora_c6`, `xiao_esp32c6` and `lora_e5_mini` have none: their companion does not fit
  in RAM (the C6 boards) or flash (`lora_e5_mini`). See `docs/PROVIDER_CATALOG.md`.
- On a WiFi companion with Zephtacore the system heap is 3 KB smaller (`wifi_companion_zephtacore.conf`), which is what
  lets the Heltec V3 and the Wireless Tracker build it.
- The web flasher writes nRF52 boards over USB, with the Adafruit bootloader's serial DFU and the `.zip` package of the
  catalog, instead of a UF2 copied by hand (the UF2 copy stays for browsers without Web Serial). The board reboots into
  the bootloader and the browser asks for its port once per step.
- The web flasher shows one flow: the board and role, whether to keep the data or do a clean install, and the install
  button, always visible. The settings to send, the console and the backup restore sit apart below it.
- On ESP32 a clean install erases the whole flash while writing. On nRF52 it loads the SoftDevice's formatter package first, which erases the file system and returns the
  board to the bootloader, and then the firmware. The formatter is the one the catalog names for the board. A clean
  install gives the node a new identity.
- The companion of a board whose USB-C is a USB-UART bridge (Heltec V3, ThinkNode M9) now serves the app and the
  text CLI over that UART, as the boards with native USB already do, so the Builder can send its settings by USB and
  observers and bots can use the port. On the Heltec V3 the system heap is 68 KB, 63 KB with
  Zephtacore (`wifi_companion_uart.conf`, `wifi_companion_zephtacore_uart.conf`).
- A push to a branch other than `master` still builds, but no longer replaces the release or `firmware-dist`.
- The catalog gives the SHA-256 of every firmware file, and the web flasher refuses a download that does not match
  it before writing anything.

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
- Meshtastic frames up to 255 bytes are repeated: the bytes past the 184th of a queued frame wait in a tail slot (`CONFIG_ZEPHTACORE_FOREIGN_TAIL_SLOTS`, 4 slots of 71 bytes by default). `get z.mt.stats` counts `long` the frames a build without slots cannot repeat, apart from `shed`.
- A Meshtastic node heard for the first time is added as a contact only as your auto-add settings
  allow; with manual adding it shows up as a discovered contact instead.
- A Meshtastic node becomes a contact when its NodeInfo carries a name and a public key, or when it sends us a direct
  text we can open (named `!<node number> (MT)` until its NodeInfo arrives). A Position from an unknown node no longer
  takes a contact slot, and a node whose firmware predates Meshtastic 2.5 (no key) stays out of the contact list and
  the map.
- Renaming the node changes the name in its Meshtastic NodeInfo too, without a restart.
- The contact key of a Meshtastic node is now derived with a secret of the companion, so nobody else can sign MeshCore
  adverts or messages as that contact. Meshtastic contacts saved by an earlier build are no longer recognised: delete
  them in the app; they come back with the node's next NodeInfo or position, and its stored PKI key is kept.
- With `z.mt.enable off` the companion sends nothing as Meshtastic: a message the app sends on a `#mt-` channel or to
  a Meshtastic contact is refused at once, instead of going out on a receiver that could not hear the answer.
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
  one line that fits a radio reply. The heap figures come from the kernel's heap counters, which every build now enables
  (`CONFIG_SYS_HEAP_RUNTIME_STATS`).
- With the Meshtastic bridge a new device starts at coding rate 4/6, the SFNarrow preset (4/5 without it). A device that
  already has settings keeps them.
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
- LR1110 and LR2021 boards (the T1000-E among them) with Zephtacore: nearby MeshCore nodes are heard again.
  The hybrid receive word was `0x1B`, which made the LR1110 drop every strong MeshCore frame, so close nodes only
  arrived repeated and never showed as neighbours. It is now `0x22`, which favours MeshCore: on these chips only weak
  Meshtastic frames are received.
