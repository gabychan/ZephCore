// helpers/PingBot.h: the ping bot's trigger rules, answer text, route, counters and "z.*ping.*" commands.
#include "test.h"
#include "PingBot.h"

#include <string>

namespace {
#define ARROW "\xE2\x86\x93"

ping::Prefs on()
{
    ping::Prefs p;
    ping::setDefaults(p);
    ping::setTrigger(p.mc, "/ping");
    ping::setTrigger(p.mt, "/ping");
    p.mc.enable = 1;
    p.mt.enable = 1;
    return p;
}

/* Names for the test: hash 0x3f is "Montjuic RPT", 0x40 is shared by two nodes, 0x41 has a long UTF-8 name. */
int names(void *, ping::Protocol, const uint8_t *hash, uint8_t size, char *out, size_t cap)
{
    if (size == 1 && hash[0] == 0x3f) { snprintf(out, cap, "Montjuic RPT"); return 1; }
    if (size == 1 && hash[0] == 0x40) { snprintf(out, cap, "Tibidabo"); return 2; }
    if (size == 1 && hash[0] == 0x41) { snprintf(out, cap, "Gr\xC3\xA0" "cia\xC3\xA7\xC3\xA7\xC3\xA7\xC3\xA7\xC3\xA7\xC3\xA7"); return 1; }
    return 0;
}

std::string answer(const ping::Prefs &p, ping::Counters &c, ping::Protocol w, const char *line, ping::Heard h,
                   bool named = true, size_t cap = 120)
{
    char out[200];
    size_t n = ping::reply(p, c, w, line, h, named ? names : nullptr, nullptr, out, cap);
    return std::string(out, n);
}

std::string mc(const ping::Prefs &p, const char *line, uint8_t hops, int8_t snr_x4, int16_t rssi,
               const uint8_t *path = nullptr, uint8_t size = 1, bool named = true)
{
    ping::Counters c = {};
    return answer(p, c, ping::PROTO_MC, line, {hops, snr_x4, rssi, {path, size, hops}}, named);
}

std::string cmd(ping::Prefs &p, const char *c, bool *changed = nullptr, const ping::Stats &st = ping::Stats())
{
    char out[160] = {0};
    ping::CmdResult r = ping::handleCommand(p, st, c, out, sizeof(out));
    if (changed) *changed = r.changed;
    return r.handled ? out : "<unhandled>";
}
}

TEST(ping_direct, "UNIT-PING-001", "A direct ping is answered with the SNR alone by default, and with the RSSI when it is on") {
    ping::Prefs p = on();
    CHECK(mc(p, "Ana: /ping", 0, 30, -92) == "@[Ana] direct (+7.5)");
    p.rssi = 1;
    CHECK(mc(p, "Ana: /ping", 0, -10, -110) == "@[Ana] direct (-110/-2.5)");
    p.snr = 0;
    CHECK(mc(p, "/ping", 0, 30, -92) == "direct (-92)");
    p.snr = 1;
    p.rssi = 0;
    CHECK(mc(p, "Ana: /ping", 0, -1, -92) == "@[Ana] direct (-0.2)");
    p.snr = 0;
    CHECK(mc(p, "Ana: /ping", 0, 30, -92) == "@[Ana] direct");
}

TEST(ping_mc_route, "UNIT-PING-002", "MeshCore: the whole route, a name when the hash is known, its hex otherwise, ? when shared") {
    ping::Prefs p = on();
    const uint8_t path[] = {0x3f, 0x77, 0x40};
    CHECK(mc(p, "Ana: /ping", 3, 30, -92, path) == "@[Ana] 3 hops\n" ARROW "Montjuic RPT\n" ARROW "77\n" ARROW "Tibidabo?(+7.5)");
    const uint8_t two[] = {0x3f, 0x9a, 0x77, 0x01};
    CHECK(mc(p, "Ana: /ping", 2, 30, -92, two, 2) == "@[Ana] 2 hops\n" ARROW "3f9a\n" ARROW "7701(+7.5)");
    CHECK(mc(p, "Ana: /ping", 1, 30, -92, path, 1, false) == "@[Ana] 1 hop\n" ARROW "3f(+7.5)");
    CHECK(mc(p, "Ana: /ping", 2, 30, -92, nullptr) == "@[Ana] 2 hops (+7.5)");
    p.rssi = 1;
    CHECK(mc(p, "Ana: /ping", 1, 30, -88, path) == "@[Ana] 1 hop\n" ARROW "Montjuic RPT(-88/+7.5)");
}

