/*
 * SPDX-License-Identifier: MIT
 * See MtRelayPort.h.
 */

#include "MtRelayPort.h"

#if IS_ENABLED(CONFIG_ZEPHTACORE_MESHTASTIC)

#include <helpers/meshtastic/MtConfig.h>
#include <mesh/ContentionTracker.h>
#include <mesh/Utils.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(zephcore_mt, CONFIG_ZEPHCORE_MAIN_LOG_LEVEL);

void MtRelayPort::aesBlock(const uint8_t key[16], const uint8_t in[16], uint8_t out[16])
{
	mesh::Utils::encrypt(key, out, in, 16);
}

void MtRelayPort::initSelf(mt::Bridge &bridge, const mesh::LocalIdentity &id, const char *name,
			   mesh::RNG &rng)
{
	char short_name[5];
	uint32_t seed = 0;

	rng.random((uint8_t *)&seed, sizeof(seed));
	mt::shortName(name, short_name);
	bridge.setSelf(mt::nodeFromPublicKey(id.pub_key), name, short_name, seed);
}

bool MtRelayPort::send(const uint8_t *frame, size_t len)
{
	if (!_mesh.sendForeign(frame, (int)len, 1, 0)) {
		LOG_WRN("mt: frame not queued (len=%u)", (unsigned)len);
		return false;
	}
	return true;
}

bool MtRelayPort::yields(uint32_t hash) const
{
	for (uint32_t h : _yield) {
		if (h == hash) {
			return true;
		}
	}
	return false;
}

/* Queues the repeat of a heard frame with the delay of the role. A repeat that
 * may give way to an echo is remembered by hash. */
void MtRelayPort::queue(const uint8_t *relay, size_t len, int8_t snr_x4, mt::RelayClass cls,
			const Policy &policy, uint8_t sf, float bw)
{
	if (len > mt::kMaxTxFrame) {
		return;
	}
	const uint32_t slot = mt::slotTimeMs(sf, bw);
	const uint32_t r = (uint32_t)_mesh.getRNG()->nextInt(0, (int)mt::relayWindow(snr_x4, policy.early));

	if (!_mesh.relayForeign(relay, len, mt::relayDelayMs(slot, policy.early, r))) {
		LOG_WRN("mt: relay not queued (len=%u)", (unsigned)len);
		return;
	}
	_stats.relayed++;
	if (policy.yields && cls == mt::RelayClass::CancelOnEcho) {
		_yield[_yield_next] = mesh::ContentionTracker::computeForeignHash32(relay, (int)len);
		_yield_next = (uint8_t)((_yield_next + 1) % kYieldKeep);
	}
}

/* Heard again with nothing to repeat. From a repeater, while we hold the frame: a
 * frame that yields is dropped unless that repeater named us next hop, the others
 * move out of its way. The originator sending it again leaves our copy alone. */
void MtRelayPort::echoHeard(const uint8_t raw[], int len, bool may_yield)
{
	mt::Header h;
	uint32_t hash = mesh::ContentionTracker::computeForeignHash32(raw, len);

	if (!mt::parseHeader(raw, (size_t)len, h) || (h.hopStart() > 0 && h.hopStart() == h.hopLimit())) {
		return;
	}
	const bool asked = h.next_hop != 0 && h.next_hop == (uint8_t)_bridge.self();

	if (may_yield && !asked && yields(hash) && _mesh.cancelForeignRelay(hash)) {
		_stats.yielded++;
	} else {
		_mesh.foreignDupeHeard(raw, len);
	}
}

mt::RxResult MtRelayPort::onFrame(const uint8_t raw[], int len, float snr, mt::Sink &sink,
				  const Policy &policy, uint8_t sf, float bw)
{
	uint8_t relay[mt::kMaxFrame];
	size_t relay_len = 0;
	mt::RelayClass cls;
	const int8_t snr_x4 = (int8_t)(snr * 4.0f);
	mt::RxResult r = _bridge.handleFrame(raw, (size_t)len, snr_x4, sink, relay, &relay_len, &cls);

	if (r == mt::RX_NOT_MESHTASTIC) {
		return r;
	}
	_stats.heard++;
	if (r == mt::RX_OPAQUE) {
		_stats.opaque++;
	}
	if (r == mt::RX_DUPLICATE && relay_len == 0) {
		echoHeard(raw, len, policy.yields);
		return r;
	}
	if (relay_len > 0 && (policy.relay_all || cls == mt::RelayClass::Always)) {
		queue(relay, relay_len, snr_x4, cls, policy, sf, bw);
	}
	return r;
}

#endif /* CONFIG_ZEPHTACORE_MESHTASTIC */
