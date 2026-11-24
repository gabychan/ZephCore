# Security Policy

## Reporting a Vulnerability

Please report security issues privately. Do not open a public issue or discussion for them.

- GitHub: use [private vulnerability reporting](https://github.com/xejarque/ZephCore/security/advisories/new)
  on this repository.
- Email: xavier.ejarque@gmail.com

**Security contact:** xejarque (maintainer of this fork)

This is a fork of [liquidraver/ZephCore](https://github.com/liquidraver/ZephCore) that adds the Meshtastic bridge
(`docs/zephtacore/`). Report to the address above; if the problem is in code this fork did not touch, it can also go
to the upstream maintainer, whose policy is in that repository.

Include the firmware version or commit, the board and role (companion, repeater, room server, observer), and the
steps or packets needed to reproduce the issue.

## What to Expect

- Acknowledgement of your report, normally within a few days.
- A fix or mitigation is prepared privately, then released with a note in the release notes. Reporters are credited
  unless they ask not to be.
- Please allow time for a fix to reach deployed nodes before any public disclosure.

## Supported Versions

Security fixes go into the latest release and the `master` branch, which is where the releases of this fork are
built from. Older releases are not patched.

## Scope

In scope: the ZephCore firmware in this repository, including the mesh protocol implementation, companion
transports (BLE, USB, TCP/WiFi), the repeater CLI and OTA paths, and the storage and crypto code.

In scope as well, because only this fork has them: the Meshtastic bridge — the `z.mt.*` commands of ADR 0014, the
channel table of ADR 0015, the zero-hop and role handling of ADR 0016 and ADR 0019, the `mt` frame parsing and PKI
path in `zephcore/helpers/meshtastic/`, the contact creation rules in `MtPolicy`, the repeater's Meshtastic firewall
and its uplink, and the observer's MQTT publishing.

Out of scope: vulnerabilities in upstream Zephyr, its HALs or other west projects (report those to the respective
project), and weaknesses in the MeshCore protocol itself that apply equally to the upstream Arduino firmware
(report those to MeshCore). A weakness that only shows with Meshtastic nodes on the air is ours, not Meshtastic's,
when it comes from how this bridge handles the frame.
