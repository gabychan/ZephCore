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
#include <helpers/battery_curve.h>
#include <mesh/Utils.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zephcore_repeater, CONFIG_ZEPHCORE_MAIN_LOG_LEVEL);

/* First NodeInfo this long after boot, then every mt.nodeinfo seconds. */
static constexpr uint32_t kFirstNodeInfoMs = 30000;
static constexpr uint32_t kFirstTelemetryMs = 60000;
static constexpr uint8_t kPrimaryChannel = 0;
/* Unix time of 2023-11-14: an earlier clock was never set. */
static constexpr uint32_t kClockSetAfter = 1700000000u;

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
	_mt_next_telemetry = (p.enable && p.telemetry_s) ? futureMillis(kFirstTelemetryMs) : 0;
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
	if (r == mt::RX_DUPLICATE && relay_len == 0) {
		/* Heard again and nothing to repeat. From a repeater, while we hold the frame: a
		 * frame that yields is dropped unless that repeater named us next hop, the others
		 * move out of its way. The originator sending it again leaves our copy alone. */
		mt::Header h;
		uint32_t hash = mesh::ContentionTracker::computeForeignHash32(raw, len);

		if (!mt::parseHeader(raw, (size_t)len, h) ||
		    (h.hopStart() > 0 && h.hopStart() == h.hopLimit())) {
			return true;
		}
		const bool asked = h.next_hop != 0 && h.next_hop == (uint8_t)_mt.self();

		if (!asked && mtYields(hash) && cancelForeignRelay(hash)) {
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

/* Our NodeInfo on the primary channel, as Meshtastic's own default does, and our
 * fixed position right after it when asked. */
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
	if (!p.position || (_prefs.node_lat == 0 && _prefs.node_lon == 0)) {
		return;
	}
	mt::Position pos = {};
	const uint32_t now_s = getRTCClock()->getCurrentTime();

	pos.has_lat = pos.has_lon = true;
	pos.lat_i = (int32_t)(_prefs.node_lat * 1e7);
	pos.lon_i = (int32_t)(_prefs.node_lon * 1e7);
	pos.time = now_s > kClockSetAfter ? now_s : 0;
	n = _mt.buildPosition(kPrimaryChannel, pos, frame, sizeof(frame));
	if (n > 0 && !sendForeign(frame, (int)n, 1, 0)) {
		LOG_WRN("mt: position not queued");
	}
}

/* Device metrics on the primary channel. Channel utilisation is the share of
 * the time since the last one that the radio spent receiving or sending. */
void RepeaterMesh::mtTelemetryTick()
{
	const mt::RepeaterPrefs &p = _prefs.mt;

	if (!_mt_next_telemetry || !millisHasNowPassed(_mt_next_telemetry)) {
		return;
	}
	_mt_next_telemetry = (p.enable && p.telemetry_s) ? futureMillis(p.telemetry_s * 1000u) : 0;

	const uint32_t now = (uint32_t)_ms->getMillis();
	const uint32_t tx = (uint32_t)getTotalAirTime();
	const uint32_t rx = (uint32_t)getReceiveAirTime();
	const uint32_t span = now - _mt_tele_at;
	mt::DeviceMetrics m = {};
	const uint16_t mv = _board.getBattMilliVolts();

	if (span > 0) {
		m.channel_utilization = 100.0f * (float)((tx - _mt_tele_tx) + (rx - _mt_tele_rx)) / (float)span;
		m.air_util_tx = 100.0f * (float)(tx - _mt_tele_tx) / (float)span;
		if (m.channel_utilization > 100.0f) {
			m.channel_utilization = 100.0f;
		}
	}
	_mt_tele_at = now;
	_mt_tele_tx = tx;
	_mt_tele_rx = rx;
	/* No battery reading: powered, which Meshtastic says as a level above 100. */
	m.battery_level = mv ? battery_curve_lookup(&battery_curve_default, mv) : 101;
	m.has_voltage = mv != 0;
	m.voltage = (float)mv / 1000.0f;
	m.uptime_s = (uint32_t)(k_uptime_get() / 1000);
	const uint32_t now_s = getRTCClock()->getCurrentTime();
	m.time = now_s > kClockSetAfter ? now_s : 0;

	uint8_t frame[mt::kMaxTxFrame];
	size_t n = _mt.buildTelemetry(kPrimaryChannel, m, frame, sizeof(frame));

	if (n > 0 && !sendForeign(frame, (int)n, 1, 0)) {
		LOG_WRN("mt: telemetry not queued");
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
