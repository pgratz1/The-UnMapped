#pragma once

// The one place of the P5-03 tests' names of library API the spec (docs/specs/p5-03-race-relations.md r1) does
// not give. Phase 1 guessed them; at integration (phase 2) each was checked against the engineer's headers:
//   5, 6.6  RelationsSubsystem (worldgen/history/relations.hpp), a Subsystem, PeopleLedger and DispositionView,
//           RelationsSubsystem(PeoplesSubsystem&, const ConflictSubsystem&, const data::ContentSet&)
//   6.4.1   PeopleLedger (peoples.hpp): moved(state, from, to, people, cause), displaced(state, from, race, people,
//           cause) (lead ruling 6); PeoplesSubsystem::set_ledger, ledger(); abandon(context, settlement, cause)
//   6.7     RelationsSubsystem::finish(end_year), race_relation_rows(), settlement_race_rows()
//   7.1     DispositionView (conflict.hpp): disposition(a, b), hatred_event(a, b); ConflictSubsystem::
//           set_dispositions(const DispositionView*), camp_table()
//   12.2    MonstersSubsystem::set_region_names(const RenamingSubsystem*); RenamingSubsystem::name_now(context, region)
//   16.1    db::BeastRow::bold_phrases, db::MonsterKindRow::raid_phrases (joined with |)
//   16.2    db::RaceRelationRow, db::SettlementRaceRow (members as the columns); HistoryResult::race_relations,
//           settlement_races
//   18      Timeline::race_relation_at, settlement_races; RaceRelationState, RaceShare at namespace scope
//
// The configuration of a run (RelConfig) is the history stage's of 6.6 item 3: peoples, conflict, renaming,
// monsters, relations, with the ledger and the disposition view set; each of the three may be left out (I0 of
// 7.4: "RelationsSubsystem not installed, no ledger and no disposition view set").

#include <cstdint>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "p3_06_conflict_support.hpp"
#include "p5_02_monsters_content.hpp"
#include "worldgen/history/conflict.hpp"
#include "worldgen/history/monsters.hpp"
#include "worldgen/history/peoples.hpp"
#include "worldgen/history/relations.hpp"
#include "worldgen/history/renaming.hpp"
#include "worldgen/history/timeline.hpp"

