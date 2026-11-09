#include "test.h"
#include <cstring>
#include <string>
#include <vector>
#include <helpers/meshtastic/MtBridge.h>

using namespace mt;

namespace {

// Independent AES-128 block encryption (FIPS-197), so the cipher tests do not
// depend on the production PSA wrapper.
const uint8_t kSbox[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16};

uint8_t xtime(uint8_t x) { return (uint8_t)((x << 1) ^ ((x & 0x80) ? 0x1b : 0)); }

void refAes(const uint8_t key[16], const uint8_t in[16], uint8_t out[16]) {
    uint8_t rk[176];
    std::memcpy(rk, key, 16);
    uint8_t rcon = 1;
    for (int i = 16; i < 176; i += 4) {
        uint8_t t[4] = {rk[i - 4], rk[i - 3], rk[i - 2], rk[i - 1]};
        if (i % 16 == 0) {
            uint8_t a = t[0];
            t[0] = (uint8_t)(kSbox[t[1]] ^ rcon); t[1] = kSbox[t[2]]; t[2] = kSbox[t[3]]; t[3] = kSbox[a];
            rcon = xtime(rcon);
        }
        for (int j = 0; j < 4; j++) rk[i + j] = (uint8_t)(rk[i - 16 + j] ^ t[j]);
    }
    uint8_t s[16];
    for (int i = 0; i < 16; i++) s[i] = (uint8_t)(in[i] ^ rk[i]);
    for (int round = 1; round <= 10; round++) {
        uint8_t t[16];
        for (int i = 0; i < 16; i++) t[i] = kSbox[s[i]];
        uint8_t u[16];  // ShiftRows
        for (int c = 0; c < 4; c++)
            for (int r = 0; r < 4; r++) u[c * 4 + r] = t[((c + r) % 4) * 4 + r];
        if (round < 10) {
            for (int c = 0; c < 4; c++) {
                uint8_t* p = &u[c * 4];
                uint8_t a0 = p[0], a1 = p[1], a2 = p[2], a3 = p[3];
                p[0] = (uint8_t)(xtime(a0) ^ (xtime(a1) ^ a1) ^ a2 ^ a3);
                p[1] = (uint8_t)(a0 ^ xtime(a1) ^ (xtime(a2) ^ a2) ^ a3);
                p[2] = (uint8_t)(a0 ^ a1 ^ xtime(a2) ^ (xtime(a3) ^ a3));
                p[3] = (uint8_t)((xtime(a0) ^ a0) ^ a1 ^ a2 ^ xtime(a3));
            }
        }
        for (int i = 0; i < 16; i++) s[i] = (uint8_t)(u[i] ^ rk[round * 16 + i]);
    }
    std::memcpy(out, s, 16);
}

const uint8_t kDefaultKey[16] = {0xd4, 0xf1, 0xbb, 0x3a, 0x20, 0x29, 0x07, 0x59,
                                 0xf0, 0xbc, 0xff, 0xab, 0xcf, 0x4e, 0x69, 0x01};

struct Capture : Sink {
    struct Text { uint8_t ch; uint32_t from, to; std::string text; uint8_t hops; };
    std::vector<Text> texts;
    std::vector<std::pair<uint32_t, uint32_t>> ids;  // (packet id, reply id) per text
    std::vector<std::pair<uint32_t, User>> users;
    std::vector<std::pair<uint32_t, Position>> positions;
    void onText(uint8_t ch, uint32_t from, uint32_t to, uint32_t id, uint32_t reply, const char* t, size_t n, int8_t, uint8_t hops) override {
        ids.push_back({id, reply});
        texts.push_back({ch, from, to, std::string(t, n), hops});
    }
    void onNodeInfo(uint32_t from, const User& u) override { users.push_back({from, u}); }
    void onPosition(uint32_t from, const Position& p) override { positions.push_back({from, p}); }
};

}  // namespace

TEST(mt_aes_reference, "UNIT-MT-001", "Reference AES-128 matches the FIPS-197 vector") {
    uint8_t key[16], in[16], out[16];
    for (int i = 0; i < 16; i++) { key[i] = (uint8_t)i; in[i] = (uint8_t)(i * 0x11); }
    refAes(key, in, out);
    const uint8_t expect[16] = {0x69,0xc4,0xe0,0xd8,0x6a,0x7b,0x04,0x30,0xd8,0xcd,0xb7,0x80,0x70,0xb4,0xc5,0x5a};
    CHECK(std::memcmp(out, expect, 16) == 0);
}

