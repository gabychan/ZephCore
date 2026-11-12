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

## Low-battery power-off that wakes again (ProMicro SX1262)

- On the ProMicro SX1262 a node that powers off on a low battery now wakes by itself once the battery recovers to
  about 3.6 to 3.9 V, so a solar repeater no longer needs a manual restart. For repeaters and room servers it is off
  until you set it: `set z.autoshutdown <mV>` (0 = off, at most 3300 mV there; three readings 30 s apart, never on
  external power), and it survives a reboot. Without a battery protection board, set it. The companion, which already
  powered off below 3200 mV, now wakes the same way.
- A `poweroff` or a power-off from the UI stays off until the button, also when the battery crosses that voltage.
- `set autoshutdown` now takes 0 or 2900 to 4200 mV (lower on a board with a battery wake): before, a value outside
  that range was answered OK and then lost at the next reboot. See `docs/CLI_commands.md`.

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
- `help` lists the groups of the `z.` commands. The repeater and room server had no `help` before. `help` at the end of a `z.` command (`get z.mt.relay help`) tells its usage, and on a group (`get z.mt help`) lists its children, with a `/` after any that has more under it;
  `set` of a key that takes text (a ping command, a channel) sets that text even when it is `help`.
  Every on/off `z.` setting (`z.mt.enable`, `z.mt.relay.opaque`, `z.ping.*`, `z.mc.ping.enable`, `z.mt.ping.enable`) also takes `1` and `0`.

## Fewer collisions when the channel is busy (MeshCore and Meshtastic)

- A packet whose turn comes while another node is transmitting no longer goes out the moment that frame ends, when every
  other waiting node would do the same. It draws its wait again, in the window of its protocol and role. Repeats can take
  somewhat longer to finish in a busy area. Not measured on the air yet.
- A packet queued later no longer delays the send of one queued earlier, and none is left waiting for an unrelated wake.
- Meshtastic: a frame waiting to be repeated draws its wait again after every frame, as Meshtastic does, so routers stay
  ahead of clients. `router_late` goes after the client window once it hears another node repeat the frame.
- The ping bots answer in the window of a repeat, so several bots answering one message no longer step on each other.

## Radio

- SX126x: changing the sync word between MeshCore and Meshtastic no longer waits for the radio's
  sleep phase when RX duty cycle is on.
- LR1110 and LR2021 boards (the T1000-E among them) with Zephtacore: nearby MeshCore nodes are heard again.
  The hybrid receive word was `0x1B`, which made the LR1110 drop every strong MeshCore frame, so close nodes only
  arrived repeated and never showed as neighbours. It is now `0x22`, which favours MeshCore: on these chips only weak
  Meshtastic frames are received.

## Adaptive CAD no longer mistakes undecodable packets for false alarms

The node tunes its listen-before-talk sensitivity by testing it in quiet moments: if the test says
"busy" and no packet follows, it counts a false alarm, and enough false alarms make it less
sensitive. A packet the node can hear but not decode, too weak, or with a damaged header, also
counted as a false alarm, which nudged the node toward ignoring real traffic. Now the radio notes
whether a packet start (preamble) or header showed up after the test; if so it counts as real
traffic. `get cad.stats` shows these as a new `u` column next to `t`.

## LilyGo T-Impulse Plus charges its battery

The T-Impulse Plus has a charger chip that starts with charging switched off and has to be set up
by the firmware. ZephCore never did, so the battery only charged after LilyGo's own firmware had
run, and stopped again once the battery ran flat. On USB the chip also restarted the whole
wristband every few minutes, which showed up as dropped Bluetooth connections.

- **The charger is now set up at every start**, the way LilyGo's firmware does it, so the battery
  charges on USB without flashing anything else first.
- **The restarts on USB stop.** ZephCore keeps the charger's watchdog switched off.
- **The GPS works.** ZephCore switched the GPS off when it meant to switch it on, so the
  wristband never found its GPS and the app showed GPS as not supported.

## Other changes

- **A node no longer falls silent after 25 days.** A node whose transmits had all gone out at the first try for
  24.9 days stopped transmitting for up to another 24.9 days, and stopped receiving too once its queue had
  filled. Only a reboot brought it back sooner. Repeaters on a quiet channel and companions that stay on for
  weeks were the ones at risk.
- **Idle nodes sleep longer.** Once a node had heard a packet repeated and then fell quiet, it woke every
  5 seconds for as long as it ran. It now returns to waking only for its regular channel measurement (15 s).
- **Companion: the app's airtime factor is limited to 0–9**, as `set af` is. A larger value used to apply until
  the next restart and then become 9 without notice; from 200 up it switched the duty-cycle limit off. The app
  now gets an error and the setting stays as it was.
- **Duty-cycle budget refills exactly.** After about five days without a transmit (at 10 %) the budget could
  come back almost empty and delay the next transmit, and rounding lost up to 5 % of the allowance while a
  packet was waiting.
- **`tempradio` takes at most 35000 minutes** (about 24 days). A longer time was accepted but never reverted.
- **Heltec receive workaround stays enabled.** Boards using the Heltec `0x08B5` workaround now apply it while
  the radio is awake during configuration and retain it across warm sleep, including the AGC reset path.
  Previously it was written after receive started and was lost at the radio's first sleep.
