/*
 * SPDX-License-Identifier: MIT
 * MeshCore receive limits of the repeater: one advert per key per window, and
 * a ceiling of flood frames a minute from one neighbouring repeater. A frame
 * over a limit is not repeated and not processed. Pure code, host tested;
 * persistence is prefs.json's zc{}.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace fw {

static constexpr size_t kAdvertSlots = 16;  /* keys remembered */
static constexpr size_t kHopSlots = 8;      /* neighbouring repeaters counted */
static constexpr uint32_t kRateWindowMs = 60000;
static constexpr size_t kHopMax = 3;        /* bytes of a path hash */

/* Plain data: NodePrefs holds one and is cleared with memset. 0 = no limit. */
struct Prefs {
	uint8_t advert_gap_min;  /* minutes between two adverts of one key that are kept */
	uint8_t hop_rate;        /* flood frames a minute accepted from one last-hop repeater */
};

/* Since boot; not persisted. */
struct Counters {
	uint32_t gap;   /* adverts dropped inside the window */
	uint32_t rate;  /* frames dropped over the ceiling */
};

inline bool within(uint32_t now, uint32_t since, uint32_t span)
{
	return (uint32_t)(now - since) < span;
}

/* RAM only. */
class Firewall {
	struct AdvertSeen {
		uint32_t key;
		uint32_t at_ms;
		bool used;
	};
	struct HopRate {
		uint8_t prefix[kHopMax];
		uint8_t size;
		uint8_t count;
		uint32_t window_ms;
	};
	AdvertSeen _adverts[kAdvertSlots] = {};
	HopRate _hops[kHopSlots] = {};
	uint8_t _advert_next = 0;
	uint8_t _hop_next = 0;
	Counters _counters = {};

public:
	const Counters &counters() const { return _counters; }

	/* True when an advert of this key was kept less than the gap ago: drop it. */
	bool advertInGap(const Prefs &p, uint32_t key, uint32_t now)
	{
		if (p.advert_gap_min == 0) {
			return false;
		}
		for (const AdvertSeen &s : _adverts) {
			if (s.used && s.key == key) {
				if (within(now, s.at_ms, (uint32_t)p.advert_gap_min * 60000UL)) {
					_counters.gap++;
					return true;
				}
				return false;
			}
		}
		return false;
	}

	/* An advert of this key passed the signature check: it opens the gap. Only a
	 * verified advert may, or a forged one could silence the real node. */
	void advertKept(const Prefs &p, uint32_t key, uint32_t now)
	{
		if (p.advert_gap_min == 0) {
			return;
		}
		AdvertSeen *slot = nullptr;

		for (AdvertSeen &s : _adverts) {
			if (s.used && s.key == key) {
				slot = &s;
				break;
			}
		}
		if (!slot) {
			slot = &_adverts[_advert_next];
			_advert_next = (uint8_t)((_advert_next + 1) % kAdvertSlots);
			slot->key = key;
			slot->used = true;
		}
		slot->at_ms = now;
	}

	/* True when this repeater, named by the last hash of the path, already got
	 * hop_rate frames through this minute: drop the frame. Counts the ones it
	 * lets through. A 1-byte hash names no repeater, so it is not counted. */
	bool hopOverRate(const Prefs &p, const uint8_t *last_hop, size_t size, uint32_t now)
	{
		if (p.hop_rate == 0 || size < 2 || size > kHopMax) {
			return false;
		}
		HopRate *slot = nullptr;

		for (HopRate &h : _hops) {
			if (h.size == size && memcmp(h.prefix, last_hop, size) == 0) {
				slot = &h;
				break;
			}
		}
		if (!slot) {
			slot = &_hops[_hop_next];
			_hop_next = (uint8_t)((_hop_next + 1) % kHopSlots);
			slot->size = (uint8_t)size;
			memcpy(slot->prefix, last_hop, size);
			slot->window_ms = now;
			slot->count = 0;
		}
		if (!within(now, slot->window_ms, kRateWindowMs)) {
			slot->window_ms = now;
			slot->count = 0;
		}
		if (slot->count >= p.hop_rate) {
			_counters.rate++;
			return true;
		}
		slot->count++;
		return false;
	}
};

struct CmdResult {
	bool handled;  /* the command was a firewall one */
	bool changed;  /* a setting changed: save it */
};

/* "get|set z.mc.fw.<key>". */
inline bool isCommand(const char *command)
{
	return (strncmp(command, "get z.mc.fw.", 12) == 0) || (strncmp(command, "set z.mc.fw.", 12) == 0);
}

inline bool parseLimit(const char *v, uint8_t &out)
{
	if (*v == '\0') {
		return false;
	}
	unsigned n = 0;

	for (; *v; v++) {
		if (*v < '0' || *v > '9' || (n = n * 10 + (unsigned)(*v - '0')) > 255) {
			return false;
		}
	}
	out = (uint8_t)n;
	return true;
}

/* The reply is always written when handled.
 *   z.mc.fw.gap    get|set 0-255: minutes between two adverts of one key (0 = no limit)
 *   z.mc.fw.rate   get|set 0-255: flood frames a minute from one neighbouring repeater (0 = no limit)
 *   z.mc.fw.stats  get only: frames dropped by each limit since boot */
inline CmdResult handleCommand(Prefs &p, const Firewall &f, const char *command, char *reply, size_t cap)
{
	if (!isCommand(command)) {
		return {false, false};
	}
	const bool get = command[0] == 'g';
	const char *key = command + 12;
	char name[8];
	size_t n = 0;

	while (key[n] && key[n] != ' ' && n < sizeof(name) - 1) {
		name[n] = key[n];
		n++;
	}
	name[n] = '\0';
	const char *val = key[n] == ' ' ? key + n + 1 : "";
	uint8_t *limit = strcmp(name, "gap") == 0 ? &p.advert_gap_min : strcmp(name, "rate") == 0 ? &p.hop_rate : nullptr;
	uint8_t v;

	if (strcmp(name, "stats") == 0) {
		if (get) {
			snprintf(reply, cap, "> gap %u, rate %u", (unsigned)f.counters().gap, (unsigned)f.counters().rate);
		} else {
			snprintf(reply, cap, "Error: stats is read only");
		}
		return {true, false};
	}
	if (!limit) {
		snprintf(reply, cap, "Error: unknown firewall setting");
		return {true, false};
	}
	if (get) {
		snprintf(reply, cap, "> %u", (unsigned)*limit);
		return {true, false};
	}
	if (!parseLimit(val, v)) {
		snprintf(reply, cap, "Error: 0 to 255");
		return {true, false};
	}
	const bool changed = *limit != v;

	*limit = v;
	snprintf(reply, cap, "OK");
	return {true, changed};
}

} /* namespace fw */
