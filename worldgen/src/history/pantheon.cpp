// The pantheon (spec docs/specs/p5-08-pantheon-cults.md): regard, the act
// (the source and the applier), its effects and the gods' flare-up. The
// cults are in pantheon_cults.cpp.
//
// Every collection is a vector indexed by id or a std::map keyed by ids;
// loops over State run by id up to a count taken before the loop; relations
// are opened and closed outside loops over State::relations() (6.12).

#include "worldgen/history/pantheon.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <utility>

#include "conflict_internal.hpp"
#include "pantheon_internal.hpp"
#include "worldgen/core/checked.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/streams.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/history/conflict.hpp"
#include "worldgen/history/draw.hpp"
#include "worldgen/history/figures.hpp"
#include "worldgen/history/monsters.hpp"
#include "worldgen/history/peoples.hpp"
#include "worldgen/history/renaming.hpp"
#include "worldgen/history/schedule.hpp"

namespace worldgen::history {

using namespace conflict_detail;
using namespace pantheon_detail;

namespace {

// T6 only for the finish of the history's last year (integration E3).
constexpr std::array<Step, 3> STEPS = {Step::t4, Step::t6, Step::q4};

constexpr std::array<std::string_view, 11> STREAM_NAMES = {
    streams::hist_pantheon_act,         streams::hist_pantheon_kind,       streams::hist_pantheon_flare,
    streams::hist_pantheon_monster_kind, streams::hist_pantheon_site_name, streams::hist_pantheon_cult_found,
    streams::hist_pantheon_cult_name,   streams::hist_pantheon_cult_spread, streams::hist_pantheon_adopt,
    streams::hist_pantheon_suppress,    streams::hist_pantheon_hide,
};

constexpr std::array<std::string_view, 5> KIND_KEYS = {"bless", "curse", "champion", "monster", "mark"};
constexpr std::array<std::string_view, 5> TEMPER_KEYS = {"steady", "warlike", "peaceable", "grasping", "just"};
constexpr std::string_view GOD_SLOT = "{god}";

std::int64_t enum_index(std::string_view value, std::span<const std::string_view> keys) {
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (keys[i] == value) {
            return static_cast<std::int64_t>(i);
        }
    }
    fail("history: pantheon: '" + std::string(value) + "' is not a known key");
}

GodActs acts_from(const data::Record& r) {
    GodActs a;
    const data::Record& fav = r.get_record("favour");
    const data::Record& wr = r.get_record("wrath");
    a.favour = {fav.get_int("bless"), fav.get_int("champion"), fav.get_int("mark")};
    a.wrath = {wr.get_int("curse"), wr.get_int("monster"), wr.get_int("mark")};
    static constexpr std::array<std::string_view, 2> BLESS = {"plenty", "valour"};
    static constexpr std::array<std::string_view, 3> CURSE = {"pestilence", "withering", "defeat"};
    static constexpr std::array<std::string_view, 3> WHO = {"kin", "any", "foe"};
    a.bless = enum_index(r.get_string("bless"), BLESS);
    a.curse = enum_index(r.get_string("curse"), CURSE);
    for (const data::Value& v : r.get_array("monsters")) {
        a.monsters.push_back(v.as_string());
    }
    for (const data::Value& v : r.get_array("deeds")) {
        const data::Record& d = v.as_record();
        GodDeed deed;
        deed.kind = d.get_string("kind");
        deed.role = d.get_string("role");
        deed.points = d.get_int("points");
        deed.who = enum_index(d.get_string("who"), WHO);
        deed.against = d.get_string("against");
        a.deeds.push_back(std::move(deed));
    }
    return a;
}

std::array<std::int64_t, 5> temper_record(const data::Record& r) {
    std::array<std::int64_t, 5> out{};
    for (std::size_t t = 0; t < out.size(); ++t) {
        out[t] = r.get_int(TEMPER_KEYS[t]);
    }
    return out;
}

std::int64_t camp_of(std::string_view camp) {
    static constexpr std::array<std::string_view, 3> CAMPS = {"good", "neutral", "evil"};
    return enum_index(camp, CAMPS);
}

}  // namespace

// --- Data (4.1, 4.2) ------------------------------------------------------------------------------------

