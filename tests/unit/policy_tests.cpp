#include "test.h"
#include <RoutingPolicy.h>
#include <OffgridPolicy.h>
#include <RateLimiter.h>
#include <mesh/Maintenance.h>
#include <AutoShutdownCli.h>
using namespace mesh;
static constexpr size_t CLI_REPLY_SIZE_TEST = 256;

TEST(reply_routes, "UNIT-POLICY-001", "Reply routing and scope truth tables") {
    const ReplyRoute routes[] = {REPLY_ROUTE_FLOOD, REPLY_ROUTE_DIRECT_OUT_PATH,
        REPLY_ROUTE_DIRECT_SUPPLIED, REPLY_ROUTE_DIRECT_SUPPLIED,
        REPLY_ROUTE_PATH_RETURN, REPLY_ROUTE_PATH_RETURN, REPLY_ROUTE_PATH_RETURN, REPLY_ROUTE_PATH_RETURN};
    const ReplyScope scopes[] = {REPLY_SCOPE_NONE, REPLY_SCOPE_DEFAULT, REPLY_SCOPE_NONE,
        REPLY_SCOPE_NONE, REPLY_SCOPE_REQUEST, REPLY_SCOPE_REQUEST, REPLY_SCOPE_REQUEST, REPLY_SCOPE_REQUEST};
    for (int bits = 0; bits < 8; ++bits) {
        CHECK(chooseReplyRoute(bits & 4, bits & 2, bits & 1) == routes[bits]);
        CHECK(chooseReplyScope(bits & 4, bits & 2, bits & 1) == scopes[bits]);
    }
}
TEST(hop_limits, "UNIT-POLICY-002", "Scoped unscoped and advert hop boundaries") {
    Packet p;
    p.transport_codes[0] = 0x1234; p.transport_codes[1] = 0;
    for (uint8_t route : {ROUTE_TYPE_FLOOD, ROUTE_TYPE_TRANSPORT_FLOOD}) {
        for (uint8_t type : {PAYLOAD_TYPE_ADVERT, PAYLOAD_TYPE_TXT_MSG}) {
            p.header = route | (type << 2);
            unsigned limit = type == PAYLOAD_TYPE_ADVERT ? 2 : (route == ROUTE_TYPE_FLOOD ? 3 : 5);
            for (unsigned hops = 0; hops <= 6; ++hops) {
                p.setPathHashSizeAndCount(3, hops);
                CHECK(isFloodHopLimitExceeded(&p, 5, 3, 2) == (hops >= limit));
            }
        }
    }
    p.header = ROUTE_TYPE_FLOOD; p.path_len = 0;
    CHECK(isFloodHopLimitExceeded(&p, 5, 0, 2));
}
TEST(code_zero_unscoped, "UNIT-POLICY-003", "Transport flood with code 0 is un-scoped, other codes and routes are not") {
    Packet p;
    p.transport_codes[1] = 0x5678;
    for (uint16_t code : {0x0000, 0x0001, 0xFFFE}) {
        p.transport_codes[0] = code;
        p.header = ROUTE_TYPE_TRANSPORT_FLOOD | (PAYLOAD_TYPE_TXT_MSG << 2);
        CHECK(isUnscopedFlood(&p) == (code == 0));
        p.setPathHashSizeAndCount(1, 3);
        CHECK(isFloodHopLimitExceeded(&p, 5, 3, 2) == (code == 0));
        p.setPathHashSizeAndCount(1, 0);
        CHECK(isFloodHopLimitExceeded(&p, 5, 0, 2) == (code == 0));
        p.header = ROUTE_TYPE_TRANSPORT_DIRECT | (PAYLOAD_TYPE_TXT_MSG << 2);
        CHECK(!isUnscopedFlood(&p));
        p.header = ROUTE_TYPE_FLOOD | (PAYLOAD_TYPE_TXT_MSG << 2);
        CHECK(isUnscopedFlood(&p));
        p.header = ROUTE_TYPE_DIRECT | (PAYLOAD_TYPE_TXT_MSG << 2);
        CHECK(!isUnscopedFlood(&p));
    }
}
TEST(rate_window, "UNIT-RATE-001", "Rate window threshold expiry and reset") {
    RateLimiter limiter(2, 10);
    CHECK(limiter.allow(100)); CHECK(limiter.allow(101));
    CHECK(!limiter.allow(109)); CHECK(limiter.allow(110));
    CHECK(limiter.allow(111)); CHECK(!limiter.allow(112));
    limiter.reset(); CHECK(limiter.allow(113)); CHECK(limiter.allow(114));
    CHECK(!limiter.allow(115));
}
TEST(maintenance_wrap, "UNIT-TIME-001", "Maintenance deadlines across uint32 wrap") {
    CHECK(maintenanceUntil(100, 101) == 1);
    CHECK(maintenanceUntil(100, 100) == 0);
    CHECK(maintenanceUntil(100, 99) == 0);
    CHECK(maintenanceUntil(0xFFFFFFF0u, 0x10u) == 32);
    CHECK(maintenanceUntil(0x10u, 0xFFFFFFF0u) == 0);
    CHECK(maintenanceSooner(MAINTENANCE_IDLE, 32) == 32);
    CHECK(maintenanceSooner(0, MAINTENANCE_IDLE) == 0);
}

