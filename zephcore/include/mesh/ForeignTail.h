/*
 * SPDX-License-Identifier: MIT
 * The part of a foreign frame that does not fit in Packet::payload. A frame of
 * another protocol can be MAX_TRANS_UNIT bytes long, a Packet holds
 * MAX_PACKET_PAYLOAD: the first bytes ride in the Packet, the rest waits in a
 * slot of this pool until the Packet is released.
 */

#pragma once

#include <mesh/MeshCore.h>
#include <stdint.h>
#include <string.h>

#ifndef CONFIG_ZEPHTACORE_FOREIGN_TAIL_SLOTS
#define CONFIG_ZEPHTACORE_FOREIGN_TAIL_SLOTS 0
#endif

namespace mesh {

class ForeignTail {
public:
	static constexpr int SLOTS = CONFIG_ZEPHTACORE_FOREIGN_TAIL_SLOTS;
	static constexpr int MAX_FRAME = MAX_TRANS_UNIT;
	static constexpr int CAPACITY = MAX_FRAME - MAX_PACKET_PAYLOAD;

	/* Copies len bytes (<= CAPACITY) into a free slot; its index, or -1 when none is free. */
	int take(const uint8_t *src, int len)
	{
		if (len <= 0 || len > CAPACITY) {
			return -1;
		}
		for (int i = 0; i < SLOTS; i++) {
			if (!_used[i]) {
				_used[i] = true;
				memcpy(_data[i], src, len);
				return i;
			}
		}
		return -1;
	}

	const uint8_t *data(int slot) const { return _data[slot]; }

	void give(int slot)
	{
		if (slot >= 0 && slot < SLOTS) {
			_used[slot] = false;
		}
	}

	int freeCount() const
	{
		int n = 0;
		for (int i = 0; i < SLOTS; i++) {
			n += !_used[i];
		}
		return n;
	}

private:
	static constexpr int N = SLOTS > 0 ? SLOTS : 1;
	uint8_t _data[N][CAPACITY];
	bool _used[N] = {};
};

}  // namespace mesh
