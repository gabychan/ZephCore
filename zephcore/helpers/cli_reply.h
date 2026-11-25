/*
 * SPDX-License-Identifier: MIT
 * cli_reply - Reply buffer sizing for CLI handlers, kept out of the Zephyr
 * layer so the host harness can test it.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace cli_reply {

/* A local caller's reply buffer (sender_timestamp 0). */
static const size_t LOCAL_REPLY_SIZE = 256;
/* A remote caller's reply buffer: what onPeerDataRecv hands over, header
 * included. Smaller than the local one because it travels in a packet. */
static const size_t REMOTE_REPLY_SIZE = 161;

/* Bytes a handler may write at `reply`, terminator included: the whole local
 * buffer, or what is left of a remote one after the caller's header.
 *
 * Two defects this keeps closed:
 *   - a remote caller sized with the local buffer writes past its 161 bytes;
 *   - a header larger than the buffer underflows a size_t and hands the
 *     handler the whole address space. */
inline size_t capFor(uint32_t sender_timestamp, uint8_t hdr_used)
{
	if (sender_timestamp == 0) {
		return hdr_used < LOCAL_REPLY_SIZE ? LOCAL_REPLY_SIZE - hdr_used : 0;
	}
	return hdr_used < REMOTE_REPLY_SIZE ? REMOTE_REPLY_SIZE - hdr_used : 0;
}

}  // namespace cli_reply