namespace {
NodePrefs offgridPrefs(float freq, uint8_t repeat = 0, float home = 0.0f) {
    NodePrefs p{};
    p.freq = freq; p.bw = 62.5f; p.sf = 7; p.cr = 5; p.tx_power_dbm = 22;
    p.client_repeat = repeat; p.offgrid_home_freq = home;
    return p;
}
}  // namespace

TEST(offgrid_freq_pick, "UNIT-OFFGRID-001", "Off-grid frequency: the three allowed, nearest per band, none above sub-GHz") {
    CHECK(isOffgridFreq(433.0f) && isOffgridFreq(869.495f) && isOffgridFreq(918.0f));
    CHECK(!isOffgridFreq(869.618f) && !isOffgridFreq(869.494f) && !isOffgridFreq(910.525f));
    CHECK(isOffgridFreqKhz(869495) && !isOffgridFreqKhz(869496));
    CHECK(nearestOffgridFreqKhz(869.618f) == 869495);
    CHECK(nearestOffgridFreqKhz(868.1f) == 869495);
    CHECK(nearestOffgridFreqKhz(910.525f) == 918000);
    CHECK(nearestOffgridFreqKhz(927.875f) == 918000);
    CHECK(nearestOffgridFreqKhz(433.875f) == 433000);
    CHECK(nearestOffgridFreqKhz(150.0f) == 433000);
    CHECK(nearestOffgridFreqKhz(869.495f) == 869495);
    CHECK(nearestOffgridFreqKhz(2445.0f) == 0);
    CHECK(nearestOffgridFreqKhz(0.0f) == 0);
}
TEST(offgrid_toggle, "UNIT-OFFGRID-002", "Off-grid toggle: to the off-grid frequency and back, preset otherwise untouched") {
    NodePrefs p = offgridPrefs(869.618f);
    CHECK(!offgridActive(p));
    CHECK(offgridSet(p, true) == OFFGRID_RETUNED);
    CHECK(offgridActive(p) && p.freq == 869.495f && p.offgrid_home_freq == 869.618f);
    CHECK(p.bw == 62.5f && p.sf == 7 && p.cr == 5 && p.tx_power_dbm == 22);
    CHECK(offgridSet(p, true) == OFFGRID_UNCHANGED);
    CHECK(offgridSet(p, false) == OFFGRID_RETUNED);
    CHECK(!offgridActive(p) && p.freq == 869.618f && p.offgrid_home_freq == 0.0f && p.client_repeat == 0);
    CHECK(offgridSet(p, false) == OFFGRID_UNCHANGED);

    // Already on an off-grid frequency (the app left it there): flag only, both ways.
    p = offgridPrefs(918.0f);
    CHECK(offgridSet(p, true) == OFFGRID_SET);
    CHECK(offgridActive(p) && p.freq == 918.0f && p.offgrid_home_freq == 0.0f);
    CHECK(offgridSet(p, false) == OFFGRID_SET);
    CHECK(!offgridActive(p) && p.freq == 918.0f);

    // No off-grid frequency on 2.4 GHz: refused, nothing changes.
    p = offgridPrefs(2445.0f);
    CHECK(offgridSet(p, true) == OFFGRID_NO_BAND);
    CHECK(p.client_repeat == 0 && p.freq == 2445.0f && p.offgrid_home_freq == 0.0f);
}
TEST(offgrid_normalize, "UNIT-OFFGRID-003", "Off-grid: a repeat flag on a mesh frequency is dropped, app and CLI edits drop the home frequency") {
    // What the pre-1.17.8 device toggle left behind.
    NodePrefs p = offgridPrefs(869.618f, 1);
    CHECK(!offgridActive(p));
    CHECK(offgridNormalize(p) && p.client_repeat == 0 && p.freq == 869.618f);
    CHECK(!offgridNormalize(p));
    // Off-grid mode on is left alone, home frequency included.
    p = offgridPrefs(869.495f, 1, 869.618f);
    CHECK(!offgridNormalize(p) && offgridActive(p) && p.offgrid_home_freq == 869.618f);
    // The app turned repeat off and stayed on the off-grid frequency.
    p = offgridPrefs(869.495f, 0, 869.618f);
    offgridFreqEdited(p);
    CHECK(p.offgrid_home_freq == 0.0f && p.freq == 869.495f && p.client_repeat == 0);
    // The app moved to another off-grid frequency with repeat on: stays on, home gone.
    p = offgridPrefs(433.0f, 1, 869.618f);
    offgridFreqEdited(p);
    CHECK(offgridActive(p) && p.offgrid_home_freq == 0.0f);
    CHECK(offgridSet(p, false) == OFFGRID_SET && p.freq == 433.0f);
    // The CLI set a mesh frequency while off-grid mode was on: off, and nothing to return to.
    p = offgridPrefs(868.0f, 1, 869.618f);
    offgridFreqEdited(p);
    CHECK(p.client_repeat == 0 && p.offgrid_home_freq == 0.0f && p.freq == 868.0f);
}

