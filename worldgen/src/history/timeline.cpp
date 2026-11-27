// The timeline of spec P4-01 (sections 4, 5.1, 5.2, 6).

#include "worldgen/history/timeline.hpp"

#include <algorithm>
#include <tuple>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "worldgen/core/error.hpp"
#include "worldgen/db/schema.hpp"

namespace worldgen::history {

namespace {

std::string num(std::int64_t v) {
    return db::format_i64(v);
}

[[noreturn]] void timeline_fatal(const std::string& what) {
    throw FatalError("timeline: " + what);
}

constexpr std::int64_t TYPE_CIVILIZATION = static_cast<std::int64_t>(EntityType::civilization);
constexpr std::int64_t TYPE_SETTLEMENT = static_cast<std::int64_t>(EntityType::settlement);
constexpr std::int64_t TYPE_REGION = static_cast<std::int64_t>(EntityType::region);

// Appends id unless it is already the list's last element (6.1: events are
// visited in id order, so each list is strictly ascending, each event once).
void append_once(std::vector<std::int64_t>& list, std::int64_t id) {
    if (list.empty() || list.back() != id) {
        list.push_back(id);
    }
}

// The index id - 1 of an id in 1 .. n, or std::nullopt.
std::optional<std::size_t> index_of(std::int64_t id, std::size_t n) noexcept {
    if (id < 1 || static_cast<std::uint64_t>(id) > static_cast<std::uint64_t>(n)) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(id - 1);
}

// The position of a region id in the ascending id list, or std::nullopt.
std::optional<std::size_t> position_of(const std::vector<std::int64_t>& ids, std::int64_t id) noexcept {
    const auto it = std::lower_bound(ids.begin(), ids.end(), id);
    if (it == ids.end() || *it != id) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(it - ids.begin());
}

// What the timeline relies on in the log, read separately from the tables:
// the message of history_reads_disagree (P3-07 5.2 item 4a) when an event is
// out of order, or a participant, place or cause of the log is not in the
// tables. Reached only when the file changed between the two reads.
std::optional<std::string> timeline_reads_disagree(const HistoryView& view, const HistoryTables& t) {
    const std::string changed = "corrupt table 'event': the file changed while it was read";
    std::vector<std::int64_t> region_ids;
    region_ids.reserve(t.regions.size());
    for (const RegionInfo& r : t.regions) {
        region_ids.push_back(r.id);
    }
    const std::size_t n = view.events().size();
    std::int64_t expected_id = 1;
    std::int64_t previous_year = 0;
    for (const HistoryView::Event& e : view.events()) {
        // The order the index relies on (P3-04 11.1, checked on the first read).
        if (e.row.id != expected_id || (expected_id > 1 && e.row.year < previous_year) ||
            e.row.end_year < e.row.year) {
            return changed;
        }
        ++expected_id;
        previous_year = e.row.year;
        if (e.row.region != 0 && !position_of(region_ids, e.row.region)) {
            return changed;
        }
        if (e.row.settlement != 0 && !index_of(e.row.settlement, t.settlements.size())) {
            return changed;
        }
        for (const db::EventParticipantRow& p : e.participants) {
            if ((p.entity_type == TYPE_CIVILIZATION && !index_of(p.entity_id, t.civilizations.size())) ||
                (p.entity_type == TYPE_SETTLEMENT && !index_of(p.entity_id, t.settlements.size())) ||
                (p.entity_type == TYPE_REGION && !position_of(region_ids, p.entity_id))) {
                return changed;
            }
        }
        for (const std::int64_t c : e.causes) {
            if (!index_of(c, n)) {
                return changed;
            }
        }
    }
    return std::nullopt;
}

// P4-03 9.4 (audit A3): within each region's run of control rows (the runs
// are contiguous, checked before), each row with a to_year ends after it
// starts, and for consecutive rows p, r of one region p is closed and
// p.to_year <= r.from_year. Rows are visited in order, a row's own end
// before its pairing with the row after it.
void check_control_runs(const std::vector<db::RegionControlRow>& rows) {
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const db::RegionControlRow& r = rows[i];
        if (r.to_year && *r.to_year <= r.from_year) {
            timeline_fatal("region_control row (" + num(r.region) + ", " + num(r.from_year) + ") ends at " +
                           num(*r.to_year) + ", not after it starts");
        }
        if (i == 0U) {
            continue;
        }
        const db::RegionControlRow& p = rows[i - 1U];
        if (p.region == r.region && (!p.to_year || *p.to_year > r.from_year)) {
            timeline_fatal("region_control rows of region " + num(r.region) + " overlap at year " +
                           num(r.from_year));
        }
    }
}

}  // namespace

// --- Build (6.1) -------------------------------------------------------------

