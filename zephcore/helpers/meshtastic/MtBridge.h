/*
 * SPDX-License-Identifier: MIT
 * Meshtastic side of Zephtacore: tells a received frame apart from a MeshCore
 * one by decrypting it on our channels, delivers text, node names and
 * positions to a sink, decides what to repeat, and builds the frames we send.
 * Pure code (the AES block function is injected), host tested.
 */

#pragma once

#include "MtConfig.h"
#include "MtPki.h"
#include "MtPolicy.h"
#include "MtWire.h"

namespace mt {

static constexpr int kMaxChannels = 8;
static constexpr int kDedupSize = 64;
/* Largest frame we build: a frame on the air can be MAX_TRANS_UNIT (255) bytes. What does not
 * fit MeshCore's 184-byte payload waits in a foreign tail slot until its frame is sent. */
static constexpr size_t kMaxTxFrame = 255;
/* Largest frame on a channel: it stays within MeshCore's payload, MAX_PACKET_PAYLOAD, so
 * only a direct text needs the foreign tail. */
static constexpr size_t kMaxChannelFrame = 184;
static constexpr uint8_t kNoChannel = 0xFF;
/* hop_limit of a frame we start on our own account: the configured one. */
static constexpr uint8_t kOwnHopLimit = 0xFF;

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

/* Rebroadcast delay of a router, which goes before every client: slot r of
 * 2 x cw (RadioInterface::getTxDelayMsecWeighted, router branch), r < 2 * cw. */
uint32_t earlyDelayMs(uint32_t slot_ms, uint32_t r);

/* A frame of our own waits slot r of 2^3, the smallest contention window
 * (RadioInterface::getTxDelayMsec). */
uint32_t originWindow();
uint32_t originDelayMs(uint32_t slot_ms, uint32_t r);

/* Hop limit of the frame that answers one which crossed hops_away hops: two more than it
 * used, and never more than our own (RoutingModule::getHopLimitForResponse). */
uint8_t responseHopLimit(uint8_t hops_away, uint8_t own);

/* Slots to draw r from: 2 x cw for a router (early), 2^cw for a client. */
uint32_t relayWindow(int8_t snr_x4, bool early);

/* Slots window w draws from, and the delay of slot r of them. LateHeard goes after the
 * worst case of the client window, by another draw of it (clampToLateRebroadcastWindow). */
uint32_t windowSlots(TxWindow w, int8_t snr_x4);
uint32_t windowDelayMs(TxWindow w, int8_t snr_x4, uint32_t slot_ms, uint32_t r);

class Sink {
public:
	virtual ~Sink() {}
	/* text is not NUL terminated. hops is 0 when the sender did not set hop_start.
	 * relay: last byte of the node number that sent us the frame (the sender itself when hops is 0). */
	virtual void onText(uint8_t channel_id, uint32_t from, uint32_t to, uint32_t id,
			    uint32_t reply_id, const char *text, size_t len, int8_t snr_x4,
			    uint8_t hops, uint8_t relay) = 0;
	virtual void onNodeInfo(uint32_t from, const User &user) = 0;
	virtual void onPosition(uint32_t from, const Position &pos) = 0;
	/* A PKI direct message for us. want_ack: the sender asks for an ACK. */
	virtual void onDirectText(uint32_t from, uint32_t id, uint32_t reply_id, bool want_ack,
				  const char *text, size_t len, int8_t snr_x4, uint8_t hops)
	{
		(void)from; (void)id; (void)reply_id; (void)want_ack;
		(void)text; (void)len; (void)snr_x4; (void)hops;
	}
	/* A frame for us that we already took came again, heard straight from its sender and
	 * asking for an ACK: answer its ACK again. Nothing is delivered twice. */
	virtual void onDirectRepeat(uint32_t from, uint32_t id, int8_t snr_x4, uint8_t hops)
	{
		(void)from; (void)id; (void)snr_x4; (void)hops;
	}
	/* An ACK (error_reason 0) or NAK for a packet of ours, from a node. */
	virtual void onAck(uint32_t from, uint32_t request_id, uint32_t error_reason)
	{
		(void)from; (void)request_id; (void)error_reason;
	}
	/* An ACK for one of our packets that asks for an ACK of its own: the node repeats it
	 * until it hears one, so a hop limit 0 ACK ends that (ReliableRouter). */
	virtual void onAckForAck(uint32_t from, uint32_t request_id, int8_t snr_x4)
	{
		(void)from; (void)request_id; (void)snr_x4;
	}
	/* A node sent us its NodeInfo asking for ours. channel_id is the channel it
	 * came on, kNoChannel when it came as PKI. snr_x4 and hops are the request's. */
	virtual void onNodeInfoRequest(uint8_t channel_id, uint32_t from, int8_t snr_x4, uint8_t hops)
	{
		(void)channel_id; (void)from; (void)snr_x4; (void)hops;
	}
	/* A PKI message for us from a node whose key we do not hold. */
	virtual void onUnknownKey(uint32_t from) { (void)from; }
	/* A PKI message for us from a node whose key we hold did not open: it may
	 * have a new key. */
	virtual void onPkiFailed(uint32_t from) { (void)from; }
	/* X25519 public key of a node, false when unknown. */
	virtual bool peerKey(uint32_t node, uint8_t key[32])
	{
		(void)node; (void)key;
		return false;
	}
};

enum RxResult {
	RX_NOT_MESHTASTIC,  /* leave the frame to MeshCore */
	RX_HANDLED,         /* decrypted on one of our channels */
	RX_DUPLICATE,       /* seen before; nothing to deliver */
	RX_OPAQUE,          /* Meshtastic on a channel we hold no key for; only relayed */
	RX_IGNORED,         /* not for MeshCore either, and Meshtastic is off */
	RX_DROPPED,         /* Meshtastic, new, and the screen drops it: nothing is delivered or repeated */
};

/* Decides, for a frame the bridge accepted as new, whether it is dropped. portnum is 0 for a
 * frame no channel of ours opens. */
class Screen {
public:
	virtual ~Screen() {}
	virtual bool drops(const Header &h, uint32_t portnum) = 0;
};

class Bridge {
public:
	explicit Bridge(AesBlockFn aes) : _aes(aes) {}

