#include "dispatcher_fixture.h"
using namespace test_support;

TEST(dispatch_send, "UNIT-DISPATCH-001", "Immediate wake delayed dispatch and exactly-once completion") {
    Fixture f; f.begin();
    f.dispatcher.sendPacket(f.packet(), 2, 20);
    CHECK(f.dispatcher.wakes == std::vector<uint32_t>({20}));
    f.clock.now = 119; f.dispatcher.loop(); CHECK(f.radio.attempts == 0);
    f.clock.now = 120; f.dispatcher.loop();
    CHECK(f.radio.sent == std::vector<std::vector<uint8_t>>({{9, 0, 0xAA}}));
    CHECK(f.pool.getFreeCount() == 31 && f.dispatcher.getNumSentFlood() == 0);
    f.complete(); f.dispatcher.loop();
    CHECK(f.radio.finished == 1 && f.pool.getFreeCount() == 32);
    CHECK(f.dispatcher.getNumSentFlood() == 1 && f.dispatcher.getTotalAirTime() == 100);
    f.dispatcher.sendPacket(f.packet(), 2, 0); CHECK(f.dispatcher.wakes.back() == 0);
    f.dispatcher.loop(); f.complete(); CHECK(f.pool.getFreeCount() == 32);
}
TEST(dispatch_timeout, "UNIT-DISPATCH-002", "TX timeout releases ownership and next send recovers") {
    Fixture f; f.begin(); f.dispatcher.sendPacket(f.packet(), 0); f.dispatcher.loop();
    f.clock.now = 400; f.dispatcher.loop(); CHECK(f.pool.getFreeCount() == 31);
    f.clock.now = 401; f.dispatcher.loop();  // expired, but the radio has not concluded: hold
    CHECK(f.dispatcher.failures == 0 && f.radio.finished == 0 && f.pool.getFreeCount() == 31);
    f.radio.active = false; f.dispatcher.loop();  // the radio's own TX watchdog gave up
    CHECK(f.dispatcher.failures == 1 && f.radio.finished == 1 && f.pool.getFreeCount() == 32);
    CHECK(f.dispatcher.getNumSentFlood() == 0 && f.dispatcher.getTotalAirTime() == 0);
    f.dispatcher.sendPacket(f.packet(), 0); f.dispatcher.loop(); f.complete();
    CHECK(f.dispatcher.getNumSentFlood() == 1 && f.pool.getFreeCount() == 32);
}
TEST(dispatch_refusal, "UNIT-DISPATCH-003", "Driver refusal preserves priority schedule and accounting") {
    Fixture f; f.begin(); f.radio.accept = false;
    auto p = f.packet(); f.dispatcher.sendPacket(p, 3); f.dispatcher.loop();
    CHECK(f.pool.getOutboundByIdx(0) == p && f.pool.getOutboundSchedule(0) == 200);
    CHECK(f.pool.peekNextOutboundPriority(200) == 3);
    CHECK(f.dispatcher.getTotalAirTime() == 0 && f.dispatcher.getNumSentFlood() == 0);
    f.clock.now = 199; f.dispatcher.loop(); CHECK(f.radio.attempts == 1);
    f.radio.accept = true; f.clock.now = 200; f.dispatcher.loop(); f.complete();
    CHECK(f.radio.attempts == 2 && f.pool.getFreeCount() == 32);
}
TEST(dispatch_lbt, "UNIT-DISPATCH-004", "LBT starvation survives interleaved retry-gap wakes") {
    Fixture f; f.begin(); f.radio.accept = false;
    f.dispatcher.sendPacket(f.packet(), 1);
    for (unsigned now = 100; now <= 60200; now += 100) {
        f.clock.now = now; f.dispatcher.loop();
        f.clock.now = now + 50; f.dispatcher.loop(); // RX wake while nothing is due
    }
    CHECK((f.dispatcher.getErrFlags() & ERR_EVENT_CAD_TIMEOUT) != 0);
    CHECK(f.radio.relaxations == 1 && f.radio.recoveries == 0);
    CHECK(f.dispatcher.getNumSentFlood() == 0 && f.pool.getOutboundTotal() == 1);
    f.radio.accept = true; f.clock.now = 60300; f.dispatcher.loop(); f.complete();
    CHECK(f.pool.getFreeCount() == 32);
    f.radio.accept = false; f.dispatcher.sendPacket(f.packet(), 1); f.dispatcher.loop();
    CHECK(f.radio.relaxations == 1); // successful send ended the old streak
    f.radio.accept = true; f.clock.now += 100; f.dispatcher.loop(); f.complete();
}
TEST(dispatch_final_gate, "UNIT-DISPATCH-005", "Final RX gate requeues without driver call or lost priority") {
    Fixture f; f.begin(); f.radio.block_final_gate = true;
    auto p = f.packet(); f.dispatcher.sendPacket(p, 3); f.dispatcher.loop();
    CHECK(f.radio.attempts == 0 && f.pool.getOutboundByIdx(0) == p);
    CHECK(f.pool.peekNextOutboundPriority(200) == 3 && f.dispatcher.wakes.back() == 100);
    f.radio.block_final_gate = false; f.clock.now = 200; f.dispatcher.loop(); f.complete();
    CHECK(f.pool.getFreeCount() == 32);
}
TEST(dispatch_busy_recovery, "UNIT-DISPATCH-006", "Persistent RX busy recovers and resumes sending") {
    Fixture f; f.begin(); f.radio.receiving = true;
    f.dispatcher.sendPacket(f.packet(), 0);
    f.clock.now = 101; f.dispatcher.loop(); CHECK(f.radio.attempts == 0);
    f.clock.now = 4102; f.dispatcher.loop();
    CHECK(f.radio.recoveries == 1 && f.radio.attempts == 1);
    CHECK((f.dispatcher.getErrFlags() & ERR_EVENT_CAD_TIMEOUT) != 0);
    f.complete(); CHECK(f.pool.getFreeCount() == 32);
}
TEST(dispatch_budget, "UNIT-DISPATCH-007", "Airtime budget gates and refills at the exact threshold") {
    Fixture f; f.begin(10); // 100ms initial budget, 50ms admission threshold
    f.dispatcher.sendPacket(f.packet(), 0); f.dispatcher.loop(); f.complete();
    f.dispatcher.sendPacket(f.packet(), 0); f.dispatcher.loop();
    CHECK(f.radio.attempts == 1 && f.dispatcher.wakes.back() == 501);
    f.clock.now = 599; f.dispatcher.loop(); CHECK(f.radio.attempts == 1);
    f.clock.now = 609; f.dispatcher.loop(); CHECK(f.radio.attempts == 2);
    f.complete(); CHECK(f.pool.getFreeCount() == 32);
}
TEST(dispatch_admin_budget, "UNIT-DISPATCH-008", "Due admin packet can transmit after budget depletion") {
    Fixture f; f.begin(10);
    f.dispatcher.sendPacket(f.packet(), 0); f.dispatcher.loop(); f.complete();
    f.dispatcher.sendPacket(f.packet(PAYLOAD_TYPE_REQ), 0, 10);
    f.dispatcher.loop(); CHECK(f.radio.attempts == 1);
    f.clock.now += 10; f.dispatcher.loop(); CHECK(f.radio.attempts == 2);
    f.complete(); CHECK(f.pool.getFreeCount() == 32);
}
TEST(dispatch_rx_drain, "UNIT-DISPATCH-009", "One wake drains RX ring rejects invalid packets and returns slots") {
    Fixture f; f.begin();
    f.radio.incoming = {{8}, {9, 0, 0xAA}, {0x49, 0, 0xAA}, {9, 0xC0, 0xAA}, {10, 0, 0xBB}};
    f.dispatcher.loop();
    CHECK(f.radio.incoming.empty() && f.dispatcher.received.size() == 2);
    CHECK(f.dispatcher.received[0]._snr == -9);
    CHECK(f.dispatcher.getNumRecvFlood() == 1 && f.dispatcher.getNumRecvDirect() == 1);
    CHECK(f.dispatcher.getReceiveAirTime() == 200 && f.pool.getFreeCount() == 32);
}
TEST(dispatch_rx_ownership, "UNIT-DISPATCH-010", "Manual hold and delayed retransmit retain correct ownership") {
    Fixture f; f.begin(); f.dispatcher.action = ACTION_MANUAL_HOLD;
    f.radio.incoming = {{9, 0, 0xAA}}; f.dispatcher.loop();
    CHECK(f.dispatcher.held.size() == 1 && f.pool.getFreeCount() == 31);
    f.dispatcher.releasePacket(f.dispatcher.held[0]); f.dispatcher.held.clear();
    f.dispatcher.action = ACTION_RETRANSMIT_DELAYED(3, 200);
    f.radio.incoming = {{9, 0, 0xBB}}; f.dispatcher.loop();
    CHECK(f.pool.getOutboundTotal() == 1 && f.radio.attempts == 0);
    CHECK(f.dispatcher.wakes.back() == 200 && f.pool.peekNextOutboundPriority(300) == 3);
    f.clock.now = 300; f.dispatcher.loop(); f.complete(); CHECK(f.pool.getFreeCount() == 32);
}
TEST(dispatch_maintenance, "UNIT-DISPATCH-011", "Maintenance never consumes TX completion and reports CAD changes") {
    Fixture f; f.begin(); f.dispatcher.sendPacket(f.packet(), 0); f.dispatcher.loop();
    f.radio.completion = true; auto queries = f.radio.completion_queries;
    f.dispatcher.maintenanceLoop(); CHECK(f.radio.completion_queries == queries && f.radio.completion);
    f.radio.cad_offset = 2; f.dispatcher.maintenanceLoop(); f.dispatcher.maintenanceLoop();
    CHECK(f.dispatcher.offsets == std::vector<int8_t>({2}));
    CHECK(f.radio.cad_ticks == 3 && f.radio.radio_ticks == 3);
    f.dispatcher.loop(); CHECK(f.dispatcher.getNumSentFlood() == 1 && f.pool.getFreeCount() == 32);
}
TEST(dispatch_stall, "UNIT-DISPATCH-012", "Stall watchdog latches without perpetual immediate deadline") {
    Fixture f; f.begin(); f.radio.recv_mode = false; f.dispatcher.maintenanceLoop();
    CHECK(f.dispatcher.msUntilNextMaintenance() == 8000);
    f.clock.now += 8001; f.dispatcher.maintenanceLoop();
    CHECK((f.dispatcher.getErrFlags() & ERR_EVENT_STARTRX_TIMEOUT) != 0);
    CHECK(f.dispatcher.msUntilNextMaintenance() == mesh::MAINTENANCE_IDLE);
    f.radio.maintenance = 75; CHECK(f.dispatcher.msUntilNextMaintenance() == 75);
}
TEST(dispatch_invalid_send, "UNIT-DISPATCH-013", "Invalid outbound packet releases slot without queue wake") {
    Fixture f; f.begin();
    auto p = f.packet(); p->path_len = 0xC0; f.dispatcher.sendPacket(p, 0);
    p = f.packet(); p->payload_len = 185; f.dispatcher.sendPacket(p, 0);
    CHECK(f.pool.getFreeCount() == 32 && f.pool.getOutboundTotal() == 0 && f.dispatcher.wakes.empty());
    std::vector<mesh::Packet*> held;
    for (int i = 0; i < 32; ++i) held.push_back(f.packet());
    CHECK(f.dispatcher.obtainNewPacket() == nullptr);
    CHECK((f.dispatcher.getErrFlags() & ERR_EVENT_FULL) != 0);
    for (auto slot : held) f.dispatcher.releasePacket(slot);
    f.dispatcher.resetStats(); CHECK(f.dispatcher.getErrFlags() == 0);
}
TEST(dispatch_truncated_header, "UNIT-DISPATCH-014", "Dispatcher parser rejects incomplete route headers before reading") {
    Fixture f; f.begin();
    for (uint8_t header : {0x08, 0x09, 0x0A, 0x0B}) {
        unsigned required = (header == 0x08 || header == 0x0B) ? 6 : 2;
        for (unsigned len = 1; len < required; ++len) {
            std::vector<uint8_t> input(len, 0); if (len) input[0] = header;
            mesh::Packet p;
            CHECK(!f.dispatcher.tryParsePacket(&p, input.data(), len));
        }
    }
    mesh::Packet p;
    CHECK(!f.dispatcher.tryParsePacket(&p, nullptr, 0));
    CHECK(!f.dispatcher.tryParsePacket(&p, nullptr, -1));
    std::vector<uint8_t> oversized(256, 0);
    CHECK(!f.dispatcher.tryParsePacket(&p, oversized.data(), 256));
}
TEST(dispatch_airtime_reset, "UNIT-DISPATCH-015", "TX accounting uses modem airtime across stats reset while active") {
    Fixture f; f.begin();
    f.dispatcher.sendPacket(f.packet(PAYLOAD_TYPE_TXT_MSG, ROUTE_TYPE_DIRECT), 0);
    f.dispatcher.loop(); f.clock.now = 350;
    f.dispatcher.resetStats(); f.complete();
    CHECK(f.dispatcher.getNumSentDirect() == 1 && f.dispatcher.getNumSentFlood() == 0);
    CHECK(f.dispatcher.getTotalAirTime() == 100); // wall time was 250ms
    CHECK(f.radio.finished == 1 && f.pool.getFreeCount() == 32);
}
TEST(dispatch_clock_wrap, "UNIT-DISPATCH-016", "Delayed send and TX expiry operate across clock wrap") {
    Fixture f; f.clock.now = 0xFFFFFFF0u; f.begin();
    f.dispatcher.sendPacket(f.packet(), 1, 32);
    f.clock.now = 0x0Fu; f.dispatcher.loop(); CHECK(f.radio.attempts == 0);
    f.clock.now = 0x10u; f.dispatcher.loop(); CHECK(f.radio.attempts == 1);
    f.clock.now = 0x10u + 301u; f.dispatcher.loop(); CHECK(f.dispatcher.failures == 0);
    f.radio.active = false; f.dispatcher.loop();
    CHECK(f.dispatcher.failures == 1 && f.pool.getFreeCount() == 32);
}
TEST(dispatch_generated_rx, "UNIT-DISPATCH-017", "Generated dispatcher inputs obey version header and path bounds") {
    Fixture f; f.begin(); uint32_t seed = 0xD15CA7C4;
    for (unsigned len = 0; len <= 255; ++len) for (unsigned n = 0; n < 16; ++n) {
        std::vector<uint8_t> input(len);
        for (auto& b : input) b = nextRandom(seed) >> 24;
        mesh::Packet p;
        if (f.dispatcher.tryParsePacket(&p, input.data(), len)) {
            CHECK(p.getPayloadVer() == 0 && p.getPathHashSize() <= 3);
            CHECK(p.getPathByteLen() <= 64 && p.payload_len <= 184);
            CHECK(p.getRawLength() == int(len));
        }
    }
    // Retain the existing Dispatcher contract for a complete, empty-payload frame.
    // Packet::readFrom has a stricter nonempty-payload contract; don't conflate them.
    const uint8_t empty[] = {9, 0}; mesh::Packet p;
    CHECK(f.dispatcher.tryParsePacket(&p, empty, sizeof(empty)) && p.payload_len == 0);
}

