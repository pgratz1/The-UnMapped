// The note of a war declaration or a flare-up (spec P3-07 r8 21.4): text
// built from the stored log only, names written as render_event writes them.

#include "worldgen/history/digest.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <utility>
#include <string_view>
#include <vector>

#include "worldgen/core/wide.hpp"
#include "worldgen/db/schema.hpp"

namespace worldgen::history {

namespace {

std::string int_text(std::int64_t v) {
    return db::format_i64(v);
}

// render_event's rule (P3-04 8.5 r10): a leading "The " is written "the "
// in the middle of a sentence.
void append_name(std::string& out, const std::string& name, bool sentence_start) {
    constexpr std::string_view THE = "The ";
    if (!sentence_start && name.size() >= THE.size() && name.compare(0, THE.size(), THE) == 0) {
        out += "the ";
        out.append(name, THE.size(), std::string::npos);
    } else {
        out += name;
    }
}

// The entity of the first participant of `role` (a role key of the event's
// kind), or 0.
std::int64_t participant(const HistoryView::Event& e, const KindDef& kind, std::string_view role) {
    const RoleDef* r = kind.find_role(role);
    if (r == nullptr) {
        return 0;
    }
    for (const db::EventParticipantRow& p : e.participants) {
        if (p.role == r->seq) {
            return p.entity_id;
        }
    }
    return 0;
}

bool is_kind(const HistoryView::Event& e, const KindDef* kind) noexcept {
    return kind != nullptr && e.row.kind == kind->id;
}

// The claim kinds of P3-07 r14 23: kind 20 claim and kind 39
// claim_settlements, found by key in the stored catalogue.
struct ClaimKinds {
    const KindDef* claim = nullptr;
    const KindDef* settlements = nullptr;
    [[nodiscard]] bool holds(const HistoryView::Event& e) const noexcept {
        return is_kind(e, claim) || is_kind(e, settlements);
    }
};

// The name of an entity in mid-sentence, appended; false when it has none.
bool append_entity(std::string& out, const HistoryView& view, EntityType type, std::int64_t id) {
    const std::string* n = view.name(type, id);
    if (n == nullptr) {
        return false;
    }
    append_name(out, *n, false);
    return true;
}

// The settlement participants of claim c (role `settlement` of its kind),
// in participant order, joined as {role:all} joins names, every name in
// mid-sentence; false when there is none or one has no name.
bool append_settlements(std::string& out, const HistoryView& view, const HistoryView::Event& c, const KindDef& kind) {
    const RoleDef* role = kind.find_role("settlement");
    std::vector<std::int64_t> settlements;
    for (const db::EventParticipantRow& p : c.participants) {
        if (role != nullptr && p.role == role->seq) {
            settlements.push_back(p.entity_id);
        }
    }
    if (settlements.empty()) {
        return false;
    }
    for (std::size_t i = 0; i < settlements.size(); ++i) {
        if (i > 0) {
            out += i + 1 == settlements.size() ? " and " : ", ";
        }
        if (!append_entity(out, view, EntityType::settlement, settlements[i])) {
            return false;
        }
    }
    return true;
}

std::string war_note(const HistoryView& view, const HistoryView::Event& e, const KindDef& war,
                     const ClaimKinds& claims) {
    // P3-07 r14 23 item 3: the cause of kind claim or claim_settlements with
    // the smallest id.
    const HistoryView::Event* c = nullptr;
    for (const std::int64_t id : e.causes) {
        const HistoryView::Event* k = view.event(id);
        if (k != nullptr && claims.holds(*k) && (c == nullptr || k->row.id < c->row.id)) {
            c = k;
        }
    }
    if (c == nullptr) {
        return {};
    }
    const bool same_year = c->row.year >= e.row.year;
    std::string out;
    if (is_kind(*c, claims.settlements)) {
        // P3-07 r15 23 item 4a: a claim of kind 39 names its towns, whether
        // or not its region is the war's.
        out = same_year ? "The claim, made the same year, was to "
                        : "The claim, since " + int_text(c->row.year) + ", was to ";
        if (!append_settlements(out, view, *c, *claims.settlements)) {
            return {};
        }
        out += " in ";
        if (!append_entity(out, view, EntityType::region, c->row.region)) {
            return {};
        }
        out += ".";
    } else if (c->row.region == e.row.region) {
        out = same_year ? "Claimed the same year." : "Claimed since " + int_text(c->row.year) + ".";
    } else {
        // P3-07 r14 23 item 3 (P3-06 r19 23.4): the war's goal is not the
        // claim's region.
        out = same_year ? "The claim, made the same year, was to " : "The claim, since " + int_text(c->row.year) + ", was to ";
        if (!append_entity(out, view, EntityType::region, c->row.region)) {
            return {};
        }
        out += ".";
    }
    const std::int64_t a = participant(e, war, "attacker");
    const std::int64_t b = participant(e, war, "defender");
    std::int64_t n = 1;
    for (const HistoryView::Event& d : view.events()) {
        if (d.row.id >= e.row.id) {
            break;
        }
        if (d.row.kind != war.id) {
            continue;
        }
        const std::int64_t da = participant(d, war, "attacker");
        const std::int64_t db_ = participant(d, war, "defender");
        n += ((da == a && db_ == b) || (da == b && db_ == a)) ? 1 : 0;
    }
    if (n >= 2) {
        out += " The " + history_ordinal(n) + " war between them.";
    }
    return out;
}

// P3-07 r14 23 item 4: a flare-up caused by a claim_settlements.
std::string flare_settlements_note(const HistoryView& view, const HistoryView::Event& c, const KindDef& kind) {
    std::string out = "The claim of ";
    if (!append_entity(out, view, EntityType::civilization, participant(c, kind, "claimant"))) {
        return {};
    }
    out += " to ";
    if (!append_settlements(out, view, c, kind)) {
        return {};
    }
    out += " of ";
    if (!append_entity(out, view, EntityType::civilization, participant(c, kind, "holder"))) {
        return {};
    }
    out += " in ";
    if (!append_entity(out, view, EntityType::region, c.row.region)) {
        return {};
    }
    out += " came to a head.";
    return out;
}

std::string flare_note(const HistoryView& view, const HistoryView::Event& e, const ClaimKinds& claims) {
    if (e.causes.empty()) {
        return {};
    }
    const HistoryView::Event* c = view.event(e.causes.front());
    if (c == nullptr) {
        return {};
    }
    if (is_kind(*c, claims.settlements)) {
        return flare_settlements_note(view, *c, *claims.settlements);
    }
    if (!is_kind(*c, claims.claim)) {
        return {};
    }
    const KindDef* claim = claims.claim;
    const std::string* a = view.name(EntityType::civilization, participant(*c, *claim, "claimant"));
    const std::string* b = view.name(EntityType::civilization, participant(*c, *claim, "holder"));
    const std::string* r = view.name(EntityType::region, c->row.region);
    if (a == nullptr || b == nullptr || r == nullptr) {
        return {};
    }
    // P3-07 r12 22.3: no pronoun, every name mid-sentence.
    std::string out = "The claim of ";
    append_name(out, *a, false);
    out += " to ";
    append_name(out, *r, false);
    out += ", held by ";
    append_name(out, *b, false);
    out += ", came to a head.";
    return out;
}

std::int64_t floor_div(std::int64_t a, std::int64_t b) noexcept {
    const std::int64_t q = a / b;
    return (a % b != 0 && (a < 0) != (b < 0)) ? q - 1 : q;
}

// P3-07 r12 22.2: the generations note of a quiet_stretch in the milestones
// of civilization `civ`, which must be one of its realms.
std::string generations_note(const HistoryView& view, const HistoryView::Event& e, const KindDef& kind,
                             std::int64_t civ) {
    const RoleDef* realm = kind.find_role("realm");
    if (realm == nullptr) {
        return {};
    }
    bool participant_of = false;
    for (const db::EventParticipantRow& p : e.participants) {
        participant_of = participant_of || (p.role == realm->seq && p.entity_id == civ);
    }
    const std::optional<HistoryView::CivilizationInfo> info = view.civilization_info(civ);
    const std::string* name = view.name(EntityType::civilization, civ);
    if (!participant_of || !info || name == nullptr) {
        return {};
    }
    const std::int64_t gy = view.race_generation_years(info->race);
    // Years are bounded by the checks of P3-04 11.1 and P3-07 5.3; a
    // non-positive generation length has no count.
    if (gy <= 0 || e.row.end_year < e.row.year || e.row.end_year >= (std::int64_t{1} << 40) ||
        e.row.year < -(std::int64_t{1} << 40) || info->founded_year < -(std::int64_t{1} << 40) ||
        info->founded_year > (std::int64_t{1} << 40)) {
        return {};
    }
    const auto g = [&](std::int64_t t) { return floor_div(t - info->founded_year, gy); };
    const std::int64_t n = g(e.row.end_year + 1) - g(e.row.year);
    std::string out = *name;  // at a sentence start: the stored name
    if (n == 0) {
        out += ": no new generation of rulers.";
    } else if (n == 1) {
        out += ": one new generation of rulers.";
    } else {
        out += ": " + int_text(n) + " new generations of rulers.";
    }
    return out;
}

}  // namespace

std::string history_ordinal(std::int64_t n) {
    static constexpr std::array<std::string_view, 9> WORDS = {"second", "third",   "fourth", "fifth", "sixth",
                                                              "seventh", "eighth", "ninth",  "tenth"};
    if (n >= 2 && n <= 10) {
        return std::string(WORDS[static_cast<std::size_t>(n - 2)]);
    }
    const std::int64_t h = n % 100;
    std::string_view suffix = "th";
    if (h < 11 || h > 13) {
        switch (n % 10) {
            case 1: suffix = "st"; break;
            case 2: suffix = "nd"; break;
            case 3: suffix = "rd"; break;
            default: break;
        }
    }
    return int_text(n) + std::string(suffix);
}

std::string history_note(const HistoryView& view, std::int64_t event_id, std::int64_t civilization) {
    const HistoryView::Event* e = view.event(event_id);
    if (e == nullptr) {
        return {};
    }
    const EventCatalogue& catalogue = view.catalogue();
    const ClaimKinds claims{catalogue.find("claim"), catalogue.find("claim_settlements")};
    const KindDef* war = catalogue.find("war_declared");
    if (war != nullptr && e->row.kind == war->id) {
        return war_note(view, *e, *war, claims);
    }
    const KindDef* flare = catalogue.find("flare_up");
    if (flare != nullptr && e->row.kind == flare->id) {
        return flare_note(view, *e, claims);
    }
    const KindDef* quiet = catalogue.find("quiet_stretch");
    if (civilization != 0 && quiet != nullptr && e->row.kind == quiet->id) {
        return generations_note(view, *e, *quiet, civilization);
    }
    return {};
}

}  // namespace worldgen::history

