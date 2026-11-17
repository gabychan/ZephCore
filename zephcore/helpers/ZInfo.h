/*
 * SPDX-License-Identifier: MIT
 * Zephtacore: the one-line replies of "get z.info" and "get z.stats", so a
 * remote CLI gets them in one radio packet. A field the role or build does not
 * have is left out of the line. Pure code, host tested.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

namespace zinfo {

struct Info {
	const char *fw;
	uint32_t uptime_s;
	uint16_t batt_mv;
	uint32_t flash_used_kb, flash_total_kb;  /* total 0: not known */
	uint32_t heap_used, heap_total, heap_peak;  /* bytes; total 0: not counted */
	uint16_t contacts, max_contacts;  /* max 0: no contact table */
};

struct Stats {
	uint32_t recv, sent, errors;
	uint32_t flood_tx, direct_tx, flood_rx, direct_rx;
	uint32_t tx_air_s, rx_air_s;
	uint32_t queue;
};

/* "1d02h", "3h05m" or "12m". */
inline int formatUptime(char *out, size_t cap, uint32_t s)
{
	const uint32_t m = s / 60, h = m / 60, d = h / 24;

	if (d > 0) {
		return snprintf(out, cap, "%ud%02uh", (unsigned)d, (unsigned)(h % 24));
	}
	if (h > 0) {
		return snprintf(out, cap, "%uh%02um", (unsigned)h, (unsigned)(m % 60));
	}
	return snprintf(out, cap, "%um", (unsigned)m);
}

/* "> fw 1.18.2 | up 1d02h | battery 4231mV | flash 96/2048K | heap 12.3/44.0K (peak 20.1K) | contacts 351/350" */
inline void formatInfo(char *out, size_t cap, const Info &i)
{
	char up[16];
	size_t n;

	formatUptime(up, sizeof(up), i.uptime_s);
	n = (size_t)snprintf(out, cap, "> fw %s | up %s | battery %umV", i.fw, up, (unsigned)i.batt_mv);
	if (n < cap && i.flash_total_kb != 0) {
		n += (size_t)snprintf(out + n, cap - n, " | flash %u/%uK", (unsigned)i.flash_used_kb,
				      (unsigned)i.flash_total_kb);
	}
	if (n < cap && i.heap_total != 0) {
		n += (size_t)snprintf(out + n, cap - n, " | heap %.1f/%.1fK (peak %.1fK)",
				      (double)i.heap_used / 1024.0, (double)i.heap_total / 1024.0,
				      (double)i.heap_peak / 1024.0);
	}
	if (n < cap && i.max_contacts != 0) {
		snprintf(out + n, cap - n, " | contacts %u/%u", (unsigned)i.contacts,
			 (unsigned)i.max_contacts);
	}
}

/* "> mc: rx 1234, tx 56, err 3, flood 40/16, direct 20/9, air 12/340s, queue 0 | mt: heard=..":
 * MeshCore, then the Meshtastic counters when the build has them. flood and direct are sent/received;
 * air is seconds on air, transmitting/receiving. */
inline void formatStats(char *out, size_t cap, const Stats &s, const char *mt)
{
	size_t n = (size_t)snprintf(out, cap,
				    "> mc: rx %u, tx %u, err %u, flood %u/%u, direct %u/%u, air %u/%us, queue %u",
				    (unsigned)s.recv, (unsigned)s.sent, (unsigned)s.errors,
				    (unsigned)s.flood_tx, (unsigned)s.flood_rx, (unsigned)s.direct_tx,
				    (unsigned)s.direct_rx, (unsigned)s.tx_air_s, (unsigned)s.rx_air_s,
				    (unsigned)s.queue);

	if (n < cap && mt != nullptr) {
		snprintf(out + n, cap - n, " | mt: %s", mt);
	}
}

} /* namespace zinfo */
