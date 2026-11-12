// helpers/ui/ui_dash_fmt.h and neighbours::top: the text of the dashboard page and the nodes it lists.
#include "test.h"
#include "ui_dash_fmt.h"
#include "NeighbourList.h"

#include <string>
#include <vector>

namespace {
std::string count(uint32_t n)
{
    char b[8];
    ui_dash_count(b, sizeof(b), n);
    return b;
}

struct Rows : neighbours::List {
    std::vector<neighbours::Row> v;
    size_t size() const override { return v.size(); }
    bool key(size_t i, int8_t &s, uint32_t &age) const override { s = v[i].snr_x4; age = v[i].age_s; return v[i].name[0] != '\0'; }
    void row(size_t i, neighbours::Row &r) const override { r = v[i]; }
};

neighbours::Row row(const char *name, int8_t snr_x4, uint32_t age_s)
{
    neighbours::Row r = {};
    neighbours::setRowName(r, name);
    r.snr_x4 = snr_x4;
    r.age_s = age_s;
    return r;
}
}

TEST(uidash_count, "UNIT-UIDASH-001", "a count is at most 4 characters: 999, 1.2k, 12k, 345k, 1M") {
    CHECK(count(0) == "0" && count(999) == "999");
    CHECK(count(1000) == "1.0k" && count(1234) == "1.2k" && count(9999) == "9.9k");
    CHECK(count(12345) == "12k" && count(345000) == "345k");
    CHECK(count(1000000) == "1M" && count(4294967295u) == "4294M");
}

TEST(uidash_age, "UNIT-UIDASH-002", "an age is in seconds, minutes, hours or days") {
    char b[8];
    ui_dash_age(b, sizeof(b), 59);
    CHECK(std::string(b) == "59s");
    ui_dash_age(b, sizeof(b), 39 * 60);
    CHECK(std::string(b) == "39m");
    ui_dash_age(b, sizeof(b), 22 * 3600);
    CHECK(std::string(b) == "22h");
    ui_dash_age(b, sizeof(b), 3 * 86400);
    CHECK(std::string(b) == "3d");
}

TEST(uidash_bars, "UNIT-UIDASH-003", "SNR lights 0 to 4 bars, a step at 8, 3, -3 and -10 dB") {
    CHECK(ui_dash_bars(4 * 13) == 4 && ui_dash_bars(4 * 8) == 4);
    CHECK(ui_dash_bars(4 * 8 - 1) == 3 && ui_dash_bars(4 * 3) == 3);
    CHECK(ui_dash_bars(4 * 3 - 1) == 2 && ui_dash_bars(-4 * 3) == 2);
    CHECK(ui_dash_bars(-4 * 3 - 1) == 1 && ui_dash_bars(-4 * 10) == 1);
    CHECK(ui_dash_bars(-4 * 10 - 1) == 0);
}

TEST(uidash_gauge, "UNIT-UIDASH-004", "a gauge fills in proportion, shows any use, never passes full or divides by 0") {
    CHECK(ui_dash_fill(0, 100, 96) == 0);
    CHECK(ui_dash_fill(50, 100, 96) == 48);
    CHECK(ui_dash_fill(1, 2048, 96) == 1);
    CHECK(ui_dash_fill(200, 100, 96) == 96);
    CHECK(ui_dash_fill(5, 0, 96) == 0);
}

TEST(uidash_duty_uptime, "UNIT-UIDASH-005", "time on air is a percentage of the uptime, and the uptime is days and hours, hours and minutes, or minutes") {
    char b[8];
    ui_dash_duty(b, sizeof(b), 36, 86400);
    CHECK(std::string(b) == "0.0%");
    ui_dash_duty(b, sizeof(b), 600, 86400);
    CHECK(std::string(b) == "0.6%");
    ui_dash_duty(b, sizeof(b), 8640, 86400);
    CHECK(std::string(b) == "10%");
    ui_dash_duty(b, sizeof(b), 5, 0);
    CHECK(std::string(b) == "-");
    char u[12];
    ui_dash_uptime(u, sizeof(u), 93784);
    CHECK(std::string(u) == "1d02h");
    ui_dash_uptime(u, sizeof(u), 3 * 3600 + 300);
    CHECK(std::string(u) == "3h05m");
    ui_dash_uptime(u, sizeof(u), 719);
    CHECK(std::string(u) == "11m");
}

TEST(uidash_top, "UNIT-UIDASH-006", "the best nodes come first, as many as asked for, and fewer when there are fewer") {
    Rows rows;
    rows.v = {row("low", -20, 10), row("", 0, 0), row("high", 40, 30), row("mid", 8, 20)};
    neighbours::Row out[3] = {};

    CHECK(neighbours::top(rows, out, 3) == 3);
    CHECK(std::string(out[0].name) == "high" && std::string(out[1].name) == "mid" && std::string(out[2].name) == "low");
    CHECK(neighbours::top(rows, out, 2) == 2);
    CHECK(std::string(out[1].name) == "mid");
    rows.v = {row("only", 4, 5)};
    CHECK(neighbours::top(rows, out, 3) == 1 && std::string(out[0].name) == "only");
    rows.v.clear();
    CHECK(neighbours::top(rows, out, 3) == 0);
}

TEST(uidash_top_recent, "UNIT-UIDASH-007", "with recent the node heard last comes first") {
    Rows rows;
    rows.v = {row("old", 40, 900), row("new", -20, 5)};
    neighbours::Row out[2] = {};

    CHECK(neighbours::top(rows, out, 2, true) == 2);
    CHECK(std::string(out[0].name) == "new");
}
