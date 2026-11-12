/*
 * SPDX-License-Identifier: MIT
 * What every "z." command shares: the on/off value and "<get|set> z.<key> help".
 * Pure code, host tested.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace zcmd {

/* on / 1 -> 1, off / 0 -> 0. */
inline bool parseOnOff(const char *v, uint8_t &b)
{
	if (strcmp(v, "on") == 0 || strcmp(v, "1") == 0) { b = 1; return true; }
	if (strcmp(v, "off") == 0 || strcmp(v, "0") == 0) { b = 0; return true; }
	return false;
}

struct Help {
	const char *key;   /* after "z." */
	const char *text;
};

static const Help kHelp[] = {
	{"mt.enable", "get|set on|off|1|0: Meshtastic side on or off"},
	{"mt.role", "get|set client|router|router_late|repeater: role in our NodeInfo and its timing"},
	{"mt.relay", "get|set off|essential|all: what is repeated"},
	{"mt.relay.opaque", "get|set on|off|1|0: also repeat channels we hold no key for"},
	{"mt.hops", "get|set 0-7: hop_limit of the frames we send"},
	{"mt.nodeinfo", "get|set <seconds>: NodeInfo interval, 0 never or 600-604800"},
	{"mt.telemetry", "get|set <seconds>: device metrics interval, 0 never or 60-86400 (repeater)"},
	{"mt.channel", "get|set <name> [default|<base64 key>]: primary channel (repeater)"},
	{"mt.channel.add", "set <name> [default|<base64 key>]: add a channel (companion)"},
	{"mt.channel.remove", "set <name>: remove a channel (companion)"},
	{"mt.channels", "get: the channels and their keys (companion)"},
	{"mt.stats", "get: heard, relayed, opaque, yielded, dropped, shed"},
	{"mt.neighbors", "get [page] [recent]: Meshtastic nodes heard directly, best SNR first"},
	{"mt.ping.enable", "get|set on [<command>]|off|1 [<command>]|0: bot on (needs a command) or off"},
	{"mt.ping.trigger", "get|set <command>: text to answer, 1-15 characters, no spaces"},
	{"mt.ping.status", "get: state, command, answered and ignored"},
	{"mc.neighbors", "get [page] [recent]: MeshCore repeaters heard directly, best SNR first"},
	{"mc.ping.enable", "get|set on [<command>]|off|1 [<command>]|0: bot on (needs a command) or off"},
	{"mc.ping.trigger", "get|set <command>: text to answer, 1-15 characters, no spaces"},
	{"mc.ping.status", "get: state, command, answered and ignored"},
	{"ping.snr", "get|set on|off|1|0: show the SNR in the answer"},
	{"ping.rssi", "get|set on|off|1|0: show the RSSI in the answer"},
	{"ping.ignore.direct", "get|set on|off|1|0: do not answer pings heard with 0 hops"},
	{"ping.ignore.remote", "get|set on|off|1|0: do not answer pings heard with 1 or more hops"},
	{"info", "get: version, uptime, battery, flash, heap and contacts"},
	{"stats", "get: MeshCore counters, then the Meshtastic ones"},
	{"tx.stalls", "get: times TX could not go out for 4 s, per cause"},
};

/* The Meshtastic keys exist only in builds that have it. */
inline bool helpListed(const Help &h, bool meshtastic)
{
	return meshtastic || strncmp(h.key, "mt.", 3) != 0;
}

/* "<get|set> z.<key> help": the usage of a key, or the keys under a group
 * ("get z.mt help"). False when the command is not that, so it goes on to its
 * own handler. */
inline bool helpReply(const char *command, bool meshtastic, char *reply, size_t cap)
{
	if (strncmp(command, "get z.", 6) != 0 && strncmp(command, "set z.", 6) != 0) {
		return false;
	}
	const char *key = command + 6;
	size_t n = strlen(key);

	while (n > 0 && key[n - 1] == ' ') {
		n--;
	}
	if (n < 5 || strncmp(key + n - 4, "help", 4) != 0 || key[n - 5] != ' ') {
		return false;
	}
	n -= 5;
	while (n > 0 && key[n - 1] == ' ') {
		n--;
	}

	for (const Help &h : kHelp) {
		if (helpListed(h, meshtastic) && strlen(h.key) == n && strncmp(h.key, key, n) == 0) {
			snprintf(reply, cap, "> z.%s: %s", h.key, h.text);
			return true;
		}
	}

	size_t used = (size_t)snprintf(reply, cap, "> z.%.*s:", (int)n, key);
	bool any = false;

	for (const Help &h : kHelp) {
		if (!helpListed(h, meshtastic) || strncmp(h.key, key, n) != 0 || h.key[n] != '.') {
			continue;
		}
		const char *name = h.key + n + 1;
		const size_t len = strlen(name) + 1;

		if (used + len >= cap) {
			break;
		}
		used += (size_t)snprintf(reply + used, cap - used, " %s", name);
		any = true;
	}
	return any;
}

} /* namespace zcmd */