// --- The digest (P3-07 section 7, 9; 21 to 23) ------------------------------

namespace worldgen::history {

namespace {

using Id = std::int64_t;

bool standing(std::int64_t status) noexcept {
    return status == 1 || status == 2;
}

// The kind ids the digest uses, by key in the stored catalogue (0 when
// absent: a world of another generator, or a fixture).
struct Kinds {
    Id war_declared = 0, peace = 0, war_won = 0, truce = 0, region_ceded = 0, settlements_ceded = 0, tribute = 0;
    Id sacked = 0, settlement_captured = 0, capital_moved = 0, refugees_settled = 0, civ_conquered = 0;
    Id civ_ended = 0, search_abandoned = 0, battle = 0, battle_at_pass = 0, siege = 0, colony_founded = 0;
    Id civ_seceded = 0, refuge_held = 0;

    explicit Kinds(const EventCatalogue& c) {
        const auto id = [&c](std::string_view key) -> Id {
            const KindDef* k = c.find(key);
            return k != nullptr ? k->id : -1;  // -1 never matches a stored kind
        };
        war_declared = id("war_declared");
        peace = id("peace");
        war_won = id("war_won");
        truce = id("truce");
        region_ceded = id("region_ceded");
        settlements_ceded = id("settlements_ceded");
        tribute = id("tribute");
        sacked = id("sacked");
        settlement_captured = id("settlement_captured");
        capital_moved = id("capital_moved");
        refugees_settled = id("refugees_settled");
        civ_conquered = id("civ_conquered");
        civ_ended = id("civ_ended");
        search_abandoned = id("search_abandoned");
        battle = id("battle");
        battle_at_pass = id("battle_at_pass");
        siege = id("siege");
        colony_founded = id("colony_founded");
        civ_seceded = id("civ_seceded");
        refuge_held = id("refuge_held");
    }
    // P3-07 r14 23 item 1: cessions are kinds 29 and 40 together.
    [[nodiscard]] bool cession(Id k) const noexcept { return k == region_ceded || k == settlements_ceded; }
    [[nodiscard]] bool cession_or_tribute(Id k) const noexcept { return cession(k) || k == tribute; }
};

constexpr std::size_t NONE = static_cast<std::size_t>(-1);

class Digester {
public:
    Digester(const HistoryView& view, const HistoryTables& t)
        : view_(view), t_(t), e_(t.end_year), events_(view.events()), kinds_(view.catalogue()) {}