PantheonTuning pantheon_tuning_from(const data::ContentSet& content) {
    const data::Definition* d = content.find(data::HISTORY_KIND, data::HISTORY_PANTHEON_ID);
    if (d == nullptr) {
        fail("history: tuning: the content has no definition history.pantheon");
    }
    const data::Record& r = d->record;
    PantheonTuning t;
    const data::Record& a = r.get_record("activity_permille");
    t.activity_permille = {a.get_int("distant"), a.get_int("normal"), a.get_int("meddling")};
    t.regard_max = r.get_int("regard_max");
    t.regard_fade_per_century = r.get_int("regard_fade_per_century");
    t.regard_memory_years = r.get_int("regard_memory_years");
    t.contributions_max = r.get_int("contributions_max");
    t.act_threshold = r.get_int("act_threshold");
    t.act_permille = r.get_int("act_permille");
    t.god_rest_years = r.get_int("god_rest_years");
    t.pair_rest_years = r.get_int("pair_rest_years");
    t.flare_regard = r.get_int("flare_regard");
    t.flare_permille = r.get_int("flare_permille");
    t.bless_years = r.get_int("bless_years");
    t.curse_years = r.get_int("curse_years");
    t.plenty_growth_permille = r.get_int("plenty_growth_permille");
    t.withering_growth_permille = r.get_int("withering_growth_permille");
    t.valour_permille = r.get_int("valour_permille");
    t.defeat_permille = r.get_int("defeat_permille");
    t.pestilence_toll_permille = r.get_int("pestilence_toll_permille");
    t.send_hops = r.get_int("send_hops");
    t.sacred_growth_permille = r.get_int("sacred_growth_permille");
    t.profane_years = r.get_int("profane_years");
    t.champion_epithet = r.get_string("champion_epithet");
    t.cult_found_permille = r.get_int("cult_found_permille");
    t.cult_fear_permille = r.get_int("cult_fear_permille");
    t.cult_spread_permille = r.get_int("cult_spread_permille");
    const data::Record& af = r.get_record("affinity_permille");
    t.affinity_permille = {af.get_int("same"), af.get_int("near"), af.get_int("opposed")};
    t.adopt_regard = r.get_int("adopt_regard");
    t.adopt_permille = r.get_int("adopt_permille");
    t.suppress_permille = r.get_int("suppress_permille");
    t.temper_adopt_permille = temper_record(r.get_record("temper_adopt_permille"));
    t.temper_suppress_permille = temper_record(r.get_record("temper_suppress_permille"));
    t.faith_points = r.get_int("faith_points");
    t.rival_faith_points = r.get_int("rival_faith_points");
    t.suppress_points = r.get_int("suppress_points");
    t.hide_permille = r.get_int("hide_permille");
    t.sanctum_hops = r.get_int("sanctum_hops");
    t.creator = acts_from(r.get_record("creator"));
    t.cult_return_years = r.get_int("cult_return_years");  // P5-11 12.2
    return t;
}

std::vector<PantheonGodDef> pantheon_god_defs_from(const data::ContentSet& content) {
    std::vector<PantheonGodDef> out;
    const data::KindContent* gods = content.find_kind(data::GOD_KIND);
    if (gods == nullptr) {
        return out;
    }
    static constexpr std::array<std::string_view, 8> DOMAINS = {"war",   "death", "harvest", "craft",
                                                                "wild",  "oaths", "lore",    "ruin"};
    for (const data::Definition& d : gods->definitions) {
        PantheonGodDef g;
        g.key = d.id;
        g.name_culture = d.record.get_string("name_culture");
        g.camp = camp_of(d.record.get_string("camp"));
        g.domain = enum_index(d.record.get_string("domain"), DOMAINS) + 2;
        g.title = d.record.get_string("title");
        g.acts = acts_from(d.record);
        out.push_back(std::move(g));
    }
    return out;
}

// --- The subsystem ------------------------------------------------------------------------------------

PantheonSubsystem::PantheonSubsystem(PeoplesSubsystem& peoples, const data::ContentSet& content)
    : peoples_(peoples), tuning_(pantheon_tuning_from(content)), defs_(pantheon_god_defs_from(content)) {}

PantheonSubsystem::PantheonSubsystem(PeoplesSubsystem& peoples, PantheonTuning tuning,
                                     std::vector<PantheonGodDef> gods)
    : peoples_(peoples), tuning_(std::move(tuning)), defs_(std::move(gods)) {}

std::span<const Step> PantheonSubsystem::steps() const {
    return STEPS;
}

void PantheonSubsystem::prepare(const StepContext& ctx) {
    if (prepared_) {
        return;
    }
    const HistoryInputs& in = ctx.inputs();
    for (std::size_t i = 0; i < STREAM_NAMES.size(); ++i) {
        keys_[i] = stream_key(in.seed, STREAM_NAMES[i]);
    }
    switch (in.params.divine_activity) {
        case data::DivineActivity::distant: activity_ = tuning_.activity_permille[0]; break;
        case data::DivineActivity::normal: activity_ = tuning_.activity_permille[1]; break;
        case data::DivineActivity::meddling: activity_ = tuning_.activity_permille[2]; break;
    }
    end_year_ = in.params.span;
    threshold_ = schedule_lengths(in.params.span, engine_tuning_from(ctx.content()), in.params.detail).flare_threshold;
    for (const db::RaceRow& r : in.races) {
        race_camps_.emplace_back(r.id, r.camp);
    }
    std::stable_sort(race_camps_.begin(), race_camps_.end());
    for (const db::CultureRow& c : in.cultures) {
        culture_keys_.emplace_back(c.id, c.key);
    }
    std::stable_sort(culture_keys_.begin(), culture_keys_.end());
    // The gods as chronology wrote them (6.1): creators first, then the
    // pantheon in the order of the definitions.
    std::int64_t pantheon_index = 0;
    for (const db::GodRow& row : in.gods) {
        GodState g;
        g.id = row.id;
        g.camp = row.camp;
        g.name = row.name;
        // 9: rows 1 ... n (n = race rows) are the creators, race = id.
        const bool creator = row.id <= static_cast<std::int64_t>(in.races.size());
        g.race = creator ? (row.race != 0 ? row.race : row.id) : 0;
        if (creator) {
            g.acts = &tuning_.creator;
        } else {
            if (pantheon_index >= static_cast<std::int64_t>(defs_.size())) {
                fail("history: pantheon: god " + int_text(row.id) + " has no god definition");
            }
            g.acts = &defs_[static_cast<std::size_t>(pantheon_index)].acts;
            ++pantheon_index;
        }
        if (sites_ != nullptr) {
            const std::span<const MonsterKindDef> kinds = sites_->kinds();
            for (const std::string& key : g.acts->monsters) {
                for (std::size_t k = 0; k < kinds.size(); ++k) {
                    if (kinds[k].key == key) {
                        g.monster_kinds.push_back(k);
                    }
                }
            }
        }
        gods_.push_back(std::move(g));
    }
    for (std::size_t i = 0; i < gods_.size(); ++i) {
        if (gods_[i].id != static_cast<std::int64_t>(i) + 1) {
            fail("history: pantheon: god ids are not 1 to " + int_text(static_cast<std::int64_t>(gods_.size())));
        }
    }
    // 6.2: the deeds by kind, ascending god, then deed index.
    for (const GodState& g : gods_) {
        for (std::size_t d = 0; d < g.acts->deeds.size(); ++d) {
            deeds_by_kind_[g.acts->deeds[d].kind].emplace_back(g.id, d);
        }
    }
    placenames_ = stages::placename_data_from(ctx.content());
    prepared_ = true;
}

