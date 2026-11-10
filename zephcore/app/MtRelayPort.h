/*
 * SPDX-License-Identifier: MIT
 * Zephtacore: what the companion and the repeater share of the Meshtastic
 * side, driven by the same mt. settings: hears a frame through the bridge,
 * repeats it with the timing of the role, lets a repeat give way to an echo,
 * and sends what we originate (NodeInfo, and what the role adds).
 */

#pragma once

#include <helpers/meshtastic/MtBridge.h>
#include <helpers/meshtastic/MtConfig.h>
#include <helpers/meshtastic/MtPolicy.h>
#include <mesh/Mesh.h>

class MtRelayPort {
public:
	struct Policy {
		bool enabled;    /* off: nothing is heard or repeated as Meshtastic */
		bool relay_all;  /* false: only what mt::RelayClass::Always names */
		bool yields;     /* a repeat that may give way to an echo does */
		bool early;      /* router timing: before every client */
	};
	struct Stats {
		uint32_t heard, relayed, opaque, yielded, dropped;
	};
	/* What a role adds to the NodeInfo: its position and its metrics, if it has them. */
	struct Source {
		virtual ~Source() {}
		virtual bool position(mt::Position &) { return false; }
		virtual bool metrics(mt::DeviceMetrics &) { return false; }
	};

	MtRelayPort(mesh::Mesh &mesh, mt::Bridge &bridge) : _mesh(mesh), _bridge(bridge) {}

	/* The bridge's AES-128 block function. */
	static void aesBlock(const uint8_t key[16], const uint8_t in[16], uint8_t out[16]);
	/* The same with an AES-256 key, for the PKI code. */
	static void aesBlock256(const uint8_t key[32], const uint8_t in[16], uint8_t out[16]);
	/* Our node number, names and packet id seed. */
	static void initSelf(mt::Bridge &bridge, const mesh::LocalIdentity &id, const char *name,
			     mesh::RNG &rng);

	/* The node was renamed. */
	static void renameSelf(mt::Bridge &bridge, const char *name);

	static Policy policyFor(const mt::RepeaterPrefs &p);

	/* Puts the settings into the bridge. Channels and keys are the role's. */
	void apply(const mt::RepeaterPrefs &p);
	/* First NodeInfo (and telemetry) soon after boot or after a change. */
	void arm(uint32_t now_ms, const mt::RepeaterPrefs &p);
	/* Sends what is due on this channel. */
	void tick(uint32_t now_ms, const mt::RepeaterPrefs &p, uint8_t channel, Source &src);
	/* The soonest deadline, false when nothing is planned. */
	bool nextAt(uint32_t &at_ms) const;

	/* RX_NOT_MESHTASTIC leaves the frame to MeshCore. sf and bw set the slot. */
	mt::RxResult onFrame(const uint8_t raw[], int len, float snr, mt::Sink &sink,
			     const Policy &policy, uint8_t sf, float bw);

	/* A frame the Meshtastic side did not take: RX_NOT_MESHTASTIC when it is shaped like
	 * MeshCore, RX_IGNORED otherwise. Only the first reaches the MeshCore stack. */
	static mt::RxResult unopened(const uint8_t raw[], int len, uint32_t self);

	/* A frame we originate, out at once. */
	bool send(const uint8_t *frame, size_t len);

	const Stats &stats() const { return _stats; }
	/* "heard=.. relayed=.." for get z.mt.stats. */
	void formatStats(char *out, size_t cap) const;

private:
	static constexpr int kYieldKeep = 8;

	mesh::Mesh &_mesh;
	mt::Bridge &_bridge;
	Stats _stats = {};
	uint32_t _yield[kYieldKeep] = {};  /* hashes of queued repeats that give way to an echo */
	uint8_t _yield_next = 0;
	mt::Timer _nodeinfo, _telemetry;

	bool yields(uint32_t hash) const;
	void echoHeard(const uint8_t raw[], int len, bool yields);
	void queue(const uint8_t *relay, size_t len, int8_t snr_x4, mt::RelayClass cls,
		   const Policy &policy, uint8_t sf, float bw);
};
