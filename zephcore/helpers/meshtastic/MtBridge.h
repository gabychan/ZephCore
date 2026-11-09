/*
 * SPDX-License-Identifier: MIT
 * Meshtastic side of Zephtacore: tells a received frame apart from a MeshCore
 * one by decrypting it on our channels, delivers text, node names and
 * positions to a sink, decides what to repeat, and builds the frames we send.
 * Pure code (the AES block function is injected), host tested.
 */

#pragma once

#include "MtWire.h"

namespace mt {

static constexpr int kMaxChannels = 8;
static constexpr int kDedupSize = 64;
/* Largest frame we queue: MeshCore's payload limit (MAX_PACKET_PAYLOAD). */
static constexpr size_t kMaxTxFrame = 184;

/* Meshtastic's contention slot on sub-GHz LoRa (RadioInterface::computeSlotTimeMsec):
 * a 2-symbol CAD plus half a symbol, plus 7.6 ms of propagation, TX/RX
 * turnaround and processing. */
uint32_t slotTimeMs(uint8_t sf, float bw_khz);

/* Contention window exponent of a rebroadcast (RadioInterface::getCWsize): the
 * SNR it was heard at, -20..+10 dB, maps to 3..8, clamped. A faint frame gets a
 * small window, so the node farthest from the sender repeats first. */
uint8_t rebroadcastCw(int8_t snr_x4);

/* Rebroadcast delay of a client node (RadioInterface::getTxDelayMsecWeighted):
 * 2 x 8 slots left to routers, then slot r of the window, r < 2^cw. */
uint32_t rebroadcastDelayMs(uint32_t slot_ms, uint32_t r);

class Sink {
public:
	virtual ~Sink() {}
	/* text is not NUL terminated. hops is 0 when the sender did not set hop_start. */
	virtual void onText(uint8_t channel_id, uint32_t from, uint32_t to, uint32_t id,
			    uint32_t reply_id, const char *text, size_t len, int8_t snr_x4,
			    uint8_t hops) = 0;
	virtual void onNodeInfo(uint32_t from, const User &user) = 0;
	virtual void onPosition(uint32_t from, const Position &pos) = 0;
};

enum RxResult {
	RX_NOT_MESHTASTIC,  /* leave the frame to MeshCore */
	RX_HANDLED,         /* decrypted on one of our channels */
	RX_DUPLICATE,       /* seen before; nothing to deliver */
};

class Bridge {
public:
	explicit Bridge(AesBlockFn aes) : _aes(aes) {}

	/* Our node number (never 0, below 4 or broadcast), names and the seed of
	 * the packet id sequence. */
	void setSelf(uint32_t node, const char *long_name, const char *short_name, uint32_t seed);
	uint32_t self() const { return _self; }

	/* channel_id is the caller's own handle (the companion channel index).
	 * False when the table is full or the PSK length is not 1 or 16. */
	bool setChannel(uint8_t channel_id, const char *name, const uint8_t *psk, size_t psk_len);
	void clearChannels();
	int channelCount() const;
	bool hasChannel(uint8_t channel_id) const;

	void setRelay(bool on) { _relay = on; }
	/* hop_limit and hop_start of the frames we originate, 0 to 7. */
	void setHopLimit(uint8_t hops) { _hops = hops > 7 ? 7 : hops; }

	/* relay_out (kMaxFrame bytes) receives the frame to repeat; *relay_len is
	 * its length, 0 when nothing is to be repeated. */
	RxResult handleFrame(const uint8_t *raw, size_t len, int8_t snr_x4, Sink &sink,
			     uint8_t *relay_out, size_t *relay_len);

	/* Frame builders: bytes written to out, 0 on failure (unknown channel,
	 * text too long for kMaxTxFrame, cap too small). to is a node number or
	 * kBroadcast. */
	size_t buildText(uint8_t channel_id, uint32_t to, const char *text, size_t len,
			 uint8_t *out, size_t cap, uint32_t reply_id = 0);
	size_t buildNodeInfo(uint8_t channel_id, uint8_t *out, size_t cap);
	size_t buildPosition(uint8_t channel_id, const Position &pos, uint8_t *out, size_t cap);

private:
	struct Channel {
		bool used;
		bool encrypted;
		uint8_t id;
		uint8_t hash;
		uint8_t key[16];
	};
	struct Seen {
		uint32_t from;
		uint32_t id;
	};

	AesBlockFn _aes;
	Channel _ch[kMaxChannels] = {};
	Seen _seen[kDedupSize] = {};
	uint8_t _seen_next = 0;
	uint32_t _self = 0;
	uint32_t _id_prefix = 0;
	uint32_t _id_counter = 0;
	bool _relay = false;
	uint8_t _hops = 3;  /* Meshtastic's own default */
	User _user = {};

	const Channel *find(uint8_t channel_id) const;
	bool seen(uint32_t from, uint32_t id) const;
	void remember(uint32_t from, uint32_t id);
	uint32_t nextId();
	size_t buildFrame(const Channel &ch, uint32_t to, uint32_t port, const uint8_t *payload,
			  size_t payload_len, uint8_t *out, size_t cap, uint32_t reply_id = 0);
};

} /* namespace mt */