const PantheonSubsystem::GodState& PantheonSubsystem::god(std::int64_t id) const {
    if (id < 1 || static_cast<std::size_t>(id) > gods_.size()) {
        fail("history: pantheon: god " + int_text(id) + " does not exist");
    }
    return gods_[static_cast<std::size_t>(id - 1)];
}

PantheonSubsystem::GodState& PantheonSubsystem::god(std::int64_t id) {
    if (id < 1 || static_cast<std::size_t>(id) > gods_.size()) {
        fail("history: pantheon: god " + int_text(id) + " does not exist");
    }
    return gods_[static_cast<std::size_t>(id - 1)];
}

std::int64_t PantheonSubsystem::race_camp(std::int64_t race) const {
    const auto it = std::lower_bound(race_camps_.begin(), race_camps_.end(), std::pair<std::int64_t, std::int64_t>{race, -1});
    if (it == race_camps_.end() || it->first != race) {
        fail("history: pantheon: race " + int_text(race) + " does not exist");
    }
    return it->second;
}

std::int64_t PantheonSubsystem::civ_camp(const StepContext& ctx, std::int64_t civ) const {
    return race_camp(ctx.state().civ(civ).race);
}

bool PantheonSubsystem::opposed_camps(std::int64_t a, std::int64_t b) const noexcept {
    return (a == 0 && b == 2) || (a == 2 && b == 0);
}

// 4.3: kin and foe.
bool PantheonSubsystem::kin(const StepContext& ctx, std::int64_t g, std::int64_t c) const {
    const GodState& gs = god(g);
    if (gs.race != 0) {
        return ctx.state().civ(c).race == gs.race;
    }
    return civ_camp(ctx, c) == gs.camp;
}

bool PantheonSubsystem::foe(const StepContext& ctx, std::int64_t g, std::int64_t c) const {
    return opposed_camps(god(g).camp, civ_camp(ctx, c));
}

bool PantheonSubsystem::alive_visible(const StepContext& ctx, std::int64_t civ) const {
    if (civ < 1 || civ > civ_count(ctx.state())) {
        return false;
    }
    const Civilization& c = ctx.state().civ(civ);
    return !c.ended_year && ctx.visible(c);
}

std::int64_t PantheonSubsystem::regard(std::int64_t g, std::int64_t c) const {
    const auto it = pairs_.find({g, c});
    return it == pairs_.end() ? 0 : it->second.regard;
}

std::int64_t PantheonSubsystem::temper_of(std::int64_t civ) const {
    return figures_ != nullptr ? figures_->temper(civ) : -1;
}

bool PantheonSubsystem::faction_exists(std::int64_t faction) const noexcept {
    return faction >= 1 && static_cast<std::size_t>(faction) <= cults_.size();
}

// --- The run (6.12 item 3) ------------------------------------------------------------------------------

void PantheonSubsystem::run(StepContext& ctx) {
    prepare(ctx);
    if (ctx.step() == Step::t6) {
        if (ctx.year() == end_year_ - 1 && !finished_) {
            finish(ctx);
        }
        return;
    }
    // 6.2: a pair whose realm has ended is dropped at the step after its
    // ending event was read.
    for (const std::int64_t c : drop_next_) {
        for (auto it = pairs_.begin(); it != pairs_.end();) {
            it = it->first.second == c ? pairs_.erase(it) : std::next(it);
        }
    }
    drop_next_.clear();
    if (ctx.step() == Step::t4 && ctx.phase_id() != phase_seen_) {
        phase_seen_ = ctx.phase_id();
        phase_start_ = ctx.year();
    }
    if (ctx.step() == Step::q4) {
        pending_flare_ = 0;  // 6.6
        flared_ = {0, 0, 0};
    }
    read_log(ctx);
    endings(ctx);  // 6.10, first after the log
    temples(ctx);
    fade(ctx);
    if (ctx.step() == Step::q4) {
        flare(ctx);
    }
    // 6.12 item 4 (integration E4): a cult founded in this step is no
    // candidate for spread, faith or suppression until the next step.
    settled_cults_ = static_cast<std::int64_t>(cults_.size());
    foundings(ctx);  // 6.7
    spreads(ctx);    // 6.8
    faiths(ctx);     // 6.9
}

// --- Regard (6.2) ------------------------------------------------------------------------------------------

void PantheonSubsystem::add(std::int64_t g, std::int64_t c, std::int64_t points, std::int64_t event,
                            std::int64_t year) {
    PairState& p = pairs_[{g, c}];
    p.regard = std::clamp(checked_add(p.regard, points), -tuning_.regard_max, tuning_.regard_max);
    p.contributions.push_back(Contribution{event, year, points});
    const std::int64_t oldest = year - tuning_.regard_memory_years;
    p.contributions.erase(std::remove_if(p.contributions.begin(), p.contributions.end(),
                                         [oldest](const Contribution& x) { return x.year < oldest; }),
                          p.contributions.end());
    while (static_cast<std::int64_t>(p.contributions.size()) > tuning_.contributions_max) {
        p.contributions.erase(p.contributions.begin());
    }
}

