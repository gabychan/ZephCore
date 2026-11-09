/*
 * SPDX-License-Identifier: MIT
 * Tells a MeshCore frame from a Meshtastic one that no channel of ours opens.
 * The receiver hears both sync words and does not say which one a frame came
 * in on, so the frame's own shape decides. A real MeshCore frame always
 * passes; about one Meshtastic frame in fifty does by chance.
 */

#pragma once

#include <mesh/Packet.h>
#include <stddef.h>
#include <stdint.h>

namespace mt {

/* Every encrypted MeshCore payload is a fixed part (hashes, 2-byte MAC) and
 * whole 16-byte AES blocks. Types with no such structure pass. */
inline bool meshcorePayloadShapeValid(uint8_t payload_type, size_t payload_len)
{
	constexpr size_t kMac = 2, kBlock = 16;
	size_t fixed;

	switch (payload_type) {
	case PAYLOAD_TYPE_REQ:
	case PAYLOAD_TYPE_RESPONSE:
	case PAYLOAD_TYPE_TXT_MSG:
	case PAYLOAD_TYPE_PATH:
		fixed = 1 + 1 + kMac;
		break;
	case PAYLOAD_TYPE_ANON_REQ:
		fixed = 1 + PUB_KEY_SIZE + kMac;
		break;
	case PAYLOAD_TYPE_GRP_TXT:
	case PAYLOAD_TYPE_GRP_DATA:
		fixed = 1 + kMac;
		break;
	default:
		return true;
	}
	return payload_len > fixed && (payload_len - fixed) % kBlock == 0;
}

/* True when raw is a well formed MeshCore frame. A frame whose Meshtastic
 * to or from is our own node number is Meshtastic, whatever else it looks like.
 * self 0 disables that veto. */
inline bool looksLikeMeshCore(const uint8_t *raw, size_t len, uint32_t self)
{
	if (len < 2 || len > MAX_TRANS_UNIT) {
		return false;
	}

	const uint8_t header = raw[0];
	const uint8_t route = header & PH_ROUTE_MASK;
	size_t i = 1;

	if (((header >> PH_VER_SHIFT) & PH_VER_MASK) > PAYLOAD_VER_1) {
		return false;
	}
	if (route == ROUTE_TYPE_TRANSPORT_FLOOD || route == ROUTE_TYPE_TRANSPORT_DIRECT) {
		if (len < 6) {
			return false;
		}
		i += 4;
	}
	const uint8_t path_len = raw[i++];

	if ((path_len >> 6) == 3) {
		return false;
	}
	size_t path_bytes = (size_t)(path_len & 63) * ((path_len >> 6) + 1);

	if (path_bytes > MAX_PATH_SIZE || i + path_bytes > len) {
		return false;
	}
	i += path_bytes;
	if (len - i > MAX_PACKET_PAYLOAD ||
	    !meshcorePayloadShapeValid((header >> PH_TYPE_SHIFT) & PH_TYPE_MASK, len - i)) {
		return false;
	}
	if (self != 0 && len >= 8) {
		uint32_t to = raw[0] | (raw[1] << 8) | (raw[2] << 16) | ((uint32_t)raw[3] << 24);
		uint32_t from = raw[4] | (raw[5] << 8) | (raw[6] << 16) | ((uint32_t)raw[7] << 24);

		if (to == self || from == self) {
			return false;
		}
	}
	return true;
}

} /* namespace mt */