Timeline Timeline::build(const HistoryView& view, const HistoryTables& tables) {
    Timeline t;
    t.end_year_ = tables.end_year;

    // Regions and their control intervals (ordered by region, from_year).
    t.region_ids_.reserve(tables.regions.size());
    for (const RegionInfo& r : tables.regions) {
        if (!t.region_ids_.empty() && r.id <= t.region_ids_.back()) {
            timeline_fatal("region " + num(r.id) + " out of order");
        }
        t.region_ids_.push_back(r.id);
    }
    t.control_ = tables.region_control;
    t.control_offsets_.assign(t.region_ids_.size() + 1U, 0U);
    {
        std::size_t row = 0;
        for (std::size_t p = 0; p < t.region_ids_.size(); ++p) {
            t.control_offsets_[p] = row;
            while (row < t.control_.size() && t.control_[row].region == t.region_ids_[p]) {
                ++row;
            }
        }
        t.control_offsets_[t.region_ids_.size()] = row;
        if (row != t.control_.size()) {
            timeline_fatal("region_control row of region " + num(t.control_[row].region) + " out of order");
        }
    }
    // r3 16: the names by year.
    for (const RegionInfo& r : tables.regions) {
        t.region_names_.push_back(r.name);
    }
    t.names_ = tables.region_names;
    t.name_offsets_.assign(t.region_ids_.size() + 1U, 0U);
    {
        std::size_t row = 0;
        for (std::size_t p = 0; p < t.region_ids_.size(); ++p) {
            t.name_offsets_[p] = row;
            while (row < t.names_.size() && t.names_[row].region == t.region_ids_[p]) {
                ++row;
            }
        }
        t.name_offsets_[t.region_ids_.size()] = row;
        if (row != t.names_.size()) {
            timeline_fatal("region_name row of region " + num(t.names_[row].region) + " out of order");
        }
    }
    check_control_runs(t.control_);
    for (const db::RegionControlRow& r : t.control_) {
        t.change_years_.push_back(r.from_year);
        if (r.to_year) {
            t.change_years_.push_back(*r.to_year);
        }
    }
    // determinism-lint: allow(unstable-sort) -- plain integers: equal elements are identical, and unique() removes them
    std::sort(t.change_years_.begin(), t.change_years_.end());
    t.change_years_.erase(std::unique(t.change_years_.begin(), t.change_years_.end()), t.change_years_.end());

    // Settlements (ids 1 .. S) and their census rows (ordered by settlement, year).
    t.settlement_founded_.reserve(tables.settlements.size());
    for (const db::SettlementRow& s : tables.settlements) {
        if (s.id != static_cast<std::int64_t>(t.settlement_founded_.size()) + 1) {
            timeline_fatal("settlement " + num(s.id) + " out of order");
        }
        t.settlement_founded_.push_back(s.founded_year);
        t.settlement_ending_.push_back(s.ending);  // P5-02 13
    }
    t.census_ = tables.census;
    t.census_offsets_.assign(t.settlement_founded_.size() + 1U, 0U);
    {
        std::size_t row = 0;
        for (std::size_t s = 0; s < t.settlement_founded_.size(); ++s) {
            t.census_offsets_[s] = row;
            const auto id = static_cast<std::int64_t>(s) + 1;
            while (row < t.census_.size() && t.census_[row].settlement == id) {
                ++row;
            }
            if (row == t.census_offsets_[s]) {
                timeline_fatal("settlement " + num(id) + " has no census row");
            }
        }
        t.census_offsets_[t.settlement_founded_.size()] = row;
        if (row != t.census_.size()) {
            timeline_fatal("census row of settlement " + num(t.census_[row].settlement) + " out of order");
        }
    }

    t.relations_ = tables.relations;

    // Civilizations (ids 1 .. C).
    for (const db::CivilizationRow& c : tables.civilizations) {
        if (c.id != static_cast<std::int64_t>(t.civ_founded_.size()) + 1) {
            timeline_fatal("civilization " + num(c.id) + " out of order");
        }
        t.civ_founded_.push_back(c.founded_year);
        t.civ_ended_.push_back(c.ended_year);
    }

    // Phases and ages (ascending id and start_year; they tile [0, E - 1]).
    for (const db::PhaseRow& p : tables.phases) {
        if (!t.phases_.empty() && p.start_year <= t.phases_.back().start_year) {
            timeline_fatal("phase " + num(p.id) + " out of order");
        }
        t.phases_.push_back(Span{p.start_year, p.end_year, p.id});
    }
    for (const db::AgeRow& a : tables.ages) {
        if (!t.ages_.empty() && a.start_year <= t.ages_.back().start_year) {
            timeline_fatal("age " + num(a.id) + " out of order");
        }
        t.ages_.push_back(Span{a.start_year, a.end_year, a.id});
    }

    // P5-02 13: sites, monsters and beast counts.
    t.sites_ = tables.sites;
    for (const db::SiteRow& x : t.sites_) {
        if (x.id != static_cast<std::int64_t>(t.site_ids_.size()) + 1 ||
            (!t.site_origin_.empty() && x.origin_year < t.site_origin_.back())) {
            timeline_fatal("site " + num(x.id) + " out of order");
        }
        t.site_ids_.push_back(x.id);
        t.site_origin_.push_back(x.origin_year);
    }
    t.monsters_ = tables.monsters;
    t.monster_awake_.resize(t.monsters_.size());
    t.monster_lairs_.resize(t.monsters_.size());
    for (std::size_t i = 0; i < t.monsters_.size(); ++i) {
        if (t.monsters_[i].id != static_cast<std::int64_t>(i) + 1) {
            timeline_fatal("monster " + num(t.monsters_[i].id) + " out of order");
        }
    }
    for (const db::SiteRow& x : t.sites_) {
        if (x.kind == 1) {
            const std::optional<std::size_t> m = index_of(x.monster, t.monsters_.size());
            if (!m) {
                timeline_fatal("site " + num(x.id) + " names no monster");
            }
            t.monster_lairs_[*m].push_back(x.id);
        }
    }
    {
        // By stored key (P3-07 21.2): 44, 45, 46 awake; 47, 51 asleep.
        const EventCatalogue& cat = view.catalogue();
        const auto id = [&cat](std::string_view key) {
            const KindDef* k = cat.find(key);
            return k != nullptr ? k->id : -1;
        };
        const std::int64_t appeared = id("monster_appeared"), rose = id("undead_rose"), woke = id("monster_woke");
        const std::int64_t slept = id("monster_slept"), driven = id("monster_driven_off");
        // P5-07 11.1: 94 awake, 95 asleep; 93 a mine's depth.
        const std::int64_t deep_woken = id("deep_woken"), deep_sealed = id("deep_sealed");
        const std::int64_t delved = id("delving_deepened");
        // r2 11.1: 98 a warren's depth, 99 asleep.
        const std::int64_t warren_delved = id("warren_delved"), warren_sealed = id("warren_sealed");
        t.warren_depths_.resize(t.settlement_founded_.size());
        const KindDef* warren_kind = cat.find_id(warren_delved);
        const std::int64_t warren_depth_seq = warren_kind != nullptr ? warren_kind->value_seq("depth") : 0;
        t.site_depths_.resize(t.sites_.size());
        t.monster_sealed_.assign(t.monsters_.size(), -1);
        const KindDef* delved_kind = cat.find_id(delved);
        const RoleDef* delved_site = delved_kind != nullptr ? delved_kind->find_role("site") : nullptr;
        const std::int64_t depth_seq = delved_kind != nullptr ? delved_kind->value_seq("depth") : 0;
        for (const HistoryView::Event& e : view.events()) {
            const std::int64_t k = e.row.kind;
            if (k == warren_delved && warren_delved >= 0) {
                const std::optional<std::size_t> s = index_of(e.row.settlement, t.warren_depths_.size());
                if (!s) {
                    timeline_fatal("event " + num(e.row.id) + " names no settlement");
                }
                const std::int64_t d =
                    warren_depth_seq >= 1 && static_cast<std::size_t>(warren_depth_seq) <= e.values.size()
                        ? e.values[static_cast<std::size_t>(warren_depth_seq - 1)]
                        : 0;
                t.warren_depths_[*s].emplace_back(e.row.year, d);
                continue;
            }
            if (k == delved && delved_site != nullptr) {
                for (const db::EventParticipantRow& p : e.participants) {
                    if (p.role != delved_site->seq) {
                        continue;
                    }
                    const std::optional<std::size_t> s = index_of(p.entity_id, t.sites_.size());
                    if (!s) {
                        timeline_fatal("event " + num(e.row.id) + " names no site");
                    }
                    const std::int64_t d = depth_seq >= 1 && static_cast<std::size_t>(depth_seq) <= e.values.size()
                                               ? e.values[static_cast<std::size_t>(depth_seq - 1)]
                                               : 0;
                    t.site_depths_[*s].emplace_back(e.row.year, d);
                    break;
                }
                continue;
            }
            const bool awake = k == appeared || k == rose || k == woke || k == deep_woken;
            if (!awake && k != slept && k != driven && k != deep_sealed && k != warren_sealed) {
                continue;
            }
            const KindDef* kd = cat.find_id(k);
            const RoleDef* role = kd != nullptr ? kd->find_role("monster") : nullptr;
            for (const db::EventParticipantRow& p : e.participants) {
                if (role != nullptr && p.role == role->seq) {
                    const std::optional<std::size_t> m = index_of(p.entity_id, t.monsters_.size());
                    if (!m) {
                        timeline_fatal("event " + num(e.row.id) + " names no monster");
                    }
                    t.monster_awake_[*m].emplace_back(e.row.year, awake);
                    if ((k == deep_sealed || k == warren_sealed) && t.monster_sealed_[*m] < 0) {
                        t.monster_sealed_[*m] = e.row.year;
                    }
                    break;
                }
            }
        }
    }
    t.beast_rows_ = tables.beast_populations;
    t.beast_kinds_ = tables.beasts.size();
    {
        const std::size_t slots = t.region_ids_.size() * t.beast_kinds_;
        t.beast_offsets_.assign(slots + 1U, 0U);
        std::size_t row = 0;
        for (std::size_t slot = 0; slot < slots; ++slot) {
            t.beast_offsets_[slot] = row;
            const std::int64_t region = t.region_ids_[slot / t.beast_kinds_];
            const auto beast = static_cast<std::int64_t>(slot % t.beast_kinds_) + 1;
            while (row < t.beast_rows_.size() && t.beast_rows_[row].region == region && t.beast_rows_[row].beast == beast) {
                ++row;
            }
        }
        t.beast_offsets_[slots] = row;
        if (row != t.beast_rows_.size()) {
            timeline_fatal("beast_population row (" + num(t.beast_rows_[row].region) + ", " +
                           num(t.beast_rows_[row].beast) + ") out of order");
        }
    }

    // P5-03 18: race relations and shares.
    for (const db::RaceRow& r : tables.races) {
        t.race_ids_.push_back(r.id);
    }
    std::stable_sort(t.race_ids_.begin(), t.race_ids_.end());
    {
        const KindDef* met = view.catalogue().find("peoples_met");
        for (const HistoryView::Event& e : view.events()) {
            if (met == nullptr || e.row.kind != met->id) {
                continue;
            }
            std::int64_t a = 0;
            std::int64_t b = 0;
            for (const db::EventParticipantRow& p : e.participants) {
                (p.role == 1 ? a : b) = p.entity_id;
            }
            t.met_years_.emplace_back(std::min(a, b), std::max(a, b), e.row.year);
        }
        std::stable_sort(t.met_years_.begin(), t.met_years_.end());
    }
    t.race_relations_ = tables.race_relations;
    t.settlement_races_ = tables.settlement_races;
    t.share_offsets_.assign(t.settlement_founded_.size() + 1U, 0U);
    {
        std::size_t row = 0;
        for (std::size_t i = 0; i < t.settlement_founded_.size(); ++i) {
            t.share_offsets_[i] = row;
            while (row < t.settlement_races_.size() &&
                   t.settlement_races_[row].settlement == static_cast<std::int64_t>(i) + 1) {
                ++row;
            }
        }
        t.share_offsets_[t.settlement_founded_.size()] = row;
        if (row != t.settlement_races_.size()) {
            timeline_fatal("settlement_race row (" + num(t.settlement_races_[row].settlement) + ", " +
                           num(t.settlement_races_[row].race) + ") out of order");
        }
    }

    // Events (index id - 1), per-entity lists and effects.
    const std::span<const HistoryView::Event> events = view.events();
    const std::size_t n = events.size();
    t.event_year_.reserve(n);
    t.event_end_.reserve(n);
    t.civ_events_.resize(t.civ_founded_.size());
    t.region_events_.resize(t.region_ids_.size());
    t.settlement_events_.resize(t.settlement_founded_.size());
    t.effects_.resize(n);
    const auto civ_list = [&t](std::int64_t c) -> std::vector<std::int64_t>& {
        const std::optional<std::size_t> i = index_of(c, t.civ_events_.size());
        if (!i) {
            timeline_fatal("no civilization " + num(c));
        }
        return t.civ_events_[*i];
    };
    const auto settlement_list = [&t](std::int64_t s) -> std::vector<std::int64_t>& {
        const std::optional<std::size_t> i = index_of(s, t.settlement_events_.size());
        if (!i) {
            timeline_fatal("no settlement " + num(s));
        }
        return t.settlement_events_[*i];
    };
    const auto region_list = [&t](std::int64_t r) -> std::vector<std::int64_t>& {
        const std::optional<std::size_t> p = position_of(t.region_ids_, r);
        if (!p) {
            timeline_fatal("no region " + num(r));
        }
        return t.region_events_[*p];
    };
    for (std::size_t i = 0; i < n; ++i) {
        const HistoryView::Event& e = events[i];
        const std::int64_t id = e.row.id;
        if (id != static_cast<std::int64_t>(i) + 1) {
            timeline_fatal("event " + num(id) + " out of order");
        }
        if (!t.event_year_.empty() && e.row.year < t.event_year_.back()) {
            timeline_fatal("event " + num(id) + " dated before the event before it");
        }
        if (e.row.end_year < e.row.year) {
            timeline_fatal("event " + num(id) + " ends before it starts");
        }
        t.event_year_.push_back(e.row.year);
        t.event_end_.push_back(e.row.end_year);
        if (e.row.end_year > e.row.year) {
            t.multi_year_.push_back(id);
        }
        if (e.row.region != 0) {
            append_once(region_list(e.row.region), id);
        }
        if (e.row.settlement != 0) {
            append_once(settlement_list(e.row.settlement), id);
        }
        for (const db::EventParticipantRow& p : e.participants) {
            if (p.entity_type == TYPE_CIVILIZATION) {
                append_once(civ_list(p.entity_id), id);
            } else if (p.entity_type == TYPE_SETTLEMENT) {
                append_once(settlement_list(p.entity_id), id);
            } else if (p.entity_type == TYPE_REGION) {
                append_once(region_list(p.entity_id), id);
            }
        }
        for (const std::int64_t c : e.causes) {
            const std::optional<std::size_t> ci = index_of(c, n);
            if (!ci) {
                timeline_fatal("no event " + num(c));
            }
            append_once(t.effects_[*ci], id);
        }
    }

    // P5-06 12: figures, their reigns and commands.
    t.figures_ = tables.figures;
    for (std::size_t i = 0; i < t.figures_.size(); ++i) {
        if (t.figures_[i].id != static_cast<std::int64_t>(i) + 1) {
            timeline_fatal("figure " + num(t.figures_[i].id) + " out of order");
        }
    }
    t.figure_events_ = figure_event_lists(view);
    if (t.figure_events_.size() != t.figures_.size()) {
        timeline_fatal("the view has " + num(static_cast<std::int64_t>(t.figure_events_.size())) + " figures, the tables " +
                       num(static_cast<std::int64_t>(t.figures_.size())));
    }
    t.figure_exiled_.assign(t.figures_.size(), -1);
    if (const KindDef* exiled = view.catalogue().find("exiled")) {
        const RoleDef* role = exiled->find_role("figure");
        for (const HistoryView::Event& e : events) {
            if (e.row.kind != exiled->id || role == nullptr) {
                continue;
            }
            for (const db::EventParticipantRow& p : e.participants) {
                if (p.role != role->seq) {
                    continue;
                }
                const std::optional<std::size_t> f = index_of(p.entity_id, t.figures_.size());
                if (!f) {
                    timeline_fatal("event " + num(e.row.id) + " names no figure");
                }
                if (t.figure_exiled_[*f] < 0) {
                    t.figure_exiled_[*f] = e.row.year;
                }
                break;
            }
        }
    }
    t.civ_rulers_.resize(t.civ_founded_.size());
    t.civ_generals_.resize(t.civ_founded_.size());
    constexpr std::int64_t TYPE_FIGURE = static_cast<std::int64_t>(EntityType::figure);
    for (const db::RelationRow& r : t.relations_) {
        const bool ruled = r.kind == "ruled";
        if ((!ruled && r.kind != "served") || r.a_type != TYPE_FIGURE || r.b_type != TYPE_CIVILIZATION) {
            continue;
        }
        const std::optional<std::size_t> f = index_of(r.a_id, t.figures_.size());
        const std::optional<std::size_t> c = index_of(r.b_id, t.civ_founded_.size());
        if (!f || !c) {
            timeline_fatal(r.kind + " row of figure " + num(r.a_id) + " and civilization " + num(r.b_id) +
                           " names no such entity");
        }
        // An open row (no until) holds through E inclusive (lead ruling 2026-11-25).
        const std::int64_t to = r.until.value_or(t.end_year_ + 1);
        if (ruled) {
            t.civ_rulers_[*c].push_back(Term{t.figures_[*f].office_from, to, r.a_id});
        } else {
            t.civ_generals_[*c].push_back(Term{r.since, to, r.a_id});
        }
    }
    for (std::vector<Term>& terms : t.civ_rulers_) {
        std::stable_sort(terms.begin(), terms.end(), [](const Term& a, const Term& b) {
            return a.from != b.from ? a.from < b.from : a.figure < b.figure;
        });
    }
    // P5-08 11.1: cults and the divine effects.
    t.factions_ = tables.factions;
    for (std::size_t i = 0; i < t.factions_.size(); ++i) {
        if (t.factions_[i].id != static_cast<std::int64_t>(i) + 1) {
            timeline_fatal("faction " + num(t.factions_[i].id) + " out of order");
        }
    }
    t.faction_hidden_.assign(t.factions_.size(), -1);
    t.divine_effects_.assign(t.civ_founded_.size(), {});
    {
        const EventCatalogue& catalogue = view.catalogue();
        const KindDef* bless = catalogue.find("divine_bless");
        const KindDef* curse = catalogue.find("divine_curse");
        const KindDef* suppressed = catalogue.find("cult_suppressed");
        const auto value = [](const KindDef& k, const HistoryView::Event& e, std::string_view key) -> std::int64_t {
            const std::int64_t seq = k.value_seq(key);
            return seq >= 1 && static_cast<std::size_t>(seq) <= e.values.size() ? e.values[static_cast<std::size_t>(seq - 1)]
                                                                                : 0;
        };
        const auto role = [](const KindDef& k, const HistoryView::Event& e, std::string_view key) -> std::int64_t {
            const RoleDef* r = k.find_role(key);
            for (const db::EventParticipantRow& p : e.participants) {
                if (r != nullptr && p.role == r->seq) {
                    return p.entity_id;
                }
            }
            return 0;
        };
        for (const HistoryView::Event& e : view.events()) {
            const std::int64_t k = e.row.kind;
            if ((bless != nullptr && k == bless->id) || (curse != nullptr && k == curse->id)) {
                const KindDef& kd = k == bless->id ? *bless : *curse;
                const std::int64_t effect = value(kd, e, "effect");
                const std::int64_t bit = &kd == bless ? (effect == 1 ? 1 : effect == 2 ? 4 : 0)
                                                      : (effect == 2 ? 2 : effect == 3 ? 8 : 0);
                const std::optional<std::size_t> c = index_of(role(kd, e, "civilization"), t.divine_effects_.size());
                if (bit != 0 && c) {
                    const std::int64_t years = std::max<std::int64_t>(0, value(kd, e, "years"));
                    const std::int64_t until = e.row.year > std::numeric_limits<std::int64_t>::max() - years
                                                   ? std::numeric_limits<std::int64_t>::max()
                                                   : e.row.year + years;
                    t.divine_effects_[*c].emplace_back(e.row.year, until, bit);
                }
            } else if (suppressed != nullptr && k == suppressed->id && value(*suppressed, e, "hidden") == 1) {
                const std::optional<std::size_t> f = index_of(role(*suppressed, e, "faction"), t.faction_hidden_.size());
                if (f && t.faction_hidden_[*f] < 0) {
                    t.faction_hidden_[*f] = e.row.year;
                }
            }
        }
    }
    return t;
}