TEST(mt_channel_key_hash, "UNIT-MT-002", "Default key variants and the LongFast channel hash") {
    uint8_t key[16];
    bool enc = false;
    const uint8_t one = 1, two = 2, zero = 0;
    CHECK(expandPsk(&one, 1, key, enc) && enc && std::memcmp(key, kDefaultKey, 16) == 0);
    CHECK(expandPsk(&two, 1, key, enc) && enc && key[15] == 0x02 && std::memcmp(key, kDefaultKey, 15) == 0);
    CHECK(expandPsk(&zero, 1, key, enc) && !enc);
    uint8_t own[16];
    for (int i = 0; i < 16; i++) own[i] = (uint8_t)(0xA0 + i);
    CHECK(expandPsk(own, 16, key, enc) && enc && std::memcmp(key, own, 16) == 0);
    CHECK(!expandPsk(own, 7, key, enc) && !expandPsk(own, 32, key, enc) && !expandPsk(own, 0, key, enc));
    // Meshtastic's default channel: "LongFast" with the default key hashes to 8.
    CHECK(channelHash("LongFast", kDefaultKey, true) == 0x08);
    CHECK(channelHash("SFNarrow", kDefaultKey, true) == 0x20);  // value in meshtacore keys-core.js
    CHECK(channelHash("AB", kDefaultKey, false) == (uint8_t)('A' ^ 'B'));
}

TEST(mt_header, "UNIT-MT-003", "Header fields, flag bits and bounds") {
    uint8_t raw[16];
    Header h = {0x11223344u, 0xAABBCCDDu, 0x01020304u, (uint8_t)(0x63 | 0x08), 0x08, 0x00, 0x7E};
    writeHeader(h, raw);
    const uint8_t expect[16] = {0x44,0x33,0x22,0x11, 0xDD,0xCC,0xBB,0xAA, 0x04,0x03,0x02,0x01, 0x6B,0x08,0x00,0x7E};
    CHECK(std::memcmp(raw, expect, 16) == 0);
    Header g;
    CHECK(parseHeader(raw, 16, g));
    CHECK(g.to == h.to && g.from == h.from && g.id == h.id && g.channel == 8 && g.relay == 0x7E);
    CHECK(g.hopLimit() == 3 && g.hopStart() == 3 && g.wantAck());
    CHECK(!parseHeader(raw, 15, g));
}

TEST(mt_ctr, "UNIT-MT-004", "CTR keystream layout, counter carry and round trip") {
    uint32_t from = 0xAABBCCDDu, id = 0x01020304u;
    std::vector<uint8_t> z(48, 0);
    ctrCrypt(refAes, kDefaultKey, from, id, z.data(), z.size());
    // Independent keystream: block n of AES(key, id64 LE | from LE | n big endian).
    for (uint32_t n = 0; n < 3; n++) {
        uint8_t ctr[16] = {0x04,0x03,0x02,0x01, 0,0,0,0, 0xDD,0xCC,0xBB,0xAA, 0,0,0,(uint8_t)n}, ks[16];
        refAes(kDefaultKey, ctr, ks);
        CHECK(std::memcmp(&z[n * 16], ks, 16) == 0);
    }
    // A partial last block and an in-place round trip.
    std::vector<uint8_t> m(37);
    for (size_t i = 0; i < m.size(); i++) m[i] = (uint8_t)(i * 7 + 1);
    std::vector<uint8_t> c = m;
    ctrCrypt(refAes, kDefaultKey, from, id, c.data(), c.size());
    CHECK(c != m);
    ctrCrypt(refAes, kDefaultKey, from, id, c.data(), c.size());
    CHECK(c == m);
    // The counter carries across the low byte into the next.
    std::vector<uint8_t> big(16 * 258, 0);
    ctrCrypt(refAes, kDefaultKey, from, id, big.data(), big.size());
    uint8_t ctr[16] = {0x04,0x03,0x02,0x01, 0,0,0,0, 0xDD,0xCC,0xBB,0xAA, 0,0,1,0}, ks[16];
    refAes(kDefaultKey, ctr, ks);
    CHECK(std::memcmp(&big[256 * 16], ks, 16) == 0);
}

