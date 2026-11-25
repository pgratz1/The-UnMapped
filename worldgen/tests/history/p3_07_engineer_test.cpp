// Engineer-side checks of the history report (spec P3-07), written by the
// generator engineer while implementing; the report's tests proper are the
// test writer's (history_report_*, P3-07 section 14).

#include <doctest/doctest.h>

#include <charconv>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "p3_03_chronology_support.hpp"
#include "worldgen/db/testing.hpp"
#include "worldgen/history/digest.hpp"
#include "worldgen/inspect/history_report.hpp"
#include "worldgen/inspect/report.hpp"

namespace {

namespace wi = worldgen::inspect;
namespace wdb = worldgen::db;

std::string report_of(const std::string& path, wi::HistoryLevel level) {
    wdb::Result<wdb::WorldReader> world = wdb::WorldReader::open(path);
    REQUIRE(world.ok());
    wdb::Result<std::string> r = wi::build_history_report(world.value(), level);
    REQUIRE_MESSAGE(r.ok(), r.error().message);
    return r.value();
}

wdb::Error error_of(const std::string& path) {
    wdb::Result<wdb::WorldReader> world = wdb::WorldReader::open(path);
    REQUIRE(world.ok());
    wdb::Result<std::string> r = wi::build_history_report(world.value(), wi::HistoryLevel::summary);
    REQUIRE_FALSE(r.ok());
    return r.error();
}

std::vector<std::string> lines_of(const std::string& text) {
    std::vector<std::string> out;
    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t end = text.find('\n', start);
        out.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    return out;
}

// The lines of one block, without its heading.
std::vector<std::string> block(const std::vector<std::string>& lines, std::string_view heading) {
    std::vector<std::string> out;
    bool in = false;
    for (const std::string& l : lines) {
        if (!l.empty() && l[0] == '[') {
            in = l == heading;
            continue;
        }
        if (in) {
            out.push_back(l);
        }
    }
    return out;
}

std::map<std::string, std::string> keyed(const std::vector<std::string>& lines) {
    std::map<std::string, std::string> out;
    for (const std::string& l : lines) {
        const std::size_t eq = l.find(" = ");
        if (eq != std::string::npos) {
            out[l.substr(0, eq)] = l.substr(eq + 3);
        }
    }
    return out;
}

// Reads the six counts of a "  wars: " line of [civilizations] (r17 24.10
// item 5): literal text and decimal numbers in turn; like the sscanf it
// replaces, text after the last number is not checked.
bool parse_wars_line(std::string_view line, std::int64_t (&out)[6]) {
    static constexpr std::string_view literals[6] = {"  wars: ", " declared, ", " defended; won ",
                                                     ", lost ", ", drawn ", ", open "};
    for (std::size_t i = 0; i < 6; ++i) {
        if (line.substr(0, literals[i].size()) != literals[i]) {
            return false;
        }
        line.remove_prefix(literals[i].size());
        const auto r = std::from_chars(line.data(), line.data() + line.size(), out[i], 10);
        if (r.ec != std::errc{}) {
            return false;
        }
        line.remove_prefix(static_cast<std::size_t>(r.ptr - line.data()));
    }
    return true;
}

std::string copy_of(const std::string& path, const std::string& name) {
    const std::string to = chrono_test::world_dir().file(name);
    std::filesystem::copy_file(path, to, std::filesystem::copy_options::overwrite_existing);
    return to;
}

const std::string& world_42() {
    static const std::string path = chrono_test::world(chrono_test::WorldKey{42, "", 512, 6000, "all", "", 1});
    return path;
}

}  // namespace

TEST_CASE("P3-07 engineer: escape, levels, ordinals") {
    CHECK(wi::escape_history_text("a\\b") == "a\\\\b");
    CHECK(wi::escape_history_text(std::string("\x01\x1F\x7F\x80\xFF", 5)) == "\\x01\\x1F\\x7F\\x80\\xFF");
    CHECK(wi::escape_history_text("Plain 'text' - ok") == "Plain 'text' - ok");
    CHECK(wi::parse_history_level("summary") == wi::HistoryLevel::summary);
    CHECK(wi::parse_history_level("full") == wi::HistoryLevel::full);
    CHECK_FALSE(wi::parse_history_level("brief").has_value());
    CHECK(worldgen::history::history_ordinal(2) == "second");
    CHECK(worldgen::history::history_ordinal(11) == "11th");
    CHECK(worldgen::history::history_ordinal(21) == "21st");
    CHECK(worldgen::history::history_ordinal(112) == "112th");
}

