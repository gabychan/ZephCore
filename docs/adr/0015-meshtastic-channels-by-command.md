# ADR 0015 — Meshtastic channels by command, shown to the app as hashtag channels

Status: proposed (2026-11-10)

## Context

A MeshCore group frame names its channel only by the first byte of the hash of the channel secret; a channel's name
never goes on the air. Two MeshCore channels with the same secret are therefore one channel, and upstream hands a
message to the first one that decrypts. The apps do the same: a message sent to `MT:Bots` shows in `MT:Test`, one
app lists only one of them, and reading one clears the unread count of the other.

A Meshtastic channel is a name and a key, and many share a key: the default one (`SFNarrow`, `Catalunya`, `Barcelona`)
and its variant 2 (`Test`, `Bots`). The bridge needs that exact key to decrypt, so the channels it carries cannot be
given distinct secrets in the app's channel slots, as they are now (`MT:<name>`, the real key as the secret).
Delivering to the right slot (`channel_idx`) is not enough: the frame is correct and the app merges the channels anyway.

## Decision

- A Meshtastic channel is created and removed by command, not from the app. The command takes the key as Meshtastic
  shows it: `AQ==` is the default key, `Ag==` its variant 2, 16 bytes in base64 a key of its own. No hexadecimal.
- The app sees a slot named `#mt-<name>`, in lower case with every character outside `a-z0-9` replaced by `-`. Its
  secret is the first 16 bytes of the SHA-256 of that name, as for any `#` channel, so each channel has its own.
- The real name (Meshtastic tells `Bots` from `bots`) and the key live in a table in `prefs.json` `mt{}`. The bridge
  decrypts with the key and delivers to the slot named `#mt-<name>`. The table creates the slots at the first boot of
  a defaults revision and on `add`, and `remove` deletes slot and entry. The app can delete a channel too: when it empties or
  rewrites a `#mt-` slot, the entry leaves the table. A `#mt-...` slot the app made, with no table entry, is an ordinary
  MeshCore hashtag channel and is not bridged.
- Commands, in the `z.` namespace (ADR 0014): `set z.mt.channel.add <name> [<key>]`, `set z.mt.channel.remove <name>`,
  `get z.mt.channels` (name, slot, key kind). `add` refuses a name whose slot name is already taken.
- Later, in a second step: `z.mc.channel.add #<name>` and `z.mc.channel.add <name> <key>` (base64 or 32 hex digits)
  with `remove` and `get z.mc.channels`, for MeshCore channels, and the same on the repeater. `add <name> <key>`
  refuses a key a slot already holds.
- The defaults (`SFNarrow`, `Test`, `Bots`, `Catalunya`, `Barcelona`, and the hashtag channels `#test`, `#bot`) are
  created by the same code at first boot. There is no migration: a node that held `MT:` slots needs a factory reset.

## Consequences

- Channels with a shared key no longer merge in the app. Any other MeshCore node that adds the same `#mt-<name>` gets
  the same secret, and its group messages land in that slot.
- Settings stay in `prefs.json` only; `PrefsCodec` does not change.
- The commands run from the USB text console and from `CMD_RUN_CLI_COMMAND`; the app has no screen for them. An app
  that normalises hashtag names differently from the rule above would write back another secret; to be checked
  against the apps before release.
- RAM: a table of `mt{}` records in `NodePrefs`, sized to the bridge's channel limit; measured against `5f25c11`.
- `MT:` slots stop meaning anything; the ping bot and the NodeInfo channel are found by the table, not by that prefix.