TEST(mt_protobuf_data, "UNIT-MT-005", "Data golden bytes, round trip and malformed input") {
    const uint8_t hi[2] = {'h', 'i'};
    Data d = {};
    d.portnum = PORT_TEXT; d.payload = hi; d.payload_len = 2;
    uint8_t buf[32];
    size_t n = encodeData(d, buf, sizeof(buf));
    const uint8_t golden[6] = {0x08, 0x01, 0x12, 0x02, 'h', 'i'};
    CHECK(n == 6 && std::memcmp(buf, golden, 6) == 0);
    Data g;
    CHECK(decodeData(buf, n, g) && g.portnum == 1 && g.payload_len == 2 && std::memcmp(g.payload, "hi", 2) == 0);
    CHECK(encodeData(d, buf, 5) == 0);

    Data full = {};
    full.portnum = 300; full.dest = 0x01020304; full.source = 5; full.request_id = 0xFFFFFFFF; full.want_response = true;
    n = encodeData(full, buf, sizeof(buf));
    CHECK(decodeData(buf, n, g) && g.portnum == 300 && g.dest == 0x01020304 && g.source == 5 &&
          g.request_id == 0xFFFFFFFFu && g.want_response);

    // Reply (field 7) and reaction (field 8) are fixed32, after request_id.
    Data rep = {};
    rep.portnum = PORT_TEXT; rep.payload = hi; rep.payload_len = 2; rep.reply_id = 0xAABBCCDDu; rep.emoji = 1;
    n = encodeData(rep, buf, sizeof(buf));
    const uint8_t golden_rep[] = {0x08, 0x01, 0x12, 0x02, 'h', 'i', 0x3D, 0xDD, 0xCC, 0xBB, 0xAA, 0x45, 0x01, 0, 0, 0};
    CHECK(n == sizeof(golden_rep) && std::memcmp(buf, golden_rep, n) == 0);
    CHECK(decodeData(buf, n, g) && g.reply_id == 0xAABBCCDDu && g.emoji == 1);

    CHECK(!decodeData(golden, 5, g));                       // payload runs past the end
    const uint8_t no_port[4] = {0x12, 0x02, 'h', 'i'};
    CHECK(!decodeData(no_port, 4, g));
    const uint8_t port0[2] = {0x08, 0x00};
    CHECK(!decodeData(port0, 2, g));
    const uint8_t port512[3] = {0x08, 0x80, 0x04};
    CHECK(!decodeData(port512, 3, g));
    const uint8_t group[3] = {0x08, 0x01, 0x0B};            // wire type 3 is never valid
    CHECK(!decodeData(group, 3, g));
    const uint8_t unknown[6] = {0x08, 0x01, 0xA8, 0x06, 0x07, 0x00};  // unknown varint field 101, then junk-free end
    CHECK(decodeData(unknown, 5, g) && g.portnum == 1);
}

TEST(mt_protobuf_pos_user, "UNIT-MT-006", "Position and User encode, decode and truncation") {
    Position p = {};
    p.has_lat = p.has_lon = p.has_altitude = true;
    p.lat_i = 411234567; p.lon_i = -22345678; p.altitude = -12; p.time = 1700000000u;
    uint8_t buf[64];
    size_t n = encodePosition(p, buf, sizeof(buf));
    CHECK(n > 0 && buf[0] == 0x0D);  // field 1, fixed32
    Position q;
    CHECK(decodePosition(buf, n, q) && q.has_lat && q.has_lon && q.lat_i == 411234567 &&
          q.lon_i == -22345678 && q.has_altitude && q.altitude == -12 && q.time == 1700000000u);
    CHECK(!decodePosition(buf, 3, q));
    User u = {};
    std::strcpy(u.id, "!aabbccdd"); std::strcpy(u.long_name, "Zeph Node"); std::strcpy(u.short_name, "ZN01");
    u.hw_model = 255;
    n = encodeUser(u, buf, sizeof(buf));
    User v;
    CHECK(decodeUser(buf, n, v) && std::strcmp(v.id, "!aabbccdd") == 0 && std::strcmp(v.long_name, "Zeph Node") == 0 &&
          std::strcmp(v.short_name, "ZN01") == 0 && v.hw_model == 255);
    // Over-long strings are cut to the buffers, never overrun.
    uint8_t longname[8 + 60] = {0x12, 60};
    std::memset(&longname[2], 'x', 60);
    CHECK(decodeUser(longname, 62, v) && std::strlen(v.long_name) == sizeof(v.long_name) - 1);
}

