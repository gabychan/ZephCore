/*
 * SPDX-License-Identifier: MIT
 * See MtConfig.h.
 */

#include "MtConfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace mt {

namespace {

struct Name {
	const char *name;
	uint8_t value;
};

const Name kRoles[] = {{"client", ROLE_CLIENT},
		       {"router", ROLE_ROUTER},
		       {"router_late", ROLE_ROUTER_LATE},
		       {"repeater", ROLE_REPEATER}};
const Name kRelays[] = {{"off", RELAY_OFF}, {"essential", RELAY_ESSENTIAL}, {"all", RELAY_ALL}};
const Name kBools[] = {{"off", 0}, {"on", 1}};

template <size_t N> const char *nameOf(const Name (&t)[N], uint8_t v)
{
	for (const Name &n : t) {
		if (n.value == v) {
			return n.name;
		}
	}
	return "?";
}

template <size_t N> bool valueOf(const Name (&t)[N], const char *s, uint8_t &out)
{
	for (const Name &n : t) {
		if (strcmp(n.name, s) == 0) {
			out = n.value;
			return true;
		}
	}
	return false;
}

template <size_t N> void choices(const Name (&t)[N], char *reply, size_t cap)
{
	size_t n = (size_t)snprintf(reply, cap, "Error: ");

	for (size_t i = 0; i < N && n < cap; i++) {
		n += (size_t)snprintf(reply + n, cap - n, "%s%s", i ? "|" : "", t[i].name);
	}
}

bool parseUint(const char *s, uint32_t max, uint32_t &out)
{
	char *end;
	unsigned long v = strtoul(s, &end, 10);

	if (end == s || *end != '\0' || v > max) {
		return false;
	}
	out = (uint32_t)v;
	return true;
}

} /* namespace */

int base64Decode(const char *s, uint8_t *out, size_t cap)
{
	uint32_t acc = 0;
	int bits = 0;
	size_t n = 0;

	for (; *s && *s != '='; s++) {
		char c = *s;
		int v = (c >= 'A' && c <= 'Z') ? c - 'A'
		      : (c >= 'a' && c <= 'z') ? c - 'a' + 26
		      : (c >= '0' && c <= '9') ? c - '0' + 52
		      : (c == '+' || c == '-') ? 62
		      : (c == '/' || c == '_') ? 63 : -1;

		if (v < 0) {
			return -1;
		}
		acc = (acc << 6) | (uint32_t)v;
		bits += 6;
		if (bits >= 8) {
			bits -= 8;
			if (n >= cap) {
				return -1;
			}
			out[n++] = (uint8_t)(acc >> bits);
		}
	}
	return (int)n;
}

static void setChannel(RepeaterPrefs &p, const char *args, char *reply, size_t cap, bool &changed)
{
	char name[kChannelNameMax + 2];
	char key[40] = "";

	int n = sscanf(args, "%12s %39s", name, key);

	if (n < 1 || !validName(name)) {
		snprintf(reply, cap, "Error: set mt.channel <name up to %u chars> [default|<base64 key>]",
			 (unsigned)kChannelNameMax);
		return;
	}
	uint8_t psk[kPskMax] = {1};
	int len = 1;

	if (n == 2 && strcmp(key, "default") != 0) {
		len = base64Decode(key, psk, sizeof(psk));
		if (len != 1 && len != 16) {
			snprintf(reply, cap, "Error: key is 1 or 16 bytes in base64");
			return;
		}
	}
	/* One name alone keeps the key. */
	changed = strcmp(p.channel, name) != 0;
	strcpy(p.channel, name);
	if (n == 2) {
		changed = changed || p.psk_len != len || memcmp(p.psk, psk, (size_t)len) != 0;
		p.psk_len = (uint8_t)len;
		memset(p.psk, 0, sizeof(p.psk));
		memcpy(p.psk, psk, (size_t)len);
	}
	snprintf(reply, cap, "OK");
}