TEST(dispatch_foreign_rx, "UNIT-DISPATCH-018", "A consumed foreign frame never reaches the MeshCore parser") {
    Fixture f; f.begin();
    const std::vector<uint8_t> meshcore = {9, 0, 0xAA}, other = {0xDE, 0xAD, 0xBE, 0xEF, 1, 2, 3};
    f.radio.incoming = {other, meshcore};
    f.dispatcher.consume_foreign = true; f.dispatcher.loop();
    CHECK(f.dispatcher.foreign_rx.size() == 2 && f.dispatcher.received.empty());
    CHECK(f.pool.getFreeCount() == 32);  // nothing was allocated for a consumed frame
    f.radio.incoming = {meshcore};
    f.dispatcher.consume_foreign = false; f.dispatcher.loop();
    CHECK(f.dispatcher.foreign_rx.size() == 3 && f.dispatcher.received.size() == 1);
    CHECK(f.dispatcher.received[0].header == 9 && f.dispatcher.received[0].payload_len == 1);
}
TEST(dispatch_foreign_tx, "UNIT-DISPATCH-019", "A foreign frame goes out as its payload alone, flagged for the radio") {
    Fixture f; f.begin();
    std::vector<uint8_t> frame(40);
    for (size_t i = 0; i < frame.size(); i++) frame[i] = (uint8_t)(i * 3 + 1);
    CHECK(f.dispatcher.sendForeign(frame.data(), (int)frame.size(), 1, 0));
    CHECK(!f.dispatcher.sendForeign(frame.data(), 0, 1, 0));
    std::vector<uint8_t> big(185, 1);
    CHECK(!f.dispatcher.sendForeign(big.data(), (int)big.size(), 1, 0));
    f.dispatcher.loop();
    CHECK(f.radio.sent.size() == 1 && f.radio.sent[0] == frame && f.radio.sent_foreign[0]);
    f.complete();
    CHECK(f.pool.getFreeCount() == 32 && f.dispatcher.successes == 1);
    // A MeshCore packet queued afterwards is not flagged and keeps its framing.
    f.dispatcher.sendPacket(f.packet(), 2, 0); f.dispatcher.loop();
    CHECK(f.radio.sent.size() == 2 && !f.radio.sent_foreign[1] &&
          f.radio.sent[1] == std::vector<uint8_t>({9, 0, 0xAA}));
    f.complete();
}