static Bridge makeNode(uint32_t node, const char* name, uint32_t seed) {
    Bridge b(refAes);
    b.setSelf(node, name, "ZN", seed);
    const uint8_t psk = 1;
    CHECK(b.setChannel(3, "LongFast", &psk, 1));
    return b;
}

TEST(mt_bridge_text, "UNIT-MT-007", "Text sent by one node is delivered once on the other") {
    Bridge a = makeNode(0xA1A1A1A1u, "alpha", 7), b = makeNode(0xB2B2B2B2u, "beta", 9);
    uint8_t frame[kMaxFrame], relay[kMaxFrame];
    size_t relay_len = 99;
    size_t n = a.buildText(3, kBroadcast, "hola mundo", 10, frame, sizeof(frame));
    CHECK(n > kHeaderLen && n <= kMaxTxFrame);
    Header h;
    CHECK(parseHeader(frame, n, h) && h.from == 0xA1A1A1A1u && h.to == kBroadcast && h.channel == 0x08 &&
          h.hopLimit() == 3 && h.hopStart() == 3 && h.relay == 0xA1 && !h.wantAck());
    // The text is not visible in the air frame.
    CHECK(std::string((const char*)frame, n).find("hola") == std::string::npos);

    Capture cap;
    CHECK(b.handleFrame(frame, n, -20, cap, relay, &relay_len) == RX_HANDLED);
    CHECK(relay_len == 0);  // relay is off by default
    CHECK(cap.texts.size() == 1 && cap.texts[0].text == "hola mundo" && cap.texts[0].from == 0xA1A1A1A1u &&
          cap.texts[0].ch == 3 && cap.texts[0].hops == 0);
    CHECK(b.handleFrame(frame, n, -20, cap, relay, &relay_len) == RX_DUPLICATE);
    CHECK(cap.texts.size() == 1);
    // Our own frame heard back is not delivered.
    CHECK(a.handleFrame(frame, n, -20, cap, relay, &relay_len) == RX_DUPLICATE);
    CHECK(cap.texts.size() == 1);
    // Packet ids are unique per frame.
    uint8_t f2[kMaxFrame];
    size_t n2 = a.buildText(3, kBroadcast, "dos", 3, f2, sizeof(f2));
    Header h2;
    CHECK(parseHeader(f2, n2, h2) && h2.id != h.id);
    CHECK(a.buildText(9, kBroadcast, "x", 1, f2, sizeof(f2)) == 0);  // unknown channel
    a.setHopLimit(7);
    size_t n3 = a.buildText(3, kBroadcast, "siete", 5, f2, sizeof(f2));
    Header h3;
    CHECK(parseHeader(f2, n3, h3) && h3.hopLimit() == 7 && h3.hopStart() == 7);
    a.setHopLimit(200);
    n3 = a.buildText(3, kBroadcast, "tope", 4, f2, sizeof(f2));
    CHECK(parseHeader(f2, n3, h3) && h3.hopLimit() == 7);
    CHECK(a.buildText(3, kBroadcast, "", 0, f2, sizeof(f2)) == 0);
    std::string big(200, 'z');
    CHECK(a.buildText(3, kBroadcast, big.data(), big.size(), f2, sizeof(f2)) == 0);

    // A reply carries the id of the text it answers, and the receiver sees both ids.
    Bridge r = makeNode(0xD4D4D4D4u, "delta", 11);
    size_t n4 = a.buildText(3, kBroadcast, "re", 2, f2, sizeof(f2), 0x12345678u);
    Capture cr;
    Header h4;
    CHECK(parseHeader(f2, n4, h4) && r.handleFrame(f2, n4, 0, cr, relay, &relay_len) == RX_HANDLED);
    CHECK(cr.ids.size() == 1 && cr.ids[0].first == h4.id && cr.ids[0].second == 0x12345678u);
    // A plain text has no reply id.
    Bridge r2 = makeNode(0xE5E5E5E5u, "eps", 12);
    Capture cp;
    CHECK(r2.handleFrame(frame, n, 0, cp, relay, &relay_len) == RX_HANDLED);
    CHECK(cp.ids.size() == 1 && cp.ids[0].second == 0);
}