    HistoryDigest run() {
        index();
        civilizations();
        summary();
        stats();
        return std::move(d_);
    }

private:
    // --- Indexes --------------------------------------------------------

    [[nodiscard]] Id kind(Id event) const { return events_[static_cast<std::size_t>(event - 1)].row.kind; }
    [[nodiscard]] std::size_t civ_index(Id c) const { return static_cast<std::size_t>(c - 1); }
    [[nodiscard]] bool is_civ(Id c) const { return c >= 1 && static_cast<std::size_t>(c) <= t_.civilizations.size(); }

    // The race row index of a civilization, or NONE.
    [[nodiscard]] std::size_t race_of(Id civ) const {
        if (!is_civ(civ)) {
            return NONE;
        }
        const Id race = t_.civilizations[civ_index(civ)].race;
        for (std::size_t k = 0; k < t_.races.size(); ++k) {
            if (t_.races[k].id == race) {
                return k;
            }
        }
        return NONE;
    }

    [[nodiscard]] const RegionInfo* region(Id id) const {
        const auto it = std::lower_bound(t_.regions.begin(), t_.regions.end(), id,
                                         [](const RegionInfo& r, Id v) { return r.id < v; });
        return it != t_.regions.end() && it->id == id ? &*it : nullptr;
    }

    [[nodiscard]] static bool islet(const RegionInfo& r) noexcept {
        return r.cells - r.lake_cells < ISLET_MAX_DRY_CELLS;
    }

    void index() {
        const std::size_t n = events_.size();
        effects_.assign(n + 1, {});
        civ_events_.assign(t_.civilizations.size(), {});
        for (const HistoryView::Event& e : events_) {
            for (const Id c : e.causes) {
                effects_[static_cast<std::size_t>(c)].push_back(e.row.id);
            }
            std::vector<Id> civs;
            for (const db::EventParticipantRow& p : e.participants) {
                if (p.entity_type == static_cast<std::int64_t>(EntityType::civilization) && is_civ(p.entity_id) &&
                    std::find(civs.begin(), civs.end(), p.entity_id) == civs.end()) {
                    civs.push_back(p.entity_id);
                }
            }
            for (const Id c : civs) {
                civ_events_[civ_index(c)].push_back(e.row.id);
            }
        }
        war_row_.assign(n + 1, NONE);
        for (std::size_t i = 0; i < t_.relations.size(); ++i) {
            const db::RelationRow& r = t_.relations[i];
            if (r.kind == "war" && r.since_event >= 1 && static_cast<std::size_t>(r.since_event) <= n &&
                war_row_[static_cast<std::size_t>(r.since_event)] == NONE) {
                war_row_[static_cast<std::size_t>(r.since_event)] = i;
            }
        }
        control_of_.assign(t_.civilizations.size(), {});
        for (std::size_t i = 0; i < t_.region_control.size(); ++i) {
            const Id c = t_.region_control[i].civ;
            if (is_civ(c)) {
                control_of_[civ_index(c)].push_back(i);
            }
        }
        for (const RegionInfo& r : t_.regions) {
            nonislet_ += islet(r) ? 0 : 1;
        }
    }

    // The ending of the war declared by event d (its war row's until_event),
    // or 0 when it has no closed row.
    [[nodiscard]] Id war_ending(Id d) const {
        const std::size_t r = war_row_[static_cast<std::size_t>(d)];
        if (r == NONE || !t_.relations[r].until) {
            return 0;
        }
        return t_.relations[r].until_event;
    }

    // --- Civilizations (6.6, 7.1 to 7.3) --------------------------------

    void civilizations() {
        d_.civilizations.resize(t_.civilizations.size());
        for (std::size_t k = 0; k < t_.civilizations.size(); ++k) {
            CivilizationDigest& c = d_.civilizations[k];
            c.id = t_.civilizations[k].id;
            c.events = static_cast<std::int64_t>(civ_events_[k].size());
            milestones(c, civ_events_[k]);
        }
        for (const db::SettlementRow& s : t_.settlements) {
            if (is_civ(s.founder_civ)) {
                CivilizationDigest& c = d_.civilizations[civ_index(s.founder_civ)];
                ++c.settlements_founded;
                c.settlements_ruined += (s.status == 3 || s.status == 4) ? 1 : 0;
            }
            if (standing(s.status) && is_civ(s.owner_civ)) {
                CivilizationDigest& c = d_.civilizations[civ_index(s.owner_civ)];
                ++c.settlements_held;
                c.population += s.population;
            }
        }
        for (const db::RegionControlRow& r : t_.region_control) {
            if (!r.to_year && is_civ(r.civ)) {
                ++d_.civilizations[civ_index(r.civ)].regions_held;
            }
        }
        relations();
        regions_held_by_year();
        population_peaks();
    }

