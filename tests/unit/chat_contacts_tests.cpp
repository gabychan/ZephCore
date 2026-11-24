#include "test.h"
#include "dispatcher_fixture.h"
#include <BaseChatMesh.h>
#include <cstring>

namespace {
struct Rng : mesh::RNG {
    void random(uint8_t *dest, size_t len) override { memset(dest, 0x42, len); }
};
struct Rtc : mesh::RTCClock {
    uint32_t now = 1700000000;
    uint32_t getCurrentTime() override { return now; }
    void setCurrentTime(uint32_t t) override { now = t; }
};
struct Tables : mesh::MeshTables {
    bool wasSeen(const mesh::Packet*) override { return false; }
    void markSeen(const mesh::Packet*) override {}
    void clear(const mesh::Packet*) override {}
};
struct ChatMesh : BaseChatMesh {
    ChatMesh(mesh::Radio& r, mesh::MillisecondClock& ms, mesh::RNG& rng, mesh::RTCClock& rtc,
             mesh::PacketManager& mgr, mesh::MeshTables& t) : BaseChatMesh(r, ms, rng, rtc, mgr, t) {}
    void onDiscoveredContact(ContactInfo&, bool, uint8_t, const uint8_t*) override {}
    ContactInfo* processAck(const uint8_t*) override { return nullptr; }
    void onContactPathUpdated(const ContactInfo&) override {}
    void onMessageRecv(const ContactInfo&, mesh::Packet*, uint32_t, const char*) override {}
    void onCommandDataRecv(const ContactInfo&, mesh::Packet*, uint32_t, const char*) override {}
    void onCLICommandRecv(const ContactInfo&, mesh::Packet*, uint32_t, const char*, char*) override {}
    void onSignedMessageRecv(const ContactInfo&, mesh::Packet*, uint32_t, const uint8_t*, const char*) override {}
    uint32_t calcFloodTimeoutMillisFor(uint32_t) const override { return 1000; }
    uint32_t calcDirectTimeoutMillisFor(uint32_t, uint8_t) const override { return 1000; }
    void onSendTimeout() override {}
    void onChannelMessageRecv(const mesh::GroupChannel&, mesh::Packet*, uint32_t, const char*) override {}
    uint8_t onContactRequest(const ContactInfo&, uint32_t, const uint8_t*, uint8_t, uint8_t*) override { return 0; }
    void onContactResponse(const ContactInfo&, const uint8_t*, uint8_t) override {}
    using BaseChatMesh::searchPeersByHash;
    bool forward = false;
    bool allowPacketForward(const mesh::Packet*) override { return forward; }
};
struct ChatFixture {
    test_support::Clock clock;
    test_support::Radio radio;
    StaticPoolPacketManager pool;
    Rng rng;
    Rtc rtc;
    Tables tables;
    ChatMesh mesh{radio, clock, rng, rtc, pool, tables};
};
ContactInfo makeContact(uint8_t first_byte, uint8_t type, const char *name) {
    ContactInfo c{};
    for (int i = 0; i < PUB_KEY_SIZE; ++i) c.id.pub_key[i] = uint8_t(i + 1);
    c.id.pub_key[0] = first_byte;
    c.type = type;
    strncpy(c.name, name, sizeof(c.name) - 1);
    return c;
}
}  // namespace

TEST(empty_slot_skipped, "UNIT-CHAT-001", "An empty contact slot (all-zero key) matches no lookup") {
    ChatFixture f;
    ContactInfo empty{};                       // what a cleared anon slot holds
    empty.type = ADV_TYPE_NONE;
    CHECK(empty.isEmptySlot());
    CHECK(f.mesh.addContact(empty));           // occupies a slot inside num_contacts
    ContactInfo real = makeContact(0x00, ADV_TYPE_CHAT, "alice");
    CHECK(!real.isEmptySlot());
    CHECK(f.mesh.addContact(real));

    const uint8_t zero_hash[1] = {0x00};
    CHECK(f.mesh.searchPeersByHash(zero_hash) == 1);  // only the real contact with hash 0x00

    uint8_t zero_key[PUB_KEY_SIZE] = {0};
    CHECK(f.mesh.lookupContactByPubKey(zero_key, PUB_KEY_SIZE) == nullptr);
    CHECK(f.mesh.lookupContactByPubKey(zero_key, 1) != nullptr);
    CHECK(f.mesh.lookupContactByPubKey(zero_key, 1)->id.matches(real.id));

    ContactInfo *by_name = f.mesh.searchContactsByPrefix("");
    CHECK(by_name != nullptr && strcmp(by_name->name, "alice") == 0);

    CHECK(!f.mesh.removeContact(empty));       // the empty slot is not a removable match
    CHECK(f.mesh.removeContact(real));
    CHECK(f.mesh.searchPeersByHash(zero_hash) == 0);
}