// --- Cults and the divine effects (P5-08 11.1) --------------------------------------

std::int64_t Timeline::faith_at(std::int64_t civ, std::int64_t year) const {
    require_year(year);
    if (!index_of(civ, civ_founded_.size())) {
        timeline_fatal("no civilization " + num(civ));
    }
    std::int64_t out = 0;
    for (const db::RelationRow& r : relations_) {
        if (r.kind == "faith_of" && r.b_id == civ && r.since <= year && (!r.until || year < *r.until)) {
            out = r.a_id;
        }
    }
    return out;
}

std::vector<std::int64_t> Timeline::cults_in_at(std::int64_t civ, std::int64_t year) const {
    require_year(year);
    if (!index_of(civ, civ_founded_.size())) {
        timeline_fatal("no civilization " + num(civ));
    }
    std::vector<std::int64_t> out;
    for (const db::RelationRow& r : relations_) {
        if (r.kind == "cult_in" && r.b_id == civ && r.since <= year && (!r.until || year < *r.until)) {
            out.push_back(r.a_id);
        }
    }
    std::stable_sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

std::int64_t Timeline::cult_status_at(std::int64_t faction, std::int64_t year) const {
    require_year(year);
    const std::optional<std::size_t> i = index_of(faction, factions_.size());
    if (!i) {
        timeline_fatal("no faction " + num(faction));
    }
    const db::FactionRow& f = factions_[*i];
    if (year < f.founded_year) {
        return 0;
    }
    if (f.ended_year && year >= *f.ended_year) {
        return 3;
    }
    return faction_hidden_[*i] >= 0 && year >= faction_hidden_[*i] ? 2 : 1;
}

std::int64_t Timeline::divine_effect_at(std::int64_t civ, std::int64_t year) const {
    require_year(year);
    const std::optional<std::size_t> c = index_of(civ, divine_effects_.size());
    if (!c) {
        timeline_fatal("no civilization " + num(civ));
    }
    std::int64_t bits = 0;
    for (const auto& [from, until, bit] : divine_effects_[*c]) {
        if (from <= year && year < until) {
            bits |= bit;
        }
    }
    return bits;
}

// --- Answers (6.2) -----------------------------------------------------------

void Timeline::require_year(std::int64_t year) const {
    if (!valid_year(year)) {
        timeline_fatal("year " + num(year) + " outside 0 to " + num(end_year_));
    }
}

std::size_t Timeline::region_position(std::int64_t region) const {
    const std::optional<std::size_t> p = position_of(region_ids_, region);
    if (!p) {
        timeline_fatal("no region " + num(region));
    }
    return *p;
}

RegionHolding Timeline::holder_at(std::size_t position, std::int64_t year) const {
    RegionHolding h;
    h.region = region_ids_[position];
    const auto first = control_.begin() + static_cast<std::ptrdiff_t>(control_offsets_[position]);
    const auto last = control_.begin() + static_cast<std::ptrdiff_t>(control_offsets_[position + 1U]);
    // The first row with from_year > Y; the row before it is the last with
    // from_year <= Y.
    const auto after = std::upper_bound(first, last, year, [](std::int64_t y, const db::RegionControlRow& r) {
        return y < r.from_year;
    });
    if (after == first) {
        return h;  // no row has started: free since the start (since 0)
    }
    const db::RegionControlRow& r = *(after - 1);
    if (!r.to_year || year < *r.to_year) {
        h.civ = r.civ;
        h.since = r.from_year;
        h.event = r.event;
    } else {
        h.since = *r.to_year;  // rows do not overlap: earlier rows end earlier
    }
    return h;
}

SettlementState Timeline::settlement_at(std::size_t index, std::int64_t year) const {
    SettlementState s;
    s.settlement = static_cast<std::int64_t>(index) + 1;
    if (settlement_founded_[index] > year) {
        return s;
    }
    const auto first = census_.begin() + static_cast<std::ptrdiff_t>(census_offsets_[index]);
    const auto last = census_.begin() + static_cast<std::ptrdiff_t>(census_offsets_[index + 1U]);
    const auto after = std::upper_bound(first, last, year, [](std::int64_t y, const db::SettlementCensusRow& r) {
        return y < r.year;
    });
    if (after == first) {
        return s;  // not reached on checked data: the first row is at founded_year
    }
    const db::SettlementCensusRow& r = *(after - 1);
    s.exists = true;
    s.owner = r.owner_civ;
    s.kind = r.kind;
    s.status = r.status;
    s.population = r.population;
    s.since = r.year;
    s.ending = (r.status == 3 || r.status == 4) ? settlement_ending_[index] : 0;  // P5-02 13
    return s;
}

std::span<const std::int64_t> Timeline::sites_existing(std::int64_t year) const {
    require_year(year);
    const auto after = std::upper_bound(site_origin_.begin(), site_origin_.end(), year);
    return std::span<const std::int64_t>(site_ids_.data(), static_cast<std::size_t>(after - site_origin_.begin()));
}

SiteState Timeline::site_at(std::int64_t site, std::int64_t year) const {
    require_year(year);
    const std::optional<std::size_t> i = index_of(site, sites_.size());
    if (!i) {
        timeline_fatal("no site " + num(site));
    }
    const db::SiteRow& x = sites_[*i];
    const bool open = x.origin_year <= year && (!x.ended_year || year < *x.ended_year);
    return SiteState{x.id,          x.kind,       x.region, x.x, x.y, x.settlement, x.monster, x.founder_civ, open,
                     open ? 0 : (x.ended_year && year >= *x.ended_year ? x.ending : 0)};
}

std::int64_t Timeline::mine_depth_at(std::int64_t site, std::int64_t year) const {
    require_year(year);
    const std::optional<std::size_t> i = index_of(site, sites_.size());
    if (!i) {
        timeline_fatal("no site " + num(site));
    }
    if (sites_[*i].kind != 3) {
        return 0;
    }
    std::int64_t depth = 0;
    for (const auto& [y, d] : site_depths_[*i]) {
        if (y <= year) {
            depth = std::max(depth, d);
        }
    }
    return depth;
}

std::int64_t Timeline::warren_depth_at(std::int64_t settlement, std::int64_t year) const {
    require_year(year);
    const std::optional<std::size_t> i = index_of(settlement, warren_depths_.size());
    if (!i) {
        timeline_fatal("no settlement " + num(settlement));
    }
    std::int64_t depth = 0;
    for (const auto& [y, d] : warren_depths_[*i]) {
        if (y <= year) {
            depth = std::max(depth, d);
        }
    }
    return depth;
}

std::int64_t Timeline::sealed_year(std::int64_t monster) const {
    const std::optional<std::size_t> i = index_of(monster, monsters_.size());
    if (!i) {
        timeline_fatal("no monster " + num(monster));
    }
    return monster_sealed_[*i];
}

MonsterState Timeline::monster_at(std::int64_t monster, std::int64_t year) const {
    require_year(year);
    const std::optional<std::size_t> i = index_of(monster, monsters_.size());
    if (!i) {
        timeline_fatal("no monster " + num(monster));
    }
    const db::MonsterRow& m = monsters_[*i];
    MonsterState out{m.id, m.kind, false, false, m.lair_site, 0};
    out.alive = m.born <= year && (!m.died || year < *m.died);
    const std::vector<std::pair<std::int64_t, bool>>& tr = monster_awake_[*i];
    const auto after = std::upper_bound(tr.begin(), tr.end(), year,
                                        [](std::int64_t y, const std::pair<std::int64_t, bool>& x) { return y < x.first; });
    out.awake = out.alive && after != tr.begin() && (after - 1)->second;
    // The lair at Y: its site with origin_year <= Y < ended_year (or open);
    // else its last lair begun by Y.
    for (const std::int64_t s : monster_lairs_[*i]) {
        const db::SiteRow& x = sites_[static_cast<std::size_t>(s - 1)];
        if (x.origin_year <= year) {
            out.lair_site = s;
            if (!x.ended_year || year < *x.ended_year) {
                break;
            }
        }
    }
    if (out.lair_site >= 1 && static_cast<std::size_t>(out.lair_site) <= sites_.size()) {
        out.region = sites_[static_cast<std::size_t>(out.lair_site - 1)].region;
    }
    return out;
}

std::int64_t Timeline::beast_count(std::int64_t region, std::int64_t beast, std::int64_t year) const {
    require_year(year);
    const std::size_t p = region_position(region);
    if (beast < 1 || static_cast<std::size_t>(beast) > beast_kinds_) {
        timeline_fatal("no beast " + num(beast));
    }
    const std::size_t slot = p * beast_kinds_ + static_cast<std::size_t>(beast - 1);
    const auto first = beast_rows_.begin() + static_cast<std::ptrdiff_t>(beast_offsets_[slot]);
    const auto last = beast_rows_.begin() + static_cast<std::ptrdiff_t>(beast_offsets_[slot + 1U]);
    const auto after = std::upper_bound(first, last, year, [](std::int64_t y, const db::BeastPopulationRow& r) {
        return y < r.year;
    });
    return after == first ? 0 : (after - 1)->count;
}

namespace {

// The id of the row with start_year <= Y <= end_year, or 0 (rows ascending
// start_year, disjoint).
template <class Row>
std::int64_t covering(const std::vector<Row>& rows, std::int64_t year) {
    const auto after = std::upper_bound(rows.begin(), rows.end(), year,
                                        [](std::int64_t y, const Row& r) { return y < r.start_year; });
    if (after == rows.begin()) {
        return 0;
    }
    const Row& r = *(after - 1);
    return year <= r.end_year ? r.id : 0;
}

}  // namespace

std::int64_t Timeline::phase_at(std::int64_t year) const {
    if (year == end_year_) {
        return 0;  // the present lies in no phase (section 4)
    }
    return covering(phases_, year);
}

std::int64_t Timeline::age_at(std::int64_t year) const {
    if (year == end_year_) {
        return ages_.empty() ? 0 : ages_.back().id;  // the present belongs to the last age
    }
    return covering(ages_, year);
}

void Timeline::events_at(std::int64_t year, std::vector<std::int64_t>& events,
                         std::vector<std::int64_t>& starting) const {
    events.clear();
    starting.clear();
    // The ids dated Y: a contiguous run, since year is non-decreasing with id.
    const auto lo = std::lower_bound(event_year_.begin(), event_year_.end(), year);
    const auto hi = std::upper_bound(lo, event_year_.end(), year);
    const auto first_id = static_cast<std::int64_t>(lo - event_year_.begin()) + 1;
    const auto last_id = static_cast<std::int64_t>(hi - event_year_.begin());  // inclusive
    for (std::int64_t id = first_id; id <= last_id; ++id) {
        starting.push_back(id);
    }
    // Merged, ascending, with the multi-year events begun before Y and not
    // yet ended.
    std::size_t s = 0;
    for (const std::int64_t id : multi_year_) {
        const auto i = static_cast<std::size_t>(id - 1);
        if (event_year_[i] >= year || event_end_[i] < year) {
            continue;
        }
        while (s < starting.size() && starting[s] < id) {
            events.push_back(starting[s]);
            ++s;
        }
        events.push_back(id);
    }
    while (s < starting.size()) {
        events.push_back(starting[s]);
        ++s;
    }
}

RegionHolding Timeline::region_holder(std::int64_t region, std::int64_t year) const {
    require_year(year);
    return holder_at(region_position(region), year);
}

std::string_view Timeline::region_name(std::int64_t region, std::int64_t year) const {
    require_year(year);
    const std::size_t p = region_position(region);
    const auto first = names_.begin() + static_cast<std::ptrdiff_t>(name_offsets_[p]);
    const auto last = names_.begin() + static_cast<std::ptrdiff_t>(name_offsets_[p + 1U]);
    const auto after = std::upper_bound(first, last, year, [](std::int64_t y, const db::RegionNameRow& r) {
        return y < r.from_year;
    });
    if (after == first) {
        return region_names_[p];
    }
    return (after - 1)->name;
}

SettlementState Timeline::settlement_state(std::int64_t settlement, std::int64_t year) const {
    require_year(year);
    const std::optional<std::size_t> i = index_of(settlement, settlement_founded_.size());
    if (!i) {
        timeline_fatal("no settlement " + num(settlement));
    }
    return settlement_at(*i, year);
}

RaceRelationState Timeline::race_relation_at(std::int64_t race_a, std::int64_t race_b, std::int64_t year) const {
    require_year(year);
    for (const std::int64_t r : {race_a, race_b}) {
        if (!std::binary_search(race_ids_.begin(), race_ids_.end(), r)) {
            timeline_fatal("no race " + num(r));
        }
    }
    const std::int64_t a = std::min(race_a, race_b);
    const std::int64_t b = std::max(race_a, race_b);
    RaceRelationState out{a, b, false, 0, 0};
    const auto m = std::lower_bound(met_years_.begin(), met_years_.end(), std::make_tuple(a, b, std::int64_t{0}));
    if (m == met_years_.end() || std::get<0>(*m) != a || std::get<1>(*m) != b || std::get<2>(*m) > year) {
        return out;
    }
    // The row of the largest year <= Y (binary search over the pair's rows).
    const auto after = std::upper_bound(race_relations_.begin(), race_relations_.end(), std::make_tuple(a, b, year),
                                        [](const std::tuple<std::int64_t, std::int64_t, std::int64_t>& k,
                                           const db::RaceRelationRow& r) {
                                            return k < std::make_tuple(r.race_a, r.race_b, r.year);
                                        });
    if (after == race_relations_.begin()) {
        return out;
    }
    const db::RaceRelationRow& r = *(after - 1);
    if (r.race_a != a || r.race_b != b) {
        return out;  // not reached on checked data: the meeting's row exists
    }
    out.met = true;
    out.disposition = r.disposition;
    out.band = r.band;
    return out;
}

std::vector<RaceShare> Timeline::settlement_races(std::int64_t settlement, std::int64_t year) const {
    require_year(year);
    const std::optional<std::size_t> i = index_of(settlement, settlement_founded_.size());
    if (!i) {
        timeline_fatal("no settlement " + num(settlement));
    }
    std::vector<RaceShare> out;
    const SettlementState s = settlement_at(*i, year);
    if (!s.exists || (s.status != 1 && s.status != 2)) {
        return out;
    }
    std::size_t row = share_offsets_[*i];
    const std::size_t last = share_offsets_[*i + 1U];
    while (row < last) {
        const std::int64_t race = settlement_races_[row].race;
        const db::SettlementRaceRow* in_force = nullptr;
        while (row < last && settlement_races_[row].race == race) {
            if (settlement_races_[row].year <= year) {
                in_force = &settlement_races_[row];
            }
            ++row;
        }
        if (in_force != nullptr && in_force->share > 0) {
            out.push_back(RaceShare{race, in_force->share, in_force->standing});
        }
    }
    return out;
}

void Timeline::year_state(std::int64_t year, YearState& out) const {
    require_year(year);
    out.year = year;
    out.phase = phase_at(year);
    out.age = age_at(year);
    out.regions.resize(region_ids_.size());
    for (std::size_t p = 0; p < region_ids_.size(); ++p) {
        out.regions[p] = holder_at(p, year);
    }
    out.settlements.resize(settlement_founded_.size());
    for (std::size_t s = 0; s < settlement_founded_.size(); ++s) {
        out.settlements[s] = settlement_at(s, year);
    }
    out.relations.clear();
    for (std::size_t i = 0; i < relations_.size(); ++i) {
        const db::RelationRow& r = relations_[i];
        if (r.since <= year && (!r.until || year < *r.until)) {
            out.relations.push_back(i);
        }
    }
    out.civilizations.clear();
    for (std::size_t c = 0; c < civ_founded_.size(); ++c) {
        if (civ_founded_[c] <= year && (!civ_ended_[c] || year < *civ_ended_[c])) {
            out.civilizations.push_back(static_cast<std::int64_t>(c) + 1);
        }
    }
    events_at(year, out.events, out.events_starting);
}

std::vector<AllianceState> Timeline::alliances_at(std::int64_t year) const {
    require_year(year);
    std::vector<AllianceState> out;
    for (const db::RelationRow& r : relations_) {
        if (r.kind == "alliance" && r.since <= year && (!r.until || year < *r.until)) {
            out.push_back(AllianceState{r.a_id, r.b_id, r.since, r.since_event});
        }
    }
    std::stable_sort(out.begin(), out.end(), [](const AllianceState& x, const AllianceState& y) {
        return x.a != y.a ? x.a < y.a : x.b < y.b;
    });
    return out;
}

std::vector<std::int64_t> Timeline::allies_of(std::int64_t civ, std::int64_t year) const {
    if (civ < 1 || static_cast<std::size_t>(civ) > civ_founded_.size()) {
        throw FatalError("timeline: no civilization " + db::format_i64(civ));
    }
    std::vector<std::int64_t> out;
    for (const AllianceState& a : alliances_at(year)) {
        if (a.a == civ) {
            out.push_back(a.b);
        } else if (a.b == civ) {
            out.push_back(a.a);
        }
    }
    std::stable_sort(out.begin(), out.end());
    return out;
}

std::vector<std::vector<std::int64_t>> Timeline::alliance_groups_at(std::int64_t year) const {
    const std::vector<AllianceState> rows = alliances_at(year);
    const auto alive = [&](std::int64_t c) {
        const auto i = static_cast<std::size_t>(c - 1);
        return c >= 1 && i < civ_founded_.size() && civ_founded_[i] <= year && (!civ_ended_[i] || year < *civ_ended_[i]);
    };
    // Union-find over the civilization ids (the smaller id is the root).
    std::map<std::int64_t, std::int64_t> parent;
    const auto find = [&parent](std::int64_t x) {
        while (parent.at(x) != x) {
            x = parent.at(x);
        }
        return x;
    };
    for (const AllianceState& a : rows) {
        if (!alive(a.a) || !alive(a.b)) {
            continue;
        }
        parent.try_emplace(a.a, a.a);
        parent.try_emplace(a.b, a.b);
        const std::int64_t ra = find(a.a);
        const std::int64_t rb = find(a.b);
        if (ra != rb) {
            parent[std::max(ra, rb)] = std::min(ra, rb);
        }
    }
    std::map<std::int64_t, std::vector<std::int64_t>> groups;  // by root, the smallest id
    for (const auto& [c, p] : parent) {
        (void)p;
        groups[find(c)].push_back(c);  // ascending, since the map is
    }
    std::vector<std::vector<std::int64_t>> out;
    for (auto& [root, list] : groups) {
        (void)root;
        out.push_back(std::move(list));
    }
    return out;
}

std::vector<std::pair<std::int64_t, std::int64_t>> Timeline::trades_at(std::int64_t year) const {
    require_year(year);
    std::vector<std::pair<std::int64_t, std::int64_t>> out;
    for (const db::RelationRow& r : relations_) {
        if (r.kind == "trade" && r.since <= year && (!r.until || year < *r.until)) {
            out.emplace_back(r.a_id, r.b_id);
        }
    }
    std::stable_sort(out.begin(), out.end());
    return out;
}

YearState Timeline::year_state(std::int64_t year) const {
    YearState out;
    year_state(year, out);
    return out;
}

std::optional<std::int64_t> Timeline::previous_event_year(std::int64_t year) const {
    require_year(year);
    const auto lo = std::lower_bound(event_year_.begin(), event_year_.end(), year);
    if (lo == event_year_.begin()) {
        return std::nullopt;
    }
    return *(lo - 1);
}

std::optional<std::int64_t> Timeline::next_event_year(std::int64_t year) const {
    require_year(year);
    const auto hi = std::upper_bound(event_year_.begin(), event_year_.end(), year);
    if (hi == event_year_.end()) {
        return std::nullopt;
    }
    return *hi;
}

std::span<const std::int64_t> Timeline::events_of_civilization(std::int64_t civ) const {
    const std::optional<std::size_t> i = index_of(civ, civ_events_.size());
    if (!i) {
        timeline_fatal("no civilization " + num(civ));
    }
    return civ_events_[*i];
}

std::span<const std::int64_t> Timeline::events_of_region(std::int64_t region) const {
    return region_events_[region_position(region)];
}

std::span<const std::int64_t> Timeline::events_of_settlement(std::int64_t settlement) const {
    const std::optional<std::size_t> i = index_of(settlement, settlement_events_.size());
    if (!i) {
        timeline_fatal("no settlement " + num(settlement));
    }
    return settlement_events_[*i];
}

std::span<const std::int64_t> Timeline::effects(std::int64_t event) const {
    const std::optional<std::size_t> i = index_of(event, effects_.size());
    if (!i) {
        timeline_fatal("no event " + num(event));
    }
    return effects_[*i];
}

// --- Figures (P5-06 12) --------------------------------------------------------

std::int64_t Timeline::ruler_at(std::int64_t civ, std::int64_t year) const {
    require_year(year);
    const std::optional<std::size_t> c = index_of(civ, civ_rulers_.size());
    if (!c) {
        timeline_fatal("no civilization " + num(civ));
    }
    std::int64_t out = 0;
    for (const Term& term : civ_rulers_[*c]) {  // ascending from: the latest reign begun wins
        if (term.from <= year && year < term.to) {
            out = term.figure;
        }
    }
    return out;
}

std::vector<std::int64_t> Timeline::figures_alive(std::int64_t year) const {
    require_year(year);
    std::vector<std::int64_t> out;
    for (const db::FigureRow& f : figures_) {
        if (f.born <= year && year < f.died.value_or(end_year_ + 1)) {
            out.push_back(f.id);
        }
    }
    return out;
}

std::vector<std::int64_t> Timeline::generals_of(std::int64_t civ, std::int64_t year) const {
    require_year(year);
    const std::optional<std::size_t> c = index_of(civ, civ_generals_.size());
    if (!c) {
        timeline_fatal("no civilization " + num(civ));
    }
    std::vector<std::int64_t> out;
    for (const Term& term : civ_generals_[*c]) {
        if (term.from <= year && year < term.to) {
            out.push_back(term.figure);
        }
    }
    std::stable_sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

FigureState Timeline::figure_at(std::int64_t figure, std::int64_t year) const {
    require_year(year);
    const std::optional<std::size_t> i = index_of(figure, figures_.size());
    if (!i) {
        timeline_fatal("no figure " + num(figure));
    }
    const db::FigureRow& f = figures_[*i];
    const bool alive = f.born <= year && year < f.died.value_or(end_year_ + 1);
    std::int64_t status = f.status;
    if (year < f.born) {
        status = 0;
    } else if (alive) {
        status = figure_exiled_[*i] >= 0 && year >= figure_exiled_[*i] ? 2 : 1;
    }
    return FigureState{f.id, f.role, f.civ, status, alive};
}

std::span<const std::int64_t> Timeline::events_of_figure(std::int64_t figure) const {
    const std::optional<std::size_t> i = index_of(figure, figure_events_.size());
    if (!i) {
        timeline_fatal("no figure " + num(figure));
    }
    return figure_events_[*i];
}

// --- load_history (5.2) ------------------------------------------------------

db::Result<LoadedHistory> load_history(const db::WorldReader& world) {
    try {
        db::Result<HistoryTables> tables = read_history_tables(world);
        if (!tables.ok()) {
            return tables.error();
        }
        db::Result<HistoryView> view = HistoryView::load(world);
        if (!view.ok()) {
            return view.error();
        }
        if (const std::optional<std::string> why = history_reads_disagree(view.value(), tables.value())) {
            return db::Error{db::ErrorCode::corrupt_layer, *why};
        }
        if (const std::optional<std::string> why = timeline_reads_disagree(view.value(), tables.value())) {
            return db::Error{db::ErrorCode::corrupt_layer, *why};
        }
        Timeline timeline = Timeline::build(view.value(), tables.value());
        return LoadedHistory{std::move(tables.value()), std::move(view.value()), std::move(timeline)};
    } catch (const std::bad_alloc&) {
        return db::Error{db::ErrorCode::out_of_memory, "out of memory while building the timeline"};
    }
}

}  // namespace worldgen::history