    void milestones(CivilizationDigest& c, std::vector<Id> ids) const {
        const auto by_salience = [this](Id a, Id b) {
            const std::int64_t sa = events_[static_cast<std::size_t>(a - 1)].row.salience;
            const std::int64_t sb = events_[static_cast<std::size_t>(b - 1)].row.salience;
            return sa != sb ? sa > sb : a < b;
        };
        std::stable_sort(ids.begin(), ids.end(), by_salience);
        ids.resize(std::min(MILESTONES, ids.size()));
        std::stable_sort(ids.begin(), ids.end());
        c.milestones = std::move(ids);
    }

    void relations() {
        for (std::size_t i = 0; i < t_.relations.size(); ++i) {
            const db::RelationRow& r = t_.relations[i];
            const bool a = r.a_type == 3 && is_civ(r.a_id);
            const bool b = r.b_type == 3 && is_civ(r.b_id) && !(r.a_type == 3 && r.a_id == r.b_id);
            if (a) {
                d_.civilizations[civ_index(r.a_id)].relations.push_back(i);
            }
            if (b) {
                d_.civilizations[civ_index(r.b_id)].relations.push_back(i);
            }
        }
        const auto order = [this](std::size_t x, std::size_t y) {
            const db::RelationRow& a = t_.relations[x];
            const db::RelationRow& b = t_.relations[y];
            if (a.since != b.since) return a.since < b.since;
            if (a.a_type != b.a_type) return a.a_type < b.a_type;
            if (a.a_id != b.a_id) return a.a_id < b.a_id;
            if (a.b_type != b.b_type) return a.b_type < b.b_type;
            if (a.b_id != b.b_id) return a.b_id < b.b_id;
            return a.kind < b.kind;  // bytes (char_traits<char> compares as unsigned char)
        };
        for (CivilizationDigest& c : d_.civilizations) {
            std::stable_sort(c.relations.begin(), c.relations.end(), order);
        }
    }

    // 7.1 and 7.5: held(c, Y) by a difference array per civilization.
    void regions_held_by_year() {
        const std::size_t span = static_cast<std::size_t>(std::max<std::int64_t>(e_, 0));
        std::vector<std::int64_t> all(span + 1, 0);
        std::vector<std::int64_t> dry(span + 1, 0);
        share_runs_.fill(0);
        for (std::size_t k = 0; k < t_.civilizations.size(); ++k) {
            if (control_of_[k].empty()) {
                continue;
            }
            std::fill(all.begin(), all.end(), 0);
            std::fill(dry.begin(), dry.end(), 0);
            for (const std::size_t i : control_of_[k]) {
                const db::RegionControlRow& r = t_.region_control[i];
                const auto from = static_cast<std::size_t>(r.from_year);
                const auto to = static_cast<std::size_t>(r.to_year.value_or(e_));
                ++all[from];
                --all[to];
                const RegionInfo* g = region(r.region);
                if (g != nullptr && !islet(*g)) {
                    ++dry[from];
                    --dry[to];
                }
            }
            RegionPeak& peak = d_.civilizations[k].peak_regions;
            std::int64_t held = 0;
            std::int64_t h = 0;
            bool in_first_run = false;
            std::array<std::int64_t, 5> run{};
            for (std::size_t y = 0; y < span; ++y) {
                held += all[y];
                h += dry[y];
                const auto year = static_cast<std::int64_t>(y);
                if (held > peak.count) {
                    peak = RegionPeak{held, year, year};
                    in_first_run = true;
                } else if (held == peak.count && in_first_run && peak.count > 0) {
                    peak.to_year = year;
                } else {
                    in_first_run = false;
                }
                if (nonislet_ > 0) {
                    const std::int64_t share = 1000 * h / nonislet_;
                    const Id civ = t_.civilizations[k].id;
                    if (share > share_max_ || (share == share_max_ && share > 0 && year < share_max_year_)) {
                        share_max_ = share;
                        share_max_year_ = year;
                        share_max_civ_ = civ;
                    }
                    for (std::size_t l = 0; l < run.size(); ++l) {
                        run[l] = share > SHARE_LEVELS[l] ? run[l] + 1 : 0;
                        share_runs_[l] = std::max(share_runs_[l], run[l]);
                    }
                }
            }
        }
    }

    // 7.2: census rows by (year, settlement), the state of each settlement
    // and a running population per civilization.
    void population_peaks() {
        std::vector<std::size_t> order(t_.census.size());
        for (std::size_t i = 0; i < order.size(); ++i) {
            order[i] = i;
        }
        std::stable_sort(order.begin(), order.end(), [this](std::size_t a, std::size_t b) {
            return t_.census[a].year < t_.census[b].year;  // settlement order kept within a year
        });
        struct State {
            Id owner = 0;
            std::int64_t population = 0;
            bool counted = false;
        };
        std::vector<State> state(t_.settlements.size());
        std::vector<std::int64_t> pop(t_.civilizations.size(), 0);
        std::vector<std::size_t> changed;
        std::vector<std::uint8_t> marked(t_.civilizations.size(), 0);
        const auto touch = [&](Id civ) {
            if (marked[civ_index(civ)] == 0) {
                marked[civ_index(civ)] = 1;
                changed.push_back(civ_index(civ));
            }
        };
        std::size_t i = 0;
        while (i < order.size()) {
            const std::int64_t year = t_.census[order[i]].year;
            for (; i < order.size() && t_.census[order[i]].year == year; ++i) {
                const db::SettlementCensusRow& r = t_.census[order[i]];
                State& s = state[static_cast<std::size_t>(r.settlement - 1)];
                if (s.counted) {
                    pop[civ_index(s.owner)] -= s.population;
                    touch(s.owner);
                }
                s.owner = r.owner_civ;
                s.population = r.population;
                s.counted = standing(r.status) && is_civ(r.owner_civ);
                if (s.counted) {
                    pop[civ_index(s.owner)] += s.population;
                    touch(s.owner);
                }
            }
            for (const std::size_t k : changed) {
                PopulationPeak& p = d_.civilizations[k].peak_population;
                if (pop[k] > p.population) {
                    p = PopulationPeak{pop[k], year};
                }
                marked[k] = 0;
            }
            changed.clear();
        }
    }

