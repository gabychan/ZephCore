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
#include <helpers/battery_curve.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zephcore_repeater, CONFIG_ZEPHCORE_MAIN_LOG_LEVEL);

static constexpr uint8_t kPrimaryChannel = 0;
/* Unix time of 2023-11-14: an earlier clock was never set. */
static constexpr uint32_t kClockSetAfter = 1700000000u;

/* The position MeshCore adverts carry, the battery and the radio's utilisation. */
bool RepeaterMesh::MtSource::position(mt::Position &pos)
{
	const NodePrefs &p = m._prefs;

	if (p.advert_loc_policy == ADVERT_LOC_NONE || (p.node_lat == 0 && p.node_lon == 0)) {
		return false;
	}
	const uint32_t now_s = m.getRTCClock()->getCurrentTime();

	pos.has_lat = pos.has_lon = true;
	pos.lat_i = (int32_t)(p.node_lat * 1e7);
	pos.lon_i = (int32_t)(p.node_lon * 1e7);
	pos.time = now_s > kClockSetAfter ? now_s : 0;
	return true;
}

/* Channel utilisation is the share of the time since the last report that the
 * radio spent receiving or sending. */
bool RepeaterMesh::MtSource::metrics(mt::DeviceMetrics &d)
{
	const uint32_t now = (uint32_t)m._ms->getMillis();
	const uint32_t tx = (uint32_t)m.getTotalAirTime();
	const uint32_t rx = (uint32_t)m.getReceiveAirTime();
	const uint32_t span = now - at;
	const uint16_t mv = m._board.getBattMilliVolts();

	if (span > 0) {
		d.channel_utilization = 100.0f * (float)((tx - last_tx) + (rx - last_rx)) / (float)span;
		d.air_util_tx = 100.0f * (float)(tx - last_tx) / (float)span;
		if (d.channel_utilization > 100.0f) {
			d.channel_utilization = 100.0f;
		}
	}
	at = now;
	last_tx = tx;
	last_rx = rx;
	/* No battery reading: powered, which Meshtastic says as a level above 100. */
	d.battery_level = mv ? battery_curve_lookup(&battery_curve_default, mv) : 101;
	d.has_voltage = mv != 0;
	d.voltage = (float)mv / 1000.0f;
	d.uptime_s = (uint32_t)(k_uptime_get() / 1000);
	const uint32_t now_s = m.getRTCClock()->getCurrentTime();

	d.time = now_s > kClockSetAfter ? now_s : 0;
	return true;
}

/* Pushes our settings into the bridge; also the first start. */
void RepeaterMesh::mtApplyPrefs()
{
	const mt::RepeaterPrefs &p = _prefs.mt;

	if (!_mt_ready) {
		MtRelayPort::initSelf(_mt, self_id, _prefs.node_name, *getRNG());
		_mt_ready = true;
	} else {
		MtRelayPort::renameSelf(_mt, _prefs.node_name);
	}
	_mtport.apply(p);
	_mt.clearChannels();
	_mt.setChannel(kPrimaryChannel, p.channel, p.psk, p.psk_len);
	_mtport.arm((uint32_t)_ms->getMillis(), p);
}

bool RepeaterMesh::onRecvForeign(const uint8_t raw[], int len, float snr, float rssi)
{
	(void)rssi;
	return _mtport.onFrame(raw, len, snr, _mt_sink, MtRelayPort::policyFor(_prefs.mt), _prefs.sf,
			       _prefs.bw) != mt::RX_NOT_MESHTASTIC;
}

/* NodeInfo, position and telemetry on the primary channel, as Meshtastic's own default does. */
void RepeaterMesh::mtTick()
{
	_mtport.tick((uint32_t)_ms->getMillis(), _prefs.mt, kPrimaryChannel, _mt_source);
}

void RepeaterMesh::mtFormatStats(char *reply, size_t cap) const
{
	_mtport.formatStats(reply, cap);
}

#endif /* CONFIG_ZEPHTACORE_MESHTASTIC */
