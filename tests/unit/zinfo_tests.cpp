// helpers/ZInfo.h: the one-line replies of "get z.info" and "get z.stats".
#include "test.h"
#include "ZInfo.h"

#include <string>

namespace {
zinfo::Info full()
{
    zinfo::Info i = {};
    i.fw = "1.18.2-zephcore";
    i.uptime_s = 93784;
    i.batt_mv = 4231;
    i.flash_used_kb = 96;
    i.flash_total_kb = 2048;
    i.heap_used = 12595;
    i.heap_total = 45056;
    i.heap_peak = 20582;
    i.contacts = 351;
    i.max_contacts = 350;
    return i;
}

std::string info(const zinfo::Info &i, size_t cap = 161)
{
    char out[161] = "";
    zinfo::formatInfo(out, cap, i);
    return out;
}
}

TEST(zinfo_full, "UNIT-ZINFO-001", "z.info gives version, uptime, battery, flash, heap and contacts in one line") {
    CHECK(info(full()) == "> fw 1.18.2-zephcore | up 1d02h | battery 4231mV | flash 96/2048K | heap 12.3/44.0K (peak 20.1K) | contacts 351/350");
}

TEST(zinfo_missing, "UNIT-ZINFO-002", "a field the role or build does not have is left out of the line") {
    zinfo::Info i = full();
    i.flash_total_kb = 0;
    i.heap_total = 0;
    i.max_contacts = 0;
    CHECK(info(i) == "> fw 1.18.2-zephcore | up 1d02h | battery 4231mV");
}

TEST(zinfo_uptime, "UNIT-ZINFO-003", "uptime is days and hours, hours and minutes, or minutes") {
    char out[16];
    zinfo::formatUptime(out, sizeof(out), 59);
    CHECK(std::string(out) == "0m");
    zinfo::formatUptime(out, sizeof(out), 3 * 3600 + 5 * 60);
    CHECK(std::string(out) == "3h05m");
    zinfo::formatUptime(out, sizeof(out), 86400);
    CHECK(std::string(out) == "1d00h");
}

TEST(zinfo_cap, "UNIT-ZINFO-004", "a small reply buffer is cut, never overrun, and stays terminated") {
    char out[20];
    std::fill(out, out + sizeof(out), 'x');
    zinfo::formatInfo(out, sizeof(out), full());
    CHECK(out[sizeof(out) - 1] == '\0' || std::string(out).size() < sizeof(out));
    CHECK(std::string(out).size() < sizeof(out));
}

TEST(zinfo_stats, "UNIT-ZINFO-005", "z.stats gives the MeshCore counters, then the Meshtastic ones when there are any") {
    zinfo::Stats s = {};
    s.recv = 1234; s.sent = 56; s.errors = 3;
    s.flood_tx = 40; s.flood_rx = 16; s.direct_tx = 20; s.direct_rx = 9;
    s.tx_air_s = 12; s.rx_air_s = 340; s.queue = 0;
    char out[161];
    zinfo::formatStats(out, sizeof(out), s, "heard=5 relayed=0");
    CHECK(std::string(out) == "> mc: rx 1234, tx 56, err 3, flood 40/16, direct 20/9, air 12/340s, queue 0 | mt: heard=5 relayed=0");
    zinfo::formatStats(out, sizeof(out), s, nullptr);
    CHECK(std::string(out) == "> mc: rx 1234, tx 56, err 3, flood 40/16, direct 20/9, air 12/340s, queue 0");
}
