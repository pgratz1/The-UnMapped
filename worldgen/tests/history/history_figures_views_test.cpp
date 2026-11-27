// Spec P5-06 (docs/specs/p5-06-figures.md r3) 18.1 items 12, 14 (in part), 15 and 16, 20 and 21 (in part), on a
// generated figures-small world (every kind of 79 to 92): report v8 (the header, [figures] at both levels with its
// count line, the rulers: lines of [civilizations], the [sites] count line, the [stats] keys of 10.5 in order, no
// ", faded 0"), the changed templates' {r:under} (a ruler named at every war_declared, 8.2), the timeline reads of 12
// (ruler_at, figures_alive, generals_of, figure_at, events_of_figure), the browser groups of 11.3 and the tower
// marker of 11.4. Hand-made fixture H17 (18.1 item 14's full list) is not part of phase 1: the report's exact lines on
// it wait for the fixture. Written from the spec before the implementation; every case starts with
// P506_REQUIRE_INTEGRATED().

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "../history_report_checks.hpp"
#include "p5_06_figures_api.hpp"
#include "p5_06_figures_checks.hpp"
#include "p5_06_figures_runs.hpp"
#include "worldgen/history/timeline.hpp"

namespace {

namespace db = worldgen::db;
namespace h = worldgen::history;
using worldgen::inspect::HistoryLevel;

const std::string& world() {
    static const std::string p = p506run::generated(42, 512, p506run::figures_small(), "small");
    return p;
}

std::vector<std::string> lines_of(const std::string& text) {
    std::vector<std::string> out;
    std::istringstream in(text);
    std::string l;
    while (std::getline(in, l)) out.push_back(l);
    return out;
}

// The lines of block `name` ("[figures]"), its heading excluded, up to the next block.
std::vector<std::string> block(const std::string& text, const std::string& name) {
    std::vector<std::string> out;
    bool in = false;
    for (const std::string& l : lines_of(text)) {
        if (!l.empty() && l[0] == '[') {
            if (in) break;
            in = l == name;
            continue;
        }
        if (in) out.push_back(l);
    }
    return out;
}

bool starts(const std::string& s, const std::string& p) { return s.rfind(p, 0) == 0U; }

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-06 10.1, 10.2, 10.4, 10.5: report v8: header, [figures], [sites] with towers, the [stats] keys") {
    P506_REQUIRE_INTEGRATED();
    const db::WorldReader r = chrono_test::open_world(world());
    const p506check::World w = p506check::world_of_file(r);
    for (const HistoryLevel level : {HistoryLevel::summary, HistoryLevel::full}) {
        const std::string text = hrtest::report_of(r, level);
        CAPTURE(level == HistoryLevel::full);
        CHECK(starts(text, "# worldgen history v11"));
        const auto fb = block(text, "[figures]");
        REQUIRE_FALSE(fb.empty());
        std::map<std::int64_t, std::int64_t> by_role;
        std::int64_t alive = 0;
        for (const auto& [id, f] : w.figs) {
            ++by_role[f.role];
            alive += f.death_event == 0 ? 1 : 0;
        }
        const std::string count = "figures: " + std::to_string(w.figs.size()) + "; rulers " +
                                  std::to_string(by_role[1]) + ", claimants " + std::to_string(by_role[2]) +
                                  ", generals " + std::to_string(by_role[3]) + ", heroes " + std::to_string(by_role[4]) +
                                  ", casters " + std::to_string(by_role[5]) + "; alive at the end " +
                                  std::to_string(alive);
        CHECK(fb.front() == count);
        std::size_t listed = 0;
        for (const std::string& l : fb) listed += starts(l, "  f") ? 1U : 0U;
        if (level == HistoryLevel::full) {
            CHECK(listed == w.figs.size());
            CHECK(starts(fb.back(), "books: " + std::to_string(w.books.size()) + ";"));
        } else if (w.civs.size() > 16U) {   // 10.2: more than SUMMARY_LINES_CIVS realms: the count line only
            CHECK(fb.size() == 1U);
        } else {
            CHECK(listed <= static_cast<std::size_t>(p506api::SUMMARY_FIGURES_MAX_SPEC));
            CHECK(fb.back() == "books: " + std::to_string(w.books.size()));
        }
        const auto sb = block(text, "[sites]");
        REQUIRE_FALSE(sb.empty());
        std::int64_t towers = 0;
        for (const auto& [id, s] : w.sites) towers += s.at(2).integer == 4 ? 1 : 0;
        CHECK(sb.front().find(", towers " + std::to_string(towers)) != std::string::npos);
        CHECK(text.find(", faded 0") == std::string::npos);   // 25.1 (d)
        CHECK(text.find(", 0 faded") == std::string::npos);
    }
    const std::string full = hrtest::report_of(r, HistoryLevel::full);
    const std::vector<std::string> keys = {
        "figures", "figures_rulers", "figures_claimants", "figures_generals", "figures_heroes", "figures_casters",
        "figures_alive", "figures_epithet", "epithet_bearers_max", "reigns", "rulers_recorded", "rulers_by_deed",
        "rulers_regnal", "successions_disputed", "exiles", "generals_appointed", "battles_led", "generals_fallen",
        "heroes_risen", "monsters_slain_by_heroes", "heroes_slain", "towers", "books", "blights",
        "figure_age_median", "summary_major_calamities"};
    std::size_t at = 0;
    for (const std::string& k : keys) {
        CAPTURE(k);
        const std::size_t p = full.find("\n" + k + " ", at);
        CHECK(p != std::string::npos);
        if (p != std::string::npos) at = p;
    }
}

