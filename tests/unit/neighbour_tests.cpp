// helpers/NeighbourList.h: the table of nodes heard directly and the "z.mc.neighbors" / "z.mt.neighbors" reply.
#include "test.h"
#include "NeighbourList.h"

#include <algorithm>
#include <string>

namespace {

neighbours::Row row(const char *name, int8_t snr_x4, uint32_t age_s)
{
    neighbours::Row r = {};
    neighbours::setRowName(r, name);
    r.snr_x4 = snr_x4;
    r.age_s = age_s;
    return r;
}

/* A list of rows; a row with no name is a free slot. */
struct Rows : neighbours::List {
    std::vector<neighbours::Row> v;
    size_t size() const override { return v.size(); }
    bool key(size_t i, int8_t &s, uint32_t &age) const override { s = v[i].snr_x4; age = v[i].age_s; return v[i].name[0] != '\0'; }
    void row(size_t i, neighbours::Row &r) const override { r = v[i]; }
};

std::string reply(const Rows &rows, size_t page = 1, size_t cap = 256, bool recent = false)
{
    char out[300];
    neighbours::format(out, cap, "MeshCore neighbors", rows, page, recent);
    return out;
}

} // namespace

TEST(neighbours_none, "UNIT-NEIGH-001", "With no neighbour the reply is the title and -none-")
{
    CHECK(reply(Rows()) == "MeshCore neighbors: -none-");
    Rows free_slots;
    free_slots.v.push_back(neighbours::Row{});
    CHECK(reply(free_slots) == "MeshCore neighbors: -none-");
}

TEST(neighbours_best_first, "UNIT-NEIGH-002", "Lines come best SNR first, with SNR to a tenth and the age in the largest unit")
{
    Rows b;
    b.v = {row("Tibidabo RPT", -8, 45), row("Montjuic RPT", 41, 4 * 60 + 10), row("!a1b2c3d4", 20, 3 * 3600),
           row("Far", -22, 2 * 86400)};
    CHECK(reply(b) == "MeshCore neighbors (4) 1/1\nMontjuic RPT (10.3, 4m)\n!a1b2c3d4 (5.0, 3h)\n"
                      "Tibidabo RPT (-2.0, 45s)\nFar (-5.5, 2d)");
}

TEST(neighbours_pages, "UNIT-NEIGH-003", "A page holds 4 lines of the ranking; a page past the last says so and a tie keeps list order")
{
    Rows b;
    for (int i = 0; i < 10; i++) {
        char n[8];
        snprintf(n, sizeof(n), "n%d", i);
        b.v.push_back(row(n, (int8_t)(i * 4), 1));
    }
    CHECK(reply(b, 1) == "MeshCore neighbors (10) 1/3\nn9 (9.0, 1s)\nn8 (8.0, 1s)\nn7 (7.0, 1s)\nn6 (6.0, 1s)");
    CHECK(reply(b, 3) == "MeshCore neighbors (10) 3/3\nn1 (1.0, 1s)\nn0 (0.0, 1s)");
    CHECK(reply(b, 4) == "MeshCore neighbors (10): page 4 of 3");
    CHECK(reply(b, 0) == "MeshCore neighbors (10): page 0 of 3");
    Rows t;
    t.v = {row("a", 8, 1), row("b", 8, 1), row("c", 8, 1)};
    CHECK(reply(t) == "MeshCore neighbors (3) 1/1\na (2.0, 1s)\nb (2.0, 1s)\nc (2.0, 1s)");
}

TEST(neighbours_cap, "UNIT-NEIGH-004", "A line that does not fit the reply is left out whole")
{
    Rows b;
    b.v = {row("Montjuic RPT", 41, 5), row("Tibidabo RPT", 20, 5)};
    CHECK(reply(b, 1, 54) == "MeshCore neighbors (2) 1/1\nMontjuic RPT (10.3, 5s)");
}

TEST(neighbours_full_page_fits_a_packet, "UNIT-NEIGH-009", "The longest page of 4 lines fits the room a remote reply has (161 bytes less a 4-byte header)")
{
    Rows b;
    for (int i = 0; i < 4; i++) {
        b.v.push_back(row("12345678901234", (int8_t)(-100 - i), 99 * 86400));
    }
    char out[161 - 4];
    neighbours::format(out, sizeof(out), "Meshtastic neighbors", b, 1);
    const std::string r = out;
    CHECK(r.find("(-25.8, 99d)") != std::string::npos);
    CHECK(std::count(r.begin(), r.end(), '\n') == 4);
}

TEST(neighbours_recent, "UNIT-NEIGH-010", "With recent the lines come heard-last first, the title says so, and a tie keeps list order")
{
    Rows b;
    b.v = {row("old", 40, 3600), row("new", -8, 5), row("mid", 12, 120), row("same", 0, 120)};
    CHECK(reply(b, 1, 256, true) == "MeshCore neighbors (4) 1/1 recent\nnew (-2.0, 5s)\nmid (3.0, 2m)\nsame (0.0, 2m)\nold (10.0, 1h)");
}

TEST(neighbours_name_cut, "UNIT-NEIGH-005", "A long name is cut to 14 bytes without splitting a UTF-8 character")
{
    neighbours::Row r = {};
    neighbours::setRowName(r, "Gr\xC3\xA0" "cia\xC3\xA7\xC3\xA7\xC3\xA7\xC3\xA7\xC3\xA7");
    CHECK(std::string(r.name) == "Gr\xC3\xA0" "cia\xC3\xA7\xC3\xA7\xC3\xA7");
}

TEST(neighbours_table, "UNIT-NEIGH-006", "A node heard again updates its entry; a full table drops the one heard longest ago")
{
    neighbours::Table t;
    for (uint32_t id = 1; id <= neighbours::kTableSize; id++) {
        t.note(id, 4, 100 + id);
    }
    t.note(5, 40, 500);
    int used = 0;
    for (const neighbours::Entry &e : t) {
        used += e.id != 0;
        if (e.id == 5) { CHECK(e.snr_x4 == 40 && e.heard_s == 500); }
    }
    CHECK(used == (int)neighbours::kTableSize);
    t.note(99, 8, 600);
    bool has1 = false, has99 = false, has2 = false;
    for (const neighbours::Entry &e : t) {
        has1 |= e.id == 1;
        has2 |= e.id == 2;
        has99 |= e.id == 99;
    }
    CHECK(!has1 && has2 && has99);
}

TEST(neighbours_free_slot_not_duplicate, "UNIT-NEIGH-007", "A known node is never entered twice while the table has free entries")
{
    neighbours::Table t;
    t.note(7, 4, 1);
    t.note(8, 4, 2);
    t.note(7, 12, 3);
    int count7 = 0;
    for (const neighbours::Entry &e : t) { count7 += e.id == 7; }
    CHECK(count7 == 1);
}

TEST(neighbours_name, "UNIT-NEIGH-008", "A short name is set only for a node already in the table")
{
    neighbours::Table t;
    t.setName(3, "ABCD");
    t.note(3, 4, 1);
    t.setName(3, "ABCD");
    for (const neighbours::Entry &e : t) {
        if (e.id == 3) { CHECK(std::string(e.name) == "ABCD"); }
    }
}
