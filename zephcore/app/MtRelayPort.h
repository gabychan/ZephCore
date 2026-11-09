/*
 * SPDX-License-Identifier: MIT
 * Zephtacore: what the companion and the repeater share of the Meshtastic
 * side. Hears a frame through the bridge, repeats it with the timing of the
 * role, lets a repeat give way to an echo, and queues what we originate.
 */

#pragma once

#include <helpers/meshtastic/MtBridge.h>
#include <mesh/Mesh.h>

class MtRelayPort {
public:
	struct Policy {
		bool relay_all;  /* false: only what mt::RelayClass::Always names */
		bool yields;     /* a repeat that may give way to an echo does */
		bool early;      /* router timing: before every client */
	};
	struct Stats {
		uint32_t heard, relayed, opaque, yielded;
	};

	MtRelayPort(mesh::Mesh &mesh, mt::Bridge &bridge) : _mesh(mesh), _bridge(bridge) {}

	/* The bridge's AES-128 block function. */
	static void aesBlock(const uint8_t key[16], const uint8_t in[16], uint8_t out[16]);
	/* Our node number, names and packet id seed. */
	static void initSelf(mt::Bridge &bridge, const mesh::LocalIdentity &id, const char *name,
			     mesh::RNG &rng);

	/* RX_NOT_MESHTASTIC leaves the frame to MeshCore. sf and bw set the slot. */
	mt::RxResult onFrame(const uint8_t raw[], int len, float snr, mt::Sink &sink,
			     const Policy &policy, uint8_t sf, float bw);

	/* A frame we originate, out at once. */
	bool send(const uint8_t *frame, size_t len);

	const Stats &stats() const { return _stats; }

private:
	static constexpr int kYieldKeep = 8;

	mesh::Mesh &_mesh;
	mt::Bridge &_bridge;
	Stats _stats = {};
	uint32_t _yield[kYieldKeep] = {};  /* hashes of queued repeats that give way to an echo */
	uint8_t _yield_next = 0;

	bool yields(uint32_t hash) const;
	void echoHeard(const uint8_t raw[], int len, bool yields);
	void queue(const uint8_t *relay, size_t len, int8_t snr_x4, mt::RelayClass cls,
		   const Policy &policy, uint8_t sf, float bw);
};