	/* Our node number (never 0, below 4 or broadcast), names and the seed of
	 * the packet id sequence. */
	void setSelf(uint32_t node, const char *long_name, const char *short_name, uint32_t seed);
	uint32_t self() const { return _self; }
	/* The names in our NodeInfo; the node number and the rest stay. */
	void setNames(const char *long_name, const char *short_name);
	/* Our role in the NodeInfo we send (Config.DeviceConfig.Role). */
	void setRole(uint8_t role) { _user.role = role; }

	/* channel_id is the caller's own handle (the companion channel index).
	 * False when the table is full or the PSK length is not 1 or 16. */
	bool setChannel(uint8_t channel_id, const char *name, const uint8_t *psk, size_t psk_len);
	void clearChannels();
	int channelCount() const;
	bool hasChannel(uint8_t channel_id) const;

	/* Enables PKI direct messages. seed: 32 bytes of the node identity. crypto
	 * must outlive the bridge. Our public key then goes out in our NodeInfo. */
	void setPki(const PkiCrypto *crypto, const uint8_t seed[32]);
	bool hasPki() const { return _pki != nullptr; }

	void setScreen(Screen *screen) { _screen = screen; }
	void setRelay(bool on) { _relay = on; }
	/* Also repeat frames no channel of ours opens (needs setRelay). */
	void setRelayOpaque(bool on) { _relay_opaque = on; }
	/* hop_limit and hop_start of the frames we originate, 0 to 7. */
	void setHopLimit(uint8_t hops) { _hops = hops > 7 ? 7 : hops; }
	uint8_t hopLimit() const { return _hops; }

	/* relay_out (kMaxFrame bytes) receives the frame to repeat; *relay_len is
	 * its length, 0 when nothing is to be repeated. *relay_class, when given,
	 * says how that repeat is treated once another node repeats it first. */
	RxResult handleFrame(const uint8_t *raw, size_t len, int8_t snr_x4, Sink &sink,
			     uint8_t *relay_out, size_t *relay_len,
			     RelayClass *relay_class = nullptr);

