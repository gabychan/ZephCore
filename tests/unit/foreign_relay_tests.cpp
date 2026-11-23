#include "test.h"
#include "dispatcher_fixture.h"
#include <mesh/Mesh.h>
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
struct TestMesh : mesh::Mesh {
    TestMesh(mesh::Radio& r, mesh::MillisecondClock& ms, mesh::RNG& rng, mesh::RTCClock& rtc,
             mesh::PacketManager& mgr, mesh::MeshTables& t) : mesh::Mesh(r, ms, rng, rtc, mgr, t) {}
    using mesh::Mesh::extendPendingRetransmit;
    using mesh::Mesh::getContentionDelay;
    using mesh::Mesh::computeAdaptiveFloodDelay;
    using mesh::Mesh::computeAdaptiveDirectDelay;
    using mesh::Mesh::originFloodDelay;
    using mesh::Mesh::_flood_is_reply;
};
struct Fixture {
    test_support::Clock clock;
    test_support::Radio radio;
    StaticPoolPacketManager pool;
    Rng rng;
    Rtc rtc;
    Tables tables;
    TestMesh mesh{radio, clock, rng, rtc, pool, tables};
};
// A 24-byte frame whose sender and id (bytes 4..11, what the repeat is told by) are n.
std::vector<uint8_t> frame(uint8_t n) {
    std::vector<uint8_t> f(24, 0x11);
    for (int i = 4; i < 12; i++) f[i] = uint8_t(n * 7 + i);
    return f;
}
}  // namespace

TEST(foreign_relay_queue, "UNIT-FOREIGN-001", "A foreign repeat is queued once, at most 8 wait, and a radio that cannot send it queues none") {
    Fixture f;
    const auto a = frame(1);
    CHECK(f.mesh.relayForeign(a.data(), a.size(), 500));
    CHECK(f.mesh.relayForeign(a.data(), a.size(), 500));            // already waiting: still true
    CHECK(f.mesh.getForeignQueued() == 1 && f.pool.getFreeCount() == 31);
    for (uint8_t n = 2; n <= 8; n++) { const auto x = frame(n); CHECK(f.mesh.relayForeign(x.data(), x.size(), 500)); }
    CHECK(f.mesh.getForeignQueued() == mesh::Mesh::FOREIGN_RELAY_MAX);
    const auto ninth = frame(9);
    CHECK(!f.mesh.relayForeign(ninth.data(), ninth.size(), 500) && f.mesh.getForeignQueued() == 8);
    CHECK(f.mesh.findForeignQueued(mesh::ContentionTracker::computeForeignHash32(a.data(), (int)a.size())) >= 0);
    CHECK(f.mesh.findForeignQueued(mesh::ContentionTracker::computeForeignHash32(ninth.data(), (int)ninth.size())) == -1);

    while (f.pool.getOutboundTotal() > 0) f.pool.free(f.pool.removeOutboundByIdx(0));   // the pool is static
    Fixture g;
    g.radio.foreign_capable = false;
    CHECK(!g.mesh.relayForeign(a.data(), a.size(), 500) && g.mesh.getForeignQueued() == 0);
    CHECK(g.mesh.getContentionTracker().msUntilNextTick(g.clock.now) == mesh::MAINTENANCE_IDLE);  // nothing tracked
}

TEST(foreign_relay_window, "UNIT-FOREIGN-002", "A queued repeat keeps its window and SNR, and only a copy of the same frame in the named window moves it") {
    Fixture f;
    const auto a = frame(1), other = frame(2);
    const uint8_t late = 3, late_heard = 4, client = 1;
    CHECK(f.mesh.relayForeign(a.data(), a.size(), 500, late, -8));
    const mesh::Packet *p = f.pool.getOutboundByIdx(0);
    CHECK(p->tx_window == late && p->_snr == -8);
    f.mesh.moveForeignWindow(other.data(), (int)other.size(), late, late_heard);
    CHECK(p->tx_window == late);
    f.mesh.moveForeignWindow(a.data(), (int)a.size(), client, late_heard);
    CHECK(p->tx_window == late);
    f.mesh.moveForeignWindow(a.data(), (int)a.size(), late, late_heard);
    CHECK(p->tx_window == late_heard && f.pool.getOutboundSchedule(0) == 600);
    while (f.pool.getOutboundTotal() > 0) f.pool.free(f.pool.removeOutboundByIdx(0));
}

TEST(foreign_relay_ema, "UNIT-FOREIGN-003", "A foreign repeat is not tracked, so it never reaches the flood delay of MeshCore") {
    Fixture f;
    for (uint8_t n = 1; n <= 4; n++) {
        const auto x = frame(n);
        CHECK(f.mesh.relayForeign(x.data(), x.size(), 500));
    }
    CHECK(f.mesh.getContentionTracker().msUntilNextTick(f.clock.now) == mesh::MAINTENANCE_IDLE);
    CHECK(f.mesh.getContentionTracker().getContentionEstimate() == 0.0f);
    while (f.pool.getOutboundTotal() > 0) f.pool.free(f.pool.removeOutboundByIdx(0));
}