// --- The finish (6.10, integration E3) -------------------------------------------------------------------

// T6 of the last year, after every other subsystem: the realms that ended
// after the pantheon's last read of the log (T4 of this year) close their
// cult rows as 6.10 would at the next step: endings, then temples, with the
// realms' ending events. The regard deeds of these events are not read: no
// step follows.
void PantheonSubsystem::finish(StepContext& ctx) {
    finished_ = true;
    const std::int64_t last = ctx.last_event_id();
    ended_realms_.clear();
    for (std::int64_t e = cursor_; e <= last; ++e) {
        const std::string_view k = ctx.event_kind(e);
        if (k == "civ_ended" || k == "civ_conquered") {
            const std::int64_t c = ctx.event_participant(e, "civilization");
            if (c != 0) {
                ended_realms_.emplace_back(c, e);
            }
        }
    }
    endings(ctx);
    temples(ctx);
}

void PantheonSubsystem::read_log(StepContext& ctx) {
    const State& state = ctx.state();
    const std::int64_t last = ctx.last_event_id();
    ended_realms_.clear();
    for (std::int64_t e = cursor_; e <= last; ++e) {
        const std::string_view k = ctx.event_kind(e);
        const std::int64_t y = ctx.event_year(e);
        if (k == "civ_ended" || k == "civ_conquered") {
            const std::int64_t c = ctx.event_participant(e, "civilization");
            if (c != 0) {
                ended_realms_.emplace_back(c, e);
                drop_next_.push_back(c);
            }
        }
        const auto it = deeds_by_kind_.find(k);
        if (it == deeds_by_kind_.end()) {
            continue;
        }
        for (const auto& [g, di] : it->second) {
            const GodDeed& d = god(g).acts->deeds[di];
            const std::vector<std::int64_t> receivers = ctx.event_participants(e, d.role);
            for (const std::int64_t r : receivers) {
                if (r == 0 || r > civ_count(state)) {
                    continue;
                }
                const bool who = d.who == 0 ? kin(ctx, g, r) : (d.who == 2 ? foe(ctx, g, r) : true);
                if (!who) {
                    continue;
                }
                if (!d.against.empty()) {
                    bool any_kin = false;
                    for (const std::int64_t a : ctx.event_participants(e, d.against)) {
                        any_kin = any_kin || (a != 0 && a <= civ_count(state) && kin(ctx, g, a));
                    }
                    if (!any_kin) {
                        continue;
                    }
                }
                if (d.points > 0 && foe(ctx, g, r)) {
                    continue;  // a god never favours its foes
                }
                const Civilization& cr = state.civ(r);
                if (cr.ended_year && *cr.ended_year < y) {
                    continue;
                }
                add(g, r, d.points, e, y);
            }
        }
    }
    cursor_ = last + 1;
}

void PantheonSubsystem::fade(const StepContext& ctx) {
    const std::int64_t y = ctx.year();
    const std::int64_t f = tuning_.regard_fade_per_century;
    const std::int64_t amount =
        floor_div(checked_mul(f, y + ctx.years()), 100) - floor_div(checked_mul(f, y), 100);
    if (amount <= 0) {
        return;
    }
    for (auto& [key, p] : pairs_) {
        (void)key;
        const std::int64_t m = std::min(p.regard < 0 ? -p.regard : p.regard, amount);
        p.regard += p.regard < 0 ? m : -m;
    }
}

// --- The act (6.3) ------------------------------------------------------------------------------------------

std::int64_t PantheonSubsystem::home_region(const StepContext& ctx, std::int64_t c) const {
    const Civilization& civ = ctx.state().civ(c);
    return civ.capital != 0 ? ctx.state().settlement(civ.capital).region : civ.home_region;
}

std::int64_t PantheonSubsystem::capital_region(const StepContext& ctx, std::int64_t c) const {
    const Civilization& civ = ctx.state().civ(c);
    if (civ.capital == 0) {
        return 0;
    }
    const Settlement& s = ctx.state().settlement(civ.capital);
    return standing(s) && ctx.visible(s) ? s.region : 0;
}

bool PantheonSubsystem::is_candidate(const StepContext& ctx, std::int64_t g, std::int64_t c, const PairState& p,
                                     std::int64_t min_regard) const {
    const std::int64_t r = p.regard;
    const std::int64_t y = ctx.year();
    if ((r < 0 ? -r : r) < min_regard || r == 0 || !alive_visible(ctx, c)) {
        return false;
    }
    const GodState& gs = god(g);
    if (gs.last_act && y - *gs.last_act < tuning_.god_rest_years) {
        return false;
    }
    if (p.last_act && y - *p.last_act < tuning_.pair_rest_years) {
        return false;
    }
    const std::int64_t oldest = y - tuning_.regard_memory_years;
    const bool signed_contribution = std::any_of(p.contributions.begin(), p.contributions.end(),
                                                 [r, oldest](const Contribution& x) {
                                                     return x.year >= oldest && (r > 0 ? x.points > 0 : x.points < 0);
                                                 });
    if (!signed_contribution) {
        return false;
    }
    if (r > 0) {
        return !foe(ctx, g, c);
    }
    const auto f = faith_of_.find(c);
    return f == faith_of_.end() || cults_[static_cast<std::size_t>(f->second.first - 1)].god != g;
}

