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

TEST(foreign_relay_defer, "UNIT-FOREIGN-002", "A copy heard from another node pushes our repeat back from its own slot, never earlier; another frame leaves it alone") {
    Fixture f;
    const auto a = frame(1), other = frame(2);
    CHECK(f.mesh.relayForeign(a.data(), a.size(), 500));
    const uint32_t slot = f.pool.getOutboundSchedule(0);
    CHECK(slot == 600);
    f.mesh.foreignDupeHeard(other.data(), other.size());
    CHECK(f.pool.getOutboundSchedule(0) == slot);
    f.mesh.foreignDupeHeard(a.data(), a.size());
    const uint32_t first = f.pool.getOutboundSchedule(0);
    CHECK(first > slot);                                           // 0.2 x the foreign airtime (60 ms) later
    f.clock.now = first + 1000;                                    // our slot passed while the copy arrives
    f.mesh.foreignDupeHeard(a.data(), a.size());
    CHECK(f.pool.getOutboundSchedule(0) >= first);
}

TEST(foreign_relay_ema, "UNIT-FOREIGN-003", "The dupes of a foreign repeat never reach the flood delay of MeshCore") {
    Fixture f;
    const auto a = frame(1);
    for (uint8_t n = 1; n <= 4; n++) {
        const auto x = frame(n);
        CHECK(f.mesh.relayForeign(x.data(), x.size(), 500));
        f.mesh.foreignDupeHeard(x.data(), x.size());
    }
    f.mesh.getContentionTracker().tick(f.clock.now + 11000);
    CHECK(f.mesh.getContentionTracker().getContentionEstimate() == 0.0f);
    CHECK(!f.mesh.getContentionTracker().isWarmedUp());
    mesh::ContentionTracker own;
    own.trackRetransmit(7, 100);
    own.recordDupeIfTracked(7, 101);
    own.tick(11000);
    CHECK(own.getContentionEstimate() > 0.0f);
    (void)a;
}

TEST(mesh_extend_pending, "UNIT-FOREIGN-004", "A dupe of a MeshCore flood we hold pushes it back by 0.2 x its airtime from its own slot, never earlier, within the cap; a flood not tracked stays") {
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
    f.clock.now = first + 5000;                                    // our slot passed
    f.mesh.extendPendingRetransmit(hash);
    CHECK(f.pool.getOutboundSchedule(0) >= f.clock.now);
    uint32_t last = f.pool.getOutboundSchedule(0);
    for (int i = 0; i < 100; i++) { f.mesh.extendPendingRetransmit(hash); CHECK(f.pool.getOutboundSchedule(0) >= last); last = f.pool.getOutboundSchedule(0); }
    CHECK(f.mesh.getContentionTracker().getReactiveHeadroom(hash, 100) == 0);   // the cap is spent
}

TEST(foreign_relay_hold, "UNIT-FOREIGN-005", "A repeat waiting longer than the observation window still hears the copy of another node and gives way; once its own wait is over it is no longer tracked") {
    Fixture f;
    const auto a = frame(1);
    CHECK(f.mesh.relayForeign(a.data(), a.size(), 20000));          // SF12, a strong signal: a wait of 20 s
    const uint32_t slot = f.pool.getOutboundSchedule(0);
    f.clock.now += 15000;                                           // past the 10 s window, before our slot
    f.mesh.foreignDupeHeard(a.data(), a.size());
    CHECK(f.pool.getOutboundSchedule(0) > slot);
    mesh::ContentionTracker t;
    t.trackRetransmit(5, 1000, true, 500);
    CHECK(t.recordDupeIfTracked(5, 1000 + 10400) && !t.recordDupeIfTracked(5, 1000 + 10501));
}