TEST(ping_mt_route, "UNIT-PING-003", "Meshtastic: the last relay only, by name or in hex") {
    ping::Prefs p = on();
    ping::Counters c = {};
    const uint8_t relay = 0x3f;
    const uint8_t unknown = 0xaa;
    CHECK(answer(p, c, ping::PROTO_MT, "!1234abcd: /ping", {2, 30, -92, {&relay, 1, 1}}) == "@[!1234abcd] 2 hops, last Montjuic RPT, SNR 7.5 dB");
    CHECK(answer(p, c, ping::PROTO_MT, "/ping", {1, 30, -92, {&unknown, 1, 1}}) == "1 hop, last aa, SNR 7.5 dB");
    CHECK(answer(p, c, ping::PROTO_MT, "/ping", {0, 30, -92, {&relay, 1, 0}}) == "direct, SNR 7.5 dB");
}

TEST(ping_names_cut, "UNIT-PING-004", "A long name is cut to 14 bytes without splitting a UTF-8 character") {
    ping::Prefs p = on();
    const uint8_t path[] = {0x41};
    std::string a = mc(p, "Ana: /ping", 1, 30, -92, path);
    CHECK(a == "@[Ana] 1 hop\n" ARROW "Gr\xC3\xA0" "cia\xC3\xA7\xC3\xA7\xC3\xA7(+7.5)");
}

TEST(ping_trigger, "UNIT-PING-005", "Each protocol answers only its own exact command, without regard to case, and only when on") {
    ping::Prefs p = on();
    ping::setTrigger(p.mt, "!eco");
    ping::Counters c = {};
    CHECK(answer(p, c, ping::PROTO_MC, "Ana: /PING", {1, 4, -90, {nullptr, 1, 0}}) != "");
    CHECK(answer(p, c, ping::PROTO_MC, "Ana: !eco", {1, 4, -90, {nullptr, 1, 0}}) == "");
    CHECK(answer(p, c, ping::PROTO_MT, "Ana: !eco", {1, 4, -90, {nullptr, 1, 0}}) != "");
    CHECK(answer(p, c, ping::PROTO_MT, "Ana: /ping", {1, 4, -90, {nullptr, 1, 0}}) == "");
    CHECK(answer(p, c, ping::PROTO_MC, "Ana: /ping now", {1, 4, -90, {nullptr, 1, 0}}) == "");
    CHECK(answer(p, c, ping::PROTO_MC, "Ana: hello /ping", {1, 4, -90, {nullptr, 1, 0}}) == "");
    p.mc.enable = 0;
    CHECK(answer(p, c, ping::PROTO_MC, "Ana: /ping", {1, 4, -90, {nullptr, 1, 0}}) == "");
    CHECK(answer(p, c, ping::PROTO_MT, "Ana: !eco", {1, 4, -90, {nullptr, 1, 0}}) != "");
    p.mc.enable = 1;
    p.mc.trigger[0] = '\0';
    CHECK(answer(p, c, ping::PROTO_MC, "Ana: ", {1, 4, -90, {nullptr, 1, 0}}) == "");
}

TEST(ping_filters_counters, "UNIT-PING-006", "ignore.direct and ignore.remote turn pings away; each protocol counts its own") {
    ping::Prefs p = on();
    ping::Stats st = {};
    char out[120];
    p.ignore_direct = 1;
    CHECK(ping::reply(p, st.mc, ping::PROTO_MC, "Ana: /ping", {0, 4, -90, {nullptr, 1, 0}}, nullptr, nullptr, out, sizeof(out)) == 0);
    CHECK(ping::reply(p, st.mc, ping::PROTO_MC, "Ana: /ping", {1, 4, -90, {nullptr, 1, 0}}, nullptr, nullptr, out, sizeof(out)) > 0);
    p.ignore_direct = 0;
    p.ignore_remote = 1;
    CHECK(ping::reply(p, st.mt, ping::PROTO_MT, "Ana: /ping", {0, 4, -90, {nullptr, 1, 0}}, nullptr, nullptr, out, sizeof(out)) > 0);
    CHECK(ping::reply(p, st.mt, ping::PROTO_MT, "Ana: /ping", {3, 4, -90, {nullptr, 1, 0}}, nullptr, nullptr, out, sizeof(out)) == 0);
    CHECK(st.mc.answered == 1 && st.mc.ignored == 1 && st.mt.answered == 1 && st.mt.ignored == 1);
    CHECK(ping::reply(p, st.mt, ping::PROTO_MT, "Ana: other", {0, 4, -90, {nullptr, 1, 0}}, nullptr, nullptr, out, sizeof(out)) == 0);
    CHECK(st.mt.answered == 1 && st.mt.ignored == 1);
}

