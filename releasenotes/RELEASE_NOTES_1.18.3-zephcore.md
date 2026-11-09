# ZephCore 1.18.3-zephcore

Zephtacore, the build that also hears and repeats Meshtastic, repeats less by default and keeps its
default channels and node name in order.

---

## Meshtastic (Zephtacore builds)

- The repeater starts with `mt.relay essential`: text, direct messages, their ACKs, NodeInfo and
  traceroute are repeated, and nothing is dropped when another node repeats it first. `mt.relay all`
  still repeats the rest as before.
- The companion repeats nothing unless asked (`set mt.relay essential` or `all`), and when asked it
  never drops a repeat because another node sent it first.
- The default `MT:` channels and `#test` / `#bot` are added once per firmware revision. Deleting one in the
  app now sticks; before, it came back on the next restart.
- A Meshtastic node heard for the first time is added as a contact only as your auto-add settings
  allow; with manual adding it shows up as a discovered contact instead.
- Renaming the node changes the name in its Meshtastic NodeInfo too, without a restart.
- Settings of the Meshtastic side live in `prefs.json` only in builds that include it.

## Radio

- SX126x: changing the sync word between MeshCore and Meshtastic no longer waits for the radio's
  sleep phase when RX duty cycle is on.
