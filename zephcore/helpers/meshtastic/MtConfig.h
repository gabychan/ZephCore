/*
 * SPDX-License-Identifier: MIT
 * The Meshtastic settings of a repeater and the "mt." CLI commands that read
 * and change them. Pure code, host tested; persistence is prefs.json's mt{}.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace mt {

/* Meshtastic's Config.DeviceConfig.Role, the value NodeInfo carries. */
enum Role : uint8_t {
	ROLE_CLIENT = 0,
	ROLE_ROUTER = 2,
	ROLE_REPEATER = 4,
	ROLE_ROUTER_LATE = 11,
};

/* What a repeater repeats. */
enum Relay : uint8_t {
	RELAY_OFF = 0,
	RELAY_ESSENTIAL = 1,  /* text, direct messages, ACKs, NodeInfo, traceroute */
	RELAY_ALL = 2,        /* the above, and the rest unless another node repeated it first */
};

static constexpr size_t kChannelNameMax = 11;
static constexpr size_t kPskMax = 16;

/* Plain data: NodePrefs holds one and is cleared with memset. setDefaults() fills it. */
struct RepeaterPrefs {
	uint8_t enable;
	uint8_t role;
	uint8_t hops;          /* hop_limit of the frames we originate, 0-7 */
	uint8_t relay;
	uint8_t relay_opaque;  /* also repeat channels we hold no key for */
	uint8_t position;      /* send our fixed position */
	uint8_t psk_len;       /* 1: the default key, whose byte is psk[0] */
	uint8_t psk[kPskMax];
	uint32_t nodeinfo_s;
	uint32_t telemetry_s;
	char channel[kChannelNameMax + 1];
};

/* hops is the one default that depends on the network joined (Kconfig). */
void setDefaults(RepeaterPrefs &p, uint8_t hops);

/* Puts every field back in range. */
void sanitize(RepeaterPrefs &p);

/* A router sends early and never yields to an echo; so does a late router,
 * but with the client's delay. */
inline bool roleSendsEarly(uint8_t role) { return role == ROLE_ROUTER; }
inline bool roleYieldsToEcho(uint8_t role) { return role != ROLE_ROUTER && role != ROLE_ROUTER_LATE; }

struct CmdResult {
	bool handled;  /* the command was an "mt." one */
	bool changed;  /* a setting changed: save it */
};

/* "get mt.<key>" and "set mt.<key> <value>". The reply is always written when
 * handled. */
CmdResult handleCommand(RepeaterPrefs &p, const char *command, char *reply, size_t cap);

/* Standard base64 into out; length, or -1 on bad input or overflow. */
int base64Decode(const char *s, uint8_t *out, size_t cap);

} /* namespace mt */
