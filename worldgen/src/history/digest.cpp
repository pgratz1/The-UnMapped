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

#include "worldgen/core/checked.hpp"
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

// P3-04 r21 23.1: a region named in a note as at the note's event `event`
// (a note is read with its sentence).
bool append_region(std::string& out, const HistoryView& view, std::int64_t region, std::int64_t event) {
    const std::string* n = view.region_name_at_event(region, event);
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
    // P3-07 r19 25.8: the note dates the grievance, root(c): c's cause of a
    // claim kind with the smallest id (c is then a fresh claim), else c.
    const HistoryView::Event* root = c;
    for (const std::int64_t id : c->causes) {
        const HistoryView::Event* k = view.event(id);
        if (k != nullptr && claims.holds(*k) && (root == c || k->row.id < root->row.id)) {
            root = k;
        }
    }
    const std::int64_t y_c = root->row.year;
    const bool same_year = y_c >= e.row.year;
    std::string out;
    if (is_kind(*c, claims.settlements)) {
        // P3-07 r15 23 item 4a: a claim of kind 39 names its towns, whether
        // or not its region is the war's.
        out = same_year ? "The claim, made the same year, was to "
                        : "The claim, since " + int_text(y_c) + ", was to ";
        if (!append_settlements(out, view, *c, *claims.settlements)) {
            return {};
        }
        out += " in ";
        if (!append_region(out, view, c->row.region, e.row.id)) {
            return {};
        }
        out += ".";
    } else if (c->row.region == e.row.region) {
        out = same_year ? "Claimed the same year." : "Claimed since " + int_text(y_c) + ".";
    } else {
        // P3-07 r14 23 item 3 (P3-06 r19 23.4): the war's goal is not the
        // claim's region.
        out = same_year ? "The claim, made the same year, was to " : "The claim, since " + int_text(y_c) + ", was to ";
        if (!append_region(out, view, c->row.region, e.row.id)) {
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

// P3-07 r14 23 item 4: a flare-up (event `event`) caused by a
// claim_settlements.
std::string flare_settlements_note(const HistoryView& view, const HistoryView::Event& c, const KindDef& kind,
                                   std::int64_t event) {
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
    if (!append_region(out, view, c.row.region, event)) {
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
        return flare_settlements_note(view, *c, *claims.settlements, e.row.id);
    }
    if (!is_kind(*c, claims.claim)) {
        return {};
    }
    const KindDef* claim = claims.claim;
    const std::string* a = view.name(EntityType::civilization, participant(*c, *claim, "claimant"));
    const std::string* b = view.name(EntityType::civilization, participant(*c, *claim, "holder"));
    const std::string* r = view.region_name_at_event(c->row.region, e.row.id);
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
        // P5-02 12.7: a flare-up started by a monster's appearance.
        const KindDef* appeared = catalogue.find("monster_appeared");
        const HistoryView::Event* c = e->causes.empty() ? nullptr : view.event(e->causes.front());
        if (appeared != nullptr && c != nullptr && c->row.kind == appeared->id) {
            const std::int64_t m = participant(*c, *appeared, "monster");
            const std::string* noun = view.monster_noun(m);
            std::string out = "The waking of ";
            if (noun == nullptr || !append_entity(out, view, EntityType::monster, m)) {
                return {};
            }
            out += ", " + *noun + ", stirred the land.";
            return out;
        }
        return flare_note(view, *e, claims);
    }
    const KindDef* quiet = catalogue.find("quiet_stretch");
    if (civilization != 0 && quiet != nullptr && e->row.kind == quiet->id) {
        return generations_note(view, *e, *quiet, civilization);
    }
    return {};
}

std::vector<std::vector<std::int64_t>> figure_event_lists(const HistoryView& view) {
    const std::size_t nf = view.figure_rows().size();
    std::vector<std::vector<std::int64_t>> out(nf);
    const auto add = [&out](std::int64_t f, std::int64_t e) {
        if (f < 1 || static_cast<std::uint64_t>(f) > out.size()) {
            return;
        }
        std::vector<std::int64_t>& list = out[static_cast<std::size_t>(f - 1)];
        if (list.empty() || list.back() != e) {
            list.push_back(e);  // events visited in id order: ascending, each once
        }
    };
    if (nf == 0) {
        return out;
    }
    // Per stored kind (index id - 1): the seqs of its civilization roles that
    // its template uses with the form under ("{<role>:under}").
    const std::vector<KindDef>& kinds = view.catalogue().kinds;
    std::vector<std::vector<std::int64_t>> under(kinds.size());
    constexpr std::string_view FORM = ":under}";
    for (std::size_t k = 0; k < kinds.size(); ++k) {
        const std::string& t = kinds[k].templ;
        for (std::size_t at = t.find(FORM); at != std::string::npos; at = t.find(FORM, at + FORM.size())) {
            const std::size_t open = t.rfind('{', at);
            if (open == std::string::npos) {
                continue;
            }
            const RoleDef* role = kinds[k].find_role(std::string_view(t).substr(open + 1, at - open - 1));
            if (role != nullptr && role->type == EntityType::civilization &&
                std::find(under[k].begin(), under[k].end(), role->seq) == under[k].end()) {
                under[k].push_back(role->seq);
            }
        }
    }
    for (const HistoryView::Event& e : view.events()) {
        for (const db::EventParticipantRow& p : e.participants) {
            if (p.entity_type == static_cast<std::int64_t>(EntityType::figure)) {
                add(p.entity_id, e.row.id);
            }
        }
        if (e.row.kind < 1 || static_cast<std::size_t>(e.row.kind) > under.size()) {
            continue;
        }
        for (const std::int64_t seq : under[static_cast<std::size_t>(e.row.kind - 1)]) {
            for (const db::EventParticipantRow& p : e.participants) {  // seq order: the first of the role
                if (p.role == seq) {
                    add(view.ruler_at_event(p.entity_id, e.row.id), e.row.id);
                    break;
                }
            }
        }
    }
    return out;
}

}  // namespace worldgen::history

// --- The digest (P3-07 section 7, 9; 21 to 23) ------------------------------

namespace worldgen::history {

namespace {

using Id = std::int64_t;

// 24.3 item 4 with the budget B (P5-02 12.9): T = the smallest t in
// [SUMMARY_MIN_SALIENCE, 100] with |floor and C(t)| <= B, else 100.
void set_summary(HistoryDigest& d, std::int64_t budget) {
    std::int64_t threshold = 100;
    for (std::int64_t t = SUMMARY_MIN_SALIENCE; t <= 100; ++t) {
        if (d.summary_floor_size + d.summary_at_least[static_cast<std::size_t>(t)] <= budget) {
            threshold = t;
            break;
        }
    }
    const std::size_t n = d.summary_floor.size();
    d.summary_budget = budget;
    d.summary_threshold = threshold;
    d.summary_events = 0;
    d.summary.assign(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        const bool shown = d.summary_floor[i] != 0 || d.summary_entered[i] >= threshold;
        d.summary[i] = shown ? 1 : 0;
        d.summary_events += shown ? 1 : 0;
    }
}

bool standing(std::int64_t status) noexcept {
    return status == 1 || status == 2;
}

// The kind ids the digest uses, by key in the stored catalogue (0 when
// absent: a world of another generator, or a fixture).
struct Kinds {
    Id war_declared = 0, peace = 0, war_won = 0, truce = 0, region_ceded = 0, settlements_ceded = 0, tribute = 0;
    Id sacked = 0, settlement_captured = 0, capital_moved = 0, refugees_settled = 0, civ_conquered = 0;
    Id civ_ended = 0, search_abandoned = 0, battle = 0, battle_at_pass = 0, allied_battle = 0, siege = 0, colony_founded = 0;
    Id ally_joined_war = 0, ally_refused = 0, alliance_betrayed = 0, spoils_shared = 0, spoils_withheld = 0;  // P5-04
    Id civ_seceded = 0, refuge_held = 0;
    Id claim = 0, claim_settlements = 0, quiet_stretch = 0, plague_spread = 0, civ_founded = 0;
    // P3-07 r19 25: kinds 41 and 42, the flare-up, the calamity kinds of
    // 25.2 and 25.7 item 3, and the ruin kinds of 25.3.
    Id claim_dropped = 0, refuge_hidden = 0, flare_up = 0;
    Id famine = 0, plague = 0, plague_outbreak = 0, succession_crisis = 0, settlement_dwindled = 0;
    Id settlement_abandoned = 0, settlement_abandoned_to = 0;
    Id region_renamed = 0;  // r21 26.5 (P5-12)
    Id monster_raid = 0, settlement_overrun = 0, undead_rose = 0, beasts_bold = 0;  // P5-02 12.6
    // P5-03: kinds 56 to 62.
    Id settlement_fled = 0, peoples_met = 0, disposition_turned = 0, refugees_arrived = 0, settlement_mixed = 0;
    Id standing_changed = 0, people_expelled = 0;
    // P5-06: kinds 79 to 92 the digest reads.
    Id battle_led = 0, succession_disputed = 0, exiled = 0, fell_in_battle = 0;

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
        allied_battle = id("allied_battle");  // P5-04 11.1
        ally_joined_war = id("ally_joined_war");
        ally_refused = id("ally_refused");
        alliance_betrayed = id("alliance_betrayed");
        spoils_shared = id("spoils_shared");
        spoils_withheld = id("spoils_withheld");
        siege = id("siege");
        colony_founded = id("colony_founded");
        civ_seceded = id("civ_seceded");
        refuge_held = id("refuge_held");
        claim = id("claim");
        claim_settlements = id("claim_settlements");
        quiet_stretch = id("quiet_stretch");
        plague_spread = id("plague_spread");
        civ_founded = id("civ_founded");
        claim_dropped = id("claim_dropped");
        refuge_hidden = id("refuge_hidden");
        flare_up = id("flare_up");
        famine = id("famine");
        plague = id("plague");
        plague_outbreak = id("plague_outbreak");
        succession_crisis = id("succession_crisis");
        settlement_dwindled = id("settlement_dwindled");
        settlement_abandoned = id("settlement_abandoned");
        settlement_abandoned_to = id("settlement_abandoned_to");
        region_renamed = id("region_renamed");
        monster_raid = id("monster_raid");
        settlement_overrun = id("settlement_overrun");
        undead_rose = id("undead_rose");
        beasts_bold = id("beasts_bold");
        settlement_fled = id("settlement_fled");
        peoples_met = id("peoples_met");
        disposition_turned = id("disposition_turned");
        refugees_arrived = id("refugees_arrived");
        settlement_mixed = id("settlement_mixed");
        standing_changed = id("standing_changed");
        people_expelled = id("people_expelled");
        battle_led = id("battle_led");  // P5-06 11.1
        succession_disputed = id("succession_disputed");
        exiled = id("exiled");
        fell_in_battle = id("fell_in_battle");
    }
    // 25.2: the calamity kinds of the summary's cap.
    [[nodiscard]] bool calamity(Id k) const noexcept {
        return k == famine || k == plague || k == plague_outbreak || k == succession_crisis;
    }
    // 25.7 item 3: cal(e) of the milestone fill.
    [[nodiscard]] bool death_toll(Id k) const noexcept {
        return calamity(k) || k == plague_spread || k == settlement_dwindled;
    }
    // P3-07 r17 24.1: the chain kinds W and the war-event kinds E_w.
    // P5-06 11.1: and battle_led.
    [[nodiscard]] bool battle_kind(Id k) const noexcept {
        return k == battle || k == battle_at_pass || k == allied_battle || k == battle_led;
    }
    [[nodiscard]] bool chain(Id k) const noexcept {
        return k == war_declared || battle_kind(k) || k == siege || k == sacked || k == settlement_captured ||
               k == refuge_held || k == peace || k == war_won || k == truce;
    }
    [[nodiscard]] bool war_event(Id k) const noexcept {
        return (chain(k) && k != war_declared) || k == search_abandoned || cession_or_tribute(k) ||
               consequence(k);
    }
    [[nodiscard]] bool consequence(Id k) const noexcept {
        return k == capital_moved || k == refugees_settled || k == civ_conquered;
    }
    [[nodiscard]] bool fall(Id k) const noexcept { return k == sacked || k == settlement_captured; }
    [[nodiscard]] bool is_claim(Id k) const noexcept { return k == claim || k == claim_settlements; }
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
        monster_stories();  // P5-03 10.1
        summary();
        figures();  // P5-06 10.2
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
            if (principal_war(r) && r.since_event >= 1 && static_cast<std::size_t>(r.since_event) <= n &&
                war_row_[static_cast<std::size_t>(r.since_event)] == NONE) {
                war_row_[static_cast<std::size_t>(r.since_event)] = i;
            }
        }
        wars_index();
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

    // P3-07 r17 24.1: war(e) for every event in ascending id, and each
    // declaration's last battle.
    void wars_index() {
        const std::size_t n = events_.size();
        std::vector<Id> until_war(n + 1, 0);  // the smallest since_event of the war rows ended by the event
        for (const db::RelationRow& r : t_.relations) {
            if (r.kind != "war" || !r.until || r.until_event < 1 || static_cast<std::size_t>(r.until_event) > n) {
                continue;
            }
            Id& w = until_war[static_cast<std::size_t>(r.until_event)];
            if (r.since_event >= 1 && (w == 0 || r.since_event < w)) {
                w = r.since_event;
            }
        }
        war_of_.assign(n + 1, 0);
        last_battle_.assign(n + 1, 0);
        for (const HistoryView::Event& e : events_) {
            const Id k = e.row.kind;
            const auto id = static_cast<std::size_t>(e.row.id);
            if (k == kinds_.war_declared) {
                war_of_[id] = e.row.id;
            } else if (kinds_.war_event(k)) {
                Id c = 0;
                for (const Id x : e.causes) {  // ascending
                    if (kinds_.chain(kind(x))) {
                        c = x;
                        break;
                    }
                }
                if (c != 0) {
                    war_of_[id] = war_of_[static_cast<std::size_t>(c)];
                } else {
                    war_of_[id] = until_war[id];
                }
            }
            const Id d = war_of_[id];
            if (kinds_.battle_kind(k) && d >= 1 && d < e.row.id) {
                last_battle_[static_cast<std::size_t>(d)] = e.row.id;  // ids ascend: the last is the largest
            }
        }
    }

    // 22.5a: a declaration whose war ended in a search below the minimum
    // salience.
    [[nodiscard]] bool excluded_declaration(Id d) const {
        const Id u = war_ending(d);
        return u != 0 && kind(u) == kinds_.search_abandoned &&
               events_[static_cast<std::size_t>(u - 1)].row.salience < SUMMARY_MIN_SALIENCE;
    }

    // Frame(d) of 24.1 appended to out (nothing for an excluded declaration).
    void append_frame(std::vector<Id>& out, Id d) const {
        if (excluded_declaration(d)) {
            return;
        }
        out.push_back(d);
        if (const Id b = last_battle_[static_cast<std::size_t>(d)]; b != 0) {
            out.push_back(b);
            // P5-06 10.3: a general's fall is a companion of the battle it
            // cites, shown with it as the war's last.
            for (const Id x : effects_[static_cast<std::size_t>(b)]) {
                if (kind(x) == kinds_.fell_in_battle) {
                    out.push_back(x);
                }
            }
        }
        if (const Id u = war_ending(d); u != 0) {
            out.push_back(u);
            for (const Id x : effects_[static_cast<std::size_t>(u)]) {
                if (kinds_.cession_or_tribute(kind(x))) {
                    out.push_back(x);
                }
            }
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
            c.milestones = milestones(c, civ_events_[k], MILESTONES);
            if (t_.civilizations[k].ended_year) {
                c.milestones_ended = milestones(c, civ_events_[k], MILESTONES_ENDED);  // r19 25.5
            }
            summary_milestones(c, civ_events_[k]);  // P5-04a 12.2
        }
        for (const db::SettlementRow& s : t_.settlements) {
            if (is_civ(s.founder_civ)) {
                CivilizationDigest& c = d_.civilizations[civ_index(s.founder_civ)];
                ++c.settlements_founded;
                c.settlements_ruined += s.status == 3 ? 1 : 0;  // r19 25.3 item 3
                c.settlements_faded += s.status == 4 ? 1 : 0;
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
        for (CivilizationDigest& c : d_.civilizations) {
            wars_and_adversaries(c);
        }
        present_lists();
        regions_held_by_year();
        population_peaks();
    }

    // The value of key `key` of event e, or 0.
    [[nodiscard]] std::int64_t value_of(const HistoryView::Event& e, std::string_view key) const {
        const KindDef* k = view_.catalogue().find_id(e.row.kind);
        const std::int64_t seq = k != nullptr ? k->value_seq(key) : 0;
        return (seq >= 1 && static_cast<std::size_t>(seq) <= e.values.size()) ? e.values[static_cast<std::size_t>(seq - 1)]
                                                                               : 0;
    }

    // own(c, e) of 24.5: c has one of the own roles of 24.1 in e.
    [[nodiscard]] bool own(Id civ, const HistoryView::Event& e) const {
        static constexpr std::array<std::string_view, 8> OWN = {"founder", "attacker", "winner", "conqueror",
                                                                "to",      "payee",    "claimant", "searcher"};
        const KindDef* k = view_.catalogue().find_id(e.row.kind);
        for (const db::EventParticipantRow& p : e.participants) {
            if (p.entity_type != static_cast<std::int64_t>(EntityType::civilization) || p.entity_id != civ ||
                k == nullptr) {
                continue;
            }
            const std::string& role = k->roles[static_cast<std::size_t>(p.role - 1)].role;  // checked by load
            if (std::find(OWN.begin(), OWN.end(), role) != OWN.end()) {
                return true;
            }
        }
        return false;
    }

    // 24.5 (replaces 7.3): founding, ending, the longest quiet stretch, then
    // the fill by (own, cal, salience, id) (r19 25.7 item 3) with at most
    // MILESTONE_KIND_MAX per kind, until `limit` are chosen.
    [[nodiscard]] std::vector<Id> milestones(const CivilizationDigest& c, const std::vector<Id>& ids,
                                             std::size_t limit) const {
        const db::CivilizationRow& row = t_.civilizations[civ_index(c.id)];
        std::vector<Id> chosen;
        const auto take = [&chosen](Id e) {
            if (e != 0 && std::find(chosen.begin(), chosen.end(), e) == chosen.end()) {
                chosen.push_back(e);
            }
        };
        take(row.founding_event);
        if (row.ended_year) {
            take(row.ending_event);
        }
        Id longest = 0;
        std::int64_t longest_years = 0;
        for (const Id e : ids) {  // ascending: the smallest id wins a tie
            const HistoryView::Event& ev = events_[static_cast<std::size_t>(e - 1)];
            if (ev.row.kind == kinds_.quiet_stretch) {
                const std::int64_t years = value_of(ev, "years");
                if (longest == 0 || years > longest_years) {
                    longest = e;
                    longest_years = years;
                }
            }
        }
        take(longest);
        std::vector<Id> rest;
        for (const Id e : ids) {
            if (kind(e) != kinds_.quiet_stretch && std::find(chosen.begin(), chosen.end(), e) == chosen.end()) {
                rest.push_back(e);
            }
        }
        std::vector<std::uint8_t> is_own(rest.size(), 0);
        std::vector<std::uint8_t> cal(rest.size(), 0);
        std::vector<std::size_t> order(rest.size());
        for (std::size_t i = 0; i < rest.size(); ++i) {
            order[i] = i;
            is_own[i] = own(c.id, events_[static_cast<std::size_t>(rest[i] - 1)]) ? 1 : 0;
            cal[i] = kinds_.death_toll(kind(rest[i])) ? 1 : 0;
        }
        std::stable_sort(order.begin(), order.end(), [&](std::size_t x, std::size_t y) {
            if (is_own[x] != is_own[y]) return is_own[x] > is_own[y];
            if (cal[x] != cal[y]) return cal[x] < cal[y];
            if (salience(rest[x]) != salience(rest[y])) return salience(rest[x]) > salience(rest[y]);
            return rest[x] < rest[y];
        });
        std::vector<std::int64_t> per_kind(view_.catalogue().kinds.size() + 1, 0);
        for (const std::size_t i : order) {
            if (chosen.size() >= limit) {
                break;
            }
            std::int64_t& n = per_kind[static_cast<std::size_t>(kind(rest[i]))];
            if (n < MILESTONE_KIND_MAX) {
                ++n;
                chosen.push_back(rest[i]);
            }
        }
        std::stable_sort(chosen.begin(), chosen.end());
        return chosen;
    }

    // P5-04a 12.2: n = the realm's own events of salience >=
    // SUMMARY_MILESTONE_SALIENCE that are not quiet stretches; the three of
    // largest salience (then the smaller id), in ascending id.
    void summary_milestones(CivilizationDigest& c, const std::vector<Id>& ids) const {
        std::vector<Id> own_ids;
        for (const Id e : ids) {
            if (kind(e) != kinds_.quiet_stretch && salience(e) >= SUMMARY_MILESTONE_SALIENCE &&
                own(c.id, events_[static_cast<std::size_t>(e - 1)])) {
                own_ids.push_back(e);
            }
        }
        c.milestones_count = static_cast<std::int64_t>(own_ids.size());
        std::stable_sort(own_ids.begin(), own_ids.end(), [this](Id x, Id y) {
            return salience(x) != salience(y) ? salience(x) > salience(y) : x < y;
        });
        if (own_ids.size() > SUMMARY_MILESTONES_TOP) {
            own_ids.resize(SUMMARY_MILESTONES_TOP);
        }
        std::stable_sort(own_ids.begin(), own_ids.end());
        c.milestones_top = std::move(own_ids);
    }

    // The participant of `role` (by key) of event `id`, or 0.
    [[nodiscard]] Id role_of(Id id, std::string_view role) const {
        return role_entity(events_[static_cast<std::size_t>(id - 1)], role);
    }

    // The outcome of a closed war row for civilization civ (24.5): 1 won,
    // -1 lost, 0 drawn.
    [[nodiscard]] int outcome(const db::RelationRow& r, Id civ) const {
        const Id u = r.until_event;
        const Id k = kind(u);
        if (k == kinds_.peace || k == kinds_.war_won) {
            return role_of(u, "winner") == civ ? 1 : (role_of(u, "loser") == civ ? -1 : 0);
        }
        if (k == kinds_.civ_conquered) {
            return role_of(u, "conqueror") == civ ? 1 : (role_of(u, "civilization") == civ ? -1 : 0);
        }
        return 0;
    }

    // 24.5: the wars line, the adversaries and seceded from.
    void wars_and_adversaries(CivilizationDigest& c) const {
        const Id civ = c.id;
        const db::CivilizationRow& row = t_.civilizations[civ_index(civ)];
        if (kind(row.founding_event) == kinds_.civ_seceded) {
            c.seceded_from = role_of(row.founding_event, "from");
        }
        using Key = std::pair<std::int64_t, std::int64_t>;  // (entity type, id)
        std::map<Key, CivilizationDigest::Adversary> adversaries;
        std::map<Key, std::int64_t> first_since;
        std::map<Key, int> state;  // 3 war, 2 peace, 1 enmity, 0 none
        for (const std::size_t i : c.relations) {
            const db::RelationRow& r = t_.relations[i];
            if ((r.kind == "war" && !principal_war(r)) || r.kind == "alliance" || r.kind == "trade") {
                continue;  // P5-04 10.4: an ally's joined row, an alliance and a trade are no adversaries
            }
            const bool is_a = r.a_type == 3 && r.a_id == civ;
            const Key other = is_a ? Key{r.b_type, r.b_id} : Key{r.a_type, r.a_id};
            CivilizationDigest::Adversary& adv = adversaries[other];
            adv.type = other.first;
            adv.id = other.second;
            const auto fs = first_since.find(other);
            if (fs == first_since.end() || r.since < fs->second) {
                first_since[other] = r.since;
            }
            if (!r.until) {
                const int st = r.kind == "war" ? 3 : r.kind == "peace" ? 2 : r.kind == "enmity" ? 1 : 0;
                state[other] = std::max(state[other], st);
            }
            if (r.kind != "war") {
                continue;
            }
            (is_a ? c.wars_declared : c.wars_defended) += 1;
            const std::int64_t last = r.until ? *r.until : e_ - 1;  // r19 25.7 item 1
            if (adv.wars == 0) {
                adv.first = r.since;
                adv.last = last;
            } else {
                adv.first = std::min(adv.first, r.since);
                adv.last = std::max(adv.last, last);
            }
            ++adv.wars;
            if (!r.until) {
                ++c.wars_open;
                ++adv.open;
                continue;
            }
            const int o = outcome(r, civ);
            (o > 0 ? c.wars_won : o < 0 ? c.wars_lost : c.wars_drawn) += 1;
            (o > 0 ? adv.won : o < 0 ? adv.lost : adv.drawn) += 1;
        }
        static constexpr std::array<std::string_view, 4> STATE = {"none", "enmity", "at peace", "at war"};
        for (auto& [key, adv] : adversaries) {
            const auto st = state.find(key);
            adv.state = std::string(STATE[static_cast<std::size_t>(st == state.end() ? 0 : st->second)]);
            c.adversaries.push_back(adv);
        }
        std::stable_sort(c.adversaries.begin(), c.adversaries.end(),
                         [&first_since, this](const CivilizationDigest::Adversary& x,
                                              const CivilizationDigest::Adversary& y) {
                             // r19 25.10: find, a missing key read as since = E.
                             const auto ix = first_since.find(Key{x.type, x.id});
                             const auto iy = first_since.find(Key{y.type, y.id});
                             const std::int64_t fx = ix != first_since.end() ? ix->second : e_;
                             const std::int64_t fy = iy != first_since.end() ? iy->second : e_;
                             if (fx != fy) return fx < fy;
                             if (x.type != y.type) return x.type < y.type;
                             return x.id < y.id;
                         });
    }

    // 24.7: last held by, per settlement, and the standing claims.
    void present_lists() {
        d_.last_held_by.assign(t_.settlements.size(), 0);
        for (const db::SettlementCensusRow& r : t_.census) {  // (settlement, year): the last wins
            if (standing(r.status)) {
                d_.last_held_by[static_cast<std::size_t>(r.settlement - 1)] = r.owner_civ;
            }
        }
        // (ii): per unordered pair the latest declaration year; (iii): per
        // ordered pair the largest claim id.
        std::map<std::pair<Id, Id>, std::int64_t> declared;
        std::map<std::pair<Id, Id>, Id> last_claim;
        for (const HistoryView::Event& e : events_) {
            if (e.row.kind == kinds_.war_declared) {
                const Id a = role_entity(e, "attacker");
                const Id b = role_entity(e, "defender");
                const auto key = std::make_pair(std::min(a, b), std::max(a, b));
                const auto it = declared.find(key);
                if (it == declared.end() || e.row.year > it->second) {
                    declared[key] = e.row.year;
                }
            } else if (kinds_.is_claim(e.row.kind)) {
                last_claim[{role_entity(e, "claimant"), role_entity(e, "holder")}] = e.row.id;  // ascending
            }
        }
        // r19 25.9: the claims a claim_dropped names as a cause, and each
        // settlement's refuge_hidden end year (the smallest id first).
        std::vector<std::uint8_t> dropped(events_.size() + 1, 0);
        d_.refuge_hidden_end.assign(t_.settlements.size(), -1);
        for (const HistoryView::Event& e : events_) {
            if (e.row.kind == kinds_.claim_dropped) {
                for (const Id c : e.causes) {
                    if (c >= 1 && static_cast<std::size_t>(c) <= events_.size()) {
                        dropped[static_cast<std::size_t>(c)] = 1;
                    }
                }
            } else if (e.row.kind == kinds_.refuge_hidden && e.row.settlement >= 1 &&
                       static_cast<std::size_t>(e.row.settlement) <= t_.settlements.size()) {
                std::int64_t& end = d_.refuge_hidden_end[static_cast<std::size_t>(e.row.settlement - 1)];
                if (end < 0) {
                    end = e.row.end_year;
                }
            }
        }
        const auto alive = [this](Id c) { return is_civ(c) && !t_.civilizations[civ_index(c)].ended_year; };
        for (const HistoryView::Event& e : events_) {
            if (!kinds_.is_claim(e.row.kind)) {
                continue;
            }
            bool pursued = false;
            for (const Id x : effects_[static_cast<std::size_t>(e.row.id)]) {
                pursued = pursued || kind(x) == kinds_.war_declared;
            }
            const Id a = role_entity(e, "claimant");
            const Id b = role_entity(e, "holder");
            if (pursued || !alive(a) || !alive(b)) {
                continue;
            }
            const auto w = declared.find(std::make_pair(std::min(a, b), std::max(a, b)));
            if (w != declared.end() && w->second >= e.row.year) {
                continue;
            }
            // r19 25.10: find; a missing key reads as not the pair's last claim.
            const auto lc = last_claim.find({a, b});
            if (lc == last_claim.end() || lc->second != e.row.id) {
                continue;
            }
            // r19 25.9 item 2, test (iv): no claim_dropped has the claim as a cause.
            if (dropped[static_cast<std::size_t>(e.row.id)] != 0) {
                continue;
            }
            d_.standing_claims.push_back(e.row.id);
        }
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

    // --- The summary set (24.2, 24.3) ----------------------------------

    // U(e) of 24.2 (may hold an event twice; callers add each once).
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
        // Part 2: 21.2's companions (21.8, 23 item 2).
        if (k == kinds_.war_declared) {
            if (const Id u = war_ending(e); u != 0) {
                out.push_back(u);
                add_effects(u, ceded);
            }
        } else if (k == kinds_.peace || k == kinds_.war_won || k == kinds_.truce) {
            add_effects(e, ceded);
        } else if (kinds_.fall(k)) {
            add_effects(e, [this](Id x) { return kinds_.consequence(x); });
        }
        // P5-04 10.3: the calls to arms and the betrayal of a declaration,
        // the shares of the spoils of a war's ending, the declaration of a
        // betrayal.
        if (k == kinds_.war_declared) {
            add_effects(e, [this](Id x) {
                return x == kinds_.ally_joined_war || x == kinds_.ally_refused || x == kinds_.alliance_betrayed;
            });
        } else if (k == kinds_.peace || k == kinds_.war_won || k == kinds_.civ_conquered) {
            add_effects(e, [this](Id x) { return x == kinds_.spoils_shared || x == kinds_.spoils_withheld; });
        } else if (kinds_.alliance_betrayed != 0 && k == kinds_.alliance_betrayed) {
            for (const Id c : events_[static_cast<std::size_t>(e - 1)].causes) {
                if (kind(c) == kinds_.war_declared) {
                    out.push_back(c);
                }
            }
        }
        // r21 26.5 item 1: every renaming e caused.
        add_effects(e, [this](Id x) { return x == kinds_.region_renamed; });
        // P5-06 10.3: a disputed succession is a companion of the crisis and
        // of the secession it cites, and brings its exile (reading: the
        // exile is named a companion of the dispute, which is itself only a
        // companion, so it is added here, one step further).
        if (k == kinds_.succession_crisis || k == kinds_.civ_seceded) {
            for (const Id x : effects_[static_cast<std::size_t>(e)]) {
                if (kind(x) != kinds_.succession_disputed) {
                    continue;
                }
                out.push_back(x);
                add_effects(x, [this](Id y) { return y == kinds_.exiled; });
            }
        } else if (k == kinds_.succession_disputed) {
            add_effects(e, [this](Id y) { return y == kinds_.exiled; });
        }
        // P5-03 11.3: a flight for fear brings the abandonment it caused.
        if (k == kinds_.settlement_fled) {
            add_effects(e, [this](Id x) { return x == kinds_.settlement_abandoned || x == kinds_.settlement_abandoned_to; });
        }
        // Part 3: the war frame.
        if (const Id d = war_of_[static_cast<std::size_t>(e)]; d != 0) {
            append_frame(out, d);
        }
        // Part 4: the cause of a consequence.
        if (kinds_.consequence(k)) {
            for (const Id c : events_[static_cast<std::size_t>(e - 1)].causes) {  // ascending
                if (kinds_.fall(kind(c))) {
                    out.push_back(c);
                    break;
                }
            }
        }
        // r21 26.5 item 1: the cause of a renaming (it has one, P5-12 8 item 5).
        if (k == kinds_.region_renamed) {
            for (const Id c : events_[static_cast<std::size_t>(e - 1)].causes) {
                out.push_back(c);
            }
        }
        return out;
    }

    // 24.3 item 1.
    [[nodiscard]] bool candidate(const HistoryView::Event& e) const {
        const Id k = e.row.kind;
        if (e.row.salience < SUMMARY_MIN_SALIENCE || kinds_.is_claim(k) || kinds_.battle_kind(k) ||
            k == kinds_.plague_spread || k == kinds_.quiet_stretch) {
            return false;
        }
        // P5-04 8.3 (reading): a call to arms is in the summary as a
        // companion of its war only.
        if (k == kinds_.ally_joined_war || k == kinds_.ally_refused) {
            return false;
        }
        // P5-06 8.2, 10.3 (reading): a disputed succession and an exile are
        // in the summary as companions only.
        if (k == kinds_.succession_disputed || k == kinds_.exiled) {
            return false;
        }
        if (k == kinds_.settlement_mixed &&
            !std::binary_search(mixed_told_.begin(), mixed_told_.end(), e.row.id)) {
            return false;  // P5-03a 9.5
        }
        return !(k == kinds_.war_declared && excluded_declaration(e.row.id));
    }

    // involves(k, e) of 24.3 item 3.
    [[nodiscard]] bool involves(const db::AgeRow& age, const HistoryView::Event& e) const {
        for (const db::EventParticipantRow& p : e.participants) {
            if (p.entity_type == static_cast<std::int64_t>(EntityType::civilization) && is_civ(p.entity_id) &&
                t_.civilizations[civ_index(p.entity_id)].race == age.race) {
                return true;
            }
            if (p.entity_type == static_cast<std::int64_t>(EntityType::race) && p.entity_id == age.race) {
                return true;
            }
        }
        return false;
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

    [[nodiscard]] std::int64_t salience(Id e) const { return events_[static_cast<std::size_t>(e - 1)].row.salience; }

    // P5-03a 9.5: the first settlement_mixed of each unordered pair {race,
    // ruling}, the first SUMMARY_MIXED_MAX of them by id.
    void mixed_candidates() {
        mixed_told_.clear();
        std::vector<std::pair<Id, Id>> pairs;
        for (const HistoryView::Event& e : events_) {
            if (e.row.kind != kinds_.settlement_mixed || mixed_told_.size() >= SUMMARY_MIXED_MAX) {
                continue;
            }
            Id race = 0;
            Id ruling = 0;
            const KindDef* k = view_.catalogue().find_id(e.row.kind);
            for (const db::EventParticipantRow& p : e.participants) {
                const std::string& role = k->roles[static_cast<std::size_t>(p.role - 1)].role;  // checked by load
                race = role == "race" ? p.entity_id : race;
                ruling = role == "ruling" ? p.entity_id : ruling;
            }
            const std::pair<Id, Id> key{std::min(race, ruling), std::max(race, ruling)};
            if (std::find(pairs.begin(), pairs.end(), key) != pairs.end()) {
                continue;
            }
            pairs.push_back(key);
            mixed_told_.push_back(e.row.id);
        }
    }

    void summary() {
        const std::size_t n = events_.size();
        mixed_candidates();
        std::vector<Id> ranked;
        for (const HistoryView::Event& e : events_) {
            if (candidate(e)) {
                ranked.push_back(e.row.id);
            }
        }
        std::stable_sort(ranked.begin(), ranked.end(), [this](Id a, Id b) {
            return salience(a) != salience(b) ? salience(a) > salience(b) : a < b;
        });
        // P5-06 25.2: G, the major calamities among the candidates.
        major_calamities(ranked);
        std::vector<std::uint8_t> is_major(n + 1, 0);
        for (const Id g : d_.summary_major) {
            is_major[static_cast<std::size_t>(g)] = 1;
        }
        // r19 25.2: per phase only the first SUMMARY_CALAMITY_PER_PHASE
        // candidates of the calamity kinds (together), in rank order, stay
        // candidates; P5-06 25.2 rule 2: the members of G of the phase stay
        // and take its slots first. The phase ids were checked
        // (reads_disagree).
        {
            const auto phase_index = [this](const HistoryView::Event& ev) -> std::size_t {
                return ev.row.phase >= 1 && static_cast<std::size_t>(ev.row.phase) <= t_.phases.size()
                           ? static_cast<std::size_t>(ev.row.phase)
                           : NONE;
            };
            std::vector<std::size_t> majors(t_.phases.size() + 1, 0);
            for (const Id g : d_.summary_major) {
                if (const std::size_t p = phase_index(events_[static_cast<std::size_t>(g - 1)]); p != NONE) {
                    ++majors[p];
                }
            }
            std::vector<std::size_t> per_phase(t_.phases.size() + 1, 0);
            std::vector<Id> kept;
            kept.reserve(ranked.size());
            for (const Id e : ranked) {
                const HistoryView::Event& ev = events_[static_cast<std::size_t>(e - 1)];
                const std::size_t p = phase_index(ev);
                if (kinds_.calamity(ev.row.kind) && p != NONE && is_major[static_cast<std::size_t>(e)] == 0) {
                    const std::size_t room =
                        SUMMARY_CALAMITY_PER_PHASE > majors[p] ? SUMMARY_CALAMITY_PER_PHASE - majors[p] : 0;
                    std::size_t& count = per_phase[p];
                    if (count >= room) {
                        continue;
                    }
                    ++count;
                }
                kept.push_back(e);
            }
            ranked = std::move(kept);
        }
        std::vector<std::size_t> event_age(n + 1, NONE);
        for (const HistoryView::Event& e : events_) {
            event_age[static_cast<std::size_t>(e.row.id)] = age_of(e.row.year);
        }
        // Items 2 and 3: the floor starts as F, every quiet stretch.
        std::vector<std::uint8_t> floor(n + 1, 0);
        std::vector<std::int64_t> in_age(t_.ages.size(), 0);
        std::int64_t floor_size = 0;
        const auto add = [&](Id u) {
            if (floor[static_cast<std::size_t>(u)] == 0) {
                floor[static_cast<std::size_t>(u)] = 1;
                ++floor_size;
                const std::size_t ua = event_age[static_cast<std::size_t>(u)];
                if (ua != NONE) {
                    ++in_age[ua];
                }
            }
        };
        // r19 25.6: F also holds every flare_up whose phase has a war_declared.
        std::vector<std::uint8_t> phase_war(t_.phases.size() + 1, 0);
        for (const HistoryView::Event& e : events_) {
            if (e.row.kind == kinds_.war_declared && e.row.phase >= 1 &&
                static_cast<std::size_t>(e.row.phase) <= t_.phases.size()) {
                phase_war[static_cast<std::size_t>(e.row.phase)] = 1;
            }
        }
        for (const HistoryView::Event& e : events_) {
            const bool war_flare = e.row.kind == kinds_.flare_up && e.row.phase >= 1 &&
                                   static_cast<std::size_t>(e.row.phase) <= t_.phases.size() &&
                                   phase_war[static_cast<std::size_t>(e.row.phase)] != 0;
            if (e.row.kind == kinds_.quiet_stretch || war_flare) {
                add(e.row.id);
            }
        }
        // P5-03 10.1: the floor starts as F with the stories of the told
        // monsters, each with its unit.
        for (const Id e : stories_) {
            for (const Id u : unit(e)) {
                add(u);
            }
        }
        // P5-06 25.2 rule 1: and the units of G's members.
        for (const Id e : d_.summary_major) {
            for (const Id u : unit(e)) {
                add(u);
            }
        }
        std::vector<std::vector<Id>> by_age(t_.ages.size());
        for (const Id e : ranked) {  // rank order
            const std::size_t a = event_age[static_cast<std::size_t>(e)];
            if (a != NONE) {
                by_age[a].push_back(e);
            }
        }
        for (std::size_t a = 0; a < t_.ages.size(); ++a) {
            std::vector<Id>& walk = by_age[a];
            // (involves descending, then rank): a stable partition keeps the rank order.
            std::stable_sort(walk.begin(), walk.end(), [&](Id x, Id y) {
                return involves(t_.ages[a], events_[static_cast<std::size_t>(x - 1)]) &&
                       !involves(t_.ages[a], events_[static_cast<std::size_t>(y - 1)]);
            });
            for (const Id e : walk) {
                if (in_age[a] >= SUMMARY_AGE_FLOOR) {
                    break;
                }
                if (floor[static_cast<std::size_t>(e)] != 0) {
                    continue;
                }
                for (const Id u : unit(e)) {
                    add(u);
                }
            }
        }
        // Item 4: the threshold. entered[u] = the highest salience of a
        // candidate whose unit holds u.
        std::vector<std::int64_t> entered(n + 1, -1);
        for (const Id e : ranked) {
            const std::int64_t s = salience(e);
            for (const Id u : unit(e)) {
                entered[static_cast<std::size_t>(u)] = std::max(entered[static_cast<std::size_t>(u)], s);
            }
        }
        std::array<std::int64_t, 102>& at_least = d_.summary_at_least;  // non-floor events with entered >= t
        at_least.fill(0);
        for (std::size_t i = 1; i <= n; ++i) {
            if (floor[i] == 0 && entered[i] >= SUMMARY_MIN_SALIENCE) {
                ++at_least[static_cast<std::size_t>(std::min<std::int64_t>(entered[i], 100))];
            }
        }
        for (std::size_t t = 100; t-- > 0;) {
            at_least[t] += at_least[t + 1];
        }
        d_.summary_floor.assign(n, 0);
        d_.summary_entered.assign(n, -1);
        for (std::size_t i = 1; i <= n; ++i) {
            d_.summary_floor[i - 1] = floor[i] != 0 ? 1 : 0;
            d_.summary_entered[i - 1] = entered[i];
        }
        d_.summary_floor_size = floor_size;
        set_summary(d_, SUMMARY_BUDGET);
    }

    // P5-06 25.2: G = the first SUMMARY_CALAMITY_MAJOR_MAX major famines,
    // plagues and plague outbreaks among the candidates `candidates`, by
    // (ended descending, impact descending, id ascending).
    void major_calamities(const std::vector<Id>& candidates) {
        d_.summary_major.clear();
        const std::size_t n = events_.size();
        // ended(e): e is a cause of the ruin event of a settlement with status 3.
        std::vector<std::uint8_t> ended(n + 1, 0);
        for (const db::SettlementRow& s : t_.settlements) {
            if (s.status != 3 || s.ruin_event < 1 || static_cast<std::size_t>(s.ruin_event) > n) {
                continue;
            }
            for (const Id c : events_[static_cast<std::size_t>(s.ruin_event - 1)].causes) {
                if (c >= 1 && static_cast<std::size_t>(c) <= n) {
                    ended[static_cast<std::size_t>(c)] = 1;
                }
            }
        }
        // The census rows of settlement index i are census[first[i]] ..
        // census[first[i + 1] - 1], ascending year ((settlement, year) order).
        const std::size_t ns = t_.settlements.size();
        std::vector<std::size_t> first(ns + 2, 0);
        for (const db::SettlementCensusRow& r : t_.census) {
            if (r.settlement >= 1 && static_cast<std::size_t>(r.settlement) <= ns) {
                ++first[static_cast<std::size_t>(r.settlement) + 1];
            }
        }
        for (std::size_t i = 1; i < first.size(); ++i) {
            first[i] += first[i - 1];
        }
        const bool census_in_order = first[ns + 1] == t_.census.size();
        struct Major {
            Id event = 0;
            std::uint8_t ended = 0;
            std::int64_t impact = 0;
        };
        std::vector<Major> majors;
        for (const Id e : candidates) {
            const HistoryView::Event& ev = events_[static_cast<std::size_t>(e - 1)];
            const Id k = ev.row.kind;
            if (k != kinds_.famine && k != kinds_.plague && k != kinds_.plague_outbreak) {
                continue;
            }
            Id civ = 0;
            for (const db::EventParticipantRow& p : ev.participants) {
                if (p.entity_type == static_cast<std::int64_t>(EntityType::civilization)) {
                    civ = p.entity_id;
                    break;
                }
            }
            // P(e): the realm's people in its standing settlements before the
            // blow (each settlement's census row of the largest year < year(e)).
            std::int64_t people = 0;
            if (civ != 0 && census_in_order) {
                for (std::size_t i = 0; i < ns; ++i) {
                    const auto lo = t_.census.begin() + static_cast<std::ptrdiff_t>(first[i + 1]);
                    const auto hi = t_.census.begin() + static_cast<std::ptrdiff_t>(first[i + 2]);
                    const auto at = std::lower_bound(lo, hi, ev.row.year,
                                                     [](const db::SettlementCensusRow& r, std::int64_t y) {
                                                         return r.year < y;
                                                     });
                    if (at == lo) {
                        continue;
                    }
                    const db::SettlementCensusRow& r = *(at - 1);
                    if (r.owner_civ == civ && standing(r.status)) {
                        const std::int64_t v = std::max<std::int64_t>(0, r.population);
                        people = people > std::numeric_limits<std::int64_t>::max() - v
                                     ? std::numeric_limits<std::int64_t>::max()
                                     : people + v;
                    }
                }
            }
            const std::int64_t dead = value_of(ev, k == kinds_.plague_outbreak ? "total" : "dead");
            const std::int64_t impact = people > 0 ? permille(dead, people) : 0;
            const std::uint8_t end = ended[static_cast<std::size_t>(e)];
            if (end != 0 || impact >= SUMMARY_CALAMITY_MAJOR_PERMILLE) {
                majors.push_back(Major{e, end, impact});
            }
        }
        std::stable_sort(majors.begin(), majors.end(), [](const Major& a, const Major& b) {
            if (a.ended != b.ended) return a.ended > b.ended;
            if (a.impact != b.impact) return a.impact > b.impact;
            return a.event < b.event;
        });
        for (std::size_t i = 0; i < majors.size() && i < SUMMARY_CALAMITY_MAJOR_MAX; ++i) {
            d_.summary_major.push_back(majors[i].event);
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
        renaming_stats();
        monster_stats();
        relations_stats();  // P5-03 17.4
        alliance_stats();   // P5-04 10.5
        figure_stats();     // P5-06 10.5
        followup_stats();   // P5-04a 12.6
    }

    // P5-04a 12.6: appended at the end, in this order (full only).
    void followup_stats() {
        const Id formed = kind_id("alliance_formed"), renewed = kind_id("alliance_renewed");
        const Id withheld = kind_id("spoils_withheld"), spurned = kind_id("alliance_spurned");
        const Id refused = kind_id("ally_refused"), betrayed = kind_id("alliance_betrayed");
        const Id turned = kind_id("disposition_turned"), arms = kind_id("arms_sold");
        std::int64_t renewals = 0;
        for (const db::RelationRow& r : t_.relations) {
            if (r.kind != "alliance" || r.since_event < 1 || static_cast<std::size_t>(r.since_event) > events_.size() ||
                kind(r.since_event) != formed) {
                continue;
            }
            const std::int64_t term = value_of(events_[static_cast<std::size_t>(r.since_event - 1)], "term");
            const std::int64_t end = r.until ? *r.until : t_.end_year;
            if (term >= 1 && end - 1 >= r.since) {
                renewals += (end - 1 - r.since) / term;
            }
        }
        std::int64_t written = 0, soured = 0, at_once = 0, excused = 0, thawed = 0, with_arms = 0;
        std::array<std::int64_t, 8> refusal{};
        std::array<std::int64_t, 4> betrayal{};
        for (const HistoryView::Event& e : events_) {
            const Id k = e.row.kind;
            written += k == renewed ? 1 : 0;
            soured += (k == withheld && value_of(e, "grievance") == 1) ? 1 : 0;
            at_once += (k == spurned && value_of(e, "reason") == 2) ? 1 : 0;
            thawed += (k == turned && value_of(e, "change") == 1) ? 1 : 0;
            if (k == refused) {
                excused += value_of(e, "count") == 0 ? 1 : 0;
                const std::int64_t m = value_of(e, "motive");
                if (m >= 0 && m <= 7) {
                    ++refusal[static_cast<std::size_t>(m)];
                }
            } else if (k == betrayed) {
                const std::int64_t m = value_of(e, "motive");
                if (m >= 0 && m <= 3) {
                    ++betrayal[static_cast<std::size_t>(m)];
                }
            } else if (kinds_.battle_kind(k)) {
                bool cites = false;
                for (const Id c : e.causes) {
                    cites = cites || kind(c) == arms;
                }
                with_arms += cites ? 1 : 0;
            }
        }
        put("alliances_renewals", renewals);
        put("alliances_renewals_written", written);
        put("alliances_soured", soured);
        put("alliances_spurned_at_once", at_once);
        put("allies_refused_excused", excused);
        for (std::size_t m = 0; m < refusal.size(); ++m) {
            put("refusal_motive_" + std::string(1, static_cast<char>('0' + m)), refusal[m]);
        }
        for (std::size_t m = 0; m < betrayal.size(); ++m) {
            put("betrayal_motive_" + std::string(1, static_cast<char>('0' + m)), betrayal[m]);
        }
        put("battles_with_arms", with_arms);
        put("dispositions_thawed", thawed);
    }

    // --- Figures (P5-06 10.2) ---------------------------------------------

    [[nodiscard]] FigureDigest* figure_slot(Id f) {
        return f >= 1 && static_cast<std::size_t>(f) <= d_.figures.size() ? &d_.figures[static_cast<std::size_t>(f - 1)]
                                                                         : nullptr;
    }

    void figures() {
        const std::span<const db::FigureRow> rows = view_.figure_rows();
        d_.figures.assign(rows.size(), {});
        for (std::size_t i = 0; i < rows.size(); ++i) {
            d_.figures[i].id = static_cast<Id>(i) + 1;  // ids 1, 2, ... (P5-06 9.4 F1)
        }
        const std::vector<std::vector<Id>> lists = figure_event_lists(view_);
        for (std::size_t i = 0; i < lists.size() && i < d_.figures.size(); ++i) {
            for (const Id e : lists[i]) {
                d_.figures[i].renown += salience(e) >= SUMMARY_MIN_SALIENCE ? 1 : 0;
                if (kind(e) == kinds_.war_declared && rows[i].role == 1) {
                    ++d_.figures[i].wars_declared;
                }
            }
        }
        const Id slain_by_hero = kind_id("monster_slain_by_hero"), hero_slain = kind_id("hero_slain");
        const Id blight = kind_id("blight");
        for (const HistoryView::Event& e : events_) {
            const Id k = e.row.kind;
            // A war won by a realm, or a realm conquered, at an ending
            // where the figure was its recorded ruler.
            Id winner = 0;
            if (k == kinds_.peace || k == kinds_.war_won) {
                winner = role_entity(e, "winner");
            } else if (k == kinds_.civ_conquered) {
                winner = role_entity(e, "conqueror");
            }
            if (winner != 0) {
                if (FigureDigest* f = figure_slot(view_.ruler_at_event(winner, e.row.id))) {
                    ++f->wars_won;
                    f->realms_conquered += k == kinds_.civ_conquered ? 1 : 0;
                }
            }
            if (k == kinds_.battle_led) {
                if (FigureDigest* f = figure_slot(role_entity(e, "winner_general"))) {
                    ++f->battles_led;
                    ++f->battles_won;
                }
                if (FigureDigest* f = figure_slot(role_entity(e, "loser_general"))) {
                    ++f->battles_led;
                }
            } else if (k == slain_by_hero) {
                if (FigureDigest* f = figure_slot(role_entity(e, "hero"))) {
                    f->slew.push_back(role_entity(e, "monster"));
                }
            } else if (k == hero_slain) {
                if (FigureDigest* f = figure_slot(role_entity(e, "figure")); f != nullptr && f->slain_by == 0) {
                    f->slain_by = role_entity(e, "monster");
                }
            } else if (k == blight) {
                if (FigureDigest* f = figure_slot(role_entity(e, "figure"))) {
                    f->blight = true;
                }
            } else if (k == kinds_.exiled) {
                if (FigureDigest* f = figure_slot(role_entity(e, "figure")); f != nullptr && f->host == 0) {
                    f->host = role_entity(e, "host");
                }
            }
        }
        for (const db::BookRow& b : view_.book_rows()) {
            if (FigureDigest* f = figure_slot(b.author)) {
                ++f->books;
            }
        }
    }

    // P5-06 10.5: appended at the end, in this order (full only).
    void figure_stats() {
        const std::span<const db::FigureRow> rows = view_.figure_rows();
        std::array<std::int64_t, 6> by_role{};
        std::int64_t alive = 0, epithet = 0, by_deed = 0, regnal = 0;
        std::vector<std::string> epithets;
        std::vector<std::int64_t> ages;
        std::vector<std::int64_t> last_reign(t_.civilizations.size(), 0);
        const Id alliance_betrayed = kinds_.alliance_betrayed, spoils_shared = kinds_.spoils_shared;
        const Id spoils_withheld = kinds_.spoils_withheld;
        for (const db::FigureRow& f : rows) {
            if (f.role >= 1 && f.role <= 5) {
                ++by_role[static_cast<std::size_t>(f.role)];
            }
            alive += (f.status == 1 || f.status == 2) ? 1 : 0;
            if (!f.epithet.empty()) {
                ++epithet;
                epithets.push_back(f.epithet);
            }
            if (f.died) {
                ages.push_back(*f.died - f.born);
            }
            if (f.role != 1) {
                continue;
            }
            regnal += f.regnal >= 2 ? 1 : 0;
            if (f.start_event >= 1 && static_cast<std::size_t>(f.start_event) <= events_.size()) {
                const Id k = kind(f.start_event);  // 6.4 rule 3: a deed of the realm
                by_deed += (k == kinds_.war_declared || k == alliance_betrayed || k == spoils_shared ||
                            k == spoils_withheld)
                               ? 1
                               : 0;
            }
            if (is_civ(f.civ)) {
                std::int64_t& r = last_reign[civ_index(f.civ)];
                r = std::max(r, f.reign_no);
            }
        }
        std::stable_sort(epithets.begin(), epithets.end());
        std::int64_t bearers_max = 0;
        for (std::size_t i = 0; i < epithets.size();) {
            std::size_t j = i;
            while (j < epithets.size() && epithets[j] == epithets[i]) {
                ++j;
            }
            bearers_max = std::max(bearers_max, static_cast<std::int64_t>(j - i));
            i = j;
        }
        std::int64_t reigns = 0;
        for (const std::int64_t r : last_reign) {
            reigns += r;
        }
        std::stable_sort(ages.begin(), ages.end());
        std::int64_t towers = 0;
        for (const db::SiteRow& x : t_.sites) {
            towers += x.kind == 4 ? 1 : 0;
        }
        const auto count_kind = [this](std::string_view key) {
            const Id k = kind_id(key);
            std::int64_t n = 0;
            for (const HistoryView::Event& e : events_) {
                n += e.row.kind == k ? 1 : 0;
            }
            return n;
        };
        put("figures", static_cast<std::int64_t>(rows.size()));
        put("figures_rulers", by_role[1]);
        put("figures_claimants", by_role[2]);
        put("figures_generals", by_role[3]);
        put("figures_heroes", by_role[4]);
        put("figures_casters", by_role[5]);
        put("figures_alive", alive);
        put("figures_epithet", epithet);
        put("epithet_bearers_max", bearers_max);
        put("reigns", reigns);
        put("rulers_recorded", by_role[1]);
        put("rulers_by_deed", by_deed);
        put("rulers_regnal", regnal);
        put("successions_disputed", count_kind("succession_disputed"));
        put("exiles", count_kind("exiled"));
        put("generals_appointed", count_kind("general_appointed"));
        put("battles_led", count_kind("battle_led"));
        put("generals_fallen", count_kind("fell_in_battle"));
        put("heroes_risen", count_kind("hero_rose"));
        put("monsters_slain_by_heroes", count_kind("monster_slain_by_hero"));
        put("heroes_slain", count_kind("hero_slain"));
        put("towers", towers);
        put("books", static_cast<std::int64_t>(view_.book_rows().size()));
        put("blights", count_kind("blight"));
        put("figure_age_median", ages.empty() ? STAT_NONE : ages[(ages.size() - 1) / 2]);
        put("summary_major_calamities", static_cast<std::int64_t>(d_.summary_major.size()));
    }

    // P5-04 10.5: appended at the end, in this order (full only).
    void alliance_stats() {
        const auto count_kind = [this](std::string_view key) {
            const KindDef* k = view_.catalogue().find(key);
            std::int64_t n = 0;
            if (k != nullptr) {
                for (const HistoryView::Event& e : events_) {
                    n += e.row.kind == k->id ? 1 : 0;
                }
            }
            return n;
        };
        const auto kind_id = [this](std::string_view key) -> Id {
            const KindDef* k = view_.catalogue().find(key);
            return k != nullptr ? k->id : -1;
        };
        const Id formed = kind_id("alliance_formed"), lapsed = kind_id("alliance_lapsed");
        const Id broken = kind_id("alliance_broken"), betrayed = kind_id("alliance_betrayed");
        const Id spurned = kind_id("alliance_spurned"), joined = kind_id("ally_joined_war");
        const Id shared = kind_id("spoils_shared"), withheld = kind_id("spoils_withheld");
        std::int64_t standing_n = 0, cross = 0, origin = 0, with_realm = 0;
        std::vector<std::int64_t> lengths;
        for (const db::RelationRow& r : t_.relations) {
            if (r.kind != "alliance") {
                continue;
            }
            standing_n += r.until ? 0 : 1;
            const std::size_t ra = race_of(r.a_id);
            const std::size_t rb = race_of(r.b_id);
            cross += ra != rb ? 1 : 0;
            if (r.until) {
                lengths.push_back(*r.until - r.since);
                const Id k = kind(r.until_event);
                with_realm += (k != lapsed && k != broken && k != betrayed && k != spurned) ? 1 : 0;
            }
        }
        std::int64_t defence = 0, people = 0;
        std::vector<Id> with_ally;
        for (const HistoryView::Event& e : events_) {
            if (e.row.kind == formed && !e.values.empty() && e.values[0] == 0) {
                ++origin;
            }
            if (e.row.kind == joined) {
                defence += (!e.values.empty() && e.values[0] == 0) ? 1 : 0;
                for (const Id c : e.causes) {
                    if (kind(c) == kinds_.war_declared) {
                        with_ally.push_back(c);
                    }
                }
            }
            if ((e.row.kind == shared || e.row.kind == withheld) && !e.values.empty()) {
                people += e.values[0];
            }
        }
        std::stable_sort(with_ally.begin(), with_ally.end());
        with_ally.erase(std::unique(with_ally.begin(), with_ally.end()), with_ally.end());
        std::stable_sort(lengths.begin(), lengths.end());
        put("alliances_formed", count_kind("alliance_formed"));
        put("alliances_standing", standing_n);
        put("alliances_cross_race", cross);
        put("alliances_origin", origin);
        put("alliances_lapsed", count_kind("alliance_lapsed"));
        put("alliances_broken", count_kind("alliance_broken"));
        put("alliances_betrayed", count_kind("alliance_betrayed"));
        put("alliances_ended_with_realm", with_realm);
        put("alliances_years_median", lengths.empty() ? STAT_NONE : lengths[(lengths.size() - 1) / 2]);
        put("allies_joined", count_kind("ally_joined_war"));
        put("allies_joined_defence", defence);
        put("allies_refused", count_kind("ally_refused"));
        put("allies_withdrew", count_kind("ally_withdrew"));
        put("wars_with_ally", static_cast<std::int64_t>(with_ally.size()));
        // P5-06 11.1: and every battle_led with an ally participant.
        std::int64_t led_allied = 0;
        for (const HistoryView::Event& e : events_) {
            if (e.row.kind == kinds_.battle_led &&
                (role_entity(e, "winner_ally") != 0 || role_entity(e, "loser_ally") != 0)) {
                ++led_allied;
            }
        }
        put("allied_battles", count_kind("allied_battle") + led_allied);
        blocs_by_year();
        put("bloc_share_max_permille", d_.bloc_any ? d_.bloc_max : 0);
        put("bloc_over_800_years", bloc_over_800_);
        std::int64_t trade_standing = 0;
        for (const db::RelationRow& r : t_.relations) {
            trade_standing += (r.kind == "trade" && !r.until) ? 1 : 0;
        }
        put("trade_opened", count_kind("trade_opened"));
        put("trade_standing", trade_standing);
        put("trade_broken", count_kind("trade_broken"));
        put("alliances_spurned", count_kind("alliance_spurned"));
        const std::int64_t granted = count_kind("spoils_shared");
        const std::int64_t owed = granted + count_kind("spoils_withheld");
        put("spoils_owed", owed);
        put("spoils_granted", granted);
        put("spoils_withheld", owed - granted);
        put("spoils_granted_permille", owed == 0 ? STAT_NONE : permille(granted, owed));
        put("spoils_people_owed", people);
        std::int64_t mines_open = 0;
        std::vector<Id> sellers;  // open mines at E - 1 (an open site row)
        for (const db::SiteRow& x : t_.sites) {
            if (x.kind == 3 && !x.ended_year) {
                ++mines_open;
                sellers.push_back(x.founder_civ);
            }
        }
        std::stable_sort(sellers.begin(), sellers.end());
        const auto seller = [&sellers](Id c) { return std::binary_search(sellers.begin(), sellers.end(), c); };
        std::vector<Id> buyers;
        for (const db::RelationRow& r : t_.relations) {
            if (r.kind != "trade" || r.until) {
                continue;
            }
            if (seller(r.a_id) && !seller(r.b_id)) {
                buyers.push_back(r.b_id);
            } else if (seller(r.b_id) && !seller(r.a_id)) {
                buyers.push_back(r.a_id);
            }
        }
        std::stable_sort(buyers.begin(), buyers.end());
        buyers.erase(std::unique(buyers.begin(), buyers.end()), buyers.end());
        put("mines_opened", count_kind("mine_opened"));
        put("mines_open", mines_open);
        put("arms_sold", count_kind("arms_sold"));
        put("arms_buyers_at_end", static_cast<std::int64_t>(buyers.size()));
    }

    // P5-04 10.2, 10.5: the bloc shares by year, from the region rows
    // (non-islet) and the alliance rows in force (since <= Y < until).
    void blocs_by_year() {
        std::vector<const db::RelationRow*> rows;
        for (const db::RelationRow& r : t_.relations) {
            if (r.kind == "alliance") {
                rows.push_back(&r);
            }
        }
        d_.bloc_any = !rows.empty();
        if (rows.empty() || nonislet_ <= 0 || e_ <= 0) {
            return;
        }
        const std::size_t span = static_cast<std::size_t>(e_);
        const std::size_t nc = t_.civilizations.size();
        // held[c][y]: non-islet regions held, by a difference array.
        std::vector<std::vector<std::int64_t>> held(nc);
        for (std::size_t k = 0; k < nc; ++k) {
            if (control_of_[k].empty()) {
                continue;
            }
            std::vector<std::int64_t>& h = held[k];
            h.assign(span + 1, 0);
            for (const std::size_t i : control_of_[k]) {
                const db::RegionControlRow& r = t_.region_control[i];
                const RegionInfo* g = region(r.region);
                if (g == nullptr || islet(*g)) {
                    continue;
                }
                ++h[static_cast<std::size_t>(r.from_year)];
                --h[static_cast<std::size_t>(r.to_year.value_or(e_))];
            }
            for (std::size_t y = 1; y <= span; ++y) {
                h[y] += h[y - 1];
            }
        }
        const auto share = [&](Id c, std::size_t y) -> std::int64_t {
            if (!is_civ(c) || held[civ_index(c)].empty()) {
                return 0;
            }
            return held[civ_index(c)][y] * 1000 / nonislet_;
        };
        std::int64_t run = 0;
        for (std::size_t y = 0; y < span; ++y) {
            const auto year = static_cast<std::int64_t>(y);
            std::map<Id, std::vector<Id>> allies;
            for (const db::RelationRow* r : rows) {
                if (r->since <= year && (!r->until || year < *r->until)) {
                    allies[r->a_id].push_back(r->b_id);
                    allies[r->b_id].push_back(r->a_id);
                }
            }
            bool over = false;
            for (auto& [c, list] : allies) {
                std::stable_sort(list.begin(), list.end());
                std::int64_t bs = share(c, y);
                for (const Id z : list) {
                    bs += share(z, y);
                }
                bs = std::min<std::int64_t>(1000, bs);
                over = over || bs > 800;
                if (bs > d_.bloc_max || (d_.bloc_max_civ == 0)) {
                    d_.bloc_max = bs;
                    d_.bloc_max_year = year;
                    d_.bloc_max_civ = c;
                    d_.bloc_max_allies = list;
                }
            }
            run = over ? run + 1 : 0;
            bloc_over_800_ = std::max(bloc_over_800_, run);
        }
    }

    // P5-02 12.6: appended at the end, in this order.
    void monster_stats() {
        put("beasts", static_cast<std::int64_t>(t_.beasts.size()));
        put("monster_kinds", static_cast<std::int64_t>(t_.monster_kinds.size()));
        put("beast_populations", static_cast<std::int64_t>(t_.beast_populations.size()));
        put("monsters", static_cast<std::int64_t>(t_.monsters.size()));
        put("sites", static_cast<std::int64_t>(t_.sites.size()));
        std::int64_t alive = 0, slain = 0, died = 0;
        for (const db::MonsterRow& m : t_.monsters) {
            alive += (m.status == 1 || m.status == 2) ? 1 : 0;
            slain += m.status == 3 ? 1 : 0;
            died += m.status == 4 ? 1 : 0;
        }
        std::int64_t raids = 0, killed = 0, overrun = 0, risen = 0, bold = 0;
        for (const HistoryView::Event& e : events_) {
            if (e.row.kind == kinds_.monster_raid) {
                ++raids;
                // Saturating: the digest does not fail on a damaged file.
                const std::int64_t v = std::max<std::int64_t>(0, value_of(e, "dead"));
                killed = killed > std::numeric_limits<std::int64_t>::max() - v ? std::numeric_limits<std::int64_t>::max()
                                                                                : killed + v;
            }
            overrun += e.row.kind == kinds_.settlement_overrun ? 1 : 0;
            risen += e.row.kind == kinds_.undead_rose ? 1 : 0;
            bold += e.row.kind == kinds_.beasts_bold ? 1 : 0;
        }
        std::int64_t outposts = 0, outposts_open = 0;
        std::vector<std::uint8_t> shunned(t_.regions.size() + 1, 0);
        for (const db::SiteRow& x : t_.sites) {
            outposts += x.kind == 2 ? 1 : 0;
            outposts_open += (x.kind == 2 && !x.ended_year) ? 1 : 0;
            if (x.kind == 1 && !x.ended_year && x.settlement != 0 && x.region >= 1 &&
                static_cast<std::size_t>(x.region) < shunned.size()) {
                shunned[static_cast<std::size_t>(x.region)] = 1;
            }
        }
        std::int64_t left = 0;
        for (const db::SettlementRow& x : t_.settlements) {
            left += (x.ending >= 3 && x.ending <= 6) ? 1 : 0;  // P5-03 11.3: and fled
        }
        std::int64_t regions_shunned = 0;
        for (const std::uint8_t b : shunned) {
            regions_shunned += b;
        }
        put("monsters_alive", alive);
        put("monsters_slain", slain);
        put("monsters_died", died);
        put("monster_raids", raids);
        put("monster_killed", killed);
        put("settlements_overrun", overrun);
        put("undead_risen", risen);
        put("outposts", outposts);
        put("outposts_open", outposts_open);
        put("beasts_bold", bold);
        put("settlements_left", left);
        put("regions_shunned", regions_shunned);
    }

    // P5-03 10.1: harm(m), the told monsters and their stories.
    void monster_stories() {
        const std::size_t nm = t_.monsters.size();
        d_.monster_harm.assign(nm, 0);
        std::vector<Id> worst(nm, 0);        // the raid of the largest dead, then smaller id
        std::vector<std::int64_t> worst_dead(nm, -1);
        std::vector<Id> overrun(nm, 0);      // the first overrun
        const auto add_harm = [](std::int64_t& h, std::int64_t v) {
            v = std::max<std::int64_t>(0, v);
            h = h > std::numeric_limits<std::int64_t>::max() - v ? std::numeric_limits<std::int64_t>::max() : h + v;
        };
        const auto monster_of = [this](const HistoryView::Event& e) -> std::size_t {
            for (const db::EventParticipantRow& p : e.participants) {
                if (p.entity_type == static_cast<std::int64_t>(EntityType::monster) && p.entity_id >= 1 &&
                    static_cast<std::size_t>(p.entity_id) <= t_.monsters.size()) {
                    return static_cast<std::size_t>(p.entity_id - 1);
                }
            }
            return NONE;
        };
        for (const HistoryView::Event& e : events_) {
            const Id k = e.row.kind;
            if (k != kinds_.monster_raid && k != kinds_.settlement_overrun && k != kinds_.settlement_fled) {
                continue;
            }
            const std::size_t m = monster_of(e);
            if (m == NONE) {
                continue;
            }
            if (k == kinds_.monster_raid) {
                const std::int64_t dead = value_of(e, "dead");
                add_harm(d_.monster_harm[m], dead);
                if (dead > worst_dead[m]) {
                    worst_dead[m] = dead;
                    worst[m] = e.row.id;
                }
            } else {
                add_harm(d_.monster_harm[m], SUMMARY_MONSTER_TAKEN_HARM);
                if (k == kinds_.settlement_overrun && overrun[m] == 0) {
                    overrun[m] = e.row.id;
                }
            }
        }
        std::vector<std::size_t> notable;
        for (std::size_t m = 0; m < nm; ++m) {
            if (d_.monster_harm[m] >= SUMMARY_MONSTER_HARM_MIN) {
                notable.push_back(m);
            }
        }
        std::stable_sort(notable.begin(), notable.end(), [this](std::size_t a, std::size_t b) {
            return d_.monster_harm[a] != d_.monster_harm[b] ? d_.monster_harm[a] > d_.monster_harm[b] : a < b;
        });
        if (notable.size() > SUMMARY_MONSTER_STORIES) {
            notable.resize(SUMMARY_MONSTER_STORIES);
        }
        d_.monster_stories = static_cast<std::int64_t>(notable.size());
        const auto n_events = static_cast<Id>(events_.size());
        for (const std::size_t m : notable) {
            const db::MonsterRow& row = t_.monsters[m];
            for (const Id e : {row.born_event, worst[m], overrun[m], row.death_event}) {
                if (e >= 1 && e <= n_events && std::find(stories_.begin(), stories_.end(), e) == stories_.end()) {
                    stories_.push_back(e);
                }
            }
        }
    }

    // P5-03 17.4: the race relations, mixed settlements and migrations.
    void relations_stats() {
        put("race_relations", static_cast<std::int64_t>(t_.race_relations.size()));
        put("settlement_races", static_cast<std::int64_t>(t_.settlement_races.size()));
        std::int64_t present = 0;
        for (const db::RaceRow& r : t_.races) {
            present += r.present != 0 ? 1 : 0;
        }
        std::int64_t met = 0, hatred = 0, friendship = 0;
        for (std::size_t i = 0; i < t_.race_relations.size(); ++i) {
            const db::RaceRelationRow& r = t_.race_relations[i];
            const bool last = i + 1 == t_.race_relations.size() || t_.race_relations[i + 1].race_a != r.race_a ||
                              t_.race_relations[i + 1].race_b != r.race_b;
            if (last) {
                ++met;
                hatred += r.band == 0 ? 1 : 0;
                friendship += r.band == 2 ? 1 : 0;
            }
        }
        std::int64_t turned = 0, arrivals = 0, arrived = 0, expelled = 0, expelled_people = 0, fled = 0;
        std::int64_t without = 0, lost = 0;
        std::vector<std::uint8_t> mixed_ever(t_.settlements.size() + 1, 0);
        const auto sat_add = [](std::int64_t& h, std::int64_t v) {
            v = std::max<std::int64_t>(0, v);
            h = h > std::numeric_limits<std::int64_t>::max() - v ? std::numeric_limits<std::int64_t>::max() : h + v;
        };
        for (const HistoryView::Event& e : events_) {
            const Id k = e.row.kind;
            turned += k == kinds_.disposition_turned ? 1 : 0;
            fled += k == kinds_.settlement_fled ? 1 : 0;
            without += k == kinds_.settlement_abandoned ? 1 : 0;
            if (k == kinds_.refugees_arrived) {
                ++arrivals;
                sat_add(arrived, value_of(e, "people"));
            } else if (k == kinds_.people_expelled) {
                ++expelled;
                sat_add(expelled_people, value_of(e, "people"));
            } else if (k == kinds_.settlement_abandoned_to) {
                sat_add(lost, value_of(e, "lost"));
            } else if (k == kinds_.settlement_mixed && e.row.settlement >= 1 &&
                       static_cast<std::size_t>(e.row.settlement) < mixed_ever.size()) {
                mixed_ever[static_cast<std::size_t>(e.row.settlement)] = 1;
            }
        }
        std::int64_t ever = 0;
        for (const std::uint8_t b : mixed_ever) {
            ever += b;
        }
        std::optional<std::int64_t> oldest;
        for (const db::SettlementRow& x : t_.settlements) {
            if (x.status == 3 && x.ruined_year && (!oldest || *x.ruined_year < *oldest)) {
                oldest = x.ruined_year;
            }
        }
        put("race_pairs", present * (present - 1) / 2);
        put("race_pairs_met", met);
        put("dispositions_turned", turned);
        put("pairs_hatred", hatred);
        put("pairs_friendship", friendship);
        put("refugees_arrived", arrivals);
        put("refugees_people", arrived);
        put("people_expelled", expelled);
        put("expelled_people", expelled_people);
        put("settlements_mixed", static_cast<std::int64_t>(mixed_at_end(t_).size()));
        put("settlements_mixed_ever", ever);
        put("settlements_fled", fled);
        put("abandoned_without_destination", without);
        put("abandonment_lost", lost);
        put("monster_stories", d_.monster_stories);
        put("ruins_oldest_year", oldest ? *oldest : STAT_NONE);
        // P5-03a 9.6: meetings by word; turns into a higher band than the
        // pair's previous band (the last turn's, else the meeting's).
        std::int64_t by_word = 0, warmed = 0;
        std::vector<std::tuple<Id, Id, std::int64_t>> bands;  // (race_a, race_b, band), small
        const auto pair_band = [&bands](Id a, Id b) -> std::int64_t* {
            for (auto& [x, y, band] : bands) {
                if (x == a && y == b) {
                    return &band;
                }
            }
            return nullptr;
        };
        for (const HistoryView::Event& e : events_) {
            const Id k = e.row.kind;
            if ((k != kinds_.peoples_met && k != kinds_.disposition_turned) || e.participants.size() < 2U) {
                continue;
            }
            const Id a = std::min(e.participants[0].entity_id, e.participants[1].entity_id);
            const Id b = std::max(e.participants[0].entity_id, e.participants[1].entity_id);
            const std::int64_t band = value_of(e, "band");
            std::int64_t* known = pair_band(a, b);
            if (k == kinds_.peoples_met) {
                by_word += value_of(e, "sealed") == 1 ? 1 : 0;
            } else if (known != nullptr && band > *known) {
                ++warmed;
            }
            if (known != nullptr) {
                *known = band;
            } else {
                bands.emplace_back(a, b, band);
            }
        }
        put("peoples_met_by_word", by_word);
        put("dispositions_warmed", warmed);
    }

    // r21 26.5 item 2 (P5-12): region_names, regions_renamed, refuges,
    // refuges_hidden (26.3's n and h).
    void renaming_stats() {
        std::int64_t renamed = 0;
        for (std::size_t i = 0; i < t_.region_names.size(); ++i) {
            renamed += (i == 0 || t_.region_names[i].region != t_.region_names[i - 1].region) ? 1 : 0;
        }
        std::int64_t refuges = 0;
        std::int64_t hidden = 0;
        for (const db::SettlementRow& s : t_.settlements) {
            const bool standing_now = s.status == 1 || s.status == 2;
            if (!standing_now || s.founding_event < 1 || static_cast<std::size_t>(s.founding_event) > events_.size() ||
                kind(s.founding_event) != kinds_.refugees_settled) {
                continue;
            }
            ++refuges;
            hidden += refuge_hidden_at_end(s.id) ? 1 : 0;
        }
        put("region_names", static_cast<std::int64_t>(t_.region_names.size()));
        put("regions_renamed", renamed);
        put("refuges", refuges);
        put("refuges_hidden", hidden);
    }

    // 26.3: the refuge's refuge_hidden (smallest id) ends at E - 1 or later.
    [[nodiscard]] bool refuge_hidden_at_end(Id settlement) const {
        if (settlement < 1 || static_cast<std::size_t>(settlement) > d_.refuge_hidden_end.size()) {
            return false;
        }
        return d_.refuge_hidden_end[static_cast<std::size_t>(settlement - 1)] >= e_ - 1;
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
            wars += principal_war(r) ? 1 : 0;  // P5-04 10.4
            wars_open += (principal_war(r) && !r.until) ? 1 : 0;
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
            if (principal_war(r)) {
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
                n += (principal_war(r) && r.a_type == 3 && race_of(r.a_id) == k) ? 1 : 0;
            }
            put("wars_attacker_" + t_.races[k].key, n);
        }
    }

    // P5-04 10.4: a war row that is a war, not an ally's joined row.
    [[nodiscard]] bool principal_war(const db::RelationRow& r) const {
        return r.kind == "war" && !(kinds_.ally_joined_war != 0 && r.since_event >= 1 &&
                                    static_cast<std::size_t>(r.since_event) <= events_.size() &&
                                    kind(r.since_event) == kinds_.ally_joined_war);
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
                if (!kinds_.battle_kind(kind(b))) {
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
    std::vector<Id> stories_;  // P5-03 10.1: the told monsters' story events
    std::vector<Id> mixed_told_;  // P5-03a 9.5: the settlement_mixed candidates, ascending
    std::vector<std::vector<Id>> effects_;           // index event id: the events it caused, ascending
    std::vector<std::vector<Id>> civ_events_;        // index civ - 1: its events, ascending
    std::vector<std::size_t> war_row_;               // index event id: its war relation row, or NONE
    std::vector<Id> war_of_;                         // index event id: war(e) of 24.1, 0 none
    std::vector<Id> last_battle_;                    // index declaration id: its last battle, 0 none
    std::vector<std::vector<std::size_t>> control_of_;  // index civ - 1: its region_control rows
    std::int64_t nonislet_ = 0;
    std::int64_t share_max_ = 0, share_max_civ_ = 0, share_max_year_ = 0;
    std::int64_t bloc_over_800_ = 0;  // P5-04 10.5
    std::array<std::int64_t, 5> share_runs_{};
    std::vector<std::int64_t> race_population_;
    std::vector<std::int64_t> race_regions_;
};

}  // namespace

HistoryDigest digest_history(const HistoryView& view, const HistoryTables& tables) {
    return Digester(view, tables).run();
}

std::int64_t summary_budget(std::int64_t civilizations, std::int64_t outside_lines, std::int64_t ages,
                            std::int64_t phases, std::int64_t lines_max) {
    const std::int64_t l = std::min(
        SUMMARY_LINES_CAP,
        checked_add(lines_max,
                    checked_mul(SUMMARY_LINES_PER_CIV, std::max<std::int64_t>(0, civilizations - SUMMARY_LINES_CIVS))));
    const std::int64_t f = checked_add(checked_add(4, ages), checked_mul(2, phases));
    const std::int64_t room = l - checked_add(outside_lines, f);
    return std::min(SUMMARY_BUDGET, std::max(SUMMARY_BUDGET_MIN, room));
}

void choose_summary_set(HistoryDigest& digest, std::int64_t budget) {
    set_summary(digest, budget);
    for (std::pair<std::string, std::int64_t>& s : digest.stats) {
        if (s.first == "summary_threshold") {
            s.second = digest.summary_threshold;
        } else if (s.first == "summary_events") {
            s.second = digest.summary_events;
        }
    }
}

std::vector<MixedSettlement> mixed_at_end(const HistoryTables& t) {
    std::vector<MixedSettlement> out;
    const std::vector<db::SettlementRaceRow>& rows = t.settlement_races;  // (settlement, race, year) ascending
    std::size_t i = 0;
    while (i < rows.size()) {
        const std::int64_t s = rows[i].settlement;
        MixedSettlement m;
        m.settlement = s;
        while (i < rows.size() && rows[i].settlement == s) {
            const std::int64_t race = rows[i].race;
            const db::SettlementRaceRow* last = nullptr;
            while (i < rows.size() && rows[i].settlement == s && rows[i].race == race) {
                if (rows[i].year <= t.end_year - 1) {
                    last = &rows[i];
                }
                ++i;
            }
            if (last != nullptr && last->share > 0) {
                m.shares.push_back(*last);
            }
        }
        const bool standing_now = s >= 1 && static_cast<std::size_t>(s) <= t.settlements.size() &&
                                  (t.settlements[static_cast<std::size_t>(s - 1)].status == 1 ||
                                   t.settlements[static_cast<std::size_t>(s - 1)].status == 2);
        const bool mixed = std::any_of(m.shares.begin(), m.shares.end(), [](const db::SettlementRaceRow& r) {
            return r.standing != 3 && r.share >= MIXED_SHARE_MIN;
        });
        if (standing_now && mixed) {
            std::stable_sort(m.shares.begin(), m.shares.end(), [](const db::SettlementRaceRow& a, const db::SettlementRaceRow& b) {
                return a.share != b.share ? a.share > b.share : a.race < b.race;
            });
            out.push_back(std::move(m));
        }
    }
    return out;
}

}  // namespace worldgen::history
