/*
 * SPDX-License-Identifier: MIT
 * The last few node keys read from or written to the mtkeys file, so the
 * nodes we talk to are not looked up in flash on every frame. Pure code, host
 * tested.
 */

#pragma once

#include <stdint.h>
#include <string.h>

namespace mt {

template <int N> class KeyCache {
public:
	bool get(uint32_t node, uint8_t key[32]) const
	{
		for (const Entry &e : _entries) {
			if (e.used && e.node == node) {
				memcpy(key, e.key, 32);
				return true;
			}
		}
		return false;
	}

	/* Keeps node's key, in place of its old one or of the entry written longest ago. */
	void put(uint32_t node, const uint8_t key[32])
	{
		Entry *slot = find(node);

		if (!slot) {
			slot = &_entries[_next];
			_next = (uint8_t)((_next + 1) % N);
		}
		slot->used = true;
		slot->node = node;
		memcpy(slot->key, key, 32);
	}

	void clear()
	{
		for (Entry &e : _entries) {
			e.used = false;
		}
	}

private:
	struct Entry {
		uint32_t node;
		uint8_t key[32];
		bool used;
	};

	Entry *find(uint32_t node)
	{
		for (Entry &e : _entries) {
			if (e.used && e.node == node) {
				return &e;
			}
		}
		return nullptr;
	}

	Entry _entries[N] = {};
	uint8_t _next = 0;
};

}  // namespace mt