TEST(mt_bridge_direct, "UNIT-MT-008", "Direct text reaches only its addressee") {
    Bridge a = makeNode(0xA1A1A1A1u, "alpha", 1), b = makeNode(0xB2B2B2B2u, "beta", 2), c = makeNode(0xC3C3C3C3u, "gamma", 3);
    uint8_t frame[kMaxFrame], relay[kMaxFrame];
    size_t relay_len;
    size_t n = a.buildText(3, 0xB2B2B2B2u, "para beta", 9, frame, sizeof(frame));
    Capture cb, cc;
    CHECK(b.handleFrame(frame, n, 0, cb, relay, &relay_len) == RX_HANDLED && cb.texts.size() == 1 && cb.texts[0].to == 0xB2B2B2B2u);
    CHECK(c.handleFrame(frame, n, 0, cc, relay, &relay_len) == RX_HANDLED && cc.texts.empty());
}

TEST(mt_bridge_nodeinfo_position, "UNIT-MT-009", "NodeInfo and Position reach the sink") {
    Bridge a = makeNode(0xA1A1A1A1u, "Alpha Node", 11), b = makeNode(0xB2B2B2B2u, "beta", 12);
    uint8_t frame[kMaxFrame], relay[kMaxFrame];
    size_t relay_len;
    Capture cap;
    size_t n = a.buildNodeInfo(3, frame, sizeof(frame));
    CHECK(n > 0 && b.handleFrame(frame, n, 0, cap, relay, &relay_len) == RX_HANDLED);
    CHECK(cap.users.size() == 1 && cap.users[0].first == 0xA1A1A1A1u &&
          std::strcmp(cap.users[0].second.long_name, "Alpha Node") == 0 &&
          std::strcmp(cap.users[0].second.id, "!a1a1a1a1") == 0 && std::strcmp(cap.users[0].second.short_name, "ZN") == 0);
    Position p = {};
    p.has_lat = p.has_lon = true; p.lat_i = 401234567; p.lon_i = -34567890; p.time = 1700000123u;
    n = a.buildPosition(3, p, frame, sizeof(frame));
    CHECK(n > 0 && b.handleFrame(frame, n, 0, cap, relay, &relay_len) == RX_HANDLED);
    CHECK(cap.positions.size() == 1 && cap.positions[0].second.lat_i == 401234567 && cap.positions[0].second.lon_i == -34567890);
    // A position without coordinates is dropped.
    p.has_lat = false;
    n = a.buildPosition(3, p, frame, sizeof(frame));
    CHECK(b.handleFrame(frame, n, 0, cap, relay, &relay_len) == RX_HANDLED && cap.positions.size() == 1);
}

TEST(mt_bridge_relay, "UNIT-MT-010", "Relay lowers hop_limit, sets the relay byte and keeps the payload") {
    Bridge a = makeNode(0xA1A1A1A1u, "alpha", 21), r = makeNode(0xD4D4D4D4u, "relay", 22);
    r.setRelay(true);
    uint8_t frame[kMaxFrame], out[kMaxFrame];
    size_t out_len = 0;
    size_t n = a.buildText(3, kBroadcast, "salto", 5, frame, sizeof(frame));
    Capture cap;
    CHECK(r.handleFrame(frame, n, 0, cap, out, &out_len) == RX_HANDLED && out_len == n);
    Header h;
    CHECK(parseHeader(out, out_len, h) && h.hopLimit() == 2 && h.hopStart() == 3 && h.relay == 0xD4 &&
          h.from == 0xA1A1A1A1u && !h.wantAck());
    CHECK(std::memcmp(out + kHeaderLen, frame + kHeaderLen, n - kHeaderLen) == 0);
    // The repeated copy is a duplicate for the relay itself and decodes elsewhere with hops = 1.
    CHECK(r.handleFrame(out, out_len, 0, cap, out, &out_len) == RX_DUPLICATE && out_len == 0);
    Bridge b = makeNode(0xB2B2B2B2u, "beta", 23);
    Capture cb;
    uint8_t scratch[kMaxFrame];
    size_t sl;
    uint8_t copy[kMaxFrame];
    std::memcpy(copy, frame, n);
    copy[12] = (uint8_t)((copy[12] & ~7) | 2);
    CHECK(b.handleFrame(copy, n, 0, cb, scratch, &sl) == RX_HANDLED && cb.texts[0].hops == 1);
    // No hops left: delivered, not repeated.
    Bridge r2 = makeNode(0xE5E5E5E5u, "relay2", 24);
    r2.setRelay(true);
    n = a.buildText(3, kBroadcast, "ultimo", 6, frame, sizeof(frame));
    frame[12] = (uint8_t)(frame[12] & ~7);
    CHECK(r2.handleFrame(frame, n, 0, cap, out, &out_len) == RX_HANDLED && out_len == 0);
    // A frame for the relay alone, or with a next hop set, is not repeated.
    n = a.buildText(3, 0xE5E5E5E5u, "tuyo", 4, frame, sizeof(frame));
    CHECK(r2.handleFrame(frame, n, 0, cap, out, &out_len) == RX_HANDLED && out_len == 0);
    n = a.buildText(3, kBroadcast, "directo", 7, frame, sizeof(frame));
    frame[14] = 0x42;
    CHECK(r2.handleFrame(frame, n, 0, cap, out, &out_len) == RX_HANDLED && out_len == 0);
}

