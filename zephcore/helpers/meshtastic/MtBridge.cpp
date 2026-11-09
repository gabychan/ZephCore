/*
 * SPDX-License-Identifier: MIT
 * Meshtastic bridge logic. See MtBridge.h.
 */

#include "MtBridge.h"

#include <stdio.h>
#include <string.h>

namespace mt {

/* HardwareModel.PRIVATE_HW. */
static constexpr uint32_t kHwPrivate = 255;

void Bridge::setSelf(uint32_t node, const char *long_name, const char *short_name, uint32_t seed)
{
	if (node < 4 || node == kBroadcast) {
		node = (node ^ 0x5A5A5A5Au) | 0x10000000u;
	}
	_self = node;
	_id_prefix = (seed & 0x3FFu) << 22;
	_id_counter = (seed >> 10) & 0x3FFFFFu;

	memset(&_user, 0, sizeof(_user));
	snprintf(_user.id, sizeof(_user.id), "!%08x", (unsigned)_self);
	snprintf(_user.long_name, sizeof(_user.long_name), "%s", long_name);
	snprintf(_user.short_name, sizeof(_user.short_name), "%s", short_name);
	_user.hw_model = kHwPrivate;
}

bool Bridge::setChannel(uint8_t channel_id, const char *name, const uint8_t *psk, size_t psk_len)
{
	uint8_t key[16];
	bool enc;

	if (!expandPsk(psk, psk_len, key, enc)) {
		return false;
	}
	Channel *slot = nullptr;

	for (auto &c : _ch) {
		if (c.used && c.id == channel_id) {
			slot = &c;
			break;
		}
		if (!c.used && !slot) {
			slot = &c;
		}
	}
	if (!slot) {
		return false;
	}
	slot->used = true;
	slot->id = channel_id;
	slot->encrypted = enc;
	memcpy(slot->key, key, 16);
	slot->hash = channelHash(name, key, enc);
	return true;
}

void Bridge::clearChannels()
{
	memset(_ch, 0, sizeof(_ch));
}

int Bridge::channelCount() const
{
	int n = 0;

	for (auto &c : _ch) {
		n += c.used ? 1 : 0;
	}
	return n;
}

const Bridge::Channel *Bridge::find(uint8_t channel_id) const
{
	for (auto &c : _ch) {
		if (c.used && c.id == channel_id) {
			return &c;
		}
	}
	return nullptr;
}

bool Bridge::hasChannel(uint8_t channel_id) const
{
	return find(channel_id) != nullptr;
}

bool Bridge::seen(uint32_t from, uint32_t id) const
{
	for (auto &s : _seen) {
		if (s.from == from && s.id == id) {
			return true;
		}
	}
	return false;
}

void Bridge::remember(uint32_t from, uint32_t id)
{
	_seen[_seen_next] = {from, id};
	_seen_next = (uint8_t)((_seen_next + 1) % kDedupSize);
}

uint32_t Bridge::nextId()
{
	_id_counter = (_id_counter + 1) & 0x3FFFFFu;
	return _id_prefix | _id_counter;
}

RxResult Bridge::handleFrame(const uint8_t *raw, size_t len, int8_t snr_x4, Sink &sink,
			     uint8_t *relay_out, size_t *relay_len)
{
	Header h;

	*relay_len = 0;
	/* A header plus at least the two bytes of the shortest Data (portnum). */
	if (_self == 0 || len < kHeaderLen + 2 || len > kMaxFrame || !parseHeader(raw, len, h) ||
	    h.from == 0) {
		return RX_NOT_MESHTASTIC;
	}

	size_t plen = len - kHeaderLen;
	uint8_t buf[kMaxFrame];
	Data d;
	const Channel *match = nullptr;

	for (auto &c : _ch) {
		if (!c.used || c.hash != h.channel) {
			continue;
		}
		memcpy(buf, raw + kHeaderLen, plen);
		if (c.encrypted) {
			ctrCrypt(_aes, c.key, h.from, h.id, buf, plen);
		}
		if (decodeData(buf, plen, d)) {
			match = &c;
			break;
		}
	}
	if (!match) {
		return RX_NOT_MESHTASTIC;
	}

	if (h.from == _self || seen(h.from, h.id)) {
		return RX_DUPLICATE;
	}
	remember(h.from, h.id);

	uint8_t hops = h.hopStart() > h.hopLimit() ? (uint8_t)(h.hopStart() - h.hopLimit()) : 0;

	if (d.portnum == PORT_TEXT) {
		if ((h.to == kBroadcast || h.to == _self) && d.payload_len > 0) {
			sink.onText(match->id, h.from, h.to, h.id, d.reply_id,
				    (const char *)d.payload, d.payload_len, snr_x4, hops);
		}
	} else if (d.portnum == PORT_NODEINFO) {
		User u;

		if (decodeUser(d.payload, d.payload_len, u)) {
			sink.onNodeInfo(h.from, u);
		}
	} else if (d.portnum == PORT_POSITION) {
		Position p;

		if (decodePosition(d.payload, d.payload_len, p) && p.has_lat && p.has_lon) {
			sink.onPosition(h.from, p);
		}
	}

	/* Repeat a flood once: next_hop 0, hops left, and not a frame for us alone. */
	if (_relay && h.next_hop == 0 && h.hopLimit() > 0 && h.to != _self) {
		memcpy(relay_out, raw, len);
		relay_out[12] = (uint8_t)((h.flags & ~0x07) | (h.hopLimit() - 1));
		relay_out[15] = (uint8_t)_self;
		*relay_len = len;
	}
	return RX_HANDLED;
}

size_t Bridge::buildFrame(const Channel &ch, uint32_t to, uint32_t port, const uint8_t *payload,
			  size_t payload_len, uint8_t *out, size_t cap, uint32_t reply_id)
{
	if (_self == 0 || cap < kHeaderLen) {
		return 0;
	}
	Data d = {};

	d.portnum = port;
	d.payload = payload;
	d.payload_len = payload_len;
	d.reply_id = reply_id;

	size_t room = (cap < kMaxTxFrame ? cap : kMaxTxFrame) - kHeaderLen;
	size_t n = encodeData(d, out + kHeaderLen, room);

	if (n == 0) {
		return 0;
	}
	Header h;

	h.to = to;
	h.from = _self;
	h.id = nextId();
	h.flags = (uint8_t)(_hops | (_hops << 5));
	h.channel = ch.hash;
	h.next_hop = 0;
	h.relay = (uint8_t)_self;
	writeHeader(h, out);

	if (ch.encrypted) {
		ctrCrypt(_aes, ch.key, _self, h.id, out + kHeaderLen, n);
	}
	remember(_self, h.id);
	return kHeaderLen + n;
}

size_t Bridge::buildText(uint8_t channel_id, uint32_t to, const char *text, size_t len,
			 uint8_t *out, size_t cap, uint32_t reply_id)
{
	const Channel *ch = find(channel_id);

	if (!ch || len == 0) {
		return 0;
	}
	return buildFrame(*ch, to, PORT_TEXT, (const uint8_t *)text, len, out, cap, reply_id);
}

size_t Bridge::buildNodeInfo(uint8_t channel_id, uint8_t *out, size_t cap)
{
	const Channel *ch = find(channel_id);
	uint8_t pb[96];

	if (!ch) {
		return 0;
	}
	size_t n = encodeUser(_user, pb, sizeof(pb));

	return n ? buildFrame(*ch, kBroadcast, PORT_NODEINFO, pb, n, out, cap) : 0;
}

size_t Bridge::buildPosition(uint8_t channel_id, const Position &pos, uint8_t *out, size_t cap)
{
	const Channel *ch = find(channel_id);
	uint8_t pb[32];

	if (!ch) {
		return 0;
	}
	size_t n = encodePosition(pos, pb, sizeof(pb));

	return n ? buildFrame(*ch, kBroadcast, PORT_POSITION, pb, n, out, cap) : 0;
}

} /* namespace mt */
