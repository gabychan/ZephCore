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
  contact has no name or no key. No periodic requests, no command.
- NodeInfo sent on our own account (answering a `want_response`, answering a `PKI_UNKNOWN_PUBKEY` NAK, asking for
  the key of a node whose PKI message we could not open) is limited to once per node per hour by an 8-entry
  table in RAM, so a stranger cannot make us transmit more than the frames it sends itself.
- Our PKI direct messages ask for an ACK. The packet id is the ack code the app waits for; an ACK counts only from
  the node the message went to. No retransmission by the node; when the app retries a message (same timestamp,
  attempt above 0) it goes out again with the packet id of the first try, so the ACK still answers the code the
  app holds and a node that has the first copy drops the second.
- A PKI frame to us is checked for a duplicate before it is opened, and a frame that does not open is not
  remembered, so the copy that arrives after we learn the key is delivered. Once opened it goes through the
  firewall of the repeater like any other frame (`z.mt.fw.drop`, `.gap`, `.rate`).
- A key we hold that no longer opens a frame (the node regenerated it) is reported to the role, which asks for
  the node's NodeInfo under the hourly limit above.
- An AES block that fails stops the frame: nothing is transmitted in clear or with a zero keystream.

## Consequences

- A direct message to a node whose key is unknown fails with an error to the app instead of going out unreadable.
- A PKI message from a node whose key we lack is lost, but it triggers a request that normally gets the key.
- A node that regenerates its key is not followed until its contact is removed in the app.
- No ACK proof, no `PKI_UNKNOWN_PUBKEY` reply, no admin messages; our ACKs for incoming messages are sent as PKI.
- An ACK can be forged by a node that sees the packet id, as with Meshtastic without ACK proof.
- Cost, wio_tracker_l1 companion with Zephtacore: flash +3.9 KB, RAM +192 B.
