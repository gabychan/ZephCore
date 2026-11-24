/*
 * SPDX-License-Identifier: MIT
 * Meshtastic bridge logic. See MtBridge.h.
 */

#include "MtBridge.h"
#include "MtDiscriminate.h"

#include <monocypher.h>
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

	bool had_key = _user.has_public_key;
	uint8_t key[32];

	memcpy(key, _user.public_key, sizeof(key));
	memset(&_user, 0, sizeof(_user));
	_user.has_public_key = had_key;
	memcpy(_user.public_key, key, sizeof(key));
	snprintf(_user.id, sizeof(_user.id), "!%08x", (unsigned)_self);
	snprintf(_user.long_name, sizeof(_user.long_name), "%s", long_name);
	snprintf(_user.short_name, sizeof(_user.short_name), "%s", short_name);
	_user.hw_model = kHwPrivate;
}

void Bridge::setNames(const char *long_name, const char *short_name)
{
	snprintf(_user.long_name, sizeof(_user.long_name), "%s", long_name);
	snprintf(_user.short_name, sizeof(_user.short_name), "%s", short_name);
}

void Bridge::setPki(const PkiCrypto *crypto, const uint8_t seed[32])
{
	_pki = crypto;
	pkiPrivateKey(*crypto, seed, _pki_priv);
	pkiPublicKey(_pki_priv, _user.public_key);
	_user.has_public_key = true;
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

Bridge::Seen *Bridge::findSeen(uint32_t from, uint32_t id)
{
	for (auto &s : _seen) {
		if (s.from == from && s.id == id) {
			return &s;
		}
	}
	return nullptr;
}

Bridge::Seen *Bridge::remember(uint32_t from, uint32_t id)
{
	Seen *s = &_seen[_seen_next];

	*s = {from, id, false};
	_seen_next = (uint8_t)((_seen_next + 1) % kDedupSize);
	return s;
}

bool Bridge::eligible(const Header &h) const
{
	return _relay && h.id != 0 && h.hopLimit() > 0 && h.from != _self && h.to != _self &&
	       (h.next_hop == 0 || h.next_hop == relayByte(_self));
}

bool Bridge::heardBefore(const Header &h, const uint8_t *raw, size_t len, uint8_t *relay_out,
			 size_t *relay_len)
{
	if (h.from == _self) {
		return true;
	}
	Seen *s = findSeen(h.from, h.id);

	if (!s) {
		return false;
	}
	const bool originator = h.fromOrigin();

	if ((!s->relayed || originator) && fillRelay(h, raw, len, relay_out, relay_len)) {
		s->relayed = true;
	}
	return true;
}

uint32_t Bridge::nextId()
{
	uint32_t id;

	do {
		_id_counter = (_id_counter + 1) & 0x3FFFFFu;
		id = _id_prefix | _id_counter;
	} while (id == 0);  /* 0 means "no id" to Meshtastic */
	return id;
}

static constexpr int kSnrMin = -20;
static constexpr int kSnrMax = 10;
static constexpr uint8_t kCwMin = 3;
static constexpr uint8_t kCwMax = 8;

uint32_t slotTimeMs(uint8_t sf, float bw_khz)
{
	if (bw_khz <= 0.0f) {
		return 0;
	}
	float symbol_ms = (float)(1UL << sf) / bw_khz;

	return (uint32_t)(2.5f * symbol_ms + 7.6f);
}

uint8_t rebroadcastCw(int8_t snr_x4)
{
	int snr = snr_x4 / 4;
	int cw = (snr - kSnrMin) * (kCwMax - kCwMin) / (kSnrMax - kSnrMin) + kCwMin;

	if (cw < kCwMin) {
		return kCwMin;
	}
	return cw > kCwMax ? kCwMax : (uint8_t)cw;
}

uint32_t rebroadcastDelayMs(uint32_t slot_ms, uint32_t r)
{
	return (2U * kCwMax + r) * slot_ms;
}

uint32_t originWindow()
{
	return 1U << kCwMin;
}

uint32_t originDelayMs(uint32_t slot_ms, uint32_t r)
{
	return r * slot_ms;
}

uint8_t responseHopLimit(uint8_t hops_away, uint8_t own)
{
	const uint8_t hops = (uint8_t)(hops_away + 2);

	return hops < own ? hops : own;
}

uint32_t earlyDelayMs(uint32_t slot_ms, uint32_t r)
{
	return originDelayMs(slot_ms, r);
}

uint32_t relayWindow(int8_t snr_x4, bool early)
{
	const uint8_t cw = rebroadcastCw(snr_x4);

	return early ? 2U * cw : 1U << cw;
}

uint32_t windowSlots(TxWindow w, int8_t snr_x4)
{
	switch (w) {
	case TxWindow::Origin:
		return originWindow();
	case TxWindow::Router:
		return relayWindow(snr_x4, true);
	default:
		return relayWindow(snr_x4, false);
	}
}

uint32_t windowDelayMs(TxWindow w, int8_t snr_x4, uint32_t slot_ms, uint32_t r)
{
	switch (w) {
	case TxWindow::Origin:
		return originDelayMs(slot_ms, r);
	case TxWindow::Router:
		return earlyDelayMs(slot_ms, r);
	case TxWindow::LateHeard:
		return rebroadcastDelayMs(slot_ms, 2U * kCwMax + relayWindow(snr_x4, false) + r);
	default:
		return rebroadcastDelayMs(slot_ms, r);
	}
}

RxResult Bridge::handleFrame(const uint8_t *raw, size_t len, int8_t snr_x4, Sink &sink,
			     uint8_t *relay_out, size_t *relay_len, RelayClass *relay_class)
{
	Header h;

	*relay_len = 0;
	if (relay_class) {
		*relay_class = RelayClass::Other;
	}
	/* A header plus at least the two bytes of the shortest Data (portnum). */
	if (_self == 0 || len < kHeaderLen + 2 || len > kMaxFrame || !parseHeader(raw, len, h) ||
	    h.from == 0) {
		return RX_NOT_MESHTASTIC;
	}

	size_t plen = len - kHeaderLen;
	uint8_t buf[kMaxFrame];
	Data d;
	const Channel *match = nullptr;

	/* Channel 0 to us alone is PKI; one that does not open may still be a
	 * channel whose hash is 0. */
	if (_pki && h.channel == 0 && h.to == _self && plen > kPkiOverhead) {
		if (h.from == _self || findSeen(h.from, h.id)) {
			repeatAck(h, raw, plen, snr_x4, sink, buf, d);
			return RX_DUPLICATE;
		}
		if (openPki(h, raw, plen, sink, buf, d)) {
			if (_screen && _screen->drops(h, d.portnum)) {
				return RX_DROPPED;
			}
			remember(h.from, h.id);
			deliverPki(h, d, snr_x4, sink);
			return RX_HANDLED;
		}
	}

	for (auto &c : _ch) {
		if (!c.used || c.hash != h.channel) {
			continue;
		}
		memcpy(buf, raw + kHeaderLen, plen);
		if (c.encrypted && !ctrCrypt(_aes, c.key, h.from, h.id, buf, plen)) {
			continue;
		}
		if (decodeData(buf, plen, d)) {
			match = &c;
			break;
		}
	}
	if (!match) {
		if (!_relay || !_relay_opaque || looksLikeMeshCore(raw, len, _self)) {
			return RX_NOT_MESHTASTIC;
		}
		/* What is in a frame no channel of ours opens we cannot tell, so it is repeated. */
		if (relay_class) {
			*relay_class = RelayClass::Essential;
		}
		if (heardBefore(h, raw, len, relay_out, relay_len)) {
			return RX_DUPLICATE;
		}
		if (_screen && _screen->drops(h, 0)) {
			return RX_DROPPED;
		}
		/* A frame to us alone is not repeated, and a retry of it may open once the key is known. */
		if (h.to == _self) {
			return RX_OPAQUE;
		}
		Seen *s = remember(h.from, h.id);

		if (fillRelay(h, raw, len, relay_out, relay_len)) {
			s->relayed = true;
		}
		return RX_OPAQUE;
	}

	if (relay_class) {
		*relay_class = relayClassOf(d.portnum);
	}
	if (heardBefore(h, raw, len, relay_out, relay_len)) {
		return RX_DUPLICATE;
	}
	if (_screen && _screen->drops(h, d.portnum)) {
		return RX_DROPPED;
	}
	Seen *seen_now = remember(h.from, h.id);

	uint8_t hops = h.hopsAway();

	if (fillRelay(h, raw, len, relay_out, relay_len)) {
		seen_now->relayed = true;
	}
	if (d.portnum == PORT_TEXT) {
		if ((h.to == kBroadcast || h.to == _self) && d.payload_len > 0) {
			sink.onText(match->id, h.from, h.to, h.id, d.reply_id,
				    (const char *)d.payload, d.payload_len, snr_x4, hops, h.relay);
		}
	} else {
		deliverData(h, d, match->id, snr_x4, sink);
	}
	return RX_HANDLED;
}

bool Bridge::fillRelay(const Header &h, const uint8_t *raw, size_t len, uint8_t *relay_out,
		       size_t *relay_len) const
{
	if (!eligible(h)) {
		return false;
	}
	memcpy(relay_out, raw, len);
	relay_out[12] = (uint8_t)((h.flags & ~0x07) | (h.hopLimit() - 1));
	relay_out[14] = 0;  /* no route learned: flood on */
	relay_out[15] = relayByte(_self);
	*relay_len = len;
	return true;
}

bool Bridge::openPki(const Header &h, const uint8_t *raw, size_t plen, Sink &sink, uint8_t *buf,
		     Data &d)
{
	uint8_t peer[32], key[32];

	if (!sink.peerKey(h.from, peer)) {
		sink.onUnknownKey(h.from);
		return false;
	}
	if (!pkiSharedKey(*_pki, _pki_priv, peer, key)) {
		return false;
	}
	int n = pkiOpen(*_pki, key, h.from, h.id, raw + kHeaderLen, plen, buf, plen);

	crypto_wipe(key, sizeof(key));
	if (n <= 0) {
		sink.onPkiFailed(h.from);
		return false;
	}
	return decodeData(buf, (size_t)n, d);
}

/* A copy of a frame we already took. A sender repeats a frame until it hears its ACK, so one
 * heard straight from it and asking for an ACK is answered again; a copy that does not open is
 * never confirmed, and nothing is delivered twice. */
void Bridge::repeatAck(const Header &h, const uint8_t *raw, size_t plen, int8_t snr_x4, Sink &sink,
		       uint8_t *buf, Data &d)
{
	if (h.from == _self || !h.wantAck() || h.hopsAway() != 0) {
		return;
	}
	if (!openPki(h, raw, plen, sink, buf, d)) {
		return;
	}
	if (d.portnum == PORT_TEXT && d.payload_len > 0) {
		sink.onDirectRepeat(h.from, h.id, snr_x4, h.hopsAway());
	} else if (d.portnum == PORT_ROUTING && d.request_id != 0) {
		uint32_t reason;

		if (decodeRouting(d.payload, d.payload_len, reason)) {
			sink.onAckForAck(h.from, h.id, snr_x4);
		}
	}
}

void Bridge::deliverPki(const Header &h, const Data &d, int8_t snr_x4, Sink &sink)
{
	if (d.portnum == PORT_TEXT) {
		uint8_t hops = h.hopsAway();

		if (d.payload_len > 0) {
			sink.onDirectText(h.from, h.id, d.reply_id, h.wantAck(),
					  (const char *)d.payload, d.payload_len, snr_x4, hops);
		}
		return;
	}
	deliverData(h, d, kNoChannel, snr_x4, sink);
}

/* What a decrypted Data carries besides text: NodeInfo, Position, ACKs. */
void Bridge::deliverData(const Header &h, const Data &d, uint8_t channel_id, int8_t snr_x4,
			 Sink &sink)
{
	if (d.portnum == PORT_NODEINFO) {
		User u;

		if (decodeUser(d.payload, d.payload_len, u)) {
			sink.onNodeInfo(h.from, u);
			if (d.want_response && h.to == _self) {
				sink.onNodeInfoRequest(channel_id, h.from, snr_x4, h.hopsAway());
			}
		}
	} else if (d.portnum == PORT_POSITION) {
		Position p;

		if (decodePosition(d.payload, d.payload_len, p) && p.has_lat && p.has_lon &&
		    p.lat_i >= -900000000 && p.lat_i <= 900000000 &&
		    p.lon_i >= -1800000000 && p.lon_i <= 1800000000) {
			sink.onPosition(h.from, p);
		}
	} else if (d.portnum == PORT_ROUTING && h.to == _self && d.request_id != 0) {
		uint32_t reason;

		if (decodeRouting(d.payload, d.payload_len, reason)) {
			sink.onAck(h.from, d.request_id, reason);
			/* A node repeats its ACK until it hears one: answer this one (its own
			 * packet id, which the peer's ReliableRouter tracks), with no hops in
			 * reserve that would make it flood. */
			if (h.wantAck() && (h.hopsAway() == 0 || h.next_hop == relayByte(_self))) {
				sink.onAckForAck(h.from, h.id, snr_x4);
			}
		}
	}
}

size_t Bridge::buildFrame(const TxSpec &t, uint8_t *out, size_t cap)
{
	if (_self == 0 || cap < kHeaderLen) {
		return 0;
	}
	const Channel *ch = t.ch;
	const uint8_t *pki_key = t.pki_key;
	Data d = {};

	d.portnum = t.port;
	d.payload = t.payload;
	d.payload_len = t.payload_len;
	d.reply_id = t.reply_id;
	d.request_id = t.request_id;
	d.want_response = t.want_response;

	size_t limit = pki_key ? kMaxTxFrame : kMaxChannelFrame;
	size_t room = (cap < limit ? cap : limit) - kHeaderLen;

	if (pki_key) {
		room = room > kPkiOverhead ? room - kPkiOverhead : 0;
	}
	size_t n = encodeData(d, out + kHeaderLen, room);

	if (n == 0) {
		return 0;
	}
	Header h;

	h.to = t.to;
	h.from = _self;
	h.id = t.packet_id ? t.packet_id : nextId();
	const uint8_t hops = t.hops == kOwnHopLimit ? _hops : t.hops;

	h.flags = (uint8_t)(hops | (hops << 5) | (t.want_ack ? 0x08 : 0));
	h.channel = pki_key ? 0 : ch->hash;
	h.next_hop = 0;
	h.relay = relayByte(_self);
	writeHeader(h, out);

	if (pki_key) {
		size_t sealed = pkiSeal(*_pki, pki_key, _self, h.id, out + kHeaderLen, n,
					out + kHeaderLen, cap - kHeaderLen);

		return sealed ? kHeaderLen + sealed : 0;
	}
	if (ch->encrypted && !ctrCrypt(_aes, ch->key, _self, h.id, out + kHeaderLen, n)) {
		return 0;
	}
	return kHeaderLen + n;
}

size_t Bridge::buildText(uint8_t channel_id, uint32_t to, const char *text, size_t len,
			 uint8_t *out, size_t cap, uint32_t reply_id)
{
	const Channel *ch = find(channel_id);

	if (!ch || len == 0) {
		return 0;
	}
	TxSpec t;

	t.ch = ch;
	t.to = to;
	t.port = PORT_TEXT;
	t.payload = (const uint8_t *)text;
	t.payload_len = len;
	t.reply_id = reply_id;
	return buildFrame(t, out, cap);
}

size_t Bridge::buildNodeInfo(uint8_t channel_id, uint8_t *out, size_t cap)
{
	return buildNodeInfoTo(channel_id, kBroadcast, false, out, cap);
}

size_t Bridge::buildNodeInfoTo(uint8_t channel_id, uint32_t to, bool want_response, uint8_t *out,
			       size_t cap, uint8_t hop_limit)
{
	const Channel *ch = find(channel_id);
	uint8_t pb[104];

	if (!ch) {
		return 0;
	}
	size_t n = encodeUser(_user, pb, sizeof(pb));

	TxSpec t;

	t.ch = ch;
	t.to = to;
	t.port = PORT_NODEINFO;
	t.payload = pb;
	t.payload_len = n;
	t.want_response = want_response;
	t.hops = hop_limit;
	return n ? buildFrame(t, out, cap) : 0;
}

size_t Bridge::buildDirectText(uint32_t to, const uint8_t peer_pub[32], const char *text,
			       size_t len, uint8_t *out, size_t cap, uint32_t reply_id, bool want_ack,
			       uint32_t packet_id)
{
	uint8_t key[32];

	if (!_pki || len == 0 || !pkiSharedKey(*_pki, _pki_priv, peer_pub, key)) {
		return 0;
	}
	TxSpec t;

	t.pki_key = key;
	t.to = to;
	t.port = PORT_TEXT;
	t.payload = (const uint8_t *)text;
	t.payload_len = len;
	t.reply_id = reply_id;
	t.packet_id = packet_id;
	t.want_ack = want_ack;
	size_t n = buildFrame(t, out, cap);

	crypto_wipe(key, sizeof(key));
	return n;
}

size_t Bridge::buildAck(uint32_t to, uint32_t request_id, const uint8_t peer_pub[32], uint8_t *out,
			size_t cap, uint8_t hop_limit)
{
	/* Routing { error_reason = NONE }. */
	static const uint8_t kRoutingAck[] = {0x18, 0x00};
	uint8_t key[32];

	if (!_pki || !pkiSharedKey(*_pki, _pki_priv, peer_pub, key)) {
		return 0;
	}
	TxSpec t;

	t.pki_key = key;
	t.to = to;
	t.port = PORT_ROUTING;
	t.payload = kRoutingAck;
	t.payload_len = sizeof(kRoutingAck);
	t.request_id = request_id;
	t.hops = hop_limit;
	size_t n = buildFrame(t, out, cap);

	crypto_wipe(key, sizeof(key));
	return n;
}

size_t Bridge::buildPosition(uint8_t channel_id, const Position &pos, uint8_t *out, size_t cap)
{
	const Channel *ch = find(channel_id);
	uint8_t pb[32];

	if (!ch) {
		return 0;
	}
	size_t n = encodePosition(pos, pb, sizeof(pb));

	TxSpec t;

	t.ch = ch;
	t.port = PORT_POSITION;
	t.payload = pb;
	t.payload_len = n;
	return n ? buildFrame(t, out, cap) : 0;
}

size_t Bridge::buildTelemetry(uint8_t channel_id, const DeviceMetrics &m, uint8_t *out, size_t cap)
{
	const Channel *ch = find(channel_id);
	uint8_t pb[48];

	if (!ch) {
		return 0;
	}
	size_t n = encodeTelemetry(m, pb, sizeof(pb));

	TxSpec t;

	t.ch = ch;
	t.port = PORT_TELEMETRY;
	t.payload = pb;
	t.payload_len = n;
	return n ? buildFrame(t, out, cap) : 0;
}

} /* namespace mt */
