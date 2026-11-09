/*
 * SPDX-License-Identifier: MIT
 * TX stalls: times the dispatcher could not transmit for getCADFailMaxDuration(),
 * per cause, for `get tx.stalls`. ERR_EVENT_CAD_TIMEOUT only says one happened.
 */

#pragma once

#include <stdint.h>
#include <stdio.h>

namespace mesh {

struct TxStall {
	uint32_t count = 0;
	uint32_t last_ms = 0;  /* uptime of the last one, 0 = never */
	uint32_t max_ms = 0;   /* longest blocked streak measured */

	void record(uint32_t now_ms, uint32_t streak_ms)
	{
		count++;
		last_ms = now_ms ? now_ms : 1;
		if (streak_ms > max_ms) {
			max_ms = streak_ms;
		}
	}
};

/* "lbt n=<count> ago=<s> max=<ms> | rx n=<count> ago=<s> max=<ms>"; ago is
 * omitted while the count is 0. lbt: the radio's CAD kept refusing the
 * transmit. rx: the receive gate (preamble or header in flight) kept it closed,
 * ending in an RX restart. Returns the snprintf length. */
inline int formatTxStalls(char *buf, int cap, const TxStall &lbt, const TxStall &rx,
			  uint32_t now_ms)
{
	char part[2][48];
	const TxStall *s[2] = {&lbt, &rx};

	for (int i = 0; i < 2; i++) {
		if (s[i]->count == 0) {
			snprintf(part[i], sizeof(part[i]), "n=0");
		} else {
			snprintf(part[i], sizeof(part[i]), "n=%u ago=%us max=%ums",
				 (unsigned)s[i]->count, (unsigned)((now_ms - s[i]->last_ms) / 1000),
				 (unsigned)s[i]->max_ms);
		}
	}
	return snprintf(buf, cap, "lbt %s | rx %s", part[0], part[1]);
}

} /* namespace mesh */