CmdResult handleCommand(RepeaterPrefs &p, const char *command, char *reply, size_t cap,
			bool standalone)
{
	const bool get = strncmp(command, "get mt.", 7) == 0;
	const bool set = strncmp(command, "set mt.", 7) == 0;

	if (!get && !set) {
		return {false, false};
	}
	const char *key = command + 7;
	char name[24];
	size_t n = 0;

	while (key[n] && key[n] != ' ' && n < sizeof(name) - 1) {
		name[n] = key[n];
		n++;
	}
	name[n] = '\0';
	const char *val = key[n] == ' ' ? key + n + 1 : "";
	uint32_t u;
	uint8_t b;
	bool changed = false;

	if (!standalone && (strcmp(name, "channel") == 0 || strcmp(name, "telemetry") == 0)) {
		snprintf(reply, cap, "Error: not available here, the app sets the channels");
		return {true, false};
	}

	if (get) {
		if (strcmp(name, "enable") == 0) {
			snprintf(reply, cap, "> %s", nameOf(kBools, p.enable));
		} else if (strcmp(name, "role") == 0) {
			snprintf(reply, cap, "> %s", nameOf(kRoles, p.role));
		} else if (strcmp(name, "hops") == 0) {
			snprintf(reply, cap, "> %u", p.hops);
		} else if (strcmp(name, "relay") == 0) {
			snprintf(reply, cap, "> %s", nameOf(kRelays, p.relay));
		} else if (strcmp(name, "relay.opaque") == 0) {
			snprintf(reply, cap, "> %s", nameOf(kBools, p.relay_opaque));
		} else if (strcmp(name, "nodeinfo") == 0) {
			snprintf(reply, cap, "> %u", (unsigned)p.nodeinfo_s);
		} else if (strcmp(name, "telemetry") == 0) {
			snprintf(reply, cap, "> %u", (unsigned)p.telemetry_s);
		} else if (strcmp(name, "channel") == 0) {
			snprintf(reply, cap, "> %s (%s key)", p.channel,
				 p.psk_len == 1 && p.psk[0] == 1 ? "default" : "custom");
		} else {
			snprintf(reply, cap, "Error: unknown mt. setting");
		}
		return {true, false};
	}

	if (strcmp(name, "enable") == 0) {
		if (valueOf(kBools, val, b)) { changed = p.enable != b; p.enable = b; snprintf(reply, cap, "OK"); }
		else { choices(kBools, reply, cap); }
	} else if (strcmp(name, "role") == 0) {
		if (valueOf(kRoles, val, b)) { changed = p.role != b; p.role = b; snprintf(reply, cap, "OK"); }
		else { choices(kRoles, reply, cap); }
	} else if (strcmp(name, "relay") == 0) {
		if (valueOf(kRelays, val, b)) { changed = p.relay != b; p.relay = b; snprintf(reply, cap, "OK"); }
		else { choices(kRelays, reply, cap); }
	} else if (strcmp(name, "relay.opaque") == 0) {
		if (valueOf(kBools, val, b)) { changed = p.relay_opaque != b; p.relay_opaque = b; snprintf(reply, cap, "OK"); }
		else { choices(kBools, reply, cap); }
	} else if (strcmp(name, "hops") == 0) {
		if (parseUint(val, 7, u)) { changed = p.hops != u; p.hops = (uint8_t)u; snprintf(reply, cap, "OK"); }
		else { snprintf(reply, cap, "Error: 0-7"); }
	} else if (strcmp(name, "nodeinfo") == 0) {
		if (parseUint(val, 604800, u)) {
			u = clampInterval(u, 600, 604800);
			changed = p.nodeinfo_s != u; p.nodeinfo_s = u;
			snprintf(reply, cap, "OK - %u s", (unsigned)u);
		} else { snprintf(reply, cap, "Error: seconds, 0 (never) or 600-604800"); }
	} else if (strcmp(name, "telemetry") == 0) {
		if (parseUint(val, 86400, u)) {
			u = clampInterval(u, 60, 86400);
			changed = p.telemetry_s != u; p.telemetry_s = u;
			snprintf(reply, cap, "OK - %u s", (unsigned)u);
		} else { snprintf(reply, cap, "Error: seconds, 0 (never) or 60-86400"); }
	} else if (strcmp(name, "channel") == 0) {
		setChannel(p, val, reply, cap, changed);
	} else {
		snprintf(reply, cap, "Error: unknown mt. setting");
	}
	return {true, changed};
}

} /* namespace mt */