TEST(autoshutdown_cli, "UNIT-POLICY-AUTOSHUTDOWN-001", "get|set autoshutdown: 0 or 2900 to the ceiling, digits only, changed flag") {
    NodePrefs p = {};
    char reply[CLI_REPLY_SIZE_TEST];
    bool changed = false;
    const uint16_t ceiling = AUTO_SHUTDOWN_MAX_MV;  // a build without a battery wake

    p.auto_shutdown_mv = 3200;
    CHECK(autoshutdown::command(p, "get autoshutdown", ceiling, reply, sizeof(reply), &changed));
    CHECK(strcmp(reply, "autoshutdown: 3200 mV") == 0);
    CHECK(!changed);

    CHECK(autoshutdown::command(p, "set autoshutdown 3400", ceiling, reply, sizeof(reply), &changed));
    CHECK(strcmp(reply, "OK - autoshutdown 3400 mV") == 0);
    CHECK(changed && p.auto_shutdown_mv == 3400 && p.auto_shutdown_set == 1);

    changed = false;
    CHECK(autoshutdown::command(p, "set autoshutdown 0", ceiling, reply, sizeof(reply), &changed));
    CHECK(strcmp(reply, "OK - autoshutdown off") == 0);
    CHECK(changed && p.auto_shutdown_mv == 0);
    CHECK(autoshutdown::command(p, "get autoshutdown", ceiling, reply, sizeof(reply), &changed));
    CHECK(strcmp(reply, "autoshutdown: off") == 0);

    // Both ends are accepted.
    CHECK(autoshutdown::command(p, "set autoshutdown 2900", ceiling, reply, sizeof(reply), &changed));
    CHECK(p.auto_shutdown_mv == 2900);
    CHECK(autoshutdown::command(p, "set autoshutdown 4200  ", ceiling, reply, sizeof(reply), &changed));
    CHECK(p.auto_shutdown_mv == 4200);

    // Rejected input stores nothing and does not flag a change.
    p.auto_shutdown_mv = 3200;
    changed = false;
    for (const char *bad : {"set autoshutdown abc", "set autoshutdown 3.4", "set autoshutdown -5",
                            "set autoshutdown 3400x", "set autoshutdown "}) {
        CHECK(autoshutdown::command(p, bad, ceiling, reply, sizeof(reply), &changed));
        CHECK(strcmp(reply, "ERROR: numbers only (0 = off, 2900-4200 mV)") == 0);
        CHECK(!changed && p.auto_shutdown_mv == 3200);
    }
    for (const char *bad : {"set autoshutdown 1", "set autoshutdown 2899", "set autoshutdown 4201",
                            "set autoshutdown 70000", "set autoshutdown 99999999999999999999"}) {
        CHECK(autoshutdown::command(p, bad, ceiling, reply, sizeof(reply), &changed));
        CHECK(strcmp(reply, "ERROR: must be 0 (off) or 2900-4200 mV") == 0);
        CHECK(!changed && p.auto_shutdown_mv == 3200);
    }

    // Not ours: left for the other handlers.
    CHECK(!autoshutdown::command(p, "get power", ceiling, reply, sizeof(reply), &changed));
    CHECK(!autoshutdown::command(p, "set autoshutdownx 1", ceiling, reply, sizeof(reply), &changed));
}

