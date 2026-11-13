/*
 * SPDX-License-Identifier: MIT
 * Zephtacore: zero hop. A frame that a fixed neighbour repeated to us is
 * repeated without lowering hop_limit. The neighbour is known by the last byte
 * of its node number, which the header carries. Pure code, host tested.
 */

#pragma once

#include "MtConfig.h"
#include "MtWire.h"

#include "../NeighbourList.h"

#include <stddef.h>
#include <stdint.h>

namespace mt {

static constexpr size_t kZeroHopFixedMax = 16;
static constexpr uint32_t kZeroHopLiveS = 72u * 3600u;

/* The roles of a node that stays in one place to repeat. */
inline bool roleIsFixed(uint32_t role)
{
	return role == ROLE_ROUTER || role == ROLE_ROUTER_LATE || role == ROLE_CLIENT_BASE;
}

/* The byte a node puts in the header when it repeats: never 0. */
inline uint8_t relayByte(uint32_t node)
{
	return (uint8_t)node ? (uint8_t)node : 0xFF;
}

class ZeroHop {
public:
	/* The role a NodeInfo announces. A node that is no longer fixed is forgotten;
	 * a new one takes a free entry, then one that is not a neighbour, then the oldest. */
	void noteRole(uint32_t node, uint32_t role, const neighbours::Table &heard)
	{
		uint32_t *known = find(node);

		if (!roleIsFixed(role)) {
			if (known) {
				*known = 0;
			}
			return;
		}
		if (known) {
			return;
		}
		uint32_t *entry = find(0);

		for (size_t i = 0; !entry && i < kZeroHopFixedMax; i++) {
			if (!neighbour(heard, _fixed[i])) {
				entry = &_fixed[i];
			}
		}
		if (!entry) {
			entry = &_fixed[_next];
			_next = (uint8_t)((_next + 1) % kZeroHopFixedMax);
		}
		*entry = node;
	}

	/* True when the frame is repeated with the hop_limit it came with. */
	bool keepsHop(const Header &h, const uint32_t listed[kZeroHopNodesMax],
		      const neighbours::Table &heard, uint32_t now_s) const
	{
		/* The first hop always counts: a frame with hop_limit == hop_start reads as heard from its sender. */
		if (h.relay == 0 || h.hopStart() == 0 || h.hopStart() <= h.hopLimit()) {
			return false;
		}
		for (size_t i = 0; i < kZeroHopNodesMax; i++) {
			if (listed[i] != 0 && relayByte(listed[i]) == h.relay) {
				return true;
			}
		}
		const neighbours::Entry *only = nullptr;

		for (const neighbours::Entry &e : heard) {
			if (e.id == 0 || relayByte(e.id) != h.relay || now_s - e.heard_s > kZeroHopLiveS) {
				continue;
			}
			if (only) {
				return false;  /* two neighbours share the byte: we cannot tell which repeated */
			}
			only = &e;
		}
		return only && isFixed(only->id);
	}

	bool isFixed(uint32_t node) const
	{
		for (uint32_t n : _fixed) {
			if (n == node && node != 0) {
				return true;
			}
		}
		return false;
	}

private:
	uint32_t *find(uint32_t node)
	{
		for (uint32_t &n : _fixed) {
			if (n == node) {
				return &n;
			}
		}
		return nullptr;
	}

	static bool neighbour(const neighbours::Table &heard, uint32_t node)
	{
		for (const neighbours::Entry &e : heard) {
			if (e.id == node) {
				return true;
			}
		}
		return false;
	}

	uint32_t _fixed[kZeroHopFixedMax] = {};
	uint8_t _next = 0;
};

} /* namespace mt */