	/* Frame builders: bytes written to out, 0 on failure (unknown channel,
	 * text too long for kMaxTxFrame, cap too small). to is a node number or
	 * kBroadcast. */
	size_t buildText(uint8_t channel_id, uint32_t to, const char *text, size_t len,
			 uint8_t *out, size_t cap, uint32_t reply_id = 0);
	size_t buildNodeInfo(uint8_t channel_id, uint8_t *out, size_t cap);
	size_t buildPosition(uint8_t channel_id, const Position &pos, uint8_t *out, size_t cap);
	size_t buildTelemetry(uint8_t channel_id, const DeviceMetrics &m, uint8_t *out, size_t cap);
	/* Our NodeInfo to one node, asking for theirs when want_response. hop_limit is the
	 * header's; kOwnHopLimit (the default) keeps the configured one. */
	size_t buildNodeInfoTo(uint8_t channel_id, uint32_t to, bool want_response, uint8_t *out,
			       size_t cap, uint8_t hop_limit = kOwnHopLimit);
	/* PKI frames to a node whose public key is peer_pub. 0 without PKI or on a weak key.
	 * packet_id 0 takes a new id; a retry passes the id of the first try, which the node's
	 * ACK answers and which a node that already has the frame drops as a duplicate. */
	size_t buildDirectText(uint32_t to, const uint8_t peer_pub[32], const char *text, size_t len,
			       uint8_t *out, size_t cap, uint32_t reply_id = 0, bool want_ack = false,
			       uint32_t packet_id = 0);
	size_t buildAck(uint32_t to, uint32_t request_id, const uint8_t peer_pub[32], uint8_t *out,
			size_t cap, uint8_t hop_limit = kOwnHopLimit);

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
		bool relayed;
	};

	AesBlockFn _aes;
	Channel _ch[kMaxChannels] = {};
	Seen _seen[kDedupSize] = {};
	uint8_t _seen_next = 0;
	uint32_t _self = 0;
	uint32_t _id_prefix = 0;
	uint32_t _id_counter = 0;
	bool _relay = false;
	bool _relay_opaque = false;
	Screen *_screen = nullptr;
	uint8_t _hops = 3;  /* Meshtastic's own default */
	User _user = {};
	const PkiCrypto *_pki = nullptr;
	uint8_t _pki_priv[32] = {};

	const Channel *find(uint8_t channel_id) const;
	Seen *findSeen(uint32_t from, uint32_t id);
	Seen *remember(uint32_t from, uint32_t id);
	/* True when the frame was heard before (or is ours). Its repeat is then
	 * still filled in when we were not asked to the first time, or when the
	 * originator sends it again. */
	bool heardBefore(const Header &h, const uint8_t *raw, size_t len, uint8_t *relay_out,
			 size_t *relay_len);
	/* We repeat a flood when it has hops left, is not ours or for us alone, and
	 * names no next hop or names us. */
	bool eligible(const Header &h) const;
	uint32_t nextId();
	/* Floods repeat once: next_hop 0, hops left, not for us alone. */
	bool fillRelay(const Header &h, const uint8_t *raw, size_t len, uint8_t *relay_out,
		       size_t *relay_len) const;
	/* What a frame carries and how it goes out. ch carries it on a channel; pki_key (32
	 * bytes) instead seals it for one node and sets channel 0. packet_id 0 takes a new id. */
	struct TxSpec {
		const Channel *ch = nullptr;
		const uint8_t *pki_key = nullptr;
		uint32_t to = kBroadcast;
		uint32_t port = 0;
		const uint8_t *payload = nullptr;
		size_t payload_len = 0;
		uint32_t reply_id = 0;
		uint32_t request_id = 0;
		uint32_t packet_id = 0;
		bool want_response = false;
		bool want_ack = false;
		uint8_t hops = kOwnHopLimit;  /* hop_limit and hop_start of this frame */
	};
	size_t buildFrame(const TxSpec &t, uint8_t *out, size_t cap);
	void deliverData(const Header &h, const Data &d, uint8_t channel_id, int8_t snr_x4, Sink &sink);
	bool openPki(const Header &h, const uint8_t *raw, size_t plen, Sink &sink, uint8_t *buf,
		     Data &d);
	void deliverPki(const Header &h, const Data &d, int8_t snr_x4, Sink &sink);
	/* A copy of a frame we already took, heard straight from its sender and asking for an
	 * ACK: ask the sink to answer it again. Only a copy that opens counts. */
	void repeatAck(const Header &h, const uint8_t *raw, size_t plen, int8_t snr_x4, Sink &sink,
		       uint8_t *buf, Data &d);
};

} /* namespace mt */