std::optional<PantheonSubsystem::Candidate> PantheonSubsystem::best_candidate(const StepContext& ctx,
                                                                              std::int64_t min_regard) const {
    std::optional<Candidate> best;
    for (const auto& [key, p] : pairs_) {  // (g, c) ascending: ties keep the smaller g, then c
        if (!is_candidate(ctx, key.first, key.second, p, min_regard)) {
            continue;
        }
        const std::int64_t a = p.regard < 0 ? -p.regard : p.regard;
        if (!best || a > (best->regard < 0 ? -best->regard : best->regard)) {
            best = Candidate{key.first, key.second, p.regard};
        }
    }
    return best;
}

// The contributions of R's sign within the memory, by (|points| descending,
// event descending), the first three, ascending; the lead deed is the first
// of the ranking (returned first in a separate call: ranked order).
std::vector<std::int64_t> PantheonSubsystem::ranked_causes(const StepContext& ctx, const PairState& p) const {
    const std::int64_t r = p.regard;
    const std::int64_t oldest = ctx.year() - tuning_.regard_memory_years;
    std::vector<Contribution> list;
    for (const Contribution& x : p.contributions) {
        if (x.year >= oldest && (r > 0 ? x.points > 0 : x.points < 0)) {
            list.push_back(x);
        }
    }
    std::stable_sort(list.begin(), list.end(), [](const Contribution& a, const Contribution& b) {
        const std::int64_t pa = a.points < 0 ? -a.points : a.points;
        const std::int64_t pb = b.points < 0 ? -b.points : b.points;
        return pa != pb ? pa > pb : a.event > b.event;
    });
    std::vector<std::int64_t> out;
    for (std::size_t i = 0; i < list.size() && out.size() < 3; ++i) {
        if (std::find(out.begin(), out.end(), list[i].event) == out.end()) {
            out.push_back(list[i].event);
        }
    }
    return out;  // ranked: out.front() is the lead deed
}

std::vector<std::size_t> PantheonSubsystem::sendable_kinds(std::int64_t g) const {
    std::vector<std::size_t> out;
    if (sites_ == nullptr) {
        return out;
    }
    const std::span<const MonsterKindDef> kinds = sites_->kinds();
    for (const std::size_t k : god(g).monster_kinds) {
        if (sites_->living_count(k) < kinds[k].max_alive && kinds[k].wake_rate > 0) {
            out.push_back(k);
        }
    }
    return out;
}

std::int64_t PantheonSubsystem::lair_region(const StepContext& ctx, std::int64_t c) const {
    if (sites_ == nullptr) {
        return 0;
    }
    return sites_->lair_region_for_sent(ctx, home_region(ctx, c), tuning_.send_hops);
}

// 6.4 mark: the place.
std::optional<PantheonSubsystem::Place> PantheonSubsystem::mark_place(const StepContext& ctx, std::int64_t c,
                                                                      std::int64_t lead) const {
    if (sites_ == nullptr || lead == 0) {
        return std::nullopt;
    }
    const State& state = ctx.state();
    const std::int64_t u = ctx.event_settlement(lead);
    if (u != 0 && shrine_settlements_.count(u) == 0) {
        const Settlement& s = state.settlement(u);
        return Place{s.region, s.x, s.y, u};
    }
    std::int64_t region = ctx.event_region(lead);
    if (region == 0) {
        region = home_region(ctx, c);
    }
    if (region == 0) {
        return std::nullopt;
    }
    const bool open_shrine = std::any_of(shrines_.begin(), shrines_.end(),
                                         [region](const Shrine& s) { return s.region == region; });
    if (open_shrine) {
        return std::nullopt;
    }
    const std::optional<MonstersSubsystem::HeightCell> cell = sites_->height_cell(ctx, region);
    if (!cell) {
        return std::nullopt;
    }
    return Place{region, cell->x, cell->y, 0};
}

// 6.3 (lead ruling): c has work for a champion in year Y: c is a side or an
// open joined ally of an open war, or an awake monster that c could hunt
// under P5-02 6.4.2 (MonstersSubsystem::huntable_by) exists.
bool PantheonSubsystem::champion_work(const StepContext& ctx, std::int64_t c) const {
    if (conflict_ != nullptr && conflict_->open_wars_of(c) > 0) {
        return true;
    }
    return sites_ != nullptr && sites_->huntable_by(ctx, c);
}

bool PantheonSubsystem::eligible(const StepContext& ctx, std::size_t kind, std::int64_t g, std::int64_t c) const {
    switch (kind) {
        case BLESS: return true;
        case CURSE: return capital_region(ctx, c) != 0;
        case CHAMPION: return figures_ != nullptr && figures_->champion_of(c) == 0 && champion_work(ctx, c);
        case MONSTER: return !sendable_kinds(g).empty() && lair_region(ctx, c) != 0;
        case MARK: {
            const auto it = pairs_.find({g, c});
            if (it == pairs_.end()) {
                return false;
            }
            const std::vector<std::int64_t> causes = ranked_causes(ctx, it->second);
            return mark_place(ctx, c, causes.empty() ? 0 : causes.front()).has_value();
        }
        default: return false;
    }
}

std::array<std::int64_t, 3> PantheonSubsystem::kind_weights(const StepContext& ctx, std::int64_t g, std::int64_t c,
                                                            bool favourable) const {
    const GodActs& a = *god(g).acts;
    std::array<std::int64_t, 3> w = favourable ? a.favour : a.wrath;
    const std::array<std::size_t, 3> kinds = favourable ? std::array<std::size_t, 3>{BLESS, CHAMPION, MARK}
                                                        : std::array<std::size_t, 3>{CURSE, MONSTER, MARK};
    for (std::size_t i = 0; i < 3; ++i) {
        if (w[i] > 0 && !eligible(ctx, kinds[i], g, c)) {
            w[i] = 0;
        }
    }
    return w;
}