    // --- The summary set (21.3 with 21.8, 22.5a, 23) --------------------

    // U(e) of 21.2.
    [[nodiscard]] std::vector<Id> unit(Id e) const {
        std::vector<Id> out{e};
        const Id k = kind(e);
        const auto add_effects = [&](Id of, auto pick) {
            for (const Id x : effects_[static_cast<std::size_t>(of)]) {
                if (pick(kind(x))) {
                    out.push_back(x);
                }
            }
        };
        const auto ceded = [this](Id x) { return kinds_.cession_or_tribute(x); };
        if (k == kinds_.war_declared) {
            if (const Id u = war_ending(e); u != 0) {
                out.push_back(u);
                add_effects(u, ceded);
            }
        } else if (k == kinds_.peace || k == kinds_.war_won || k == kinds_.truce) {
            add_effects(e, ceded);
        } else if (k == kinds_.sacked || k == kinds_.settlement_captured) {
            add_effects(e, [this](Id x) {
                return x == kinds_.capital_moved || x == kinds_.refugees_settled || x == kinds_.civ_conquered;
            });
        }
        return out;
    }

    // 22.5a: a declaration whose war ended in a search below the minimum
    // salience is no candidate.
    [[nodiscard]] bool candidate(const HistoryView::Event& e) const {
        if (e.row.salience < SUMMARY_MIN_SALIENCE) {
            return false;
        }
        if (e.row.kind == kinds_.war_declared) {
            const Id u = war_ending(e.row.id);
            if (u != 0 && kind(u) == kinds_.search_abandoned &&
                events_[static_cast<std::size_t>(u - 1)].row.salience < SUMMARY_MIN_SALIENCE) {
                return false;
            }
        }
        return true;
    }

    // The age index of a year, or NONE (ages tile [0, E - 1]).
    [[nodiscard]] std::size_t age_of(std::int64_t year) const {
        for (std::size_t k = 0; k < t_.ages.size(); ++k) {
            if (t_.ages[k].start_year <= year && year <= t_.ages[k].end_year) {
                return k;
            }
        }
        return NONE;
    }

    void summary() {
        const std::size_t n = events_.size();
        std::vector<Id> ranked;
        for (const HistoryView::Event& e : events_) {
            if (candidate(e)) {
                ranked.push_back(e.row.id);
            }
        }
        std::stable_sort(ranked.begin(), ranked.end(), [this](Id a, Id b) {
            const std::int64_t sa = events_[static_cast<std::size_t>(a - 1)].row.salience;
            const std::int64_t sb = events_[static_cast<std::size_t>(b - 1)].row.salience;
            return sa != sb ? sa > sb : a < b;
        });
        std::vector<std::size_t> event_age(n + 1, NONE);
        {
            std::size_t k = 0;
            for (const HistoryView::Event& e : events_) {
                while (k < t_.ages.size() && t_.ages[k].end_year < e.row.year) {
                    ++k;
                }
                event_age[static_cast<std::size_t>(e.row.id)] =
                    (k < t_.ages.size() && t_.ages[k].start_year <= e.row.year) ? k : age_of(e.row.year);
            }
        }
        // Item 2: the age floor.
        std::vector<std::uint8_t> floor(n + 1, 0);
        std::vector<std::int64_t> in_age(t_.ages.size(), 0);
        std::int64_t floor_size = 0;
        std::vector<std::vector<Id>> by_age(t_.ages.size());
        for (const Id e : ranked) {
            const std::size_t a = event_age[static_cast<std::size_t>(e)];
            if (a != NONE) {
                by_age[a].push_back(e);
            }
        }
        for (std::size_t a = 0; a < t_.ages.size(); ++a) {
            for (const Id e : by_age[a]) {
                if (in_age[a] >= SUMMARY_AGE_FLOOR) {
                    break;
                }
                if (floor[static_cast<std::size_t>(e)] != 0) {
                    continue;
                }
                for (const Id u : unit(e)) {
                    if (floor[static_cast<std::size_t>(u)] == 0) {
                        floor[static_cast<std::size_t>(u)] = 1;
                        ++floor_size;
                        const std::size_t ua = event_age[static_cast<std::size_t>(u)];
                        if (ua != NONE) {
                            ++in_age[ua];
                        }
                    }
                }
            }
        }
        // Item 3: the threshold. entered[u] = the highest salience of a
        // candidate whose unit holds u.
        std::vector<std::int64_t> entered(n + 1, -1);
        for (const Id e : ranked) {
            const std::int64_t s = events_[static_cast<std::size_t>(e - 1)].row.salience;
            for (const Id u : unit(e)) {
                entered[static_cast<std::size_t>(u)] = std::max(entered[static_cast<std::size_t>(u)], s);
            }
        }
        std::array<std::int64_t, 102> at_least{};  // at_least[t]: non-floor events with entered >= t, t <= 100
        for (std::size_t i = 1; i <= n; ++i) {
            if (floor[i] == 0 && entered[i] >= SUMMARY_MIN_SALIENCE) {
                ++at_least[static_cast<std::size_t>(std::min<std::int64_t>(entered[i], 100))];
            }
        }
        for (std::size_t t = 100; t-- > 0;) {
            at_least[t] += at_least[t + 1];
        }
        std::int64_t threshold = 100;
        for (std::int64_t t = SUMMARY_MIN_SALIENCE; t <= 100; ++t) {
            if (floor_size + at_least[static_cast<std::size_t>(t)] <= SUMMARY_BUDGET) {
                threshold = t;
                break;
            }
        }
        d_.summary_threshold = threshold;
        d_.summary.assign(n, 0);
        for (std::size_t i = 1; i <= n; ++i) {
            const bool shown = floor[i] != 0 || entered[i] >= threshold;
            d_.summary[i - 1] = shown ? 1 : 0;
            d_.summary_events += shown ? 1 : 0;
        }
    }

    // --- [stats] (section 9 with 21.5, 21.8, 22, 23) --------------------

    void put(std::string key, std::int64_t value) { d_.stats.emplace_back(std::move(key), value); }