TEST(ping_truncation, "UNIT-PING-007", "A long route is cut with .. keeping the signal; a small buffer is never overrun") {
    ping::Prefs p = on();
    ping::Counters c = {};
    uint8_t path[20];
    for (int i = 0; i < 20; i++) path[i] = (uint8_t)(0x50 + i);
    std::string a = answer(p, c, ping::PROTO_MC, "Ana: /ping", {20, 30, -92, {path, 1, 20}}, false, 60);
    CHECK(a.size() < 60);
    CHECK(a.find("\n..") != std::string::npos);
    CHECK(a.size() >= 6 && a.substr(a.size() - 6) == "(+7.5)");
    char out[12];
    size_t n = ping::reply(p, c, ping::PROTO_MC, "Ana: /ping", {2, 30, -92, {path, 1, 2}}, nullptr, nullptr, out, sizeof(out));
    CHECK(n < sizeof(out));
    CHECK(out[n] == '\0');
}

TEST(ping_commands, "UNIT-PING-008", "z.mc.ping.*, z.mt.ping.* and z.ping.* read and change the settings; a protocol needs a command to turn on") {
    ping::Prefs p;
    ping::setDefaults(p);
    bool changed = false;
    CHECK(cmd(p, "get z.mc.ping.enable") == "> off");
    CHECK(cmd(p, "get z.mt.ping.trigger") == "> ");
    CHECK(cmd(p, "set z.mc.ping.enable on", &changed).rfind("Error", 0) == 0);
    CHECK(!changed && p.mc.enable == 0);
    CHECK(cmd(p, "set z.mc.ping.enable on /ping", &changed) == "OK");
    CHECK(changed && p.mc.enable == 1 && std::string(p.mc.trigger) == "/ping");
    CHECK(p.mt.enable == 0 && std::string(p.mt.trigger) == "");
    CHECK(cmd(p, "set z.mt.ping.enable on !eco", &changed) == "OK");
    CHECK(p.mt.enable == 1 && std::string(p.mt.trigger) == "!eco" && std::string(p.mc.trigger) == "/ping");
    CHECK(cmd(p, "set z.mc.ping.enable on /ping", &changed) == "OK");
    CHECK(!changed);
    CHECK(cmd(p, "set z.mc.ping.enable off", &changed) == "OK");
    CHECK(changed && p.mc.enable == 0 && std::string(p.mc.trigger) == "/ping");
    CHECK(cmd(p, "set z.mc.ping.enable on", &changed) == "OK");
    CHECK(changed && p.mc.enable == 1);
    CHECK(cmd(p, "set z.mc.ping.enable maybe").rfind("Error", 0) == 0);
    CHECK(cmd(p, "set z.mc.ping.enable on two words").rfind("Error", 0) == 0);
    CHECK(cmd(p, "set z.mc.ping.trigger .ping") == "OK");
    CHECK(cmd(p, "get z.mc.ping.trigger") == "> .ping");
    CHECK(cmd(p, "set z.mc.ping.trigger two words").rfind("Error", 0) == 0);
    CHECK(cmd(p, "get z.ping.snr") == "> on");
    CHECK(cmd(p, "get z.ping.rssi") == "> off");
    CHECK(cmd(p, "set z.ping.rssi on") == "OK");
    CHECK(cmd(p, "set z.ping.ignore.direct on") == "OK" && p.ignore_direct == 1);
    CHECK(cmd(p, "set z.ping.ignore.remote perhaps").rfind("Error", 0) == 0);
    CHECK(cmd(p, "get z.ping.bogus").rfind("Error", 0) == 0);
    CHECK(cmd(p, "get z.mc.ping.channel").rfind("Error", 0) == 0);
    CHECK(cmd(p, "get z.mt.relay") == "<unhandled>");
    CHECK(cmd(p, "get mt.relay") == "<unhandled>");
    CHECK(cmd(p, "get ping.enable") == "<unhandled>");
}

TEST(ping_status, "UNIT-PING-009", "status is per protocol: state, command and counters; it cannot be set") {
    ping::Prefs p = on();
    ping::Stats st = {};
    st.mc.answered = 12;
    st.mc.ignored = 3;
    st.mt.answered = 1;
    ping::setTrigger(p.mt, "!eco");
    p.mt.enable = 0;
    CHECK(cmd(p, "get z.mc.ping.status", nullptr, st) == "> on /ping | answered 12, ignored 3");
    CHECK(cmd(p, "get z.mt.ping.status", nullptr, st) == "> off !eco | answered 1, ignored 0");
    ping::Prefs q;
    ping::setDefaults(q);
    CHECK(cmd(q, "get z.mt.ping.status", nullptr, st) == "> off (no command) | answered 1, ignored 0");
    CHECK(cmd(p, "set z.mc.ping.status 0").rfind("Error", 0) == 0);
}