TEST_CASE("P3-07 engineer: the text conventions and the invariants of 13 on seed 42 at 512") {
    const std::string summary = report_of(world_42(), wi::HistoryLevel::summary);
    const std::string full = report_of(world_42(), wi::HistoryLevel::full);
    for (const std::string* text : {&summary, &full}) {
        REQUIRE(!text->empty());
        CHECK(text->back() == '\n');
        for (const char c : *text) {
            const auto b = static_cast<unsigned char>(c);
            REQUIRE((b == 0x0AU || (b >= 0x20U && b <= 0x7EU)));
        }
        const std::vector<std::string> lines = lines_of(*text);
        std::vector<std::string> headings;
        for (const std::string& l : lines) {
            REQUIRE(!l.empty());
            REQUIRE(l.back() != ' ');
            if (l[0] == '[') {
                headings.push_back(l);
            }
        }
        CHECK(lines[0] == "# worldgen history v7");  // P3-07 r21 26.1; P5-02 12.1; P5-03a 9.1
        // r17 24.4: the chronicle before the civilizations, [phases] at full only.
        if (text == &full) {
            CHECK(headings == std::vector<std::string>{"[settings]", "[ages]", "[phases]", "[chronicle]",
                                                       "[civilizations]", "[present]", "[monsters]", "[sites]",
                                                       "[peoples]", "[alliances]", "[stats]"});   // P5-02 12.4, 12.5; P5-03 17.2; P5-04 10.2
        } else {
            CHECK(headings == std::vector<std::string>{"[settings]", "[ages]", "[chronicle]", "[civilizations]",
                                                       "[present]", "[monsters]", "[sites]", "[peoples]",
                                                       "[alliances]", "[stats]"});   // P5-03 17.2; P5-04 10.2
        }
    }
    const std::vector<std::string> s = lines_of(summary);
    const std::vector<std::string> f = lines_of(full);
    for (const std::string_view b : {"[settings]", "[ages]"}) {
        CHECK(block(s, b) == block(f, b));
    }
    // r19 25.12 item 2: [stats] at summary is the SUMMARY_STATS subset of full's, in full's order.
    {
        std::vector<std::string> subset;
        for (const std::string& l : block(f, "[stats]")) {
            const std::string key = l.substr(0, l.find(" = "));
            for (const std::string_view k : worldgen::history::SUMMARY_STATS) {
                if (key == k) {
                    subset.push_back(l);
                }
            }
        }
        CHECK(block(s, "[stats]") == subset);
        CHECK(subset.size() == worldgen::history::SUMMARY_STATS.size());
    }
    // 24.10 item 5: the wars line's outcomes add up; item 6: one quiet
    // stretch milestone with its note per realm that lived through one.
    for (const std::string& l : block(s, "[civilizations]")) {
        if (l.rfind("  wars: ", 0) == 0) {
            std::int64_t n[6] = {};
            REQUIRE_MESSAGE(parse_wars_line(l, n), l);
            CHECK(n[2] + n[3] + n[4] + n[5] == n[0] + n[1]);
        }
    }
    {
        std::int64_t blocks = 0, stretch_milestones = 0;
        for (const std::string& l : block(s, "[civilizations]")) {
            blocks += (l.rfind("c", 0) == 0) ? 1 : 0;
            if (l.find(" quiet_stretch: ") != std::string::npos) {
                ++stretch_milestones;
                CHECK(l.find("new generation") != std::string::npos);
            }
        }
        CHECK(stretch_milestones > 0);
        CHECK(stretch_milestones <= blocks);
    }
    // r19 25.12 item 2: invariant 6 is read on full.
    const std::map<std::string, std::string> stats = keyed(block(f, "[stats]"));
    // Invariant 6: every key of the world report's [history] with its value.
    wdb::Result<wdb::WorldReader> world = wdb::WorldReader::open(world_42());
    REQUIRE(world.ok());
    wdb::Result<std::string> report = wi::build_report(world.value());
    REQUIRE(report.ok());
    const std::map<std::string, std::string> history = keyed(block(lines_of(report.value()), "[history]"));
    REQUIRE(history.size() > 20U);
    for (const auto& [key, value] : history) {
        CHECK_MESSAGE(stats.count(key) == 1U, key);
        if (stats.count(key) == 1U) {
            CHECK_MESSAGE(stats.at(key) == value, key);
        }
    }
    // Invariants 2 and 3: event lines, shown and hidden counts.
    const auto event_lines = [](const std::vector<std::string>& chronicle, std::int64_t& hidden) {
        std::int64_t n = 0;
        hidden = 0;
        for (const std::string& l : chronicle) {
            if (!l.empty() && l[0] >= '0' && l[0] <= '9') {
                ++n;
            } else if (l.rfind("(", 0) == 0) {
                hidden += std::stoll(l.substr(1));
            }
        }
        return n;
    };
    std::int64_t hidden_s = 0;
    std::int64_t hidden_f = 0;
    const std::vector<std::string> cs = block(s, "[chronicle]");
    const std::vector<std::string> cf = block(f, "[chronicle]");
    const std::int64_t shown_s = event_lines(cs, hidden_s);
    const std::int64_t shown_f = event_lines(cf, hidden_f);
    const std::int64_t events = std::stoll(stats.at("events"));
    CHECK(shown_f == events);
    CHECK(hidden_f == 0);
    CHECK(shown_s == std::stoll(stats.at("summary_events")));
    CHECK(shown_s + hidden_s == events);
    CHECK(keyed(cs).at("shown") == stats.at("summary_events"));
    CHECK(keyed(cf).at("threshold") == "0");
    CHECK(keyed(cs).at("threshold") == stats.at("summary_threshold"));
    CHECK(shown_s <= 250);
}