    // floor(1000 n / d) for n >= 0, d > 0, through the exact 128-bit product
    // (core/wide.hpp), so that no count read from a file can overflow it;
    // 0 when d <= 0. A quotient above INT64_MAX (n far above d, which no
    // statistic here has) saturates.
    static std::int64_t permille(std::int64_t n, std::int64_t d) {
        if (d <= 0 || n <= 0) {
            return 0;
        }
        const U128DivMod q = divmod_u128_u64(mul_u64_wide(static_cast<std::uint64_t>(n), 1000U),
                                             static_cast<std::uint64_t>(d));
        constexpr auto MAX = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
        return (q.quotient.hi != 0 || q.quotient.lo > MAX) ? std::numeric_limits<std::int64_t>::max()
                                                           : static_cast<std::int64_t>(q.quotient.lo);
    }

    void stats() {
        frame_stats();
        civilization_stats();
        settlement_stats();
        region_stats();
        relation_stats();
        race_stats();
        conflict_stats();
        war_stats();
    }

    void frame_stats() {
        std::array<std::int64_t, 5> phases{};
        std::int64_t simulated = 0, quiet = 0, flare = 0;
        for (const db::PhaseRow& p : t_.phases) {
            const std::int64_t years = p.end_year - p.start_year + 1;
            ++phases[static_cast<std::size_t>(p.kind)];  // 1 to 4, checked by P3-04 11.1
            simulated += p.kind != 2 ? years : 0;
            quiet += p.kind == 2 ? years : 0;
            flare += p.kind == 3 ? years : 0;
        }
        put("end_year", e_);
        put("ages", static_cast<std::int64_t>(t_.ages.size()));
        put("phases", static_cast<std::int64_t>(t_.phases.size()));
        put("phases_opening", phases[1]);
        put("phases_quiet", phases[2]);
        put("phases_flare", phases[3]);
        put("phases_final", phases[4]);
        put("simulated_years", simulated);
        put("simulated_permille", permille(simulated, e_));
        put("quiet_years", quiet);
        put("flare_years", flare);
        const auto events = static_cast<std::int64_t>(events_.size());
        put("events", events);
        std::vector<std::int64_t> by_kind(view_.catalogue().kinds.size(), 0);
        std::int64_t caused = 0;
        for (const HistoryView::Event& e : events_) {
            ++by_kind[static_cast<std::size_t>(e.row.kind - 1)];
            caused += e.causes.empty() ? 0 : 1;
        }
        for (std::size_t k = 0; k < by_kind.size(); ++k) {
            put("events_" + view_.catalogue().kinds[k].key, by_kind[k]);
        }
        put("events_caused", caused);
        put("cause_links", t_.cause_rows);
        put("events_per_1000_simulated_years", permille(events, simulated));
        put("summary_threshold", d_.summary_threshold);
        put("summary_events", d_.summary_events);
    }

    void civilization_stats() {
        std::int64_t alive = 0, lifetime = 0, regions_peak = 0, population_peak = 0;
        for (const db::CivilizationRow& c : t_.civilizations) {
            alive += c.ended_year ? 0 : 1;
            lifetime = std::max(lifetime, c.ended_year.value_or(e_) - c.founded_year);
        }
        for (const CivilizationDigest& c : d_.civilizations) {
            regions_peak = std::max(regions_peak, c.peak_regions.count);
            population_peak = std::max(population_peak, c.peak_population.population);
        }
        const auto civs = static_cast<std::int64_t>(t_.civilizations.size());
        put("civilizations", civs);
        put("civilizations_alive", alive);
        put("civilizations_ended", civs - alive);
        for (const db::RaceRow& r : t_.races) {
            std::int64_t n = 0;
            for (const db::CivilizationRow& c : t_.civilizations) {
                n += c.race == r.id ? 1 : 0;
            }
            put("civilizations_" + r.key, n);
        }
        put("civ_lifetime_max", lifetime);
        put("civ_regions_peak_max", regions_peak);
        put("civ_population_peak_max", population_peak);
    }

    void settlement_stats() {
        std::array<std::int64_t, 6> status{};
        std::array<std::int64_t, 6> kind{};
        std::int64_t standing_n = 0, unowned = 0, population = 0, largest = 0;
        for (const db::SettlementRow& s : t_.settlements) {
            ++status[static_cast<std::size_t>(s.status)];
            ++kind[static_cast<std::size_t>(s.kind)];
            if (standing(s.status)) {
                ++standing_n;
                unowned += s.owner_civ == 0 ? 1 : 0;
                population += s.population;
                largest = std::max(largest, s.population);
            }
        }
        put("settlements", static_cast<std::int64_t>(t_.settlements.size()));
        put("settlements_standing", standing_n);
        put("settlements_thriving", status[1]);
        put("settlements_declining", status[2]);
        put("settlements_ruined", status[3]);
        put("settlements_faded", status[4]);
        put("settlements_hamlet", kind[1]);
        put("settlements_village", kind[2]);
        put("settlements_town", kind[3]);
        put("settlements_city", kind[4]);
        put("settlements_fortress", kind[5]);
        put("settlements_unowned", unowned);
        put("population", population);
        put("population_settlement_max", largest);
        put("census_rows", static_cast<std::int64_t>(t_.census.size()));
    }

    void region_stats() {
        std::int64_t held = 0;
        std::vector<Id> ever;
        for (const db::RegionControlRow& r : t_.region_control) {
            held += r.to_year ? 0 : 1;
            if (ever.empty() || ever.back() != r.region) {
                ever.push_back(r.region);  // rows in (region, from_year) order
            }
        }
        put("regions", static_cast<std::int64_t>(t_.regions.size()));
        put("regions_nonislet", nonislet_);
        put("regions_held", held);
        put("region_control_rows", static_cast<std::int64_t>(t_.region_control.size()));
        put("regions_ever_held", static_cast<std::int64_t>(ever.size()));
        const bool any = nonislet_ > 0 && !t_.civilizations.empty() && share_max_ > 0;
        put("share_max_permille", any ? share_max_ : 0);
        put("share_max_civ", any ? share_max_civ_ : 0);
        put("share_max_year", any ? share_max_year_ : 0);
        for (std::size_t l = 0; l < share_runs_.size(); ++l) {
            put("share_over_" + db::format_i64(SHARE_LEVELS[l]) + "_years", share_runs_[l]);
        }
    }