TEST(z_autoshutdown_cli, "UNIT-POLICY-AUTOSHUTDOWN-002", "get|set z.autoshutdown: the same setting in the z. replies, capped under the wake") {
    NodePrefs p = {};
    char reply[CLI_REPLY_SIZE_TEST];
    bool changed = false;
    // A board that wakes at 3600 mV: nothing above 3300 mV may be stored.
    const uint16_t ceiling = 3600 - AUTO_SHUTDOWN_WAKE_MARGIN_MV;

    CHECK(autoshutdown::zCommand(p, "get z.autoshutdown", ceiling, reply, sizeof(reply), &changed));
    CHECK(strcmp(reply, "> off") == 0 && !changed);
    CHECK(autoshutdown::zCommand(p, "set z.autoshutdown 3300", ceiling, reply, sizeof(reply), &changed));
    CHECK(strcmp(reply, "OK") == 0 && changed && p.auto_shutdown_mv == 3300 && p.auto_shutdown_set == 1);
    CHECK(autoshutdown::zCommand(p, "get z.autoshutdown", ceiling, reply, sizeof(reply), &changed));
    CHECK(strcmp(reply, "> 3300 mV") == 0);

    changed = false;
    for (const char *bad : {"set z.autoshutdown abc", "set z.autoshutdown 3301", "set z.autoshutdown 4200",
                            "set z.autoshutdown 2899", "set z.autoshutdown -1", "set z.autoshutdown "}) {
        CHECK(autoshutdown::zCommand(p, bad, ceiling, reply, sizeof(reply), &changed));
        CHECK(strcmp(reply, "Error: 0 (off) or 2900-3300 mV") == 0 && !changed && p.auto_shutdown_mv == 3300);
    }
    CHECK(!autoshutdown::zCommand(p, "get autoshutdown", ceiling, reply, sizeof(reply), &changed));
    CHECK(!autoshutdown::zCommand(p, "get z.info", ceiling, reply, sizeof(reply), &changed));

    CHECK(autoshutdown::isZCommand("get z.autoshutdown") && autoshutdown::isZCommand("set z.autoshutdown 1"));
    CHECK(!autoshutdown::isZCommand("get z.autoshutdownx") && !autoshutdown::isZCommand("set autoshutdown 3300"));
}

TEST(autoshutdown_sane, "UNIT-POLICY-AUTOSHUTDOWN-003", "What the CLI accepts is exactly what sanitizeNodePrefs keeps") {
    for (uint16_t ceiling : {(uint16_t)AUTO_SHUTDOWN_MAX_MV, (uint16_t)3300}) {
        for (uint32_t mv = 0; mv <= 5000; mv++) {
            char arg[8];
            uint16_t parsed = 0;
            snprintf(arg, sizeof(arg), "%u", (unsigned)mv);
            bool cli_ok = autoshutdown::parseMv(arg, ceiling, &parsed) == autoshutdown::Parse::OK;
            CHECK(cli_ok == auto_shutdown_mv_valid((uint16_t)mv, ceiling));
            CHECK(!cli_ok || parsed == mv);
        }
    }

    // A stored value outside the range falls back to the role's default (0 here: not a companion).
    NodePrefs p = {};
    p.auto_shutdown_mv = 5000;
    sanitizeNodePrefs(&p);
    CHECK(p.auto_shutdown_mv == 0);
    p.auto_shutdown_mv = 3300;
    sanitizeNodePrefs(&p);
    CHECK(p.auto_shutdown_mv == 3300);
}

TEST(autoshutdown_default, "UNIT-POLICY-AUTOSHUTDOWN-004", "Auto-shutdown starts off on a role that is not a companion, whatever the Kconfig threshold") {
    static_assert(CONFIG_ZEPHCORE_AUTO_SHUTDOWN_MILLIVOLTS == 3200, "the test build sets a Kconfig threshold");
    NodePrefs p;
    initNodePrefs(&p);
    CHECK(p.auto_shutdown_mv == 0);

    p.auto_shutdown_mv = 4500;
    sanitizeNodePrefs(&p);
    CHECK(p.auto_shutdown_mv == 0);
}