- **KISS modem: the encrypt command no longer overwrites memory.** A host that sent the
  `EncryptData` command with a plaintext longer than 192 bytes wrote past a buffer in the modem's
  memory. Plaintexts over 184 bytes, the most a packet can carry, are now refused with
  `EncryptFailed`. Only the computer on the modem's serial port could send it.
- **KISS modem: protocol version 2.** A host can now ask which radio controls the modem has
  (`GetCapabilities`), switch the radio's boosted receive gain (`SetRxBoostedGain`) and, on boards
  with a switchable LNA in the front end, turn that LNA on or off (`SetFemState`). These follow
  MeshCore's KISS protocol 2. ZephCore does not take MeshCore's timed AGC reset: the modem keeps
  resetting a stuck receiver only after a long silence, as every ZephCore role does, so the AGC
  interval commands answer `Unsupported`. None of these settings is saved across a restart.
- **SenseCAP Solar: dimmer heartbeat LED.** The white LED's heartbeat blink can now be dimmed with
  `set leds.brightness <0-100>`, and a newly set-up node starts at 10%, as in MeshCore. A node you
  already use keeps its current brightness (100%); `set leds.brightness 10` gives the new default.
- **SX1262 and LR2021 boards at SF8 with 15.6 kHz bandwidth can talk to the rest of the mesh.** On this one
  setting the two radio types left low-data-rate optimisation off, while LR1110 boards and MeshCore nodes
  have it on, so packets between them failed. Other settings were not affected.
- **SX1262 boards report signal-to-noise in quarter-dB steps.** They used to round it down to a whole dB, so
  SNR values read up to 0.75 dB low. Neighbour lists, statistics and the app now show the finer value.
- **A bandwidth that is not a LoRa bandwidth is refused.** `set radio`, `tempradio` and the app accepted any
  number from 7 to 500 kHz. A value such as 42 (for 41.7) or 100 was saved and shown, but the radio ran at
  125 kHz and the node dropped off the mesh. Now only 7.8, 10.4, 15.6, 20.8, 31.25, 41.7, 62.5, 125, 250 and
  500 kHz are accepted, and only those the board's radio has (LR1110 from 10.4, LR2021 from 31.25 plus 203, 406, 812 and 1000, SX127x from 62.5). A node that
  has such a value saved starts on the default radio settings after the update.
- **LR1110 and LR2021 boards: receive duty cycle keeps its safety margin on fast presets.** The sleep time was rounded up
  to a whole millisecond, which on presets with short symbols used up the margin against missing a packet.
- **LR1110 and LR2021 boards never transmit when the channel check itself fails.** If the listen-before-talk check did
  not complete, the node used to transmit anyway. It now restores reception and tries again later.
- **LR1110 and LR2021 boards report signal-to-noise in quarter-dB steps**, like SX1262 boards.
- **GPS commands follow MeshCore 1.18's.** `gps` now reads like `> on, waiting, 7 sats 3m ago, periodic (300s)`
  (`> off, continuous` when off), and replies are `OK` or `Error, ...` as in MeshCore. New: `gps fix` (the last
  position and its age), `gps once` (one fix now; a GPS that was off goes back off afterwards), `gps period <time>`
  (MeshCore's name for `set gps duty`; takes `30m`, `48h`, `2d`, and also turns the GPS on) and `gps fix_stats`
  (how long fixes take). A companion contact you gave remote CLI rights can now run these `gps` commands and
  `get/set adc.multiplier` over the mesh, as on MeshCore. Scripts that read the old `gps` line (`on, active, fix, 7 sats`)
  or the lowercase `ok` need updating.
- **`get advert.loc` / `set advert.loc`.** MeshCore's new name for the location-in-adverts setting
  (`none`, `share` or `prefs`) works on every node. `gps advert` still works and changes the same setting.
- **GPS anti-spoofing guard, as a build option.** Builds with `CONFIG_ZEPHCORE_GPS_GUARD` (off in the
  release builds, as MeshCore's `GPS_GUARD`) check each GPS fix before using it: enough satellites and precision,
  no jumps faster than 250 km/h, and on repeaters and room servers no further than `gps fence` metres (1000 by
  default) from the configured location. GPS time sets the clock only from a trusted fix and, once the clock is
  right, only by small steps; a GPS that keeps disagreeing raises an alert that `gps guard accept` resolves. New
  commands: `gps guard`, `gps guard on|off|accept`, `gps fence`, `gps ref`.
- **E-paper boards (T-Echo, ThinkNode M1): the display controller sleeps while the screen is off.** The image
  stays; the controller wakes on the next screen update. Saves a little standby current, as MeshCore now does.
- **RAK3401 firmware on a RAK4631 core with a RAK13302: the core's own radio is put to sleep.** It used to idle
  powered for as long as the board ran.
- **RAK4631 and RAK3401 1W draw less when switched off.** `poweroff` and the low-battery shutdown left the
  WisBlock slot power on, so a GPS module, slot sensors and display (and on the RAK3401 1W the amplifier's 5 V
  supply) kept running while the node was off. They are now switched off too. MeshCore measured a RAK4631 with
  GPS going from about 30 mA to about 1.3 mA when off.