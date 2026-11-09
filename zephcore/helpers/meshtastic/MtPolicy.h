/*
 * SPDX-License-Identifier: MIT
 * Zephtacore decisions that need no hardware, kept apart so they can be tested
 * on the host: the per-node limiter of the NodeInfo traffic we start or answer,
 * and when marking a favourite asks a node for its NodeInfo.
 */

#pragma once

#include <stdint.h>

namespace mt {

/* Lets a node through once per interval. Fixed table, the least recently
 * allowed node is forgotten first. now_ms may wrap. */
class NodeRateLimit {
public:
	static constexpr int kSlots = 8;

	bool allow(uint32_t node, uint32_t now_ms, uint32_t interval_ms)
	{
		Slot *free_slot = &_slot[0];

		for (auto &s : _slot) {
			if (s.used && s.node == node) {
				if ((uint32_t)(now_ms - s.at_ms) < interval_ms) {
					return false;
				}
				s.at_ms = now_ms;
				return true;
			}
			if (!s.used) {
				free_slot = &s;
			} else if (free_slot->used &&
				   (int32_t)(s.at_ms - free_slot->at_ms) < 0) {
				free_slot = &s;
			}
		}
		*free_slot = {node, now_ms, true};
		return true;
	}

private:
	struct Slot {
		uint32_t node;
		uint32_t at_ms;
		bool used;
	};
	Slot _slot[kSlots] = {};
};

/* Marking a contact as favourite asks the node once, and only when something
 * is missing: its name or its public key. */
inline bool favouriteAsksNodeInfo(bool was_favourite, bool is_favourite, bool has_name,
				  bool has_key)
{
	return !was_favourite && is_favourite && !(has_name && has_key);
}

} /* namespace mt */