std::int64_t PantheonSubsystem::act_region(const StepContext& ctx, std::size_t kind, std::int64_t g,
                                           std::int64_t c, std::int64_t lead) const {
    (void)g;
    switch (kind) {
        case CURSE: return capital_region(ctx, c);
        case MONSTER: return lair_region(ctx, c);
        case MARK: {
            const std::optional<Place> p = mark_place(ctx, c, lead);
            return p ? p->region : 0;
        }
        default: return home_region(ctx, c);
    }
}

std::vector<Intervention> PantheonSubsystem::interventions(const StepContext& ctx) {
    if (!prepared_ || pairs_.empty()) {
        return {};
    }
    const std::int64_t y = ctx.year();
    // 6.6: in the first tick year of a gods' flare-up whose trigger is the
    // flared pair's deed, that pair acts with no act draw if it is still a
    // candidate (engineer's reading: then it is the candidate).
    std::optional<Candidate> cand;
    bool certain = false;
    if (flared_[0] != 0 && ctx.phase_kind() == PhaseKind::flare && ctx.phase_trigger_event() == flared_[2] &&
        y == phase_start_) {
        const auto it = pairs_.find({flared_[0], flared_[1]});
        if (it != pairs_.end() && is_candidate(ctx, flared_[0], flared_[1], it->second, tuning_.act_threshold)) {
            cand = Candidate{flared_[0], flared_[1], it->second.regard};
            certain = true;
        }
    }
    if (!cand) {
        cand = best_candidate(ctx, tuning_.act_threshold);
    }
    if (!cand) {
        return {};
    }
    const std::int64_t g = cand->god;
    const std::int64_t c = cand->civ;
    const std::int64_t p = std::min<std::int64_t>(1000, floor_div(checked_mul(tuning_.act_permille, activity_), 1000));
    if (!certain && !chance_pm(ctx.draw(keys_[ACT], word(g), word(c), 0), p)) {
        return {};
    }
    const bool favourable = cand->regard > 0;
    const std::array<std::int64_t, 3> w = kind_weights(ctx, g, c, favourable);
    if (w[0] == 0 && w[1] == 0 && w[2] == 0) {
        return {};
    }
    const std::size_t i = weighted(ctx.draw(keys_[KIND], word(g), word(c), 0), w);
    const std::size_t kind = favourable ? std::array<std::size_t, 3>{BLESS, CHAMPION, MARK}[i]
                                        : std::array<std::size_t, 3>{CURSE, MONSTER, MARK}[i];
    const std::vector<std::int64_t> causes = ranked_causes(ctx, pairs_.at({g, c}));
    Intervention out;
    out.year = y;
    out.god = g;
    out.kind = std::string(KIND_KEYS[kind]);
    out.target_type = EntityType::civilization;
    out.target_id = c;
    out.region = act_region(ctx, kind, g, c, causes.empty() ? 0 : causes.front());
    out.scripted = false;
    return {out};
}

void PantheonSubsystem::spend(std::int64_t g, std::int64_t c, std::int64_t y) {
    PairState& p = pairs_[{g, c}];
    p.regard = 0;
    p.contributions.clear();
    p.last_act = y;
    GodState& gs = god(g);
    gs.last_act = y;
    gs.acts_count += 1;
}