    void relation_stats() {
        std::int64_t open = 0, wars = 0, wars_open = 0;
        for (const db::RelationRow& r : t_.relations) {
            open += r.until ? 0 : 1;
            wars += r.kind == "war" ? 1 : 0;
            wars_open += (r.kind == "war" && !r.until) ? 1 : 0;
        }
        put("relations", static_cast<std::int64_t>(t_.relations.size()));
        put("relations_open", open);
        put("wars", wars);
        put("wars_open", wars_open);
    }

    void race_stats() {
        const std::size_t races = t_.races.size();
        race_population_.assign(races, 0);
        std::vector<std::int64_t> standing_n(races, 0);
        race_regions_.assign(races, 0);
        for (const db::SettlementRow& s : t_.settlements) {
            const std::size_t r = race_of(s.owner_civ);
            if (standing(s.status) && r != NONE) {
                race_population_[r] += s.population;
                ++standing_n[r];
            }
        }
        for (const db::RegionControlRow& rc : t_.region_control) {
            const std::size_t r = race_of(rc.civ);
            if (!rc.to_year && r != NONE) {
                ++race_regions_[r];
            }
        }
        for (std::size_t k = 0; k < races; ++k) {
            put("population_" + t_.races[k].key, race_population_[k]);
        }
        for (std::size_t k = 0; k < races; ++k) {
            put("settlements_standing_" + t_.races[k].key, standing_n[k]);
        }
        for (std::size_t k = 0; k < races; ++k) {
            put("regions_held_" + t_.races[k].key, race_regions_[k]);
        }
        std::int64_t most = 0;
        for (std::size_t k = 0; k < t_.civilizations.size(); ++k) {
            if (!t_.civilizations[k].ended_year) {
                most = std::max(most, d_.civilizations[k].settlements_held);
            }
        }
        put("civ_settlements_max", most);
        std::int64_t colonies = 0, seceded = 0;
        for (const db::SettlementRow& s : t_.settlements) {
            colonies += kind(s.founding_event) == kind_id("colony_founded") ? 1 : 0;
        }
        for (const db::CivilizationRow& c : t_.civilizations) {
            seceded += kind(c.founding_event) == kinds_.civ_seceded ? 1 : 0;
        }
        put("settlements_colonies", colonies);
        put("civilizations_seceded", seceded);
    }

    [[nodiscard]] Id kind_id(std::string_view key) const {
        const KindDef* k = view_.catalogue().find(key);
        return k != nullptr ? k->id : -1;
    }

    void conflict_stats() {
        std::int64_t enmity = 0, peace = 0, war_total = 0, war_max = 0;
        for (const db::RelationRow& r : t_.relations) {
            enmity += r.kind == "enmity" ? 1 : 0;
            peace += r.kind == "peace" ? 1 : 0;
            if (r.kind == "war") {
                const std::int64_t years = r.until.value_or(e_) - r.since;
                war_total += years;
                war_max = std::max(war_max, years);
            }
        }
        put("relations_enmity", enmity);
        put("relations_peace", peace);
        put("war_years_total", war_total);
        put("war_years_max", war_max);
        std::int64_t conquered = 0, sacked = 0, regions = 0, races_alive = 0;
        for (const db::CivilizationRow& c : t_.civilizations) {
            conquered += (c.ending_event != 0 && kind(c.ending_event) == kinds_.civ_conquered) ? 1 : 0;
        }
        for (const db::SettlementRow& s : t_.settlements) {
            sacked += (s.ruin_event != 0 && kind(s.ruin_event) == kinds_.sacked) ? 1 : 0;
        }
        std::vector<Id> captured;
        for (const HistoryView::Event& e : events_) {
            if (e.row.kind == kinds_.settlement_captured) {
                captured.push_back(e.row.settlement);
            }
        }
        std::stable_sort(captured.begin(), captured.end());
        captured.erase(std::unique(captured.begin(), captured.end()), captured.end());
        for (const db::RegionControlRow& r : t_.region_control) {
            const Id k = kind(r.event);
            regions += (k == kinds_.settlement_captured || kinds_.cession(k)) ? 1 : 0;
        }
        for (const db::RaceRow& r : t_.races) {
            bool alive = false;
            for (const db::CivilizationRow& c : t_.civilizations) {
                alive = alive || (c.race == r.id && !c.ended_year);
            }
            races_alive += alive ? 1 : 0;
        }
        put("civilizations_conquered", conquered);
        put("settlements_sacked", sacked);
        put("settlements_captured", static_cast<std::int64_t>(captured.size()));
        put("regions_conquered", regions);
        put("races_alive", races_alive);
        for (std::size_t k = 0; k < t_.races.size(); ++k) {
            std::int64_t n = 0;
            for (const db::RelationRow& r : t_.relations) {
                n += (r.kind == "war" && r.a_type == 3 && race_of(r.a_id) == k) ? 1 : 0;
            }
            put("wars_attacker_" + t_.races[k].key, n);
        }
    }

    // The participant of `role` (by key) of event e, or 0.
    [[nodiscard]] Id role_entity(const HistoryView::Event& e, std::string_view role) const {
        const KindDef* k = view_.catalogue().find_id(e.row.kind);
        const RoleDef* r = k != nullptr ? k->find_role(role) : nullptr;
        if (r == nullptr) {
            return 0;
        }
        for (const db::EventParticipantRow& p : e.participants) {
            if (p.role == r->seq) {
                return p.entity_id;
            }
        }
        return 0;
    }