namespace p503api {

namespace h = worldgen::history;
namespace db = worldgen::db;
namespace wd = worldgen::data;
using Row = eng::Row;
using eng::Tables;

// --- Codes (normative) --------------------------------------------------------------------------------------

inline constexpr std::int64_t HATRED = 0, WARINESS = 1, FRIENDSHIP = 2;              // 6.2.3, 16.2
inline constexpr std::int64_t SUBJECT = 0, TOLERATED = 1, ACCEPTED = 2, RULING = 3;  // 6.3, 16.2
inline constexpr std::int64_t ENTITY_RACE = 1;                                       // 16.4 (entity type 1)
inline constexpr std::int64_t ENDING_FLED = 6;                                       // 11.3

// 8.1, 11.3: the new kinds by id (provisional ids, the next free ones at merge).
inline const std::vector<std::pair<std::int64_t, std::string>>& new_kinds() {
    static const std::vector<std::pair<std::int64_t, std::string>> v = {
        {56, "settlement_fled"},  {57, "peoples_met"},     {58, "disposition_turned"}, {59, "refugees_arrived"},
        {60, "settlement_mixed"}, {61, "standing_changed"}, {62, "people_expelled"}};
    return v;
}

// --- The library types -----------------------------------------------------------------------------------

using RelationsSubsystem = h::RelationsSubsystem;
using PeopleLedger = h::PeopleLedger;
using DispositionView = h::DispositionView;
using RaceRelationState = h::RaceRelationState;
using RaceShare = h::RaceShare;

inline std::unique_ptr<RelationsSubsystem> make_relations(h::PeoplesSubsystem& peoples, h::ConflictSubsystem& conflict,
                                                          const wd::ContentSet& c) {
    return std::make_unique<RelationsSubsystem>(peoples, conflict, c);
}

// --- Rows of the two tables (16.2) -----------------------------------------------------------------------

struct RTables {
    Tables t;
    std::vector<Row> race_relation, settlement_race;
};

inline Row rr_row(const db::RaceRelationRow& x) {
    return {eng::I(x.race_a), eng::I(x.race_b), eng::I(x.year), eng::I(x.disposition), eng::I(x.band)};
}
inline Row sr_row(const db::SettlementRaceRow& x) {
    return {eng::I(x.settlement), eng::I(x.race), eng::I(x.year), eng::I(x.share), eng::I(x.standing)};
}

// The history stage's result as rows.
inline RTables history_stage(const wd::ContentSet& c, const eng::HistoryInputs& in) {
    const h::HistoryResult r = worldgen::stages::history_stage(c, in);
    RTables out;
    out.t = eng::tables_of(r);
    for (const auto& x : r.race_relations) out.race_relation.push_back(rr_row(x));
    for (const auto& x : r.settlement_races) out.settlement_race.push_back(sr_row(x));
    return out;
}

// A stored world's rows (the two tables by name, no new API).
inline RTables tables_of_world(const db::WorldReader& r) {
    RTables out;
    out.t = engine_test::tables_of_world(r);
    for (auto& x : chrono_test::rows_of(r, "race_relation")) out.race_relation.push_back(x);
    for (auto& x : chrono_test::rows_of(r, "settlement_race")) out.settlement_race.push_back(x);
    return out;
}

// --- Test doubles of the seams --------------------------------------------------------------------------

// 6.4.1: a ledger that records every call (in call order).
struct LedgerCall {
    bool moved = false;
    std::int64_t from = 0, to = 0, race = 0, people = 0, cause = 0;
};
class RecordingLedger final : public PeopleLedger {
public:
    // Lead ruling 6 (integration): a move carries its cause; both calls see the state.
    void moved(const h::State& /*state*/, std::int64_t from, std::int64_t to, std::int64_t people,
               std::int64_t cause) override {
        calls.push_back({true, from, to, 0, people, cause});
    }
    void displaced(const h::State& /*state*/, std::int64_t from, std::int64_t race, std::int64_t people,
                   std::int64_t cause) override {
        calls.push_back({false, from, 0, race, people, cause});
    }
    std::vector<LedgerCall> calls;
};

// 7.1: a disposition view given by a function of the two races (and a fixed hatred event).
class FixedView final : public DispositionView {
public:
    using Fn = std::function<std::int64_t(std::int64_t, std::int64_t)>;
    explicit FixedView(Fn d, Fn hatred = [](std::int64_t, std::int64_t) { return std::int64_t{0}; })
        : d_(std::move(d)), hatred_(std::move(hatred)) {}
    [[nodiscard]] std::int64_t disposition(std::int64_t a, std::int64_t b) const override { return d_(a, b); }
    [[nodiscard]] std::int64_t hatred_event(std::int64_t a, std::int64_t b) const override { return hatred_(a, b); }

private:
    Fn d_, hatred_;
};

// --- The configuration of 6.6 item 3 -----------------------------------------------------------------------

struct RelConfig {
    std::unique_ptr<cfl::Conflict> pair;
    std::unique_ptr<eng::InterventionSource> interventions = std::make_unique<eng::EmptyInterventionSource>();
    eng::EventCatalogue catalogue;
    std::unique_ptr<h::RenamingSubsystem> renaming;
    std::unique_ptr<h::MonstersSubsystem> monsters;
    std::unique_ptr<RelationsSubsystem> relations;
    std::vector<std::unique_ptr<engine_test::FnSubsystem>> after;

    struct Options {
        bool relations = true, ledger = true, view = true;
        bool monsters = true;                             // false: P5-02's configuration without the subsystem
        PeopleLedger* other_ledger = nullptr;             // a test ledger in place of the subsystem's
        const DispositionView* other_view = nullptr;      // a test view in place of the subsystem's
    };