TEST_CASE("P5-06 10.3: every civilization block ends with its rulers: line; at summary the count line only") {
    P506_REQUIRE_INTEGRATED();
    const db::WorldReader r = chrono_test::open_world(world());
    const std::string text = hrtest::report_of(r, HistoryLevel::summary);
    std::size_t n = 0;
    for (const std::string& l : block(text, "[civilizations]")) {
        if (starts(l, "  rulers: ")) {
            ++n;
            CHECK(l.find(" reign") != std::string::npos);
            CHECK(l.find(" recorded; first ") != std::string::npos);
        }
    }
    const p506check::World w = p506check::world_of_file(r);
    if (w.civs.size() <= 16U) CHECK(n == w.civs.size());   // SUMMARY_LINES_CIVS (16) as [monsters]
}

TEST_CASE("P5-06 8.2, 7.9: war_declared names its ruler ({attacker:under}); battle_led names its generals") {
    P506_REQUIRE_INTEGRATED();
    const std::string text = hrtest::report_of(chrono_test::open_world(world()), HistoryLevel::full);
    std::size_t wars = 0, under = 0;
    for (const std::string& l : lines_of(text)) {
        if (l.find("war_declared/") != std::string::npos) {
            ++wars;
            under += l.find(" under ") != std::string::npos ? 1U : 0U;
        }
    }
    CHECK(wars > 0U);
    CHECK(under == wars);   // invariant 3: a recorded ruler at every declaration, so the clause is never empty
}

TEST_CASE("P5-06 12: ruler_at, figures_alive, generals_of, figure_at, events_of_figure agree with the rows") {
    P506_REQUIRE_INTEGRATED();
    const db::WorldReader r = chrono_test::open_world(world());
    auto lh = h::load_history(r);
    REQUIRE_MESSAGE(lh.ok(), (lh.ok() ? "" : lh.error().message));
    const h::Timeline& t = lh.value().timeline;
    const p506check::World w = p506check::world_of_file(r);
    std::size_t checked = 0;
    for (const auto& [id, f] : w.figs) {
        if (f.role != 1 || checked > 40U) continue;
        ++checked;
        const std::int64_t end = f.died >= 0 ? f.died : w.E;
        if (f.office_from < end) CHECK(p506api::ruler_at(t, f.civ, f.office_from) == id);
        if (end - 1 >= f.office_from) CHECK(p506api::ruler_at(t, f.civ, end - 1) == id);
        const auto alive = p506api::figures_alive(t, f.born);
        CHECK(std::find(alive.begin(), alive.end(), id) != alive.end());
        const p506api::FigureState st = p506api::figure_at(t, id, f.born);
        CHECK(st.id == id);
        CHECK(st.civ == f.civ);
        CHECK(st.alive);
        const auto ev = p506api::events_of_figure(t, id);
        CHECK(std::find(ev.begin(), ev.end(), f.start_event) != ev.end());
        CHECK(std::is_sorted(ev.begin(), ev.end()));
    }
    for (const std::int64_t b : w.of("battle_led")) {
        for (const auto& [role, side] : std::vector<std::pair<std::string, std::string>>{{"winner_general", "winner"},
                                                                                         {"loser_general", "loser"}}) {
            for (const std::int64_t g : w.role(b, role)) {
                const std::int64_t c = w.one(b, side), y = w.year(b);
                const auto gs = p506api::generals_of(t, c, y);
                // 12: in force at Y is since <= Y < until; a command that ends in the battle's own year (a fall in
                // it, the war's end in that year) is not in force at Y (integration: a test reading, not the spec's)
                bool ends_that_year = false;
                for (const p506check::Rel& rel : w.rels) {
                    ends_that_year = ends_that_year || (rel.kind == "served" && rel.a == g && rel.b == c &&
                                                        rel.since <= y && rel.until_event != 0 &&
                                                        w.year(rel.until_event) == y);
                }
                CHECK((std::find(gs.begin(), gs.end(), g) != gs.end() || ends_that_year));
            }
        }
    }
}

}  // TEST_SUITE

// The lead's ruling of 2026-11-25 (integration): a reign or a life still open at the end year E holds at Y = E, so
// ruler_at(c, E) is the present ruler and figures_alive(E) holds every living figure. EXPECTED TO FAIL until the
// library fix that follows the P5-06 integration (the engine of 2750025 uses since <= Y < E for open rows).
TEST_SUITE("history") {

TEST_CASE("P5-06 12 (lead's ruling): ruler_at and figures_alive hold an open reign or life at Y = E") {
    P506_REQUIRE_INTEGRATED();
    const db::WorldReader r = chrono_test::open_world(world());
    auto lh = h::load_history(r);
    REQUIRE_MESSAGE(lh.ok(), (lh.ok() ? "" : lh.error().message));
    const h::Timeline& t = lh.value().timeline;
    const p506check::World w = p506check::world_of_file(r);
    const auto alive = p506api::figures_alive(t, w.E);
    std::size_t open_reigns = 0;
    for (const auto& [id, f] : w.figs) {
        CAPTURE(id);
        if (f.death_event == 0) CHECK(std::find(alive.begin(), alive.end(), id) != alive.end());
        for (const p506check::Rel& rel : w.rels) {
            if (rel.kind == "ruled" && rel.a == id && rel.until_event == 0) {
                ++open_reigns;
                CHECK(p506api::ruler_at(t, f.civ, w.E) == id);
            }
        }
    }
    CHECK(open_reigns > 0U);
}

}  // TEST_SUITE
