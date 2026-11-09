/*
 * SPDX-License-Identifier: MIT
 * Zephtacore: the repeater's Meshtastic side. It repeats Meshtastic floods on
 * the radio it shares with MeshCore, announces itself with a NodeInfo on its
 * primary channel, and answers the "mt." CLI commands. Nothing here enters the
 * MeshCore routing path.
 */

#include "RepeaterMesh.h"

#if IS_ENABLED(CONFIG_ZEPHTACORE_MESHTASTIC)

#include <helpers/meshtastic/MtConfig.h>
#include <helpers/meshtastic/MtDiscriminate.h>
#include <mesh/Utils.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zephcore_repeater, CONFIG_ZEPHCORE_MAIN_LOG_LEVEL);

/* First NodeInfo this long after boot, then every mt.nodeinfo seconds. */
static constexpr uint32_t kFirstNodeInfoMs = 30000;
static constexpr uint8_t kPrimaryChannel = 0;

void RepeaterMesh::mtAesBlock(const uint8_t key[16], const uint8_t in[16], uint8_t out[16])
{
	mesh::Utils::encrypt(key, out, in, 16);
}

/* Pushes our settings into the bridge; also the first start. */
void RepeaterMesh::mtApplyPrefs()
{
	const mt::RepeaterPrefs &p = _prefs.mt;
	char short_name[5];
	uint32_t seed = 0;

	if (!_mt_ready) {
		getRNG()->random((uint8_t *)&seed, sizeof(seed));
		mt::shortName(_prefs.node_name, short_name);
		_mt.setSelf(mt::nodeFromPublicKey(self_id.pub_key), _prefs.node_name, short_name, seed);
		_mt_ready = true;
	}
	_mt.setRole(p.role);
	_mt.setHopLimit(p.hops);
	_mt.setRelay(p.enable && p.relay != mt::RELAY_OFF);
	_mt.setRelayOpaque(p.relay_opaque != 0);
	_mt.clearChannels();
	_mt.setChannel(kPrimaryChannel, p.channel, p.psk, p.psk_len);
	_mt_next_nodeinfo = (p.enable && p.nodeinfo_s) ? futureMillis(kFirstNodeInfoMs) : 0;
}

bool RepeaterMesh::mtYields(uint32_t hash) const
{
	for (uint32_t h : _mt_yield) {
		if (h == hash) {
			return true;
		}
	}
	return false;
}

/* Queues the repeat of a heard frame with the delay of our role. A frame that
 * gives way to an echo is remembered by hash. */
void RepeaterMesh::mtQueueRelay(const uint8_t *relay, size_t len, int8_t snr_x4, mt::RelayClass cls)
{
	const mt::RepeaterPrefs &p = _prefs.mt;
	const uint32_t slot = mt::slotTimeMs(_prefs.sf, _prefs.bw);
	const uint8_t cw = mt::rebroadcastCw(snr_x4);
	uint32_t delay;

	if (len > mt::kMaxTxFrame) {
		return;
	}
	if (mt::roleSendsEarly(p.role)) {
		delay = mt::earlyDelayMs(slot, (uint32_t)getRNG()->nextInt(0, 2 * cw));
	} else {
		delay = mt::rebroadcastDelayMs(slot, (uint32_t)getRNG()->nextInt(0, 1 << cw));
	}
	if (!relayForeign(relay, len, delay)) {
		LOG_WRN("mt: relay not queued (len=%u)", (unsigned)len);
		return;
	}
	_mt_stats.relayed++;
	if (cls == mt::RelayClass::CancelOnEcho && mt::roleYieldsToEcho(p.role)) {
		_mt_yield[_mt_yield_next] = mesh::ContentionTracker::computeForeignHash32(relay, (int)len);
		_mt_yield_next = (uint8_t)((_mt_yield_next + 1) % MT_YIELD_KEEP);
	}
}

bool RepeaterMesh::onRecvForeign(const uint8_t raw[], int len, float snr, float rssi)
{
	(void)rssi;
	const mt::RepeaterPrefs &p = _prefs.mt;

	if (!p.enable) {
		/* Off: what is not shaped like MeshCore is not ours to repeat as MeshCore either. */
		if (mt::looksLikeMeshCore(raw, (size_t)len, mt::nodeFromPublicKey(self_id.pub_key))) {
			return false;
		}
		_mt_stats.dropped++;
		return true;
	}

	uint8_t relay[mt::kMaxFrame];
	size_t relay_len = 0;
	mt::RelayClass cls;
	const int8_t snr_x4 = (int8_t)(snr * 4.0f);
	mt::RxResult r = _mt.handleFrame(raw, (size_t)len, snr_x4, _mt_sink, relay, &relay_len, &cls);

	if (r == mt::RX_NOT_MESHTASTIC) {
		return false;
	}
	_mt_stats.heard++;
	if (r == mt::RX_OPAQUE) {
		_mt_stats.opaque++;
	}
	if (r == mt::RX_DUPLICATE) {
		/* Another node repeated a frame we hold: a frame that yields is dropped,
		 * the others move out of its way. */
		uint32_t hash = mesh::ContentionTracker::computeForeignHash32(raw, len);

		if (mtYields(hash) && cancelForeignRelay(hash)) {
			_mt_stats.yielded++;
		} else {
			foreignDupeHeard(raw, len);
		}
		return true;
	}
	if (relay_len > 0 && (p.relay == mt::RELAY_ALL || cls == mt::RelayClass::Always)) {
		mtQueueRelay(relay, relay_len, snr_x4, cls);
	}
	return true;
}

/* Our NodeInfo on the primary channel, as Meshtastic's own default does. */
void RepeaterMesh::mtNodeInfoTick()
{
	const mt::RepeaterPrefs &p = _prefs.mt;

	if (!_mt_next_nodeinfo || !millisHasNowPassed(_mt_next_nodeinfo)) {
		return;
	}
	_mt_next_nodeinfo = (p.enable && p.nodeinfo_s) ? futureMillis(p.nodeinfo_s * 1000u) : 0;

	uint8_t frame[mt::kMaxTxFrame];
	size_t n = _mt.buildNodeInfo(kPrimaryChannel, frame, sizeof(frame));

	if (n > 0 && !sendForeign(frame, (int)n, 1, 0)) {
		LOG_WRN("mt: NodeInfo not queued");
	}
}

/* "get mt.stats", and the settings of mt::handleCommand. */
bool RepeaterMesh::mtHandleCommand(char *command, char *reply)
{
	if (strcmp(command, "get mt.stats") == 0) {
		snprintf(reply, 160, "> heard=%u relayed=%u opaque=%u yielded=%u dropped=%u",
			 (unsigned)_mt_stats.heard, (unsigned)_mt_stats.relayed,
			 (unsigned)_mt_stats.opaque, (unsigned)_mt_stats.yielded,
			 (unsigned)_mt_stats.dropped);
		return true;
	}
	mt::CmdResult r = mt::handleCommand(_prefs.mt, command, reply, 160);

	if (r.changed) {
		mt::sanitize(_prefs.mt);
		mtApplyPrefs();
		savePrefs();
	}
	return r.handled;
}

#endif /* CONFIG_ZEPHTACORE_MESHTASTIC */
