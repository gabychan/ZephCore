/*
 * SPDX-License-Identifier: MIT
 * Zephtacore: the companion's Meshtastic bridge.
 *
 *   - A group channel named "MT:<name>" with a 16 byte secret is a Meshtastic
 *     channel with that name and PSK. Text the app sends on it leaves as a
 *     Meshtastic frame; text heard on it reaches the app as a channel message.
 *   - Meshtastic nodes heard on those channels (NodeInfo, Position) become
 *     contacts named "MT:<long name>", keyed by an Ed25519 key derived from the
 *     node number, so they appear in the contact list and on the map.
 *   - A direct message to such a contact goes out as a channel-encrypted text
 *     addressed to that node.
 * Nothing here enters the MeshCore routing path.
 */

#include "CompanionMesh.h"

#if IS_ENABLED(CONFIG_ZEPHTACORE_MESHTASTIC)

#include <mesh/Utils.h>
#include <string.h>
#include <stdio.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zephcore_companion, CONFIG_ZEPHCORE_MAIN_LOG_LEVEL);

static const char kMtPrefix[] = "MT:";
static constexpr size_t kMtPrefixLen = sizeof(kMtPrefix) - 1;

void CompanionMesh::mtAesBlock(const uint8_t key[16], const uint8_t in[16], uint8_t out[16])
{
	mesh::Utils::encrypt(key, out, in, 16);
}

bool CompanionMesh::mtIsChannelName(const char *name)
{
	return strncmp(name, kMtPrefix, kMtPrefixLen) == 0 && name[kMtPrefixLen] != '\0';
}

/* Our Meshtastic node number: the last four bytes of the MeshCore public key. */
static uint32_t mtSelfNode(const mesh::LocalIdentity &id)
{
	const uint8_t *k = id.pub_key;

	return ((uint32_t)k[PUB_KEY_SIZE - 4] << 24) | ((uint32_t)k[PUB_KEY_SIZE - 3] << 16) |
	       ((uint32_t)k[PUB_KEY_SIZE - 2] << 8) | (uint32_t)k[PUB_KEY_SIZE - 1];
}

