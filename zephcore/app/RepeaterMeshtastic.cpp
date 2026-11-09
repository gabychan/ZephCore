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

/* Pushes our settings into the bridge; also the first start. */
void RepeaterMesh::mtApplyPrefs()
{
	const mt::RepeaterPrefs &p = _prefs.mt;

	if (!_mt_ready) {
		MtRelayPort::initSelf(_mt, self_id, _prefs.node_name, *getRNG());
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

bool RepeaterMesh::onRecvForeign(const uint8_t raw[], int len, float snr, float rssi)
{
	(void)rssi;
	const mt::RepeaterPrefs &p = _prefs.mt;

	if (!p.enable) {
		/* Off: what is not shaped like MeshCore is not ours to repeat as MeshCore either. */
		if (mt::looksLikeMeshCore(raw, (size_t)len, mt::nodeFromPublicKey(self_id.pub_key))) {
			return false;
		}
		_mt_dropped++;
		return true;
	}
	const MtRelayPort::Policy policy = {p.relay == mt::RELAY_ALL, mt::roleYieldsToEcho(p.role),
					    mt::roleSendsEarly(p.role)};

	return _mtport.onFrame(raw, len, snr, _mt_sink, policy, _prefs.sf, _prefs.bw) !=
	       mt::RX_NOT_MESHTASTIC;
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

	if (n > 0) {
		_mtport.send(frame, n);
	}
	/* The position MeshCore adverts carry: the same coordinates, under the same policy. */
	if (_prefs.advert_loc_policy == ADVERT_LOC_NONE || (_prefs.node_lat == 0 && _prefs.node_lon == 0)) {
		return;
	}
	mt::Position pos = {};
	const uint32_t now_s = getRTCClock()->getCurrentTime();

	pos.has_lat = pos.has_lon = true;
	pos.lat_i = (int32_t)(_prefs.node_lat * 1e7);
	pos.lon_i = (int32_t)(_prefs.node_lon * 1e7);
	pos.time = now_s > kClockSetAfter ? now_s : 0;
	n = _mt.buildPosition(kPrimaryChannel, pos, frame, sizeof(frame));
	if (n > 0) {
		_mtport.send(frame, n);
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

	if (n > 0) {
		_mtport.send(frame, n);
	}
}

/* "get mt.stats", and the settings of mt::handleCommand. */
bool RepeaterMesh::mtHandleCommand(char *command, char *reply)
{
	if (strcmp(command, "get mt.stats") == 0) {
		const MtRelayPort::Stats &st = _mtport.stats();

		snprintf(reply, 160, "> heard=%u relayed=%u opaque=%u yielded=%u dropped=%u",
			 (unsigned)st.heard, (unsigned)st.relayed, (unsigned)st.opaque,
			 (unsigned)st.yielded, (unsigned)_mt_dropped);
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