TEST(group_msg_negative_len, "UNIT-CHAT-002", "sendGroupMessage refuses a negative text length") {
    ChatFixture f;
    mesh::GroupChannel ch{};
    CHECK(!f.mesh.sendGroupMessage(1700000000, ch, "me", "x", -4));
}

TEST(trace_partial_hash, "UNIT-CHAT-003", "A TRACE ending in a partial hash entry is not forwarded") {
    ChatFixture f; f.mesh.forward = true;
    uint8_t seed[SEED_SIZE]; memset(seed, 7, sizeof(seed));
    f.mesh.self_id.fromSeed(seed);
    const uint8_t *pub = f.mesh.self_id.pub_key;
    f.mesh.begin();
    // Prime every pool slot so payload[12] holds the second byte of our key.
    for (int i = 0; i < 32; ++i) {
        std::vector<uint8_t> raw(2 + 13, 0);
        raw[0] = uint8_t((0x0C << 2) | ROUTE_TYPE_DIRECT); raw[2 + 12] = pub[1];
        f.radio.incoming.push_back(raw);
    }
    f.mesh.loop();
    CHECK(f.pool.getFreeCount() == 32);
    // 2-byte hashes, one hop done, a 3-byte hash list: one whole entry, then only the first byte of ours.
    f.radio.incoming.push_back({uint8_t((PAYLOAD_TYPE_TRACE << 2) | ROUTE_TYPE_DIRECT), 1, 0x55,
                                1, 2, 3, 4, 5, 6, 7, 8, 0x01, 0xEE, 0xEE, pub[0]});
    f.mesh.loop();
    CHECK(f.radio.attempts == 0 && f.pool.getOutboundTotal() == 0 && f.pool.getFreeCount() == 32);
}

TEST(flood_full_path, "UNIT-CHAT-004", "A flood that already carries 63 one-byte hops is not forwarded") {
    ChatFixture f; f.mesh.forward = true;
    f.mesh.begin();
    // The hop count is six bits: appending to a full 63-hop path would set the hash-size bits.
    std::vector<uint8_t> raw(1 + 1 + 63 + 4, 0);
    raw[0] = uint8_t((PAYLOAD_TYPE_GRP_TXT << 2) | ROUTE_TYPE_FLOOD);
    raw[1] = 63;
    for (int i = 0; i < 63; ++i) raw[2 + i] = uint8_t(0x10 + i);
    f.radio.incoming.push_back(raw);
    f.mesh.loop();
    CHECK(f.pool.getOutboundTotal() == 0 && f.radio.attempts == 0 && f.pool.getFreeCount() == 32);
}

TEST(import_two_burst, "UNIT-CHAT-005", "A second import before the loop-back ran is refused and leaks no slot") {
    ChatFixture f;
    std::vector<uint8_t> advert(2 + 100, 0);   // an advert frame: key, timestamp, signature
    advert[0] = uint8_t((PAYLOAD_TYPE_ADVERT << 2) | ROUTE_TYPE_FLOOD);
    advert[3] = 0x11;
    CHECK(f.mesh.importContact(advert.data(), (uint8_t)advert.size()));
    CHECK(f.pool.getFreeCount() == 31);
    CHECK(!f.mesh.importContact(advert.data(), (uint8_t)advert.size()));
    CHECK(f.pool.getFreeCount() == 31);        // the pending one is still the only slot out
    f.mesh.loop();
    CHECK(f.pool.getFreeCount() == 32);        // and the loop-back returned it
}

TEST(direct_zero_hops, "UNIT-CHAT-006", "A direct packet with no recorded hops is not forwarded, so no hop is invented in the path it returns") {
    ChatFixture f; f.mesh.forward = true;
    uint8_t seed[SEED_SIZE]; memset(seed, 7, sizeof(seed));
    f.mesh.self_id.fromSeed(seed);
    f.mesh.begin();
    const uint8_t ours = f.mesh.self_id.pub_key[0];
    // Slots that carried a packet routed through us keep our hash as their first path byte,
    // so the hash match below would hold if the forwarding branch were entered at all.
    std::vector<mesh::Packet *> held;
    while (auto p = f.pool.allocNew()) { p->path[0] = ours; held.push_back(p); }
    for (auto p : held) f.pool.free(p);
    f.radio.incoming.push_back({uint8_t((PAYLOAD_TYPE_ACK << 2) | ROUTE_TYPE_DIRECT), 0, 1, 2, 3, 4});
    f.mesh.loop();
    CHECK(f.pool.getOutboundTotal() == 0 && f.radio.attempts == 0 && f.pool.getFreeCount() == 32);
}
