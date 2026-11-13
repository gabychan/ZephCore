// helpers/ZCommand.h: the on/off value of the "z." commands and "<get|set> z.<key> help".
#include "test.h"
#include "ZCommand.h"

#include <string>

namespace {
std::string help(const char *command, bool mt = true)
{
    char out[161] = "";
    return zcmd::helpReply(command, mt, out, sizeof(out)) ? std::string(out) : std::string("<not mine>");
}
}

TEST(zcmd_on_off, "UNIT-ZCMD-001", "on and 1 are on, off and 0 are off, anything else is not a value") {
    uint8_t b = 9;
    CHECK(zcmd::parseOnOff("on", b) && b == 1);
    CHECK(zcmd::parseOnOff("1", b) && b == 1);
    CHECK(zcmd::parseOnOff("off", b) && b == 0);
    CHECK(zcmd::parseOnOff("0", b) && b == 0);
    b = 9;
    CHECK(!zcmd::parseOnOff("", b) && !zcmd::parseOnOff("2", b) && !zcmd::parseOnOff("on ", b) && !zcmd::parseOnOff("ON", b) && b == 9);
}

TEST(zcmd_help_key, "UNIT-ZCMD-002", "help at the end of get or set gives the usage of that key") {
    CHECK(help("get z.mt.enable help").find("> z.mt.enable: get|set on|off|1|0") == 0);
    CHECK(help("set z.mt.relay help").find("off|essential|all") != std::string::npos);
    CHECK(help("get z.mc.ping.enable   help ") == help("set z.mc.ping.enable help"));
    CHECK(help("get z.ping.snr help").find("SNR") != std::string::npos);
    CHECK(help("get z.tx.stalls help").find("> z.tx.stalls:") == 0);
    CHECK(help("get z.mt.neighbors help").find("Meshtastic") != std::string::npos);
}

TEST(zcmd_help_group, "UNIT-ZCMD-003", "help on a group lists its keys, whole words only, within the reply") {
    CHECK(help("get z.mt help").find("> z.mt: enable role relay relay.opaque") == 0);
    CHECK(help("get z.mt help").find("ping.enable") != std::string::npos);
    CHECK(help("get z.ping help") == "> z.ping: snr rssi ignore.direct ignore.remote");
    char out[40] = "";
    CHECK(zcmd::helpReply("get z.mt help", true, out, sizeof(out)));
    CHECK(strlen(out) < sizeof(out) && out[strlen(out) - 1] != ':' && strncmp(out, "> z.mt: enable role", 19) == 0 && out[strlen(out) - 1] != ' ');
}

TEST(zcmd_help_other, "UNIT-ZCMD-004", "what is not a help request, or names no key, goes on to its own handler") {
    CHECK(help("get z.mt.enable") == "<not mine>");
    CHECK(help("get z.mt.nope help") == "<not mine>");
    CHECK(help("get z.mt.enable helpme") == "<not mine>");
    CHECK(help("set z.mt.channel.add Foo help") == "<not mine>");  // a channel key
    CHECK(help("get dutycycle help") == "<not mine>");
    CHECK(help("get z. help") == "<not mine>");
}

TEST(zcmd_help_no_meshtastic, "UNIT-ZCMD-005", "a build without Meshtastic has no z.mt help") {
    CHECK(help("get z.mt.enable help", false) == "<not mine>");
    CHECK(help("get z.mt help", false) == "<not mine>");
    CHECK(help("get z.mc help", false).find("> z.mc: neighbors") == 0);
}