TEST(ping_sanitize, "UNIT-PING-010", "A prefs blob out of range comes back valid; a protocol without a trigger is off") {
    ping::Prefs p;
    memset(&p, 0xFF, sizeof(p));
    ping::sanitize(p);
    CHECK(p.rssi == 1 && p.snr == 1 && p.ignore_direct == 1 && p.ignore_remote == 1);
    CHECK(std::string(p.mc.trigger) == "" && p.mc.enable == 0 && std::string(p.mt.trigger) == "" && p.mt.enable == 0);
    ping::setDefaults(p);
    CHECK(p.snr == 1 && p.rssi == 0 && p.mc.enable == 0 && p.mt.enable == 0);
    ping::setTrigger(p.mt, "/ping");
    p.mt.enable = 1;
    ping::sanitize(p);
    CHECK(p.mt.enable == 1 && std::string(p.mt.trigger) == "/ping" && p.mc.enable == 0);
}

TEST(ping_enable_strict, "UNIT-PING-011", "enable takes exactly on or off: offline, off junk and a command that is too long are errors") {
    ping::Prefs p;
    ping::setDefaults(p);
    bool changed = false;
    CHECK(cmd(p, "set z.mc.ping.enable on /ping", &changed) == "OK" && p.mc.enable == 1);
    CHECK(cmd(p, "set z.mc.ping.enable offline", &changed).rfind("Error", 0) == 0 && p.mc.enable == 1 && !changed);
    CHECK(cmd(p, "set z.mc.ping.enable offf", &changed).rfind("Error", 0) == 0 && p.mc.enable == 1);
    CHECK(cmd(p, "set z.mc.ping.enable off junk", &changed).rfind("Error", 0) == 0 && p.mc.enable == 1);
    CHECK(cmd(p, "set z.mc.ping.enable onward", &changed).rfind("Error", 0) == 0);
    CHECK(cmd(p, "set z.mc.ping.enable on 0123456789abcdef").find("1 to 15 characters") != std::string::npos);
    CHECK(cmd(p, "set z.mc.ping.enable on 0123456789abcdefg").find("1 to 15 characters") != std::string::npos);
    CHECK(cmd(p, "set z.mc.ping.enable off", &changed) == "OK" && p.mc.enable == 0 && changed);
}

TEST(ping_plain_text, "UNIT-PING-012", "A line flagged plain has no sender, so a colon in the text is not one") {
    ping::Prefs p = on();
    ping::Counters c = {};
    ping::Heard h = {1, 30, -92, {nullptr, 1, 0}};
    h.plain = true;
    CHECK(answer(p, c, ping::PROTO_MT, "foo: /ping", h) == "");
    CHECK(answer(p, c, ping::PROTO_MT, "/ping", h) == "1 hop, SNR 7.5 dB");
    h.plain = false;
    CHECK(answer(p, c, ping::PROTO_MT, "foo: /ping", h) == "@[foo] 1 hop, SNR 7.5 dB");
}

TEST(ping_relayed_unknown_hops, "UNIT-PING-013", "No hop_start but a relay: not direct, so ignore.direct keeps it and ignore.remote drops it") {
    ping::Prefs p = on();
    ping::Counters c = {};
    const uint8_t relay = 0x3f;
    ping::Heard h = {0, 30, -92, {&relay, 1, 1}};
    h.relayed = true;
    CHECK(answer(p, c, ping::PROTO_MT, "Ana: /ping", h) == "@[Ana] relayed, last Montjuic RPT, SNR 7.5 dB");
    p.ignore_direct = 1;
    CHECK(answer(p, c, ping::PROTO_MT, "Ana: /ping", h) != "");
    p.ignore_direct = 0;
    p.ignore_remote = 1;
    CHECK(answer(p, c, ping::PROTO_MT, "Ana: /ping", h) == "");
}

TEST(ping_on_off_digits, "UNIT-PING-014", "1 and 0 mean on and off in every on/off setting, with and without a command") {
    ping::Prefs p = on();
    bool changed = false;
    CHECK(cmd(p, "set z.ping.rssi 1", &changed) == "OK" && p.rssi == 1 && changed);
    CHECK(cmd(p, "set z.ping.rssi 0", &changed) == "OK" && p.rssi == 0 && changed);
    CHECK(cmd(p, "set z.ping.ignore.direct 2").rfind("Error", 0) == 0);
    CHECK(cmd(p, "set z.mc.ping.enable 0", &changed) == "OK" && p.mc.enable == 0 && changed);
    CHECK(cmd(p, "set z.mc.ping.enable 1", &changed) == "OK" && p.mc.enable == 1 && changed);
    CHECK(cmd(p, "set z.mt.ping.enable 1 /hola", &changed) == "OK" && p.mt.enable == 1 && std::string(p.mt.trigger) == "/hola");
    CHECK(cmd(p, "set z.mt.ping.enable 0 /hola").rfind("Error", 0) == 0);
    CHECK(cmd(p, "set z.mt.ping.enable 10").rfind("Error", 0) == 0);
}