// 6.3 apply, 6.4.
void PantheonSubsystem::apply(StepContext& ctx, const Intervention& iv) {
    prepare(ctx);
    const std::int64_t g = iv.god;
    const std::int64_t c = iv.target_id;
    const std::int64_t y = ctx.year();
    const std::size_t kind = static_cast<std::size_t>(enum_index(iv.kind, KIND_KEYS));
    const auto impossible = [&] {
        fail("history: pantheon: " + iv.kind + " of god " + int_text(g) + " on civilization " + int_text(c) +
             " could not be applied");
    };
    if (!alive_visible(ctx, c) || !eligible(ctx, kind, g, c)) {
        impossible();
    }
    const auto pit = pairs_.find({g, c});
    const std::int64_t r = pit == pairs_.end() ? 0 : pit->second.regard;
    std::vector<std::int64_t> causes;
    if (pit != pairs_.end()) {
        causes = ranked_causes(ctx, pit->second);
    }
    const std::int64_t lead = causes.empty() ? 0 : causes.front();
    std::stable_sort(causes.begin(), causes.end());
    const std::int64_t telling = (c + god(g).acts_count) % 3;
    const std::int64_t script = iv.scripted ? 1 : 0;
    const GodActs& acts = *god(g).acts;
    const std::int64_t region = act_region(ctx, kind, g, c, lead);
    if (region == 0) {
        impossible();
    }
    EventSpec e;
    e.year = y;
    e.end_year = y;
    e.region = region;
    e.causes = causes;
    const Participant actor{"actor", EntityType::god, g};
    std::int64_t figure = 0;
    std::int64_t shrine_settlement = 0;
    std::int64_t ev = 0;  // the act's event
    bool favourable = kind == BLESS || kind == CHAMPION;
    switch (kind) {
        case BLESS: {
            e.kind = "divine_bless";
            e.participants = {actor, civ_role("civilization", c)};
            e.values = {acts.bless + 1, tuning_.bless_years, telling, script};
            ev = ctx.append(e);
            effects_.resize(std::max(effects_.size(), static_cast<std::size_t>(c)));
            effects_[static_cast<std::size_t>(c - 1)].push_back(
                Effect{acts.bless == 0 ? 1 : 3, y, checked_add(y, tuning_.bless_years), ev});
            break;
        }
        case CURSE: {
            e.kind = "divine_curse";
            e.participants = {actor, civ_role("civilization", c)};
            std::vector<std::pair<std::int64_t, std::int64_t>> tolls;  // (settlement, dead)
            std::int64_t dead = 0;
            if (acts.curse == 0) {
                const State& state = ctx.state();
                const std::int64_t ns = settlement_count(state);
                for (std::int64_t u = 1; u <= ns; ++u) {
                    const Settlement& s = state.settlement(u);
                    if (s.owner_civ != c || s.region != region || !standing(s) || !ctx.visible(s)) {
                        continue;
                    }
                    const std::int64_t d = floor_div(checked_mul(s.population, tuning_.pestilence_toll_permille), 1000);
                    tolls.emplace_back(u, d);
                    dead = checked_add(dead, d);
                }
            }
            e.values = {acts.curse + 1, dead, acts.curse == 0 ? 0 : tuning_.curse_years, telling, script};
            ev = ctx.append(e);
            for (const auto& [u, d] : tolls) {
                if (d > 0) {
                    ctx.state().set_population(u, ctx.state().settlement(u).population - d);
                    peoples_.note_reduction(u, ev);
                }
            }
            if (acts.curse != 0) {
                effects_.resize(std::max(effects_.size(), static_cast<std::size_t>(c)));
                effects_[static_cast<std::size_t>(c - 1)].push_back(
                    Effect{acts.curse == 1 ? 2 : 4, y, checked_add(y, tuning_.curse_years), ev});
            }
            stats_.dead = checked_add(stats_.dead, dead);
            break;
        }
        case CHAMPION: {
            figure = figures_->next_figure_id();
            e.kind = "divine_champion";
            e.participants = {actor, Participant{"figure", EntityType::figure, figure}, civ_role("civilization", c)};
            e.values = {telling, script};
            ev = ctx.append(e);
            std::string epithet = tuning_.champion_epithet;
            if (const std::size_t at = epithet.find(GOD_SLOT); at != std::string::npos) {
                epithet.replace(at, GOD_SLOT.size(), god(g).name);
            }
            figures_->raise_divine(ctx, figure, 6, c, ev, epithet);
            break;
        }
        case MONSTER: {
            const std::vector<std::size_t> kinds = sendable_kinds(g);
            std::vector<std::int64_t> w;
            for (const std::size_t k : kinds) {
                w.push_back(sites_->kinds()[k].wake_rate);
            }
            const std::size_t k = kinds[weighted(ctx.draw(keys_[MONSTER_KIND], word(g), word(c), 0), w)];
            const MonstersSubsystem::SentMonster sent = sites_->prepare_sent(ctx, k, region);
            e.kind = "divine_monster";
            e.participants = {actor, Participant{"monster", EntityType::monster, sent.monster},
                              civ_role("civilization", c), Participant{"lair", EntityType::site, sent.lair}};
            e.values = {telling, script};
            ev = ctx.append(e);
            sites_->create_sent(ctx, sent, ev);
            break;
        }
        case MARK: {
            const std::optional<Place> place = mark_place(ctx, c, lead);
            const bool profane = r < 0;
            favourable = !profane;
            const std::int64_t s = sites_->next_site_id();
            const std::string name = place_name(ctx, ctx.state().civ(c).culture,
                                                profane ? "profane_forms" : "sacred_forms", keys_[SITE_NAME], s, g);
            e.kind = "divine_mark";
            e.settlement = place->settlement;
            e.participants = {actor, civ_role("civilization", c), Participant{"site", EntityType::site, s}};
            e.values = {profane ? 1 : 0, telling, script};
            ev = ctx.append(e);
            MonstersSubsystem::SiteSpec spec;
            spec.id = s;
            spec.kind = 5;
            spec.name = name;
            spec.region = place->region;
            spec.x = place->x;
            spec.y = place->y;
            spec.settlement = place->settlement;
            spec.founder_civ = c;
            spec.origin_year = y;
            spec.origin_event = ev;
            sites_->open_site(spec);
            shrines_.push_back(Shrine{s, place->region, place->settlement, y, profane});
            if (place->settlement != 0) {
                shrine_settlements_.insert(place->settlement);
                shrine_settlement = place->settlement;
            }
            break;
        }
        default: impossible();
    }
    stats_.acts[kind] += 1;
    if (flared_[0] == g && flared_[1] == c && ctx.phase_trigger_event() == flared_[2]) {
        stats_.flares_acted += 1;
    }
    spend(g, c, y);
    pending_acts_.push_back(PendingAct{ev, g, c, kind, favourable, figure, shrine_settlement});
}

// --- The gods' flare-up (6.6) --------------------------------------------------------------------------------

void PantheonSubsystem::flare(const StepContext& ctx) {
    const std::optional<Candidate> cand = best_candidate(ctx, tuning_.flare_regard);
    if (!cand) {
        return;
    }
    const std::int64_t p =
        std::min<std::int64_t>(1000, floor_div(checked_mul(tuning_.flare_permille, activity_), 1000));
    if (!chance_pm(ctx.draw(keys_[FLARE], word(cand->god), word(cand->civ), 0), p)) {
        return;
    }
    const std::vector<std::int64_t> causes = ranked_causes(ctx, pairs_.at({cand->god, cand->civ}));
    if (causes.empty()) {
        return;
    }
    pending_flare_ = causes.front();
    flared_ = {cand->god, cand->civ, causes.front()};
    stats_.flares += 1;
}

