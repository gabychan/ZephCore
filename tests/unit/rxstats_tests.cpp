// helpers/RxStats.h: the hop histogram and the frames per relay of "get z.mc|mt.hopstats" and "get z.mc|mt.relays".
#include "test.h"
#include "RxStats.h"

#include <string>

namespace {
std::string hops(const rxstats::Observer &o) { char out[100]; o.formatHops(out, sizeof(out)); return out; }
std::string relays(const rxstats::Observer &o, size_t cap = 100) { char out[100]; o.formatRelays(out, cap); return out; }
std::string cmd(const rxstats::Observer &o, const char *line)
{
    char out[100];
    rxstats::handleCommand(o, line, out, sizeof(out));
    return out;
}
const uint8_t a[1] = {0x3f}, b[2] = {0xa1, 0xb2}, c[3] = {0x01, 0x02, 0x03};
}

TEST(rx_hops, "UNIT-RX-001", "Frames are counted by hops crossed, 7 or more together, unknown apart; empty buckets are left out") {
    rxstats::Observer o;
    CHECK(hops(o) == "> none");
    o.heard(0, nullptr, 0);
    o.heard(0, nullptr, 0);
    o.heard(3, nullptr, 0);
    o.heard(7, nullptr, 0);
    o.heard(40, nullptr, 0);
    o.heard(rxstats::kUnknownHops, nullptr, 0);
    CHECK(hops(o) == "> 0:2 3:1 7+:2 ?:1");
}

TEST(rx_relays, "UNIT-RX-002", "Frames are counted per relay of 1 to 3 bytes, most frames first; no relay counts none") {
    rxstats::Observer o;
    CHECK(relays(o) == "> none");
    o.heard(1, b, 2);
    o.heard(1, a, 1);
    o.heard(1, a, 1);
    o.heard(1, c, 3);
    o.heard(1, nullptr, 0);
    o.heard(1, c, 4);
    CHECK(relays(o) == "> 3f:2 a1b2:1 010203:1");
}

TEST(rx_relays_full, "UNIT-RX-003", "A full table drops the relay that carried least to make room") {
    rxstats::Observer o;
    for (uint8_t i = 1; i <= 16; i++) {
        const uint8_t id[1] = {i};
        for (uint8_t n = 0; n < i; n++) o.heard(1, id, 1);
    }
    const uint8_t fresh[1] = {0xee};
    o.heard(1, fresh, 1);
    const std::string r = relays(o);
    CHECK(r.find(" ee:1") != std::string::npos);
    CHECK(r.find(" 01:1") == std::string::npos);
    CHECK(r.find(" 10:16") != std::string::npos);
}

TEST(rx_relays_fit, "UNIT-RX-004", "A line that does not fit the reply is left out whole") {
    rxstats::Observer o;
    o.heard(1, b, 2);
    o.heard(1, a, 1);
    o.heard(1, a, 1);
    CHECK(relays(o, 9) == "> 3f:2");
    CHECK(relays(o, 100) == "> 3f:2 a1b2:1");
}

TEST(rx_clear, "UNIT-RX-005", "clear empties both tables") {
    rxstats::Observer o;
    o.heard(2, a, 1);
    o.clear();
    CHECK(hops(o) == "> none" && relays(o) == "> none");
}

TEST(rx_commands, "UNIT-RX-006", "get answers the histogram or the relays of its protocol; set is refused; other z commands are not handled") {
    CHECK(rxstats::isCommand("get z.mc.hopstats") && rxstats::isCommand("get z.mt.relays") && rxstats::isCommand("set z.mc.relays"));
    CHECK(!rxstats::isCommand("get z.mt.hops") && !rxstats::isCommand("get z.mc.neighbors") && !rxstats::isCommand("get z.mc.relays x"));
    CHECK(rxstats::isMeshtastic("get z.mt.relays") && !rxstats::isMeshtastic("get z.mc.relays"));
    rxstats::Observer o;
    o.heard(1, a, 1);
    CHECK(cmd(o, "get z.mc.hopstats") == "> 1:1");
    CHECK(cmd(o, "get z.mt.relays") == "> 3f:1");
    CHECK(cmd(o, "set z.mc.relays").rfind("Error", 0) == 0);
}