TEST(dispatch_foreign_airtime, "UNIT-DISPATCH-021", "A foreign TX is charged its own airtime on its payload length and is not a MeshCore flood") {
    Fixture f; f.begin();
    std::vector<uint8_t> frame(40, 7);
    CHECK(f.dispatcher.sendForeign(frame.data(), (int)frame.size(), 1, 0));
    f.dispatcher.loop(); f.complete();
    CHECK(f.radio.foreign_airtime_len == 40 && f.dispatcher.getTotalAirTime() == 60);
    CHECK(f.dispatcher.getNumSentFlood() == 0 && f.dispatcher.getNumSentDirect() == 0);
    f.dispatcher.sendPacket(f.packet(), 2, 0); f.dispatcher.loop(); f.complete();
    CHECK(f.dispatcher.getTotalAirTime() == 160 && f.dispatcher.getNumSentFlood() == 1);
}

TEST(dispatch_foreign_rx_airtime, "UNIT-DISPATCH-022", "A consumed foreign frame counts as receive airtime, with its own preamble; a MeshCore one with the MeshCore estimate") {
    Fixture f; f.begin();
    const std::vector<uint8_t> meshcore = {9, 0, 0xAA}, other(30, 0xDE);
    f.radio.incoming = {other};
    f.dispatcher.consume_foreign = true; f.dispatcher.loop();
    CHECK(f.radio.foreign_airtime_len == 30 && f.dispatcher.getReceiveAirTime() == 60);
    f.radio.incoming = {meshcore};
    f.dispatcher.consume_foreign = false; f.dispatcher.loop();
    CHECK(f.dispatcher.getReceiveAirTime() == 160);
}