TensionReport PantheonSubsystem::tension(const StepContext&) {
    if (pending_flare_ == 0) {
        return TensionReport{};
    }
    return TensionReport{threshold_, pending_flare_};
}

// --- DivineView (7.1) ----------------------------------------------------------------------------------------

std::int64_t PantheonSubsystem::growth_permille(std::int64_t civ, std::int64_t settlement, std::int64_t year) const {
    std::int64_t g = 0;
    if (civ >= 1 && static_cast<std::size_t>(civ) <= effects_.size()) {
        for (const Effect& x : effects_[static_cast<std::size_t>(civ - 1)]) {
            if (x.from <= year && year < x.until) {
                g += x.kind == 1 ? tuning_.plenty_growth_permille
                                 : (x.kind == 2 ? -tuning_.withering_growth_permille : 0);
            }
        }
    }
    if (settlement != 0) {
        for (const Shrine& s : shrines_) {  // shrines never end (9)
            if (!s.profane && s.settlement == settlement) {
                g += tuning_.sacred_growth_permille;
            }
        }
    }
    return std::clamp<std::int64_t>(g, -900, 1000);
}

std::int64_t PantheonSubsystem::force_act(std::int64_t civ, std::int64_t year, bool valour) const {
    std::int64_t best = 0;
    if (civ >= 1 && static_cast<std::size_t>(civ) <= effects_.size()) {
        for (const Effect& x : effects_[static_cast<std::size_t>(civ - 1)]) {
            if (x.kind == (valour ? 3 : 4) && x.from <= year && year < x.until) {
                best = std::max(best, x.event);
            }
        }
    }
    return best;
}

std::int64_t PantheonSubsystem::force_permille(std::int64_t civ, std::int64_t year) const {
    std::int64_t f = 0;
    if (civ >= 1 && static_cast<std::size_t>(civ) <= effects_.size()) {
        for (const Effect& x : effects_[static_cast<std::size_t>(civ - 1)]) {
            if (x.from <= year && year < x.until) {
                f += x.kind == 3 ? tuning_.valour_permille : (x.kind == 4 ? -tuning_.defeat_permille : 0);
            }
        }
    }
    return std::clamp<std::int64_t>(f, -500, 500);
}

bool PantheonSubsystem::shunned(std::int64_t region, std::int64_t year) const {
    return std::any_of(shrines_.begin(), shrines_.end(), [&](const Shrine& s) {
        return s.profane && s.region == region && s.origin <= year && year < s.origin + tuning_.profane_years;
    });
}

// --- Names (4.4) ------------------------------------------------------------------------------------------------

std::string PantheonSubsystem::place_name(const StepContext& ctx, std::int64_t culture, std::string_view field,
                                          std::uint64_t stream, std::int64_t id, std::int64_t g) {
    const auto it = std::lower_bound(culture_keys_.begin(), culture_keys_.end(),
                                     std::pair<std::int64_t, std::string>{culture, std::string()});
    if (it == culture_keys_.end() || it->first != culture) {
        fail("history: pantheon: culture " + int_text(culture) + " does not exist");
    }
    const data::Definition* d = ctx.content().find(data::CULTURE_KIND, it->second);
    if (d == nullptr) {
        fail("history: pantheon: culture '" + it->second + "' is not in the content");
    }
    std::vector<std::string> forms;
    for (const data::Value& v : d->record.get_record("naming").get_array(field)) {
        forms.push_back(v.as_string());
    }
    if (forms.empty()) {
        fail("history: pantheon: culture '" + it->second + "' has no " + std::string(field));
    }
    const std::uint64_t n = forms.size();
    const std::uint64_t start = pick(ctx.draw(stream, word(id), 0, 0), n);
    NameRegistry& registry = peoples_.mutable_name_registry();
    const std::array<SlotValue, 1> slots = {SlotValue{GOD_SLOT, god(g).name}};
    for (std::uint64_t j = 0; j < n; ++j) {
        RenamedName filled = fill_slots(forms[static_cast<std::size_t>((start + j) % n)], slots);
        if (stages::renamed_region_name_valid(filled.name, filled.joins, placenames_) &&
            !registry.contains(filled.name)) {
            registry.add(filled.name);
            return std::move(filled.name);
        }
    }
    const std::string base = fill_slots(forms[static_cast<std::size_t>(start)], slots).name;
    for (std::uint64_t k = 2;; ++k) {
        std::string name = base + " " + roman(k);
        if (name.size() > NAME_MAX_BYTES) {
            fail("names: a name longer than 64 bytes");
        }
        if (!registry.contains(name)) {
            registry.add(name);
            return name;
        }
    }
}

// --- Rows (9) ------------------------------------------------------------------------------------------------------

std::vector<db::FactionRow> PantheonSubsystem::faction_rows() const {
    std::vector<db::FactionRow> out;
    for (const Cult& k : cults_) {
        db::FactionRow r;
        r.id = k.id;
        r.name = k.name;
        r.kind = 1;
        r.civ = k.civ;
        r.god = k.god;
        r.founder = k.founder;
        r.seat = k.seat;
        r.founded_year = k.founded_year;
        r.founding_event = k.founding_event;
        r.ended_year = k.ended_year;
        r.ending_event = k.ending_event;
        r.ending = k.ending;
        r.status = k.status;
        out.push_back(std::move(r));
    }
    return out;
}

}  // namespace worldgen::history