    RelConfig(const wd::ContentSet& c, Options o)
        : pair(std::make_unique<cfl::Conflict>(c)), catalogue(eng::catalogue_of(c)),
          renaming(std::make_unique<h::RenamingSubsystem>(*pair->peoples.sub, h::renaming_tuning_from(c), catalogue)) {
        if (o.monsters) {
            monsters = std::make_unique<h::MonstersSubsystem>(*pair->peoples.sub, c);
            pair->peoples.sub->set_wild(monsters.get());
            renaming->set_monsters(monsters.get());
            monsters->set_region_names(renaming.get());
        }
        if (o.relations) relations = make_relations(*pair->peoples.sub, *pair->sub, c);
        if (o.other_ledger != nullptr) {
            pair->peoples.sub->set_ledger(o.other_ledger);
        } else if (o.ledger && relations) {
            pair->peoples.sub->set_ledger(relations.get());
        }
        if (o.other_view != nullptr) {
            pair->sub->set_dispositions(o.other_view);
        } else if (o.view && relations) {
            pair->sub->set_dispositions(relations.get());
        }
    }
    void add_after(std::vector<eng::Step> steps, engine_test::FnSubsystem::Fn fn) {
        after.push_back(std::make_unique<engine_test::FnSubsystem>(std::move(steps), std::move(fn)));
    }
    [[nodiscard]] eng::EngineConfig get() const {
        eng::EngineConfig e{};
        e.awakening = pair->peoples.awakening.get();
        e.subsystems.push_back(pair->peoples.sub.get());
        e.subsystems.push_back(pair->subsystem());
        e.subsystems.push_back(renaming.get());
        if (monsters) e.subsystems.push_back(monsters.get());
        if (relations) e.subsystems.push_back(relations.get());
        for (const auto& s : after) e.subsystems.push_back(s.get());
        e.tension.push_back(pair->tension());
        if (monsters) e.tension.push_back(monsters.get());
        e.interventions = interventions.get();
        e.catalogue = &catalogue;
        return e;
    }
    // One run per configuration (the subsystems keep their state); the rows filled as the stage fills them.
    [[nodiscard]] RTables run(const wd::ContentSet& c, const eng::HistoryInputs& in) const {
        h::HistoryResult r = eng::with_data_rows(c, h::run_engine(c, in, get()));
        r.region_names = renaming->sorted_rows();
        if (monsters) {
            monsters->finish(in.params.span);
            r.beast_populations = monsters->beast_population_rows();
            r.monsters = monsters->monster_rows();
            r.sites = monsters->site_rows();
        }
        RTables out;
        out.t = eng::tables_of(r);
        if (relations) {
            relations->finish(in.params.span);
            for (const auto& x : relations->race_relation_rows()) out.race_relation.push_back(rr_row(x));
            for (const auto& x : relations->settlement_race_rows()) out.settlement_race.push_back(sr_row(x));
        }
        return out;
    }
};

// --- Content edits past the loader ---------------------------------------------------------------------

// Fields of the history definition `id` replaced (each must exist).
inline wd::ContentSet with_history(const wd::ContentSet& base, const std::string& id,
                                   const std::map<std::string, wd::Value>& edits) {
    wd::ContentSet c = base;
    for (wd::KindContent& k : c.kinds) {
        if (k.kind != "history") continue;
        for (wd::Definition& d : k.definitions) {
            if (d.id != id) continue;
            wd::Record r;
            std::size_t used = 0;
            for (std::size_t i = 0; i < d.record.size(); ++i) {
                const auto it = edits.find(d.record.name_at(i));
                used += it != edits.end() ? 1U : 0U;
                r.add(d.record.name_at(i), it != edits.end() ? it->second : d.record.value_at(i));
            }
            REQUIRE_MESSAGE(used == edits.size(), "history." << id << ": an edited field is not in the data");
            d.record = std::move(r);
        }
    }
    return c;
}

// Every race's `stances` replaced by the empty array (4.2's zero fixture, 7.4).
inline wd::ContentSet without_stances(const wd::ContentSet& base) {
    wd::ContentSet c = base;
    for (wd::KindContent& k : c.kinds) {
        if (k.kind != "race") continue;
        for (wd::Definition& d : k.definitions) {
            wd::Record r;
            for (std::size_t i = 0; i < d.record.size(); ++i) {
                const bool st = d.record.name_at(i) == "stances";
                r.add(d.record.name_at(i), st ? wd::Value::array({}) : d.record.value_at(i));
            }
            d.record = std::move(r);
        }
    }
    return c;
}

inline wd::Value V(std::int64_t v) { return wd::Value::integer(v); }

// Every race's `origin_allies` replaced by the empty array (P5-04 4.2's zero fixture).
inline wd::ContentSet without_origin_allies(const wd::ContentSet& base) {
    wd::ContentSet c = base;
    for (wd::KindContent& k : c.kinds) {
        if (k.kind != "race") continue;
        for (wd::Definition& d : k.definitions) {
            wd::Record r;
            for (std::size_t i = 0; i < d.record.size(); ++i) {
                const bool o = d.record.name_at(i) == "origin_allies";
                r.add(d.record.name_at(i), o ? wd::Value::array({}) : d.record.value_at(i));
            }
            d.record = std::move(r);
        }
    }
    return c;
}

// P5-04 4.2: the zero tuning of P5-04's I0 (7.14): no reach, no trade reach, no mine, no origin ally. The
// configurations "without" a subsystem of P5-02 and P5-03 build no alliances either, so their I0 runs the stage with
// this tuning (by P5-04's I0, the same as without the subsystem).
// P5-06 4.4 (integration): and P5-06's zero tuning (name_rulers false, no generals, heroes, casters, exiles or split
// bonus, neutral tempers): those configurations build no figures either, and by P5-06's I0 (7.11) the zero tuning is
// the same as no subsystem.
inline wd::Value tempers_of(std::int64_t v) {
    wd::Record r;
    for (const char* t : {"steady", "warlike", "peaceable", "grasping", "just"}) r.add(t, V(v));
    return wd::Value::record(std::move(r));
}
// P5-04a integration (lead ruling 6a): the zero configuration also sets alliances.grasping_join_permille to 1000
// (neutral), so that the zero figures tuning, whose reigns still draw tempers, equals "no figures subsystem" at 0.28.0.
inline wd::ContentSet figures_zero_of(const wd::ContentSet& base0) {
    const wd::ContentSet base = with_history(base0, "alliances", {{"grasping_join_permille", V(1000)}});
    return with_history(base, "figures",
                        {{"name_rulers", wd::Value::boolean(false)}, {"general_scale_permille", V(0)},
                         {"hero_scale_permille", V(0)}, {"caster_ppm", V(0)}, {"exile_permille", V(0)},
                         {"short_reign_split_permille", V(0)}, {"temper_strain_permille", tempers_of(1000)},
                         {"temper_allied_permille", tempers_of(1000)}, {"temper_spoils_permille", tempers_of(0)}});
}
inline wd::ContentSet alliances_zero_of(const wd::ContentSet& base) {
    return figures_zero_of(without_origin_allies(
        with_history(base, "alliances", {{"ally_hops", V(-1)}, {"trade_hops", V(-1)}, {"mine_permille", V(0)}})));
}

// 4.2 the zero tuning of I0 (7.4), with every race's stances empty.
inline wd::ContentSet zero_of(const wd::ContentSet& base) {
    return without_stances(with_history(
        base, "relations",
        {{"war_memory", V(0)}, {"conquest_memory", V(0)}, {"sack_memory", V(0)}, {"capture_memory", V(0)},
         {"expel_memory", V(0)}, {"shelter_memory", V(0)}, {"meet_hops", V(-1)}, {"welcome_min", V(2001)},
         {"subject_years", V(100000)}, {"mixed_min_permille", V(1001)}, {"expel_permille", V(0)},
         // P5-03a 10.1: the new *_memory fields and peace_memory_per_century 0, meet_sealed_hops -1, the
         // settlers' span 0 (the reference's RELATIONS_ZERO_025)
         {"foe_war_memory", V(0)}, {"foe_monster_memory", V(0)}, {"peace_memory_per_century", V(0)},
         {"meet_sealed_hops", V(-1)}, {"conquest_settlers_span_permille", V(0)}}));
}
inline const wd::ContentSet& zero_content() {
    static const wd::ContentSet c = zero_of(conflict_test::shipped());
    return c;
}

// 24.3 relations-small: the conflict stress content with large *_memory values, band_margin 0, expel_permille
// 1000, welcome_min -2000 and history.peoples abandon_reach_hops 0. The spec names no values for "large": these
// (the memory bounds opened to +-2000) are the test writer's and must equal the reference's (p5_03_relations_
// reference.py) when its fixture lines are recorded (phase 2).
inline const std::map<std::string, wd::Value>& relations_small_edits() {
    static const std::map<std::string, wd::Value> m = {
        // The reference's RELATIONS_SMALL (p5_03_relations_reference.py, readings X2: the last three beyond 24.3's
        // list, so that standings change and expulsions pass the majority guard on the small stress maps).
        {"band_margin", V(0)},       {"memory_min", V(-2000)},     {"memory_max", V(2000)},
        {"war_memory", V(400)},      {"conquest_memory", V(600)},  {"sack_memory", V(300)},
        {"capture_memory", V(200)},  {"expel_memory", V(300)},     {"shelter_memory", V(300)},
        {"expel_permille", V(1000)}, {"welcome_min", V(-2000)},    {"conquest_settlers_permille", V(600)},
        {"subject_years", V(0)},
        // P5-03a 10.1 (the reference's RELATIONS_SMALL_025): meetings by word within 3 borders after 0 years, the
        // new *_memory fields large as the others, no expulsion gap; the settlers' span 400 (600 + 400 = 1000 passes
        // relations_settlers; reading X4)
        {"meet_sealed_hops", V(3)},  {"meet_sealed_years", V(0)},  {"foe_war_memory", V(300)},
        {"foe_monster_memory", V(300)}, {"expel_gap_years", V(0)}, {"conquest_settlers_span_permille", V(400)},
        // P5-04a 14.2 (0.28.0): relations-small holds turn_cooldown_years 0 (the reference's RELATIONS_SMALL_028)
        {"turn_cooldown_years", V(0)}};
    return m;
}
inline const wd::ContentSet& relations_small_content() {
    static const wd::ContentSet c = with_history(with_history(conflict_test::stress_content(), "relations",
                                                              relations_small_edits()),
                                                 "peoples", {{"abandon_reach_hops", V(0)}});
    return c;
}

// 24.3 monsters-small (P5-02) with fear_raids 2 and fear_permille 1000, so that flights happen.
inline const wd::ContentSet& monsters_small_fear_content() {
    static const wd::ContentSet c = with_history(monsters_content::monsters_small_content(), "monsters",
                                                 {{"fear_raids", V(2)}, {"fear_permille", V(1000)}});
    return c;
}

// A field of a history definition of `c`.
inline std::int64_t tuning_int(const wd::ContentSet& c, const std::string& id, const std::string& field) {
    for (const wd::KindContent& k : c.kinds) {
        if (k.kind != "history") continue;
        for (const wd::Definition& d : k.definitions) {
            if (d.id == id) return d.record.get_int(field);
        }
    }
    FAIL("no history." << id);
    return 0;
}

// --- The golden file p5-03-relations.txt (23; written by reference/p5_03_relations_reference.py) ---------
//
// Format (the reference's): LF, one space between tokens, `#` comments;
//   case <case> <seed> <map_size> <span> <races> <extra args or -> [data]
//       world cases: generate's arguments as world-hashes.sh; fixtures: data relations-small | monsters-small |
//       zero on the conflict stress geography <seed>
//   <case> table race_relation <hex>      (= hash --detail's table line, P0-04 8)
//   <case> table settlement_race <hex>
//   <case> events_<key> <n>               (the seven keys of kinds 56 to 62)
//   <case> stat <key> <value>             (17.4's [stats] keys in order, report --history --level full)
// Phase 1 records only what does not depend on calibrated data: every item is optional, a test skips an absent
// one with a MESSAGE.

struct RCase {
    std::string name, data, extra;
    std::uint64_t seed = 0;
    std::int64_t size = 0, span = 0;
    std::string races, detail = "normal", civs = "auto", intensity = "normal", pressure = "normal", phrase;
    std::map<std::string, std::string> tables;
    std::map<std::string, std::int64_t> events;
    std::vector<std::pair<std::string, std::string>> stats;
    [[nodiscard]] bool fixture() const { return !data.empty(); }
};

inline void parse_extra(RCase& c, const std::vector<std::string>& w, std::size_t from, std::size_t to) {
    for (std::size_t i = from; i < to; ++i) {
        const std::string& k = w[i];
        if (k == "-") continue;
        std::string v;
        std::size_t j = i + 1;
        while (j < to && w[j].rfind("--", 0) != 0) {
            v += (v.empty() ? "" : " ") + w[j];
            ++j;
        }
        if (v.size() >= 2U && (v.front() == '"' || v.front() == '\'')) v = v.substr(1, v.size() - 2);
        if (k == "--monster-pressure") c.pressure = v;
        else if (k == "--history-detail") c.detail = v;
        else if (k == "--civs-per-race") c.civs = v;
        else if (k == "--event-intensity") c.intensity = v;
        else if (k == "--seed-phrase") c.phrase = v;
        else if (k == "--history-span") c.span = std::stoll(v);
        else if (k == "--races") c.races = v;
        i = j - 1;
    }
}

inline const std::vector<RCase>& relations_golden() {
    static const std::vector<RCase> g = [] {
        std::vector<RCase> out;
        std::ifstream f(UNMAPPED_P5_03_GOLDEN_PATH, std::ios::binary);
        if (!f.good()) return out;   // phase 1: the file may not exist yet
        std::map<std::string, std::size_t> idx;
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;
            const std::vector<std::string> w = engine_test::split(line, ' ');
            if (w[0] == "case") {
                REQUIRE_MESSAGE(w.size() >= 7U, "short case line: " << line);
                RCase c;
                c.name = w[1];
                c.seed = std::stoull(w[2]);
                c.size = std::stoll(w[3]);
                c.span = std::stoll(w[4]);
                c.races = w[5];
                std::size_t end = w.size();
                const std::string& last = w.back();
                if (last == "relations-small" || last == "monsters-small" || last == "monsters-small-fear" ||
                    last == "zero") {
                    c.data = last;
                    --end;
                }
                parse_extra(c, w, 6, end);
                idx[c.name] = out.size();
                out.push_back(c);
                continue;
            }
            const auto it = idx.find(w[0]);
            REQUIRE_MESSAGE(it != idx.end(), "golden line of no case: " << line);
            RCase& c = out[it->second];
            if (w.at(1) == "table") {
                c.tables[w.at(2)] = w.at(3);
            } else if (w[1] == "stat") {
                c.stats.emplace_back(w.at(2), w.at(3));
            } else if (w[1].rfind("events_", 0) == 0) {
                c.events[w[1].substr(7)] = std::stoll(w.at(2));
            } else {
                FAIL("unknown golden item: " << line);
            }
        }
        return out;
    }();
    return g;
}

}  // namespace p503api