TEST(dispatch_foreign_queue, "UNIT-DISPATCH-023", "Foreign frames in the queue are counted, and the lowest priority leaves after a due MeshCore packet") {
    Fixture f; f.begin();
    CHECK(f.dispatcher.getForeignQueued() == 0);
    std::vector<uint8_t> frame(30, 9);
    for (int i = 0; i < 3; i++) CHECK(f.dispatcher.sendForeign(frame.data(), (int)frame.size(), 0xFE, 0));
    f.dispatcher.sendPacket(f.packet(), 5, 0);
    CHECK(f.dispatcher.getForeignQueued() == 3);
    f.dispatcher.loop();
    CHECK(f.radio.sent.size() == 1 && !f.radio.sent_foreign[0]);   // the MeshCore packet first
    f.complete();
    f.dispatcher.loop();
    CHECK(f.radio.sent.size() == 2 && f.radio.sent_foreign[1]);     // then the foreign ones
    CHECK(f.dispatcher.getForeignQueued() == 2);
}

TEST(dispatch_tx_stalls, "UNIT-DISPATCH-020", "TX stalls are counted per cause with age and longest streak") {
    mesh::TxStall lbt, rx;
    char buf[96];
    mesh::formatTxStalls(buf, sizeof(buf), lbt, rx, 5000);
    CHECK(std::string(buf) == "lbt n=0 | rx n=0");
    lbt.record(10000, 4100);
    lbt.record(20000, 8200);
    lbt.record(30000, 4100);
    rx.record(0, 4050);  // uptime 0 still reads as "happened"
    mesh::formatTxStalls(buf, sizeof(buf), lbt, rx, 95000);
    CHECK(std::string(buf) == "lbt n=3 ago=65s max=8200ms | rx n=1 ago=94s max=4050ms");
    CHECK(mesh::formatTxStalls(buf, 8, lbt, rx, 95000) > 8);  // truncated, never overrun
    CHECK(std::strlen(buf) == 7);
}
