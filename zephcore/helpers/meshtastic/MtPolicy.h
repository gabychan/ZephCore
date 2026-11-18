/*
 * SPDX-License-Identifier: MIT
 * Zephtacore decisions that need no hardware, kept apart so they can be tested
 * on the host: the per-node limiter of the NodeInfo traffic we start or answer,
 * and when marking a favourite asks a node for its NodeInfo.
 */

#pragma once

#include <stdint.h>
#include "MtWire.h"

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

/* A Meshtastic node becomes a contact only when its NodeInfo arrived complete:
 * a name and the public key a direct message needs. A Position or a text from a
 * node not heard before leaves its slot free; the NodeInfo comes later. */
inline bool createsContact(bool has_name, bool has_key)
{
	return has_name && has_key;
}

/* A one-shot deadline in milliseconds that may wrap: the NodeInfo and the
 * telemetry we send now and then. */
class Timer {
public:
	void arm(uint32_t now_ms, uint32_t delay_ms)
	{
		_at = now_ms + delay_ms;
		_on = true;
	}
	void stop() { _on = false; }
	/* The next round after one that came due: another period when on, else stopped. */
	void rearm(uint32_t now_ms, bool on, uint32_t period_s)
	{
		if (on && period_s) {
			arm(now_ms, period_s * 1000u);
		} else {
			stop();
		}
	}
	bool armed() const { return _on; }
	uint32_t at() const { return _at; }
	bool due(uint32_t now_ms) const { return _on && (int32_t)(now_ms - _at) >= 0; }

private:
	uint32_t _at = 0;
	bool _on = false;
};

/* Which relay modes repeat a frame: Essential in essential and all, Other in all only. */
enum class RelayClass : uint8_t { Essential, Other };

/* Traffic a mesh cannot do without is repeated in every mode but off: text and
 * direct messages, their ACKs, NodeInfo, traceroute and position, so nodes show
 * on the map. portnum 0 is a frame we could not open (another network's channel). */
inline RelayClass relayClassOf(uint32_t portnum)
{
	switch (portnum) {
	case PORT_TEXT:
	case PORT_ROUTING:
	case PORT_NODEINFO:
	case PORT_TRACEROUTE:
	case PORT_POSITION:
		return RelayClass::Essential;
	default:
		return RelayClass::Other;
	}
}

/* Whether a repeat is queued: essential repeats everything but Other, all repeats it too. */
inline bool repeatsInMode(bool relay_all, RelayClass cls)
{
	return relay_all || cls != RelayClass::Other;
}

} /* namespace mt */