TEST_CASE("P3-07 engineer: worlds without history are refused") {
    const std::string path = chrono_test::world(chrono_test::WorldKey{42, "", 512, 6000, "all", "chronology", 1});
    const wdb::Error e = error_of(path);
    CHECK(e.code == wdb::ErrorCode::history_missing);
    CHECK(e.message == "world file '" + path +
                           "' has no history (stages: elevation,hydrology,climate,biomes,resources,regions,"
                           "chronology); generate it with the history stage");
}

TEST_CASE("P3-07 engineer: a few corrupt tables of 5.3") {
    struct Case {
        const char* sql;
        const char* message;
    };
    const Case cases[] = {
        {"UPDATE settlement SET population = -1 WHERE id = 2",
         "corrupt table 'settlement': row 2: population -1 is negative"},
        {"UPDATE settlement SET population = -5 WHERE id = (SELECT MIN(id) FROM settlement WHERE status IN (1, 2))",
         nullptr},
        {"UPDATE settlement SET population = 2147483648 WHERE id = (SELECT MIN(id) FROM settlement WHERE status IN (1, 2))",
         nullptr},
        {"DELETE FROM settlement_census WHERE settlement = 3",
         "corrupt table 'settlement_census': settlement 3: no rows"},
        {"UPDATE civilization SET home_region = 999 WHERE id = 1",
         "corrupt table 'civilization': row 1: home_region 999 is not a region"},
    };
    int k = 0;
    for (const Case& c : cases) {
        const std::string path = copy_of(world_42(), "p3-07-corrupt-" + std::to_string(k++) + ".db");
        REQUIRE(wdb::testing::execute_on_file(path, c.sql).ok());
        const wdb::Error e = error_of(path);
        CHECK(e.code == wdb::ErrorCode::corrupt_layer);
        if (c.message != nullptr) {
            CHECK(e.message == c.message);
        } else {
            // A standing settlement: 5.3's wording, not the world report's
            // "outside 0 to 2147483647".
            const bool negative = std::string(c.sql).find("= -5") != std::string::npos;
            CHECK(e.message.rfind("corrupt table 'settlement': row ", 0) == 0);
            CHECK(e.message.find(negative ? ": population -5 is negative" : ": population 2147483648 above 2147483647") !=
                  std::string::npos);
            CHECK(e.message.find("outside") == std::string::npos);
        }
    }
    {
        const std::string path = copy_of(world_42(), "p3-07-corrupt-phase.db");
        REQUIRE(wdb::testing::execute_on_file(path, "UPDATE event SET phase = 1 WHERE id = (SELECT MAX(id) FROM event)").ok());
        const wdb::Error e = error_of(path);
        CHECK(e.code == wdb::ErrorCode::corrupt_layer);
        CHECK(e.message.rfind("corrupt table 'event': row ", 0) == 0);
        CHECK(e.message.find(": phase 1, expected ") != std::string::npos);
    }
}

TEST_CASE("P3-07 engineer: the upland flag on a damaged region row has no overflow (audit S1)") {
    // cells = walk_cells = 2^62 passes every check of 5.3; 4 * walk_cells
    // would overflow. The flag is false (walk_cells is not below cells / 4).
    // cells = 2^62 + 1, walk_cells = 2^60: upland (4 * 2^60 < 2^62 + 1);
    // cells = 2^63 - 1, walk_cells = 2^61 - 1: upland (2^63 - 4 < 2^63 - 1).
    struct Case {
        const char* sql;
        bool upland;
    };
    const Case cases[] = {
        {"UPDATE region SET cells = 4611686018427387904, walk_cells = 4611686018427387904, lake_cells = 0 "
         "WHERE id = (SELECT home_region FROM civilization WHERE id = 1)",
         false},
        {"UPDATE region SET cells = 4611686018427387905, walk_cells = 1152921504606846976, lake_cells = 0 "
         "WHERE id = (SELECT home_region FROM civilization WHERE id = 1)",
         true},
        {"UPDATE region SET cells = 9223372036854775807, walk_cells = 2305843009213693951, lake_cells = 0 "
         "WHERE id = (SELECT home_region FROM civilization WHERE id = 1)",
         true},
    };
    int k = 0;
    for (const Case& c : cases) {
        const std::string path = copy_of(world_42(), "p3-07-upland-" + std::to_string(k++) + ".db");
        REQUIRE(wdb::testing::execute_on_file(path, c.sql).ok());
        for (const wi::HistoryLevel level : {wi::HistoryLevel::summary, wi::HistoryLevel::full}) {
            const std::vector<std::string> lines = lines_of(report_of(path, level));
            std::string home;
            for (std::size_t i = 0; i + 2 < lines.size(); ++i) {
                if (lines[i].rfind("c1 ", 0) == 0 && lines[i + 1].rfind("  race: ", 0) == 0) {
                    home = lines[i + 2];
                    break;
                }
            }
            REQUIRE(home.rfind("  home: ", 0) == 0);
            CHECK((home.size() >= 8U && home.compare(home.size() - 8U, 8U, ", upland") == 0) == c.upland);
        }
    }
}
