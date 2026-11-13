/*
 * SPDX-License-Identifier: MIT
 * What the repeater heard, per protocol: how many hops each frame had crossed and
 * which neighbouring repeater delivered it. RAM only, read with
 * "get z.mc|mt.hopstats" and "get z.mc|mt.relays". Pure code, host tested.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace rxstats {

static constexpr size_t kBuckets = 8;     /* hops 0 to 6, and 7 or more */
static constexpr size_t kRelaySlots = 16;
static constexpr size_t kRelayMax = 3;    /* bytes of a relay name: a MeshCore path hash is up to 3 */
static constexpr int kUnknownHops = -1;   /* a Meshtastic frame whose sender set no hop_start */

class Observer {
	struct Relay {
		uint8_t id[kRelayMax];
		uint8_t size;
		uint32_t count;
	};
	uint32_t _hops[kBuckets] = {};
	uint32_t _unknown = 0;
	Relay _relays[kRelaySlots] = {};

public:
	/* One frame heard: the hops it had crossed (kUnknownHops when not known) and the
	 * node that sent it to us (size 0 when not known). A full table drops the relay
	 * that carried least. */
	void heard(int hops, const uint8_t *relay, size_t size)
	{
		if (hops < 0) {
			_unknown++;
		} else {
			_hops[(size_t)hops < kBuckets ? (size_t)hops : kBuckets - 1]++;
		}
		if (size == 0 || size > kRelayMax) {
			return;
		}
		Relay *slot = nullptr;

		for (Relay &r : _relays) {
			if (r.size == size && memcmp(r.id, relay, size) == 0) {
				slot = &r;
				break;
			}
		}
		if (!slot) {
			slot = &_relays[0];
			for (Relay &r : _relays) {
				if (r.size == 0) {
					slot = &r;
					break;
				}
				if (r.count < slot->count) {
					slot = &r;
				}
			}
			slot->size = (uint8_t)size;
			memcpy(slot->id, relay, size);
			slot->count = 0;
		}
		slot->count++;
	}

	void clear() { *this = Observer(); }

	/* "> 0:12 1:5 7+:1 ?:2", only the buckets with frames; "> none". */
	void formatHops(char *out, size_t cap) const
	{
		size_t used = (size_t)snprintf(out, cap, ">");
		bool any = false;

		for (size_t i = 0; i < kBuckets && used < cap; i++) {
			if (_hops[i]) {
				used += (size_t)snprintf(out + used, cap - used, " %u%s:%u", (unsigned)i, i == kBuckets - 1 ? "+" : "",
							 (unsigned)_hops[i]);
				any = true;
			}
		}
		if (_unknown && used < cap) {
			used += (size_t)snprintf(out + used, cap - used, " ?:%u", (unsigned)_unknown);
			any = true;
		}
		if (!any) {
			snprintf(out, cap, "> none");
		}
	}

	/* "> 3f:120 a1b2:40", most frames first, as many as fit; "> none". */
	void formatRelays(char *out, size_t cap) const
	{
		bool taken[kRelaySlots] = {};
		size_t used = (size_t)snprintf(out, cap, ">");
		bool any = false;

		for (size_t n = 0; n < kRelaySlots; n++) {
			size_t best = kRelaySlots;

			for (size_t i = 0; i < kRelaySlots; i++) {
				if (!taken[i] && _relays[i].size && (best == kRelaySlots || _relays[i].count > _relays[best].count)) {
					best = i;
				}
			}
			if (best == kRelaySlots) {
				break;
			}
			taken[best] = true;
			char line[kRelayMax * 2 + 14];
			int len = 0;
			const Relay &r = _relays[best];

			line[len++] = ' ';
			for (size_t b = 0; b < r.size; b++) {
				len += snprintf(line + len, sizeof(line) - (size_t)len, "%02x", r.id[b]);
			}
			len += snprintf(line + len, sizeof(line) - (size_t)len, ":%u", (unsigned)r.count);
			if (used + (size_t)len >= cap) {
				break;
			}
			memcpy(out + used, line, (size_t)len + 1);
			used += (size_t)len;
			any = true;
		}
		if (!any) {
			snprintf(out, cap, "> none");
		}
	}
};

/* "get|set z.mc.hopstats", "z.mt.hopstats", "z.mc.relays", "z.mt.relays". */
inline bool isCommand(const char *command)
{
	if (strncmp(command, "get z.m", 7) != 0 && strncmp(command, "set z.m", 7) != 0) {
		return false;
	}
	const char *rest = command + 7;

	return (rest[0] == 'c' || rest[0] == 't') && rest[1] == '.' &&
	       (strcmp(rest + 2, "hopstats") == 0 || strcmp(rest + 2, "relays") == 0);
}

/* True for the Meshtastic one. */
inline bool isMeshtastic(const char *command) { return command[7] == 't'; }

inline void handleCommand(const Observer &o, const char *command, char *reply, size_t cap)
{
	if (command[0] != 'g') {
		snprintf(reply, cap, "Error: read only");
	} else if (command[9] == 'h') {
		o.formatHops(reply, cap);
	} else {
		o.formatRelays(reply, cap);
	}
}

} /* namespace rxstats */