    // The years in which region g's holder changes (22.5b): the from_year of
    // each region_control row whose civ differs from the previous row's (the
    // first row is a change).
    [[nodiscard]] std::vector<std::int64_t> holder_changes(Id g) const {
        std::vector<std::int64_t> out;
        const auto first = std::lower_bound(t_.region_control.begin(), t_.region_control.end(), g,
                                            [](const db::RegionControlRow& r, Id v) { return r.region < v; });
        Id previous = -1;
        for (auto it = first; it != t_.region_control.end() && it->region == g; ++it) {
            if (it->civ != previous) {
                out.push_back(it->from_year);
            }
            previous = it->civ;
        }
        return out;
    }

    void war_stats() {
        std::int64_t declared = 0, hollow = 0, unfound = 0;
        // (defender, region) -> declarations in id order.
        std::map<std::pair<Id, Id>, std::vector<Id>> goals;
        for (const HistoryView::Event& e : events_) {
            if (e.row.kind != kinds_.war_declared) {
                continue;
            }
            const Id d = e.row.id;
            ++declared;
            goals[{role_entity(e, "defender"), e.row.region}].push_back(d);
            bool changed = false;
            for (const Id b : effects_[static_cast<std::size_t>(d)]) {
                if (kind(b) != kinds_.battle && kind(b) != kinds_.battle_at_pass) {
                    continue;
                }
                for (const Id s : effects_[static_cast<std::size_t>(b)]) {
                    if (kind(s) != kinds_.siege) {
                        continue;
                    }
                    for (const Id f : effects_[static_cast<std::size_t>(s)]) {
                        changed = changed || kind(f) == kinds_.sacked || kind(f) == kinds_.settlement_captured;
                    }
                }
            }
            if (const Id u = war_ending(d); u != 0) {
                if (kind(u) == kinds_.search_abandoned) {
                    ++unfound;  // 21.8 item 2: counted apart
                    continue;
                }
                changed = changed || kind(u) == kinds_.civ_conquered;
                for (const Id x : effects_[static_cast<std::size_t>(u)]) {
                    changed = changed || kinds_.cession_or_tribute(kind(x));
                }
            }
            hollow += changed ? 0 : 1;
        }
        std::int64_t same_max = 0, unchanged_max = 0;
        for (const auto& [goal, wars] : goals) {
            same_max = std::max(same_max, static_cast<std::int64_t>(wars.size()));
            unchanged_max = std::max(unchanged_max, unchanged_run(goal.second, wars));
        }
        put("wars_hollow", hollow);
        put("wars_hollow_permille", permille(hollow, declared - unfound));
        put("wars_same_goal_max", same_max);
        put("wars_same_goal_unchanged_max", unchanged_max);
        std::int64_t population = 0, held = 0;
        for (const db::SettlementRow& s : t_.settlements) {
            population += standing(s.status) ? s.population : 0;
        }
        for (const db::RegionControlRow& r : t_.region_control) {
            held += r.to_year ? 0 : 1;
        }
        std::int64_t population_share = 0, regions_share = 0;
        for (std::size_t k = 0; k < t_.races.size(); ++k) {
            population_share = std::max(population_share, permille(race_population_[k], population));
            regions_share = std::max(regions_share, permille(race_regions_[k], held));
        }
        put("race_population_share_max_permille", population_share);
        put("race_regions_share_max_permille", regions_share);
        put("wars_unfound", unfound);
        std::int64_t no_destination = 0;
        const KindDef* refuge = view_.catalogue().find_id(kinds_.refuge_held);
        const std::int64_t reason = refuge != nullptr ? refuge->value_seq("reason") : 0;
        for (const HistoryView::Event& e : events_) {
            if (e.row.kind == kinds_.refuge_held && reason >= 1 &&
                static_cast<std::size_t>(reason) <= e.values.size() &&
                e.values[static_cast<std::size_t>(reason - 1)] == 1) {
                ++no_destination;
            }
        }
        put("refuges_held_no_destination", no_destination);
    }

    // 21.8 item 5 (r11, P3-06 20.28) with 22.5b, the incremental form.
    [[nodiscard]] std::int64_t unchanged_run(Id g, const std::vector<Id>& wars) const {
        const std::vector<std::int64_t> changes = holder_changes(g);
        const auto moved = [&changes](std::int64_t a, std::int64_t b) {
            const auto it = std::lower_bound(changes.begin(), changes.end(), a);
            return it != changes.end() && *it <= b;
        };
        std::int64_t run = 0, lo = 0, hi = 0, best = 0;
        for (const Id d : wars) {
            const std::size_t r = war_row_[static_cast<std::size_t>(d)];
            if (r == NONE) {
                continue;  // no war row: no span (not in a world of the generator)
            }
            const db::RelationRow& row = t_.relations[r];
            if (row.until && (kind(row.until_event) == kinds_.civ_conquered || kind(row.until_event) == kinds_.civ_ended)) {
                continue;  // closed by a side's end: neither extends nor breaks a run
            }
            const std::int64_t s = row.since;
            const std::int64_t u = row.until.value_or(e_);
            if (run > 0 && !moved(lo, std::max(hi, u))) {
                ++run;
                hi = std::max(hi, u);
            } else if (!moved(s, u)) {
                run = 1;
                lo = s;
                hi = u;
            } else {
                run = 0;
            }
            best = std::max(best, run);
        }
        return best;
    }

    const HistoryView& view_;
    const HistoryTables& t_;
    std::int64_t e_;
    std::span<const HistoryView::Event> events_;
    Kinds kinds_;
    HistoryDigest d_;
    std::vector<std::vector<Id>> effects_;           // index event id: the events it caused, ascending
    std::vector<std::vector<Id>> civ_events_;        // index civ - 1: its events, ascending
    std::vector<std::size_t> war_row_;               // index event id: its war relation row, or NONE
    std::vector<std::vector<std::size_t>> control_of_;  // index civ - 1: its region_control rows
    std::int64_t nonislet_ = 0;
    std::int64_t share_max_ = 0, share_max_civ_ = 0, share_max_year_ = 0;
    std::array<std::int64_t, 5> share_runs_{};
    std::vector<std::int64_t> race_population_;
    std::vector<std::int64_t> race_regions_;
};

}  // namespace

HistoryDigest digest_history(const HistoryView& view, const HistoryTables& tables) {
    return Digester(view, tables).run();
}

}  // namespace worldgen::history