/* Four letters or digits of the node name, upper case. */
static void mtShortName(const char *name, char out[5])
{
	int n = 0;

	for (const char *c = name; *c && n < 4; c++) {
		if ((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9')) {
			out[n++] = (*c >= 'a' && *c <= 'z') ? (char)(*c - 'a' + 'A') : *c;
		}
	}
	if (n == 0) {
		out[n++] = 'Z';
	}
	out[n] = '\0';
}

/* Meshtastic's default key has PSK index 1; index n adds n-1 to its last byte. */
struct MtDefaultChannel {
	const char *name;
	uint8_t psk_index;
};

static const MtDefaultChannel kMtDefaults[] = {
	{"MT:SFNarrow", 1}, {"MT:Test", 2}, {"MT:Bots", 2}, {"MT:Catalunya", 1}, {"MT:Barcelona", 1},
};
static const char *const kMcHashtagDefaults[] = {"#test", "#bot"};

void CompanionMesh::mtAddDefaultChannels()
{
	/* The default Meshtastic key (PSK "AQ=="). */
	static const uint8_t kDefaultKey[16] = {0xd4, 0xf1, 0xbb, 0x3a, 0x20, 0x29, 0x07, 0x59,
						0xf0, 0xbc, 0xff, 0xab, 0xcf, 0x4e, 0x69, 0x01};
	uint8_t key[16];

	for (const auto &c : kMtDefaults) {
		memcpy(key, kDefaultKey, sizeof(key));
		key[15] = (uint8_t)(key[15] + c.psk_index - 1);
		addChannel(c.name, key, sizeof(key));
	}
	/* A MeshCore hashtag channel's secret is the first 16 bytes of SHA256 of its name. */
	for (const char *name : kMcHashtagDefaults) {
		uint8_t digest[32];

		mesh::Utils::sha256(digest, sizeof(digest), (const uint8_t *)name, (int)strlen(name));
		addChannel(name, digest, 16);
	}
}

void CompanionMesh::mtSetup()
{
	char short_name[5];
	uint32_t seed = 0;

	getRNG()->random((uint8_t *)&seed, sizeof(seed));
	mtShortName(prefs.node_name, short_name);
	_mt.setSelf(mtSelfNode(self_id), prefs.node_name, short_name, seed);
	_mt.setRelay(IS_ENABLED(CONFIG_ZEPHTACORE_MT_RELAY));
	_mt.setHopLimit(CONFIG_ZEPHTACORE_MT_HOP_LIMIT);
	mtSyncChannels();
	/* Channels are loaded once the app side is up; with none yet, try again. */
	_mt_ready = getNumChannels() > 0;
	/* First NodeInfo shortly after boot, then every interval. */
	_mt_nodeinfo_next_s = (uint32_t)(k_uptime_get() / 1000) + 30;
}

void CompanionMesh::mtSyncChannels()
{
	_mt.clearChannels();

	for (int i = 0; i < getNumChannels(); i++) {
		ChannelDetails ch;

		if (!getChannel(i, ch) || !mtIsChannelName(ch.name)) {
			continue;
		}
		if (!_mt.setChannel((uint8_t)i, ch.name + kMtPrefixLen, ch.channel.secret, 16)) {
			LOG_WRN("mt: channel '%s' not usable (table full)", ch.name);
		}
	}
	LOG_INF("mt: %d Meshtastic channel(s)", _mt.channelCount());
}

int CompanionMesh::mtFirstChannel() const
{
	for (int i = 0; i < getNumChannels(); i++) {
		if (_mt.hasChannel((uint8_t)i)) {
			return i;
		}
	}
	return -1;
}

/* The contact key of a Meshtastic node: the public key of a keypair seeded
 * from SHA256("zt-mtnode" || node). A real Ed25519 point (strict clients reject
 * anything else) whose private half nobody keeps. Cached: a derivation is a
 * scalar multiplication. */
bool CompanionMesh::mtKeyFor(uint32_t node, uint8_t key[PUB_KEY_SIZE])
{
	for (auto &e : _mt_keys) {
		if (e.node == node && e.key[0] != 0) {
			memcpy(key, e.key, PUB_KEY_SIZE);
			return true;
		}
	}

	static const char salt[] = "zt-mtnode";
	uint8_t material[4];
	uint8_t seed[SEED_SIZE];
	mesh::LocalIdentity id;

	for (int i = 0; i < 4; i++) {
		material[i] = (uint8_t)(node >> (24 - 8 * i));
	}
	mesh::Utils::sha256(seed, SEED_SIZE, (const uint8_t *)salt, sizeof(salt) - 1, material,
			    sizeof(material));
	id.fromSeed(seed);

	MtKey &slot = _mt_keys[_mt_key_next];

	_mt_key_next = (uint8_t)((_mt_key_next + 1) % MT_KEY_CACHE);
	slot.node = node;
	memcpy(slot.key, id.pub_key, PUB_KEY_SIZE);
	memcpy(key, slot.key, PUB_KEY_SIZE);
	return true;
}

/* The node number behind a contact. A Meshtastic contact keeps it in
 * sync_since (unused for chat contacts); the key derived from it must match,
 * so a contact the app edited or imported is never taken for one. */
bool CompanionMesh::mtNodeOf(const ContactInfo &contact, uint32_t &node)
{
	uint8_t key[PUB_KEY_SIZE];

	if (contact.sync_since == 0 || !mtKeyFor(contact.sync_since, key) ||
	    memcmp(key, contact.id.pub_key, PUB_KEY_SIZE) != 0) {
		return false;
	}
	node = contact.sync_since;
	return true;
}

void CompanionMesh::mtUpsertContact(uint32_t node, const char *name, bool has_pos, int32_t lat_i,
				    int32_t lon_i)
{
	uint8_t key[PUB_KEY_SIZE];

	if (!mtKeyFor(node, key)) {
		return;
	}
	uint32_t now = (uint32_t)getRTCClock()->getCurrentTime();
	ContactInfo *c = lookupContactByPubKey(key, PUB_KEY_SIZE);
	bool is_new = (c == nullptr);

	if (is_new) {
		ContactInfo fresh;

		memcpy(fresh.id.pub_key, key, PUB_KEY_SIZE);
		fresh.type = ADV_TYPE_CHAT;
		fresh.flags = 0;
		fresh.out_path_len = OUT_PATH_UNKNOWN;
		fresh.shared_secret_valid = false;
		memset(fresh.out_path, 0, sizeof(fresh.out_path));
		snprintf(fresh.name, sizeof(fresh.name), "%s!%08x", kMtPrefix, (unsigned)node);
		fresh.gps_lat = 0;
		fresh.gps_lon = 0;
		fresh.sync_since = node;
		fresh.last_advert_timestamp = now;
		fresh.lastmod = now;
		if (!addContact(fresh)) {
			LOG_WRN("mt: contacts full, node %08x not added", (unsigned)node);
			return;
		}
		c = lookupContactByPubKey(key, PUB_KEY_SIZE);
		if (!c) {
			return;
		}
	}
	if (name && name[0]) {
		snprintf(c->name, sizeof(c->name), "%s%s", kMtPrefix, name);
	}
	if (has_pos) {
		c->gps_lat = lat_i / 10;  /* 1e-7 degrees to 1e-6 */
		c->gps_lon = lon_i / 10;
	}
	c->last_advert_timestamp = now;
	c->lastmod = now;
	onDiscoveredContact(*c, is_new, 0, nullptr);
}

void CompanionMesh::mtQueue(const uint8_t *frame, size_t len, uint32_t delay_ms)
{
	if (!sendForeign(frame, (int)len, 1, delay_ms)) {
		LOG_WRN("mt: frame not queued (len=%u)", (unsigned)len);
	}
}

/* ── RX ───────────────────────────────────────────────────────────────── */

bool CompanionMesh::onRecvForeign(const uint8_t raw[], int len, float snr, float rssi)
{
	(void)rssi;
	if (getNumChannels() == 0) {
		return false;
	}
	if (!_mt_ready) {
		mtSetup();
	}
	if (_mt.channelCount() == 0) {
		return false;
	}

	uint8_t relay[mt::kMaxFrame];
	size_t relay_len = 0;
	mt::RxResult r = _mt.handleFrame(raw, (size_t)len, (int8_t)(snr * 4.0f), _mt_sink, relay,
					 &relay_len);

	if (r == mt::RX_NOT_MESHTASTIC) {
		return false;
	}
	if (relay_len > 0 && relay_len <= mt::kMaxTxFrame) {
		mtQueue(relay, relay_len, 50 + getRNG()->nextInt(0, 351));
	}
	return true;
}

void CompanionMesh::MtSink::onText(uint8_t channel_id, uint32_t from, uint32_t to,
				   const char *text, size_t len, int8_t snr_x4, uint8_t hops)
{
	(void)to;
	(void)snr_x4;
	(void)hops;

	ChannelDetails ch;

	if (!m.getChannel(channel_id, ch)) {
		return;
	}
	/* "<sender>: <text>", the form MeshCore channel messages have. */
	char sender[32];
	uint8_t key[PUB_KEY_SIZE];
	ContactInfo *c = nullptr;

	if (m.mtKeyFor(from, key)) {
		c = m.lookupContactByPubKey(key, PUB_KEY_SIZE);
	}
	if (c && strncmp(c->name, kMtPrefix, kMtPrefixLen) == 0) {
		snprintf(sender, sizeof(sender), "%s", c->name + kMtPrefixLen);
	} else {
		snprintf(sender, sizeof(sender), "!%08x", (unsigned)from);
	}

	char line[MAX_TEXT_LEN + 1];
	int n = snprintf(line, sizeof(line), "%s: ", sender);

	if (n < 0 || (size_t)n >= sizeof(line)) {
		return;
	}
	size_t room = sizeof(line) - 1 - (size_t)n;

	if (len > room) {
		len = room;
	}
	memcpy(line + n, text, len);
	line[n + len] = '\0';

	m.onChannelMessageRecv(ch.channel, nullptr, m.getRTCClock()->getCurrentTimeUnique(), line);
}

void CompanionMesh::MtSink::onNodeInfo(uint32_t from, const mt::User &user)
{
	const char *name = user.long_name[0] ? user.long_name : user.short_name;

	m.mtUpsertContact(from, name, false, 0, 0);
}

void CompanionMesh::MtSink::onPosition(uint32_t from, const mt::Position &pos)
{
	m.mtUpsertContact(from, nullptr, true, pos.lat_i, pos.lon_i);
}

/* ── TX ───────────────────────────────────────────────────────────────── */

bool CompanionMesh::mtSendChannelText(int ch_idx, const char *text, size_t len, bool &sent)
{
	ChannelDetails ch;

	if (!getChannel(ch_idx, ch) || !mtIsChannelName(ch.name)) {
		return false;
	}
	sent = false;
	if (!_mt_ready) {
		mtSetup();
	}
	uint8_t frame[mt::kMaxTxFrame];
	size_t n = _mt.buildText((uint8_t)ch_idx, mt::kBroadcast, text, len, frame, sizeof(frame));

	if (n > 0) {
		mtQueue(frame, n, 0);
		sent = true;
	}
	return true;
}

bool CompanionMesh::mtSendDirectText(const ContactInfo &contact, const char *text, size_t len,
				     bool &sent)
{
	uint32_t node;

	if (!mtNodeOf(contact, node)) {
		return false;
	}
	sent = false;
	if (!_mt_ready) {
		mtSetup();
	}
	int ch = mtFirstChannel();

	if (ch < 0) {
		return true;
	}
	uint8_t frame[mt::kMaxTxFrame];
	size_t n = _mt.buildText((uint8_t)ch, node, text, len, frame, sizeof(frame));

	if (n > 0) {
		mtQueue(frame, n, 0);
		sent = true;
	}
	return true;
}

/* Our NodeInfo on every Meshtastic channel is too chatty: the first one only,
 * as Meshtastic's own default sends it on the primary channel. */
void CompanionMesh::mtNodeInfoTick()
{
	if (getNumChannels() == 0) {
		return;
	}
	if (!_mt_ready) {
		mtSetup();
	}
	if (CONFIG_ZEPHTACORE_MT_NODEINFO_INTERVAL_S == 0 || _mt.channelCount() == 0) {
		return;
	}
	uint32_t now_s = (uint32_t)(k_uptime_get() / 1000);

	if ((int32_t)(now_s - _mt_nodeinfo_next_s) < 0) {
		return;
	}
	_mt_nodeinfo_next_s = now_s + CONFIG_ZEPHTACORE_MT_NODEINFO_INTERVAL_S;

	int ch = mtFirstChannel();
	uint8_t frame[mt::kMaxTxFrame];
	size_t n = ch < 0 ? 0 : _mt.buildNodeInfo((uint8_t)ch, frame, sizeof(frame));

	if (n > 0) {
		mtQueue(frame, n, 0);
	}
}

#endif /* CONFIG_ZEPHTACORE_MESHTASTIC */