TEST(mesh_extend_pending, "UNIT-FOREIGN-004", "A dupe of a MeshCore flood we hold pushes a slot still ahead back by 0.2 x its airtime, within the cap; a due slot and a flood not tracked stay") {
    Fixture f;
    auto *p = f.pool.allocNew();
    p->header = (PAYLOAD_TYPE_TXT_MSG << 2) | ROUTE_TYPE_FLOOD;
    p->payload_len = 3; p->payload[0] = 1; p->payload[1] = 2; p->payload[2] = 3;
    p->path_len = 0;
    f.pool.queueOutbound(p, 1, f.clock.now + 400);
    const uint32_t hash = mesh::ContentionTracker::computePacketHash32(p);
    f.mesh.extendPendingRetransmit(hash);                          // not tracked: no headroom, nothing moves
    CHECK(f.pool.getOutboundSchedule(0) == f.clock.now + 400);
    f.mesh.getContentionTracker().trackRetransmit(hash, f.clock.now);
    f.mesh.extendPendingRetransmit(hash);
    const uint32_t first = f.pool.getOutboundSchedule(0);
    CHECK(first > f.clock.now + 400);
    f.clock.now = first + 5;                                       // our slot passed: left to recontend()
    f.mesh.extendPendingRetransmit(hash);
    CHECK(f.pool.getOutboundSchedule(0) == first);
    f.pool.rescheduleOutbound(0, f.clock.now + 400);
    uint32_t last = f.pool.getOutboundSchedule(0);
    for (int i = 0; i < 100; i++) { f.mesh.extendPendingRetransmit(hash); CHECK(f.pool.getOutboundSchedule(0) >= last); last = f.pool.getOutboundSchedule(0); }
    CHECK(f.mesh.getContentionTracker().getReactiveHeadroom(hash, 100) == 0);   // the cap is spent
    f.pool.free(f.pool.removeOutboundByIdx(0));
}

TEST(mesh_contention_window, "UNIT-FOREIGN-007", "When the channel comes free a relayed flood draws from the flood window; our own flood and a direct packet from the short one") {
    Fixture f;
    auto *p = f.pool.allocNew();
    p->header = (PAYLOAD_TYPE_TXT_MSG << 2) | ROUTE_TYPE_FLOOD;
    p->payload_len = 3;
    p->setPathHashSizeAndCount(1, 1);                              // relayed: a hop in its path
    CHECK(f.mesh.getContentionDelay(p) == f.mesh.computeAdaptiveFloodDelay(p));
    p->setPathHashSizeAndCount(1, 0);                              // ours
    CHECK(f.mesh.getContentionDelay(p) == f.mesh.computeAdaptiveDirectDelay(p));
    p->header = (PAYLOAD_TYPE_TXT_MSG << 2) | ROUTE_TYPE_DIRECT;
    p->setPathHashSizeAndCount(1, 1);
    CHECK(f.mesh.getContentionDelay(p) == f.mesh.computeAdaptiveDirectDelay(p));
    f.pool.free(p);
}

TEST(mesh_flood_reply, "UNIT-FOREIGN-008", "A flood that answers a heard frame waits in the flood window and draws from it again; any other of our floods keeps the short one") {
    Fixture f;
    auto *p = f.pool.allocNew();
    p->header = (PAYLOAD_TYPE_GRP_TXT << 2) | ROUTE_TYPE_FLOOD;
    p->payload_len = 3;
    p->setPathHashSizeAndCount(1, 0);
    CHECK(f.mesh.originFloodDelay(p) == 0 && p->tx_window == 0);
    CHECK(f.mesh.getContentionDelay(p) == f.mesh.computeAdaptiveDirectDelay(p));
    f.mesh._flood_is_reply = true;
    CHECK(f.mesh.originFloodDelay(p) == f.mesh.computeAdaptiveFloodDelay(p));
    CHECK(p->tx_window == mesh::Packet::TX_WINDOW_FLOOD);
    CHECK(f.mesh.getContentionDelay(p) == f.mesh.computeAdaptiveFloodDelay(p));
    f.pool.free(p);
}

TEST(foreign_relay_on_air, "UNIT-FOREIGN-006", "A foreign repeat on the air still counts as waiting: hearing the frame again queues no second copy") {
    Fixture f;
    while (f.pool.getOutboundTotal() > 0) f.pool.free(f.pool.removeOutboundByIdx(0));   // the pool is static
    f.mesh.begin();
    const auto a = frame(1);
    CHECK(f.mesh.relayForeign(a.data(), a.size(), 0));
    f.mesh.loop();
    CHECK(f.radio.sent.size() == 1 && f.mesh.getForeignQueued() == 0);
    CHECK(f.mesh.foreignWaiting(a.data(), a.size()));
    CHECK(f.mesh.relayForeign(a.data(), a.size(), 0) && f.mesh.getForeignQueued() == 0);
    f.radio.completion = true;
    f.mesh.loop();
    CHECK(!f.mesh.foreignWaiting(a.data(), a.size()));
    while (f.pool.getOutboundTotal() > 0) f.pool.free(f.pool.removeOutboundByIdx(0));
}
