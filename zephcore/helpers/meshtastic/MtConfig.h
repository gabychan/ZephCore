/*
 * SPDX-License-Identifier: MIT
 * The Meshtastic settings of a repeater and the "z.mt." CLI commands that read
 * and change them. Pure code, host tested; persistence is prefs.json's mt{}.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

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

/* The channel both roles start on: Meshtastic's default key under this name. */
#define MT_DEFAULT_CHANNEL "SFNarrow"

static constexpr size_t kChannelNameMax = 11;
static constexpr size_t kPskMax = 16;

/* Plain data: NodePrefs holds one and is cleared with memset. setDefaults() fills it. */
struct RepeaterPrefs {
	uint8_t enable;
	uint8_t role;
	uint8_t hops;          /* hop_limit of the frames we originate, 0-7 */
	uint8_t relay;
	uint8_t relay_opaque;  /* also repeat channels we hold no key for */
	uint8_t psk_len;       /* 1: the default key, whose byte is psk[0] */
	uint8_t psk[kPskMax];
	uint32_t nodeinfo_s;
	uint32_t telemetry_s;
	char channel[kChannelNameMax + 1];
};

/* 0 (off) or at least min, at most max. */
inline uint32_t clampInterval(uint32_t v, uint32_t min, uint32_t max)
{
	return v == 0 ? 0 : (v < min ? min : (v > max ? max : v));
}

inline bool validName(const char *s)
{
	size_t n = strlen(s);

	if (n == 0 || n > kChannelNameMax) {
		return false;
	}
	for (size_t i = 0; i < n; i++) {
		if ((uint8_t)s[i] < 0x20 || (uint8_t)s[i] > 0x7E) {
			return false;
		}
	}
	return true;
}

inline bool validRole(uint8_t r)
{
	return r == ROLE_CLIENT || r == ROLE_ROUTER || r == ROLE_REPEATER || r == ROLE_ROUTER_LATE;
}

/* Puts every field back in range. */
inline void sanitize(RepeaterPrefs &p)
{
	p.enable = p.enable > 1 ? 1 : p.enable;
	if (!validRole(p.role)) {
		p.role = ROLE_CLIENT;
	}
	p.hops = p.hops > 7 ? 7 : p.hops;
	p.relay = p.relay > RELAY_ALL ? RELAY_ALL : p.relay;
	p.relay_opaque = p.relay_opaque > 1 ? 1 : p.relay_opaque;
	if (p.psk_len != 1 && p.psk_len != kPskMax) {
		p.psk_len = 1;
		p.psk[0] = 1;
	}
	p.nodeinfo_s = clampInterval(p.nodeinfo_s, 600, 604800);
	p.telemetry_s = clampInterval(p.telemetry_s, 60, 86400);
	p.channel[kChannelNameMax] = '\0';
	if (!validName(p.channel)) {
		const char kDefault[] = MT_DEFAULT_CHANNEL;

		for (size_t i = 0; i < sizeof(kDefault); i++) {
			p.channel[i] = kDefault[i];
		}
	}
}

/* hops depends on the network joined (Kconfig); a companion starts with relay off, a repeater on. */
inline void setDefaults(RepeaterPrefs &p, uint8_t hops, uint8_t relay)
{
	for (size_t i = 0; i < sizeof(p); i++) {
		((uint8_t *)&p)[i] = 0;
	}
	p.enable = 1;
	p.role = ROLE_CLIENT;
	p.hops = hops;
	p.relay = relay;
	p.relay_opaque = 1;
	p.psk_len = 1;
	p.psk[0] = 1;
	p.nodeinfo_s = 43200;
	sanitize(p);
}

/* Our Meshtastic node number: the last four bytes of the MeshCore public key. */
inline uint32_t nodeFromPublicKey(const uint8_t pub[32])
{
	return ((uint32_t)pub[28] << 24) | ((uint32_t)pub[29] << 16) | ((uint32_t)pub[30] << 8) |
	       (uint32_t)pub[31];
}

/* Four letters or digits of the node name, upper case. */
inline void shortName(const char *name, char out[5])
{
	int n = 0;

	for (const char *c = name; *c && n < 4; c++) {
		if ((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9')) {
			out[n++] = (*c >= 'a' && *c <= 'z') ? (char)(*c - 'a' + 'A') : *c;
		}
	}
	if (n == 0) {
		out[n++] = 'Z';
	}
	out[n] = '\0';
}

/* A router sends early and never yields to an echo; so does a late router,
 * but with the client's delay. */
inline bool roleSendsEarly(uint8_t role) { return role == ROLE_ROUTER; }
inline bool roleYieldsToEcho(uint8_t role) { return role != ROLE_ROUTER && role != ROLE_ROUTER_LATE; }

struct CmdResult {
	bool handled;  /* the command was an "mt." one */
	bool changed;  /* a setting changed: save it */
};

/* "get z.mt.<key>" and "set z.mt.<key> <value>". The reply is always written when
 * handled. standalone: the node has one channel of its own (a repeater) and
 * sends telemetry; a companion's channels are the app's. */
CmdResult handleCommand(RepeaterPrefs &p, const char *command, char *reply, size_t cap,
			bool standalone);

/* Standard base64 into out; length, or -1 on bad input or overflow. */
int base64Decode(const char *s, uint8_t *out, size_t cap);

} /* namespace mt */
