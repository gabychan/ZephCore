# ADR 0012 — Meshtastic PKI direct messages in Zephtacore

Status: accepted (2026-11-09)

## Context

Meshtastic 2.5+ nodes only accept a direct message encrypted for them (X25519 key agreement, SHA256 of the shared
secret as AES-256 key, AES-CCM with an 8 byte tag, channel byte 0). Zephtacore sent direct messages with the
channel PSK, which those nodes ignore, and dropped the PKI messages they sent to us. Nothing held a Meshtastic
node's public key or ours.

## Decision

- Our X25519 key is `SHA256("zt-mtpki" || first 32 bytes of the MeshCore private key)`: no new secret to store,
  and it survives a reflash like the identity. It is published in our NodeInfo (`User.public_key`).
- A node's public key is kept in its own file, `mtkeys`, 36 bytes per node (node number and key), in the
  directory of `contacts3`. Appended when first learned, never rewritten for a change (a different key is
  refused, as Meshtastic does); a rewrite happens only when the file is full (records without a contact are
  dropped) or when the app removes the contact. Nothing is cached in RAM: the file is read when a PKI message is
  sent or received. `contacts3` keeps name, position and node number, so no data is stored twice.
- The key is not put in `contacts3` (`out_path` is exported to the app and overwritten by it) nor in
  `prefs.json` (settings only).
- AES-CCM is built on the PSA AES-256 block (CCM is not enabled in PSA), with the block function and SHA256
  injected so `MtPki` is pure code, tested on the host against a packet from Meshtastic's own test suite.
- A NodeInfo request (`want_response`) goes out once, when the app marks a Meshtastic contact as favourite and the
  contact has no name or no key. No automatic requests, no command.

## Consequences

- A direct message to a node whose key is unknown fails with an error to the app instead of going out unreadable.
- A node that regenerates its key is not followed until its contact is removed in the app.
- No ACK proof, no `PKI_UNKNOWN_PUBKEY` reply, no admin messages; our ACKs for incoming messages are sent as PKI.
- Cost, wio_tracker_l1 companion with Zephtacore: flash +3.2 KB, RAM +64 B.
