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
	bool free_text;    /* "set" takes any text, so "set <key> help" sets it */
};

inline constexpr Help kHelp[] = {
#ifdef CONFIG_ZEPHTACORE_MESHTASTIC
	{"mt.enable", "get|set on|off|1|0: Meshtastic side on or off"},
	{"mt.role", "get|set client|client_base|router|router_late|repeater (companion: client_mute|client): role in our NodeInfo"},
	{"mt.relay", "get|set off|essential|all: what is repeated"},
	{"mt.relay.opaque", "get|set on|off|1|0: also repeat channels we hold no key for"},
	{"mt.hops", "get|set 0-7: hop_limit of the frames we send"},
	{"mt.zerohop", "get|set off|auto: keep hop_limit of what a fixed neighbour repeated; set .add|.remove <node>, .clear: nodes listed by hand"},
	{"mt.nodeinfo", "get|set <seconds>: NodeInfo interval, 0 never or 600-604800"},
	{"mt.telemetry", "get|set <seconds>: device metrics interval, 0 never or 60-86400 (repeater)"},
	{"mt.channel", "get|set <name> [default|<base64 key>]: primary channel (repeater)", true},
	{"mt.channel.add", "set <name> [default|<base64 key>]: add a channel (companion)", true},
	{"mt.channel.remove", "set <name>: remove a channel (companion)", true},
	{"mt.channels", "get: the channels and their keys (companion)"},
	{"mt.stats", "get: heard, relayed, opaque, dropped, shed, long, kept"},
	{"mt.neighbors", "get [page] [recent]: Meshtastic nodes heard directly, best SNR first"},
	{"mt.ping.enable", "get|set on [<command>]|off|1 [<command>]|0: bot on (needs a command) or off"},
	{"mt.ping.trigger", "get|set <command>: text to answer, 1-15 characters, no spaces", true},
	{"mt.ping.status", "get: state, command, answered and ignored"},
	{"mt.hopstats", "get: frames heard by hops crossed, ? when the sender set no hop_start (repeater)"},
	{"mt.relays", "get: relay byte of the frames heard, most frames first (repeater)"},
	{"mt.fw.drop", "get|set <rule> on|off|1|0 or none: ports dropped: text position nodeinfo telemetry traceroute other (repeater)"},
	{"mt.fw.gap", "get|set 0-255: minutes between two NodeInfo of one node that are kept, 0 no limit (repeater)"},
	{"mt.fw.rate", "get|set 0-255: frames a minute from one neighbouring repeater, 0 no limit (repeater)"},
	{"mt.fw.stats", "get: frames dropped by each limit since boot (repeater)"},
#endif
	{"mc.neighbors", "get [page] [recent]: MeshCore repeaters heard directly, best SNR first"},
	{"mc.ping.enable", "get|set on [<command>]|off|1 [<command>]|0: bot on (needs a command) or off"},
	{"mc.ping.trigger", "get|set <command>: text to answer, 1-15 characters, no spaces", true},
	{"mc.ping.status", "get: state, command, answered and ignored"},
	{"mc.hopstats", "get: flood frames heard by hops crossed, 7+ at the end (repeater)"},
	{"mc.relays", "get: last-hop repeater of the flood frames heard, most frames first (repeater)"},
	{"mc.fw.drop", "get|set <rule> on|off|1|0 or none: payload types dropped: advert request text group ack path trace (repeater)"},
	{"mc.fw.gap", "get|set 0-255: minutes between two adverts of one key that are kept, 0 no limit (repeater)"},
	{"mc.fw.rate", "get|set 0-255: flood frames a minute from one neighbouring repeater, 0 no limit (repeater)"},
	{"mc.fw.stats", "get: frames dropped by each limit since boot (repeater)"},
	{"ping.snr", "get|set on|off|1|0: show the SNR in the answer"},
	{"ping.rssi", "get|set on|off|1|0: show the RSSI in the answer"},
	{"ping.ignore.direct", "get|set on|off|1|0: do not answer pings heard with 0 hops"},
	{"ping.ignore.remote", "get|set on|off|1|0: do not answer pings heard with 1 or more hops"},
	{"autoshutdown", "get|set <mV>: battery voltage the node powers off under, 0 off; it wakes itself when the battery recovers"},
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
 * ("get z.mt help"). False, with reply untouched, when the command is not that,
 * so it goes on to its own handler. "set" leaves a key that takes any text alone:
 * "help" is a valid value for it. */
inline bool helpReply(const char *command, bool meshtastic, char *reply, size_t cap)
{
	const bool get = strncmp(command, "get z.", 6) == 0;

	if (!get && strncmp(command, "set z.", 6) != 0) {
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
			if (!get && h.free_text) {
				return false;
			}
			snprintf(reply, cap, "> z.%s: %s", h.key, h.text);
			return true;
		}
	}

	bool any = false;

	for (const Help &h : kHelp) {
		any = any || (helpListed(h, meshtastic) && strncmp(h.key, key, n) == 0 && h.key[n] == '.');
	}
	if (!any) {
		return false;
	}
	size_t used = (size_t)snprintf(reply, cap, "> z.%.*s:", (int)n, key);

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
	}
	return true;
}

} /* namespace zcmd */