TEST(mt_bridge_foreign_channel, "UNIT-MT-011", "Frames of another channel or key are left to MeshCore") {
    Bridge a(refAes), b(refAes);
    a.setSelf(0xA1A1A1A1u, "a", "A", 1);
    b.setSelf(0xB2B2B2B2u, "b", "B", 2);
    uint8_t ka[16], kb[16];
    for (int i = 0; i < 16; i++) { ka[i] = (uint8_t)i; kb[i] = (uint8_t)(i + 1); }
    CHECK(a.setChannel(0, "secret", ka, 16));
    uint8_t frame[kMaxFrame], out[kMaxFrame];
    size_t out_len, n = a.buildText(0, kBroadcast, "privado", 7, frame, sizeof(frame));
    Capture cap;
    CHECK(b.handleFrame(frame, n, 0, cap, out, &out_len) == RX_NOT_MESHTASTIC);  // no such channel
    CHECK(b.setChannel(1, "secret", kb, 16));  // same name, different key: same hash only by luck
    RxResult r = b.handleFrame(frame, n, 0, cap, out, &out_len);
    CHECK(r == RX_NOT_MESHTASTIC && cap.texts.empty());
    // Table limits and handle reuse.
    Bridge t(refAes);
    t.setSelf(0xA1A1A1A1u, "t", "T", 1);
    const uint8_t psk = 1;
    for (int i = 0; i < kMaxChannels; i++) CHECK(t.setChannel((uint8_t)i, "c", &psk, 1));
    CHECK(!t.setChannel(200, "c", &psk, 1));
    CHECK(t.setChannel(2, "c2", &psk, 1) && t.channelCount() == kMaxChannels);
    t.clearChannels();
    CHECK(t.channelCount() == 0 && !t.hasChannel(2));
}

TEST(mt_bridge_not_meshtastic, "UNIT-MT-012", "MeshCore-like and random frames are not claimed") {
    Bridge b = makeNode(0xB2B2B2B2u, "beta", 5);
    Capture cap;
    uint8_t out[kMaxFrame];
    size_t out_len;
    uint32_t seed = 12345;
    int claimed = 0;
    for (int i = 0; i < 20000; i++) {
        uint8_t raw[kMaxFrame];
        size_t len = 18 + nextRandom(seed) % 100;
        for (size_t j = 0; j < len; j++) raw[j] = (uint8_t)(nextRandom(seed) >> 16);
        raw[13] = 0x08;  // force the hash match so the cipher and decoder are exercised
        if (b.handleFrame(raw, len, 0, cap, out, &out_len) != RX_NOT_MESHTASTIC) claimed++;
    }
    CHECK(claimed <= 20);  // measured and bounded: a random frame passes the whole decoder ~1 in 10^4
    uint8_t tiny[17] = {0};
    CHECK(b.handleFrame(tiny, 17, 0, cap, out, &out_len) == RX_NOT_MESHTASTIC);
    uint8_t oversized[300] = {0};
    CHECK(b.handleFrame(oversized, 300, 0, cap, out, &out_len) == RX_NOT_MESHTASTIC);
    Bridge unset(refAes);
    CHECK(unset.handleFrame(oversized, 40, 0, cap, out, &out_len) == RX_NOT_MESHTASTIC);
}
