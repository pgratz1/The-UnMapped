// Figures (spec docs/specs/p5-06-figures.md): ruling lines, rulers named by
// deed, disputed successions and exiles, generals, heroes, casters, regnal
// numbers and epithets.
//
// Every collection is a vector indexed by id or a std::map keyed by ids;
// loops over State run by id up to a count taken before the loop; relations
// are closed on key copies (6.13 item 1).

#include "worldgen/history/figures.hpp"

#include <algorithm>
#include <array>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "conflict_internal.hpp"
#include "worldgen/core/checked.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/streams.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/history/conflict.hpp"
#include "worldgen/history/draw.hpp"
#include "worldgen/history/monsters.hpp"
#include "worldgen/history/peoples.hpp"
#include "worldgen/history/renaming.hpp"
#include "worldgen/stages/regions.hpp"

namespace worldgen::history {

using namespace conflict_detail;

namespace {

constexpr std::array<Step, 4> STEPS = {Step::t3, Step::t4, Step::t6, Step::q4};

constexpr std::int64_t FIG = static_cast<std::int64_t>(EntityType::figure);
constexpr std::int64_t CIV = static_cast<std::int64_t>(EntityType::civilization);

// The streams of 3, in the order of 14, index into keys_.
enum FigureStream : std::size_t {
    REIGN = 0,
    REGNAL,
    LIFE,
    APPOINT,
    SKILL,
    FALL,
    EXILE,
    HERO,
    CASTER,
    BOOK,
    BLIGHT,
    TOWER_NAME,
    NAMES_PERSON,
    NAMES_BOOK,
    HERO_FALL_UNUSED,  // hist.monsters.hero_fall is drawn by the monsters
};
constexpr std::array<std::string_view, 15> STREAM_NAMES = {
    streams::hist_figures_reign,      streams::hist_figures_regnal,  streams::hist_figures_life,
    streams::hist_figures_appoint,    streams::hist_figures_skill,   streams::hist_figures_fall,
    streams::hist_figures_exile,      streams::hist_figures_hero,    streams::hist_figures_caster,
    streams::hist_figures_book,       streams::hist_figures_blight,  streams::hist_figures_tower_name,
    streams::hist_names_person,       streams::hist_names_book,      streams::hist_monsters_hero_fall,
};

constexpr std::array<std::string_view, TEMPERS> TEMPER_KEYS = {"steady", "warlike", "peaceable", "grasping", "just"};
constexpr std::array<std::string_view, 12> DEED_KEYS = {
    "founded_years", "wars_won",       "conquests",        "betrayals",  "realm_fell", "reign_permille",
    "battles_won",   "battles_unbeaten", "slew_might",     "exiled",     "books",      "blights"};
constexpr std::array<std::string_view, 5> ROLE_KEYS = {"ruler", "claimant", "general", "hero", "caster"};
constexpr std::string_view ROOT_SLOT = "{root}";
constexpr std::string_view SLAYER = "{slayer}";

enum Role : std::int64_t { RULER = 1, CLAIMANT = 2, GENERAL = 3, HERO_ROLE = 4, CASTER_ROLE = 5 };
enum Status : std::int64_t { ALIVE = 1, IN_EXILE = 2, DIED = 3, FELL = 4, SLAIN = 5, FELL_WITH_REALM = 6 };

bool chance_ppm(std::uint64_t u, std::int64_t p) {
    constexpr std::int64_t ONE_RAW = std::int64_t{1} << 32;
    const std::int64_t raw =
        std::min(ONE_RAW, floor_div(checked_mul(std::max<std::int64_t>(0, p), ONE_RAW), 1000000));
    return chance(u, Fx::from_raw(raw));
}

std::size_t index_of(std::span<const std::string_view> keys, std::string_view key) {
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (keys[i] == key) {
            return i;
        }
    }
    fail("history: figures: '" + std::string(key) + "' is not a known key");
}

std::array<std::int64_t, TEMPERS> temper_record(const data::Record& r) {
    std::array<std::int64_t, TEMPERS> out{};
    for (std::size_t t = 0; t < TEMPERS; ++t) {
        out[t] = r.get_int(TEMPER_KEYS[t]);
    }
    return out;
}

RelationKey figure_civ_key(std::int64_t figure, std::int64_t civ, std::string_view kind) {
    return RelationKey{FIG, figure, CIV, civ, std::string(kind)};
}

RelationKey figure_pair_key(std::int64_t a, std::int64_t b, std::string_view kind) {
    return RelationKey{FIG, a, FIG, b, std::string(kind)};
}

Participant fig_role(std::string_view role, std::int64_t figure) {
    return Participant{role, EntityType::figure, figure};
}

}  // namespace

std::span<const std::string_view> figure_deed_keys() noexcept {
    return DEED_KEYS;
}

FiguresTuning figures_tuning_from(const data::ContentSet& content) {
    const data::Definition* d = content.find(data::HISTORY_KIND, data::HISTORY_FIGURES_ID);
    if (d == nullptr) {
        fail("history: tuning: the content has no definition history.figures");
    }
    const data::Record& r = d->record;
    FiguresTuning t;
    t.name_rulers = r.get_bool("name_rulers");
    t.reign_min_permille = r.get_int("reign_min_permille");
    t.reign_span_permille = r.get_int("reign_span_permille");
    t.accession_age_permille = r.get_int("accession_age_permille");
    t.accession_span_permille = r.get_int("accession_span_permille");
    t.temper_strain_permille = temper_record(r.get_record("temper_strain_permille"));
    t.temper_allied_permille = temper_record(r.get_record("temper_allied_permille"));
    t.temper_spoils_permille = temper_record(r.get_record("temper_spoils_permille"));
    t.short_reign_permille = r.get_int("short_reign_permille");
    t.short_reign_split_permille = r.get_int("short_reign_split_permille");
    t.exile_permille = r.get_int("exile_permille");
    t.exile_hops = r.get_int("exile_hops");
    t.general_scale_permille = r.get_int("general_scale_permille");
    t.skill_min = r.get_int("skill_min");
    t.skill_max = r.get_int("skill_max");
    t.career_permille = r.get_int("career_permille");
    t.fall_loser_permille = r.get_int("fall_loser_permille");
    t.fall_winner_permille = r.get_int("fall_winner_permille");
    t.general_age_permille = r.get_int("general_age_permille");
    t.general_age_span_permille = r.get_int("general_age_span_permille");
    t.hero_age_permille = r.get_int("hero_age_permille");
    t.hero_age_span_permille = r.get_int("hero_age_span_permille");
    t.life_min_permille = r.get_int("life_min_permille");
    t.life_span_permille = r.get_int("life_span_permille");
    t.hero_scale_permille = r.get_int("hero_scale_permille");
    t.hero_hunt_permille = r.get_int("hero_hunt_permille");
    t.hero_might_permille = r.get_int("hero_might_permille");
    t.hero_fall_permille = r.get_int("hero_fall_permille");
    t.caster_ppm = r.get_int("caster_ppm");
    t.caster_age_permille = r.get_int("caster_age_permille");
    t.caster_age_span_permille = r.get_int("caster_age_span_permille");
    t.book_permille = r.get_int("book_permille");
    t.books_max = r.get_int("books_max");
    t.blight_ppm = r.get_int("blight_ppm");
    t.blight_toll_permille = r.get_int("blight_toll_permille");
    t.regnal_max = r.get_int("regnal_max");
    for (const data::Value& v : r.get_array("epithets")) {
        const data::Record& e = v.as_record();
        EpithetRule rule;
        rule.role = static_cast<std::int64_t>(index_of(ROLE_KEYS, e.get_string("role"))) + 1;
        rule.deed = static_cast<std::int64_t>(index_of(DEED_KEYS, e.get_string("deed")));
        rule.min = e.get_int("min");
        rule.substantial = e.get_int("substantial");
        rule.text = e.get_string("text");
        t.epithets.push_back(std::move(rule));
    }
    t.epithet_repeat_max = r.get_int("epithet_repeat_max");
    return t;
}

// --- Setup ------------------------------------------------------------------------------------------

FiguresSubsystem::FiguresSubsystem(PeoplesSubsystem& peoples, const data::ContentSet& content)
    : FiguresSubsystem(peoples, figures_tuning_from(content)) {}

FiguresSubsystem::FiguresSubsystem(PeoplesSubsystem& peoples, FiguresTuning tuning)
    : peoples_(peoples), tuning_(std::move(tuning)) {
    if (tuning_.skill_min > tuning_.skill_max) {
        fail("history: figures: skill_min is above skill_max");
    }
}

std::span<const Step> FiguresSubsystem::steps() const {
    return STEPS;
}

void FiguresSubsystem::prepare(const StepContext& ctx) {
    if (prepared_) {
        return;
    }
    const HistoryInputs& in = ctx.inputs();
    for (std::size_t i = 0; i < STREAM_NAMES.size(); ++i) {
        keys_[i] = stream_key(in.seed, STREAM_NAMES[i]);
    }
    end_year_ = in.params.span;
    for (const db::RaceRow& row : in.races) {
        const data::Definition* d = ctx.content().find(data::RACE_KIND, row.key);
        if (d == nullptr) {
            fail("history: figures: race '" + row.key + "' is not in the content");
        }
        RaceData r;
        r.id = row.id;
        r.lifespan = std::max<std::int64_t>(1, row.lifespan_years);
        r.generation = std::max<std::int64_t>(1, row.generation_years);
        r.awakening = row.awakening_year.value_or(0);
        r.temper_weights = temper_record(d->record.get_record("temper_weights"));
        r.general = d->record.get_int("general_permille");
        r.hero = d->record.get_int("hero_permille");
        r.magic = d->record.get_int("magic_permille");
        // 7.2: mean_r, once per race.
        std::int64_t sum_w = 0;
        std::int64_t sum = 0;
        for (std::size_t t = 0; t < TEMPERS; ++t) {
            sum_w = checked_add(sum_w, r.temper_weights[t]);
            sum = checked_add(sum, checked_mul(r.temper_weights[t], tuning_.temper_strain_permille[t]));
        }
        r.mean_strain = sum_w > 0 ? floor_div(sum, sum_w) : 1000;
        races_.push_back(r);
    }
    std::stable_sort(races_.begin(), races_.end(), [](const RaceData& a, const RaceData& b) { return a.id < b.id; });
    for (const db::CultureRow& row : in.cultures) {
        const data::Definition* d = ctx.content().find(data::CULTURE_KIND, row.key);
        if (d == nullptr) {
            fail("history: figures: culture '" + row.key + "' is not in the content");
        }
        cultures_.push_back(CultureData{row.id, row.key, culture_naming_from(d->record.get_record("naming"))});
    }
    std::stable_sort(cultures_.begin(), cultures_.end(),
                     [](const CultureData& a, const CultureData& b) { return a.id < b.id; });
    if (sites_ != nullptr) {
        for (const MonsterKindDef& k : sites_->kinds()) {
            monster_kinds_.emplace_back(k.might, k.slayer_epithet);
        }
    }
    placenames_ = stages::placename_data_from(ctx.content());
    prepared_ = true;
}

const FiguresSubsystem::RaceData& FiguresSubsystem::race(std::int64_t race_id) const {
    const auto it = std::lower_bound(races_.begin(), races_.end(), race_id,
                                     [](const RaceData& r, std::int64_t id) { return r.id < id; });
    if (it == races_.end() || it->id != race_id) {
        fail("history: figures: race " + int_text(race_id) + " does not exist");
    }
    return *it;
}

const FiguresSubsystem::CultureData& FiguresSubsystem::culture(std::int64_t culture_id) const {
    const auto it = std::lower_bound(cultures_.begin(), cultures_.end(), culture_id,
                                     [](const CultureData& c, std::int64_t id) { return c.id < id; });
    if (it == cultures_.end() || it->id != culture_id) {
        fail("history: figures: culture " + int_text(culture_id) + " does not exist");
    }
    return *it;
}

FiguresSubsystem::Figure& FiguresSubsystem::figure(std::int64_t id) {
    if (id < 1 || static_cast<std::size_t>(id) > figures_.size()) {
        fail("history: figures: figure " + int_text(id) + " does not exist");
    }
    return figures_[static_cast<std::size_t>(id - 1)];
}

const FiguresSubsystem::Figure& FiguresSubsystem::figure(std::int64_t id) const {
    if (id < 1 || static_cast<std::size_t>(id) > figures_.size()) {
        fail("history: figures: figure " + int_text(id) + " does not exist");
    }
    return figures_[static_cast<std::size_t>(id - 1)];
}

FiguresSubsystem::Line& FiguresSubsystem::line(std::int64_t civ) {
    if (civ < 1) {
        fail("history: figures: civilization " + int_text(civ) + " does not exist");
    }
    if (static_cast<std::size_t>(civ) > lines_.size()) {
        lines_.resize(static_cast<std::size_t>(civ));
        hero_.resize(static_cast<std::size_t>(civ), 0);
        caster_.resize(static_cast<std::size_t>(civ), 0);
    }
    return lines_[static_cast<std::size_t>(civ - 1)];
}

const FiguresSubsystem::Line* FiguresSubsystem::line_if(std::int64_t civ) const {
    if (civ < 1 || static_cast<std::size_t>(civ) > lines_.size() || !lines_[static_cast<std::size_t>(civ - 1)].has) {
        return nullptr;
    }
    return &lines_[static_cast<std::size_t>(civ - 1)];
}

bool FiguresSubsystem::alive_visible(const StepContext& ctx, std::int64_t civ) const {
    const Civilization& c = ctx.state().civ(civ);
    return !c.ended_year && ctx.visible(c);
}

FiguresSubsystem::Tally& FiguresSubsystem::tally_of(std::int64_t civ) {
    Line& l = line(civ);
    if (l.reign.figure != 0) {
        return figure(l.reign.figure).tally;
    }
    return l.reign.tally;
}

// --- FigureView (7.1) ------------------------------------------------------------------------------

std::int64_t FiguresSubsystem::strain_permille(std::int64_t civ) const {
    const Line* l = line_if(civ);
    if (l == nullptr) {
        return 1000;
    }
    const auto t = static_cast<std::size_t>(l->reign.temper);
    return floor_div(checked_mul(tuning_.temper_strain_permille[t], 1000),
                     std::max<std::int64_t>(1, race(l->race).mean_strain));
}

std::int64_t FiguresSubsystem::allied_strain_permille(std::int64_t civ) const {
    const Line* l = line_if(civ);
    return l == nullptr ? 1000 : tuning_.temper_allied_permille[static_cast<std::size_t>(l->reign.temper)];
}

std::int64_t FiguresSubsystem::spoils_permille(std::int64_t civ) const {
    const Line* l = line_if(civ);
    return l == nullptr ? 0 : tuning_.temper_spoils_permille[static_cast<std::size_t>(l->reign.temper)];
}

std::int64_t FiguresSubsystem::split_bonus_permille(std::int64_t civ) const {
    const Line* l = line_if(civ);
    if (l == nullptr) {
        return 0;
    }
    // 7.4: Y is the year of this subsystem's last step, its T3 of the year
    // (peoples' crises are in T4).
    const std::int64_t short_years =
        floor_div(checked_mul(tuning_.short_reign_permille, race(l->race).generation), 1000);
    return year_ - l->reign.start < short_years ? tuning_.short_reign_split_permille : 0;
}

FigureView::General FiguresSubsystem::general_of(std::int64_t war, std::int64_t civ) const {
    const auto it = wars_.find(war);
    if (it == wars_.end()) {
        return {};
    }
    const KnownWar& w = it->second;
    const std::int64_t f = civ == w.attacker ? w.general[0] : (civ == w.defender ? w.general[1] : 0);
    if (f == 0) {
        return {};
    }
    return General{f, figure(f).skill};
}

FigureView::Hero FiguresSubsystem::hero_of(std::int64_t civ) const {
    if (civ < 1 || static_cast<std::size_t>(civ) > hero_.size() || hero_[static_cast<std::size_t>(civ - 1)] == 0) {
        return {};
    }
    return Hero{hero_[static_cast<std::size_t>(civ - 1)], tuning_.hero_hunt_permille, tuning_.hero_might_permille,
                tuning_.hero_fall_permille};
}

bool FiguresSubsystem::figure_exists(std::int64_t f) const {
    return f >= 1 && static_cast<std::size_t>(f) <= figures_.size();
}

// --- The run (6.1) ------------------------------------------------------------------------------------

void FiguresSubsystem::run(StepContext& ctx) {
    prepare(ctx);
    year_ = ctx.year();
    (void)line(civ_count(ctx.state()) > 0 ? civ_count(ctx.state()) : 1);  // the per-civilization vectors
    switch (ctx.step()) {
        case Step::t3:
            read_log(ctx);
            lines_tick(ctx);
            deaths_of_age(ctx);
            vacancies(ctx);
            caster_rises(ctx);
            books(ctx);
            blights(ctx);
            break;
        case Step::t4: read_log(ctx); break;
        case Step::t6:
            read_log(ctx);
            stand_down(ctx);
            if (ctx.year() == end_year_ - 1 && !ended_) {
                present(ctx);
                epithets(ctx);
                ended_ = true;
            }
            break;
        case Step::q4:
            read_log(ctx);
            stand_down(ctx);
            lines_quiet(ctx);
            deaths_of_age(ctx);
            books(ctx);
            break;
        default: break;
    }
}

// --- The log (6.2) ------------------------------------------------------------------------------------

void FiguresSubsystem::read_log(StepContext& ctx) {
    const std::int64_t last = ctx.last_event_id();
    std::set<std::int64_t> handled;  // civ_seceded handled with their crisis
    for (std::int64_t e = cursor_; e <= last; ++e) {
        const std::string_view k = ctx.event_kind(e);
        if (k == "civ_founded") {
            on_founded(ctx, ctx.event_participant(e, "civilization"), e);
        } else if (k == "succession_crisis") {
            on_crisis(ctx, e, last);
            for (std::int64_t x = e + 1; x <= last; ++x) {
                if (ctx.event_kind(x) == "civ_seceded") {
                    const std::vector<std::int64_t> causes = ctx.event_causes(x);
                    if (std::find(causes.begin(), causes.end(), e) != causes.end()) {
                        handled.insert(x);
                    }
                }
            }
        } else if (k == "civ_seceded") {
            if (handled.count(e) == 0) {
                on_founded(ctx, ctx.event_participant(e, "civilization"), e);  // a guard (6.2)
            }
        } else if (k == "war_declared") {
            on_war_declared(ctx, e);
        } else if (k == "alliance_betrayed") {
            const std::int64_t c = ctx.event_participant(e, "betrayer");
            if (line(c).has && line(c).reign.figure == 0 && tuning_.name_rulers && !ctx.state().civ(c).ended_year) {
                const std::int64_t f = name_reign(ctx, c, e);
                ctx.state().open_relation(figure_civ_key(f, c, "ruled"), e);
            }
            if (line(c).has) {
                tally_of(c).betrayals += 1;
            }
        } else if (k == "spoils_shared" || k == "spoils_withheld") {
            const std::int64_t c = ctx.event_participant(e, "principal");
            if (line(c).has && line(c).reign.figure == 0 && tuning_.name_rulers && !ctx.state().civ(c).ended_year) {
                const std::int64_t f = name_reign(ctx, c, e);
                ctx.state().open_relation(figure_civ_key(f, c, "ruled"), e);
            }
        } else if (k == "peace" || k == "war_won") {
            const std::int64_t c = ctx.event_participant(e, "winner");
            if (c != 0 && line(c).has) {
                tally_of(c).wars_won += 1;
            }
        } else if (k == "civ_conquered") {
            const std::int64_t c = ctx.event_participant(e, "conqueror");
            if (c != 0 && line(c).has) {
                tally_of(c).conquests += 1;
            }
            on_realm_end(ctx, ctx.event_participant(e, "civilization"), e);
        } else if (k == "civ_ended") {
            on_realm_end(ctx, ctx.event_participant(e, "civilization"), e);
        } else if (k == "battle_led") {
            on_battle_led(ctx, e);
        } else if (k == "monster_raid") {
            on_raid(ctx, e);
        } else if (k == "monster_slain_by_hero") {
            const std::int64_t f = ctx.event_participant(e, "hero");
            const std::int64_t m = ctx.event_participant(e, "monster");
            Figure& h = figure(f);
            h.slain += 1;
            if (sites_ != nullptr) {
                const std::size_t kind = sites_->monster_kind_of(m);
                const std::int64_t might = kind < monster_kinds_.size() ? monster_kinds_[kind].first : 0;
                if (might >= h.slew_might) {  // the last slain at a tie
                    h.slew_might = might;
                    h.slew_kind = kind;
                }
            }
        } else if (k == "hero_slain") {
            const std::int64_t f = ctx.event_participant(e, "figure");
            Figure& h = figure(f);
            h.status = SLAIN;
            h.died = ctx.event_year(e);
            h.death_event = e;
            h.death_region = ctx.event_region(e);
            h.death_settlement = 0;
            if (h.civ >= 1 && static_cast<std::size_t>(h.civ) <= hero_.size() &&
                hero_[static_cast<std::size_t>(h.civ - 1)] == f) {
                hero_[static_cast<std::size_t>(h.civ - 1)] = 0;
            }
        }
    }
    cursor_ = last + 1;
}

void FiguresSubsystem::on_founded(StepContext& ctx, std::int64_t c, std::int64_t event) {
    if (c == 0) {
        return;
    }
    start_reign(ctx, c, 1, ctx.year());
    if (!tuning_.name_rulers || ctx.step() == Step::q4) {
        return;  // reading R21: a founding read in a quiet step names no founder there
    }
    const std::int64_t f = name_reign(ctx, c, 0);
    EventSpec e = point_event(ctx, "came_to_rule", ctx.state().civ(c).home_region, 0);
    e.participants = {fig_role("figure", f), civ_role("civilization", c)};
    e.causes = {event};
    const std::int64_t ev = ctx.append(e);
    figure(f).start_event = ev;
    ctx.state().open_relation(figure_civ_key(f, c, "ruled"), ev);
}

void FiguresSubsystem::on_war_declared(StepContext& ctx, std::int64_t w) {
    const std::int64_t a = ctx.event_participant(w, "attacker");
    const std::int64_t b = ctx.event_participant(w, "defender");
    if (!line(a).has) {
        start_reign(ctx, a, 1, ctx.year());  // a guard: every realm has a line from its founding
    }
    if (line(a).reign.figure == 0 && tuning_.name_rulers && !ctx.state().civ(a).ended_year) {
        const std::int64_t f = name_reign(ctx, a, w);
        ctx.state().open_relation(figure_civ_key(f, a, "ruled"), w);
    }
    tally_of(a).wars_declared += 1;
    KnownWar kw;
    kw.attacker = a;
    kw.defender = b;
    kw.goal = ctx.event_region(w);
    wars_[w] = kw;
    if (conflict_ == nullptr || ctx.step() != Step::t3) {
        return;  // 6.6: generals are named in T3 only
    }
    for (std::size_t side = 0; side < 2; ++side) {
        const std::int64_t c = side == 0 ? a : b;
        if (!ctx.state().civ(c).ended_year) {
            appoint(ctx, w, side);
        }
    }
}

void FiguresSubsystem::on_realm_end(StepContext& ctx, std::int64_t c, std::int64_t event) {
    if (c == 0 || !line(c).has) {
        return;
    }
    if (line(c).reign.figure == 0 && tuning_.name_rulers) {
        const std::int64_t f = name_reign(ctx, c, event);
        ctx.state().open_relation(figure_civ_key(f, c, "ruled"), event);
    }
    const std::int64_t f = line(c).reign.figure;
    if (f != 0) {
        ctx.state().close_relation(figure_civ_key(f, c, "ruled"), event);
        Figure& r = figure(f);
        r.status = FELL_WITH_REALM;
        r.died = ctx.event_year(event);
        r.death_event = event;
        r.death_region = ctx.event_region(event);
        r.death_settlement = 0;
        r.realm_fell = true;
        close_relations_of(ctx, f, event, true);
    }
    Line& l = line(c);
    if (l.log_index < reign_log_.size()) {
        reign_log_[l.log_index].end = ctx.event_year(event);
    }
    l.has = false;
}

// --- Lines (6.3) --------------------------------------------------------------------------------------

FiguresSubsystem::Reign FiguresSubsystem::draw_reign(const StepContext& ctx, std::int64_t c, std::int64_t n,
                                                     std::int64_t s) const {
    const RaceData& r = race(ctx.state().civ(c).race);
    const std::uint64_t k = keys_[REIGN];
    Reign out;
    out.number = n;
    out.start = s;
    const auto span_len = static_cast<std::uint64_t>(tuning_.reign_span_permille + 1);
    const auto span_age = static_cast<std::uint64_t>(tuning_.accession_span_permille + 1);
    const std::int64_t len = floor_div(
        checked_mul(r.generation,
                    checked_add(tuning_.reign_min_permille,
                                static_cast<std::int64_t>(pick(ctx.draw_at(k, s, word(c), word(n), 0), span_len)))),
        1000);
    out.end = checked_add(s, std::max<std::int64_t>(1, len));
    out.age = floor_div(
        checked_mul(r.lifespan,
                    checked_add(tuning_.accession_age_permille,
                                static_cast<std::int64_t>(pick(ctx.draw_at(k, s, word(c), word(n), 1), span_age)))),
        1000);
    out.temper = static_cast<std::int64_t>(weighted(ctx.draw_at(k, s, word(c), word(n), 2), r.temper_weights));
    return out;
}

void FiguresSubsystem::start_reign(const StepContext& ctx, std::int64_t c, std::int64_t n, std::int64_t s) {
    Reign r = draw_reign(ctx, c, n, s);
    Line& l = line(c);
    if (l.has && l.log_index < reign_log_.size()) {
        reign_log_[l.log_index].end = s;
    }
    l.has = true;
    l.race = ctx.state().civ(c).race;
    l.reign = r;
    l.log_index = reign_log_.size();
    reign_log_.push_back(ReignRecord{c, n, s, r.end, r.temper, 0});
    stats_.reigns += 1;
}

void FiguresSubsystem::lines_tick(StepContext& ctx) {
    const std::int64_t nc = civ_count(ctx.state());
    const std::int64_t y = ctx.year();
    for (std::int64_t c = 1; c <= nc; ++c) {
        if (ctx.state().civ(c).ended_year || !line(c).has || line(c).reign.end > y) {
            continue;
        }
        if (line(c).reign.figure != 0) {
            die(ctx, line(c).reign.figure, 0);
        }
        start_reign(ctx, c, line(c).reign.number + 1, y);
    }
}

void FiguresSubsystem::lines_quiet(StepContext& ctx) {
    const std::int64_t nc = civ_count(ctx.state());
    const std::int64_t y = ctx.year();
    const std::int64_t z = y + ctx.years() - 1;
    for (std::int64_t c = 1; c <= nc; ++c) {
        if (ctx.state().civ(c).ended_year || !line(c).has) {
            continue;
        }
        while (line(c).reign.end <= z) {
            std::int64_t s = 0;
            if (line(c).reign.figure != 0) {
                if (ctx.quiet_events_left(c) <= 0) {
                    break;  // the death waits for the next step; the reign goes on
                }
                die(ctx, line(c).reign.figure, 0);
                s = y;
            } else {
                s = std::max(y, line(c).reign.end);
            }
            start_reign(ctx, c, line(c).reign.number + 1, s);
        }
    }
}

// --- Naming (6.4, 6.11, 6.14) ---------------------------------------------------------------------------

std::int64_t FiguresSubsystem::new_figure(StepContext& ctx, std::int64_t role, std::int64_t c) {
    const auto id = static_cast<std::int64_t>(figures_.size()) + 1;
    if (id > MAX_FIGURES) {
        fail("history: bound: figures exceed 100000");
    }
    const Civilization& civ = ctx.state().civ(c);
    Figure f;
    f.id = id;
    f.role = role;
    f.race = civ.race;
    f.culture = civ.culture;
    f.civ = c;
    f.office_from = ctx.year();
    figures_.push_back(std::move(f));
    return id;
}

void FiguresSubsystem::draw_person_name(StepContext& ctx, std::int64_t id) {
    const CultureData& cu = culture(figure(id).culture);
    const NameChoice choice = choose_name(cu.naming, peoples_.name_filter(), cu.key, NameKind::person,
                                          ctx.inputs().seed, word(cu.id), word(id), peoples_.mutable_name_registry());
    Figure& f = figure(id);
    f.name = choice.name;
    std::string base = choice.name;
    if (choice.fallback) {  // reading R16: a fallback's root drops the numeral
        const std::size_t sp = base.rfind(' ');
        if (sp != std::string::npos) {
            base.resize(sp);
        }
    }
    f.root = realm_root(base, cu.naming.person_forms);
    f.regnal_fallback = choice.fallback;
}

void FiguresSubsystem::life_draws(const StepContext& ctx, std::int64_t id, std::int64_t age_permille,
                                  std::int64_t age_span_permille) {
    Figure& f = figure(id);
    const RaceData& r = race(f.race);
    const std::int64_t y = ctx.year();
    const std::uint64_t k = keys_[LIFE];
    const std::int64_t age0 = floor_div(
        checked_mul(r.lifespan, checked_add(age_permille, static_cast<std::int64_t>(pick(
                                                              ctx.draw_at(k, y, word(id), 0, 0),
                                                              static_cast<std::uint64_t>(age_span_permille + 1))))),
        1000);
    f.born = std::max(r.awakening, y - age0);
    const std::int64_t life = floor_div(
        checked_mul(r.lifespan,
                    checked_add(tuning_.life_min_permille,
                                static_cast<std::int64_t>(pick(ctx.draw_at(k, y, word(id), 0, 1),
                                                               static_cast<std::uint64_t>(tuning_.life_span_permille + 1))))),
        1000);
    f.planned_death = std::max(y + 1, checked_add(f.born, life));
}

std::int64_t FiguresSubsystem::name_reign(StepContext& ctx, std::int64_t c, std::int64_t record) {
    const std::int64_t id = new_figure(ctx, RULER, c);
    const Reign reign = line(c).reign;
    const RaceData& r = race(ctx.state().civ(c).race);
    {
        Figure& f = figure(id);
        f.reign_no = reign.number;
        f.temper = reign.temper;
        f.office_from = reign.start;
        f.born = std::max(r.awakening, reign.start - reign.age);
        f.planned_death = reign.end;
        f.start_event = record;
        f.tally = reign.tally;
    }
    // 6.14: a name of the line reused with a regnal number.
    const CultureData& cu = culture(ctx.state().civ(c).culture);
    bool reused = false;
    if (cu.naming.regnal_permille > 0) {
        std::vector<std::size_t> eligible;
        NameRegistry& registry = peoples_.mutable_name_registry();
        const Line& l = line(c);
        for (std::size_t i = 0; i < l.names.size(); ++i) {
            const auto& [x, k] = l.names[i];
            if (k < tuning_.regnal_max && !registry.contains(x + " " + roman(static_cast<std::uint64_t>(k + 1)))) {
                eligible.push_back(i);
            }
        }
        const std::uint64_t key = keys_[REGNAL];
        if (!eligible.empty() &&
            chance_pm(ctx.draw(key, word(c), word(reign.number), 0), cu.naming.regnal_permille)) {
            const std::size_t i =
                eligible[static_cast<std::size_t>(pick(ctx.draw(key, word(c), word(reign.number), 1), eligible.size()))];
            Line& lw = line(c);
            const std::string x = lw.names[i].first;
            const std::int64_t k = lw.names[i].second + 1;
            lw.names[i].second = k;
            registry.add(x + " " + roman(static_cast<std::uint64_t>(k)));
            Figure& f = figure(id);
            f.name = x;
            f.root = realm_root(x, cu.naming.person_forms);
            f.regnal = k;
            reused = true;
            stats_.regnal += 1;
        }
    }
    if (!reused) {
        draw_person_name(ctx, id);
        if (!figure(id).regnal_fallback) {
            line(c).names.emplace_back(figure(id).name, 1);
        }
    }
    line(c).reign.figure = id;
    line(c).reign.tally = Tally{};
    if (line(c).log_index < reign_log_.size()) {
        reign_log_[line(c).log_index].figure = id;
    }
    stats_.named += 1;
    return id;
}

// --- Disputed successions and exiles (6.5) ---------------------------------------------------------------

std::int64_t FiguresSubsystem::reach(const StepContext& ctx, std::int64_t from, std::int64_t to) const {
    const State& state = ctx.state();
    const PeoplesGeo& g = peoples_.geo();
    const std::int64_t nr = g.region_count();
    const auto presence = [&](std::int64_t civ) {
        std::vector<std::uint8_t> in(static_cast<std::size_t>(nr) + 1, 0);
        for (std::int64_t r = 1; r <= nr; ++r) {
            if (state.region(r).owner == civ) {
                in[static_cast<std::size_t>(r)] = 1;
            }
        }
        const std::int64_t ns = settlement_count(state);
        for (std::int64_t id = 1; id <= ns; ++id) {
            const Settlement& s = state.settlement(id);
            if (s.owner_civ == civ && standing(s) && ctx.visible(s) && s.region >= 1 && s.region <= nr) {
                in[static_cast<std::size_t>(s.region)] = 1;
            }
        }
        return in;
    };
    const std::vector<std::uint8_t> pa = presence(from);
    const std::vector<std::uint8_t> pb = presence(to);
    std::vector<std::int64_t> dist(static_cast<std::size_t>(nr) + 1, -1);
    std::vector<std::int64_t> queue;
    for (std::int64_t r = 1; r <= nr; ++r) {
        if (pa[static_cast<std::size_t>(r)] != 0) {
            dist[static_cast<std::size_t>(r)] = 0;
            queue.push_back(r);
        }
    }
    for (std::size_t head = 0; head < queue.size(); ++head) {
        const std::int64_t u = queue[head];
        const std::int64_t du = dist[static_cast<std::size_t>(u)];
        if (pb[static_cast<std::size_t>(u)] != 0) {
            return du;  // breadth first: the first reached is the nearest
        }
        if (du >= tuning_.exile_hops) {
            continue;
        }
        for (const PeoplesNeighbour& v : g.neighbours(u)) {
            std::int64_t& dv = dist[static_cast<std::size_t>(v.region)];
            if (dv < 0) {
                dv = du + 1;
                queue.push_back(v.region);
            }
        }
    }
    return -1;
}

void FiguresSubsystem::on_crisis(StepContext& ctx, std::int64_t e, std::int64_t last) {
    const std::int64_t c = ctx.event_participant(e, "civilization");
    const std::int64_t y = ctx.year();
    std::int64_t sigma = 0;
    for (std::int64_t x = e + 1; x <= last && sigma == 0; ++x) {
        if (ctx.event_kind(x) == "civ_seceded") {
            const std::vector<std::int64_t> causes = ctx.event_causes(x);
            if (std::find(causes.begin(), causes.end(), e) != causes.end()) {
                sigma = x;
            }
        }
    }
    if (!line(c).has) {
        start_reign(ctx, c, 1, y);  // a guard
    }
    // 1. The reign ends, unless it started in this year (6.3).
    std::int64_t dead = 0;
    if (line(c).reign.start != y) {
        dead = line(c).reign.figure;
        if (dead != 0) {
            die(ctx, dead, 1);
        }
        start_reign(ctx, c, line(c).reign.number + 1, y);
    }
    const std::int64_t c2 = sigma != 0 ? ctx.event_participant(sigma, "civilization") : 0;
    if (c2 != 0) {
        start_reign(ctx, c2, 1, y);  // the new realm's first reign
    }
    if (!tuning_.name_rulers) {
        return;
    }
    const bool exile = sigma == 0 && chance_pm(ctx.draw(keys_[EXILE], word(e), word(c), 0), tuning_.exile_permille);
    if (sigma == 0 && !exile) {
        return;  // 4. nothing is named
    }
    // The winner: the new reign of c (or, in the year of an accession, the
    // current one, named already or now).
    const bool winner_new = line(c).reign.figure == 0;
    const std::int64_t fw = winner_new ? name_reign(ctx, c, 0) : line(c).reign.figure;
    std::int64_t fc = 0;
    if (c2 != 0) {
        fc = name_reign(ctx, c2, 0);
    } else {
        fc = new_figure(ctx, CLAIMANT, c);
        life_draws(ctx, fc, tuning_.accession_age_permille, tuning_.accession_span_permille);
        draw_person_name(ctx, fc);
    }
    EventSpec d = point_event(ctx, "succession_disputed", ctx.event_region(e), ctx.event_settlement(e));
    d.participants = {civ_role("civilization", c), fig_role("ruler", fw), fig_role("claimant", fc)};
    if (c2 != 0) {
        d.participants.push_back(civ_role("new_realm", c2));
        d.causes = {e, sigma};
    } else {
        d.causes = {e};
    }
    d.values = {c2 != 0 ? 1 : 0};
    const std::int64_t ev = ctx.append(d);
    stats_.disputes += 1;
    if (winner_new) {
        figure(fw).start_event = ev;
        ctx.state().open_relation(figure_civ_key(fw, c, "ruled"), ev);
    }
    figure(fc).start_event = ev;
    if (c2 != 0) {
        ctx.state().open_relation(figure_civ_key(fc, c2, "ruled"), ev);
    }
    ctx.state().open_relation(figure_pair_key(std::min(fw, fc), std::max(fw, fc), "rival"), ev);
    if (dead != 0) {
        ctx.state().open_relation(figure_pair_key(dead, fw, "parent"), ev);
        ctx.state().open_relation(figure_pair_key(dead, fc, "parent"), ev);
    }
    if (c2 != 0) {
        return;
    }
    // 3. The exile: the friendliest realm in reach not at war with c.
    std::int64_t host = 0;
    if (conflict_ != nullptr) {
        std::int64_t best = 0;
        const std::int64_t nc = civ_count(ctx.state());
        for (std::int64_t h = 1; h <= nc; ++h) {
            if (h == c || !alive_visible(ctx, h) || conflict_->at_war(c, h)) {
                continue;
            }
            const std::int64_t hops = reach(ctx, c, h);
            if (hops < 0 || hops > tuning_.exile_hops) {
                continue;
            }
            const std::int64_t disp = conflict_->disposition_of(ctx, c, h);
            if (host == 0 || disp > best) {
                host = h;
                best = disp;
            }
        }
    }
    std::int64_t region = ctx.event_region(e);
    if (host != 0 && ctx.state().civ(host).capital != 0) {
        region = ctx.state().settlement(ctx.state().civ(host).capital).region;
    }
    EventSpec x = point_event(ctx, "exiled", region, 0);
    x.participants = {fig_role("figure", fc), civ_role("civilization", c)};
    if (host != 0) {
        x.participants.push_back(civ_role("host", host));
    }
    x.values = {host != 0 ? 1 : 0};
    x.causes = {ev};
    (void)ctx.append(x);
    Figure& f = figure(fc);
    f.status = IN_EXILE;
    f.exiled = true;
    f.host = host;
    f.host_region = host != 0 ? region : 0;
    stats_.exiles += 1;
}

// --- Lives and deaths (6.9) ---------------------------------------------------------------------------

void FiguresSubsystem::close_relations_of(StepContext& ctx, std::int64_t id, std::int64_t event, bool rivals_only) {
    std::vector<RelationKey> keys;
    for (const auto& [key, open] : ctx.state().relations()) {
        (void)open;
        const bool mine = (key.a_type == FIG && key.a_id == id) || (key.b_type == FIG && key.b_id == id);
        const bool kind = key.kind == "rival" || (!rivals_only && (key.kind == "ruled" || key.kind == "served"));
        if (mine && kind) {
            keys.push_back(key);
        }
    }
    for (const RelationKey& k : keys) {
        ctx.state().close_relation(k, event);
    }
}

void FiguresSubsystem::die(StepContext& ctx, std::int64_t id, std::int64_t how) {
    const State& state = ctx.state();
    const Figure f0 = figure(id);
    std::int64_t region = ctx.event_region(f0.start_event);
    std::int64_t settlement = 0;
    const Civilization& civ = state.civ(f0.civ);
    const bool realm_alive = !civ.ended_year && civ.capital != 0;
    if (f0.status == IN_EXILE) {
        if (f0.host != 0 && !state.civ(f0.host).ended_year && state.civ(f0.host).capital != 0) {
            region = state.settlement(state.civ(f0.host).capital).region;
        } else if (f0.host_region != 0) {
            region = f0.host_region;  // reading R6: the host's capital at the exile
        }
    } else if (f0.role == CASTER_ROLE && f0.tower != 0 && sites_ != nullptr) {
        region = ctx.event_region(f0.tower_event);
    } else if (realm_alive) {
        region = state.settlement(civ.capital).region;
        if (f0.role == RULER) {
            settlement = civ.capital;
        }
    }
    const std::int64_t y = ctx.year();
    EventSpec e = point_event(ctx, "figure_died", region, settlement);
    e.participants = {fig_role("figure", id), civ_role("civilization", f0.civ)};
    e.values = {f0.status == IN_EXILE ? 2 : how, std::max<std::int64_t>(0, y - f0.born)};
    e.causes = {f0.start_event};
    const std::int64_t ev = ctx.append(e);
    Figure& f = figure(id);
    f.status = DIED;
    f.died = y;
    f.death_event = ev;
    f.death_region = region;
    f.death_settlement = settlement;
    close_relations_of(ctx, id, ev);
    if (f.serving != 0) {
        const auto it = wars_.find(f.serving);
        if (it != wars_.end()) {
            for (std::size_t s = 0; s < 2; ++s) {
                if (it->second.general[s] == id) {
                    it->second.general[s] = 0;
                }
            }
        }
        f.serving = 0;
    }
    const auto ci = static_cast<std::size_t>(f.civ - 1);
    if (ci < hero_.size() && hero_[ci] == id) {
        hero_[ci] = 0;
    }
    if (ci < caster_.size() && caster_[ci] == id) {
        caster_[ci] = 0;
    }
}

void FiguresSubsystem::deaths_of_age(StepContext& ctx) {
    const bool quiet = ctx.step() == Step::q4;
    const std::int64_t last = ctx.year() + ctx.years() - 1;
    const auto n = static_cast<std::int64_t>(figures_.size());
    for (std::int64_t id = 1; id <= n; ++id) {
        const Figure& f = figure(id);
        if (f.role == RULER || (f.status != ALIVE && f.status != IN_EXILE) || f.planned_death > last) {
            continue;
        }
        if (quiet && ctx.quiet_events_left(f.civ) <= 0) {
            continue;  // it waits
        }
        die(ctx, id, 0);
    }
}

// --- Generals (6.6) -------------------------------------------------------------------------------------

void FiguresSubsystem::appoint(StepContext& ctx, std::int64_t w, std::size_t side) {
    KnownWar& kw0 = wars_.at(w);
    const std::int64_t c = side == 0 ? kw0.attacker : kw0.defender;
    const std::int64_t enemy = side == 0 ? kw0.defender : kw0.attacker;
    const std::int64_t v = kw0.vacancy[side];
    const RaceData& r = race(ctx.state().civ(c).race);
    const std::int64_t p = floor_div(checked_mul(r.general, tuning_.general_scale_permille), 1000);
    if (!chance_pm(ctx.draw(keys_[APPOINT], word(w), word(c), word(v)), p)) {
        return;
    }
    const std::int64_t y = ctx.year();
    const std::int64_t career = floor_div(checked_mul(r.lifespan, tuning_.career_permille), 1000);
    std::int64_t g = 0;
    const auto n = static_cast<std::int64_t>(figures_.size());
    for (std::int64_t id = 1; id <= n && g == 0; ++id) {
        const Figure& f = figure(id);
        if (f.role == GENERAL && f.civ == c && f.status == ALIVE && f.serving == 0 && y - f.first_command <= career) {
            g = id;
        }
    }
    const bool again = g != 0;
    if (!again) {
        g = new_figure(ctx, GENERAL, c);
        life_draws(ctx, g, tuning_.general_age_permille, tuning_.general_age_span_permille);
        draw_person_name(ctx, g);
        Figure& f = figure(g);
        // Lead ruling G7: a skill is at least 1.
        f.skill = std::max<std::int64_t>(
            1, tuning_.skill_min + static_cast<std::int64_t>(pick(ctx.draw(keys_[SKILL], word(g), 0, 0),
                                                                  static_cast<std::uint64_t>(tuning_.skill_max -
                                                                                             tuning_.skill_min + 1))));
        f.first_command = y;
    }
    EventSpec e = point_event(ctx, "general_appointed", kw0.goal, 0);
    e.participants = {fig_role("figure", g), civ_role("civilization", c), civ_role("enemy", enemy)};
    e.values = {again ? 1 : 0};
    e.causes = {w};
    const std::int64_t ev = ctx.append(e);
    if (!again) {
        figure(g).start_event = ev;
    }
    figure(g).serving = w;
    ctx.state().open_relation(figure_civ_key(g, c, "served"), ev);
    KnownWar& kw = wars_.at(w);
    kw.general[side] = g;
    kw.vacant[side] = false;
    stats_.appointments += 1;
}

void FiguresSubsystem::vacancies(StepContext& ctx) {
    if (conflict_ == nullptr) {
        return;
    }
    std::vector<std::int64_t> ids;
    for (const auto& [w, kw] : wars_) {
        (void)kw;
        ids.push_back(w);
    }
    for (const std::int64_t w : ids) {
        if (!conflict_->war_open(w)) {
            continue;
        }
        for (std::size_t side = 0; side < 2; ++side) {
            KnownWar& kw = wars_.at(w);
            const std::int64_t c = side == 0 ? kw.attacker : kw.defender;
            if (!kw.vacant[side] || kw.general[side] != 0 || ctx.state().civ(c).ended_year) {
                continue;
            }
            kw.vacancy[side] += 1;
            appoint(ctx, w, side);
        }
    }
}

void FiguresSubsystem::on_battle_led(StepContext& ctx, std::int64_t b) {
    const std::vector<std::int64_t> causes = ctx.event_causes(b);
    const std::int64_t w = causes.empty() ? 0 : causes.front();
    const std::int64_t winner = ctx.event_participant(b, "winner");
    const std::int64_t loser = ctx.event_participant(b, "loser");
    for (std::size_t r = 0; r < 2; ++r) {
        const std::int64_t f = ctx.event_participant(b, r == 0 ? "winner_general" : "loser_general");
        if (f == 0) {
            continue;
        }
        Figure& g = figure(f);
        if (r == 0) {
            g.battles_won += 1;
        } else {
            g.battles_lost += 1;
        }
        const std::int64_t p = r == 0 ? tuning_.fall_winner_permille : tuning_.fall_loser_permille;
        if (!chance_pm(ctx.draw(keys_[FALL], word(b), word(f), 0), p)) {
            continue;
        }
        const std::int64_t c = figure(f).civ;
        EventSpec e = point_event(ctx, "fell_in_battle", ctx.event_region(b), 0);
        e.participants = {fig_role("figure", f), civ_role("civilization", c),
                          civ_role("enemy", r == 0 ? loser : winner)};
        e.causes = {b};
        const std::int64_t ev = ctx.append(e);
        Figure& h = figure(f);
        h.status = FELL;
        h.died = ctx.year();
        h.death_event = ev;
        h.death_region = ctx.event_region(b);
        h.death_settlement = 0;
        h.serving = 0;
        close_relations_of(ctx, f, ev);
        const auto it = wars_.find(w);
        if (it != wars_.end()) {
            for (std::size_t s = 0; s < 2; ++s) {
                if (it->second.general[s] == f) {
                    it->second.general[s] = 0;
                    it->second.vacant[s] = true;
                }
            }
        }
        stats_.falls += 1;
    }
}

void FiguresSubsystem::stand_down(StepContext& ctx) {
    if (conflict_ == nullptr) {
        return;
    }
    std::vector<std::int64_t> ids;
    for (const auto& [w, kw] : wars_) {
        (void)kw;
        ids.push_back(w);
    }
    for (const std::int64_t w : ids) {
        if (conflict_->war_open(w)) {
            continue;
        }
        // Lead ruling G6 (R45): a war closed by a principal's end stands its
        // generals down with that realm's ending event.
        std::int64_t end = conflict_->war_end_event(w);
        {
            const KnownWar& k0 = wars_.at(w);
            const Civilization& ca = ctx.state().civ(k0.attacker);
            const Civilization& cd = ctx.state().civ(k0.defender);
            if (ca.ended_year && ca.ending_event != 0) {
                end = ca.ending_event;
            } else if (cd.ended_year && cd.ending_event != 0) {
                end = cd.ending_event;
            }
        }
        for (std::size_t s = 0; s < 2; ++s) {
            KnownWar& kw = wars_.at(w);
            const std::int64_t g = kw.general[s];
            if (g == 0) {
                continue;
            }
            const std::int64_t c = s == 0 ? kw.attacker : kw.defender;
            ctx.state().close_relation(figure_civ_key(g, c, "served"), end);
            figure(g).serving = 0;
            wars_.at(w).general[s] = 0;
        }
        wars_.erase(w);
    }
}

// --- Heroes (6.7) -----------------------------------------------------------------------------------------

void FiguresSubsystem::on_raid(StepContext& ctx, std::int64_t r) {
    const std::int64_t c = ctx.event_participant(r, "civilization");
    const std::int64_t m = ctx.event_participant(r, "monster");
    if (c == 0 || !alive_visible(ctx, c) || hero_[static_cast<std::size_t>(c - 1)] != 0) {
        return;
    }
    const RaceData& ra = race(ctx.state().civ(c).race);
    const std::int64_t p = floor_div(checked_mul(ra.hero, tuning_.hero_scale_permille), 1000);
    if (!chance_pm(ctx.draw(keys_[HERO], word(r), word(c), 0), p)) {
        return;
    }
    const std::int64_t f = new_figure(ctx, HERO_ROLE, c);
    life_draws(ctx, f, tuning_.hero_age_permille, tuning_.hero_age_span_permille);
    draw_person_name(ctx, f);
    EventSpec e = point_event(ctx, "hero_rose", ctx.event_region(r), ctx.event_settlement(r));
    e.participants = {fig_role("figure", f), civ_role("civilization", c),
                      Participant{"monster", EntityType::monster, m}};
    e.causes = {r};
    figure(f).start_event = ctx.append(e);
    hero_[static_cast<std::size_t>(c - 1)] = f;
    stats_.heroes += 1;
}

// --- Casters (6.8) ------------------------------------------------------------------------------------------

void FiguresSubsystem::caster_rises(StepContext& ctx) {
    if (sites_ == nullptr) {
        return;
    }
    const std::int64_t nc = civ_count(ctx.state());
    for (std::int64_t c = 1; c <= nc; ++c) {
        if (!alive_visible(ctx, c) || caster_[static_cast<std::size_t>(c - 1)] != 0) {
            continue;
        }
        const RaceData& r = race(ctx.state().civ(c).race);
        const std::int64_t p = floor_div(checked_mul(tuning_.caster_ppm, r.magic), 1000);
        if (!chance_ppm(ctx.draw(keys_[CASTER], word(c), 0, 0), p)) {
            continue;
        }
        const std::int64_t t = ctx.state().civ(c).capital;
        if (t == 0) {
            continue;
        }
        const Settlement seat = ctx.state().settlement(t);
        const bool library = seat.kind == SettlementKind::city && libraries_.count(t) == 0;
        std::int64_t x = seat.x;
        std::int64_t yc = seat.y;
        if (!library) {
            const std::optional<MonstersSubsystem::HeightCell> cell = sites_->height_cell(ctx, seat.region);
            if (!cell) {
                continue;
            }
            x = cell->x;
            yc = cell->y;
        }
        const std::int64_t f = new_figure(ctx, CASTER_ROLE, c);
        life_draws(ctx, f, tuning_.caster_age_permille, tuning_.caster_age_span_permille);
        draw_person_name(ctx, f);
        const std::int64_t s = sites_->next_site_id();
        const CultureData& cu = culture(ctx.state().civ(c).culture);
        const std::vector<std::string>& forms = library ? cu.naming.library_forms : cu.naming.tower_forms;
        if (forms.empty()) {
            fail("history: figures: culture '" + cu.key + "' has no tower or library forms");
        }
        const std::uint64_t nf = forms.size();
        const std::uint64_t start = pick(ctx.draw(keys_[TOWER_NAME], word(s), 0, 0), nf);
        NameRegistry& registry = peoples_.mutable_name_registry();
        const std::string root = figure(f).root;
        const std::array<SlotValue, 1> slots = {SlotValue{ROOT_SLOT, root}};
        std::string name;
        for (std::uint64_t j = 0; j < nf && name.empty(); ++j) {
            RenamedName filled = fill_slots(forms[static_cast<std::size_t>((start + j) % nf)], slots);
            if (stages::renamed_region_name_valid(filled.name, filled.joins, placenames_) &&
                !registry.contains(filled.name)) {
                name = std::move(filled.name);
            }
        }
        if (name.empty()) {
            const std::string base = fill_slots(forms[static_cast<std::size_t>(start)], slots).name;
            for (std::uint64_t k = 2; name.empty(); ++k) {
                std::string candidate = base + " " + roman(k);
                if (candidate.size() > NAME_MAX_BYTES) {
                    fail("names: a name longer than 64 bytes");
                }
                if (!registry.contains(candidate)) {
                    name = std::move(candidate);
                }
            }
        }
        registry.add(name);
        EventSpec e = point_event(ctx, "tower_founded", seat.region, library ? t : 0);
        e.participants = {fig_role("figure", f), civ_role("civilization", c),
                          Participant{"site", EntityType::site, s}};
        e.values = {library ? 1 : 0};
        e.causes = {std::max(seat.founding_event, ctx.state().civ(c).founding_event)};
        const std::int64_t ev = ctx.append(e);
        MonstersSubsystem::SiteSpec spec;
        spec.id = s;
        spec.kind = 4;
        spec.name = name;
        spec.region = seat.region;
        spec.x = x;
        spec.y = yc;
        spec.settlement = library ? t : 0;
        spec.founder_civ = c;
        spec.origin_year = ctx.year();
        spec.origin_event = ev;
        sites_->open_site(spec);
        if (library) {
            libraries_.insert(t);
        }
        Figure& g = figure(f);
        g.start_event = ev;
        g.tower = s;
        g.tower_event = ev;
        caster_[static_cast<std::size_t>(c - 1)] = f;
        stats_.casters += 1;
    }
}

void FiguresSubsystem::books(StepContext& ctx) {
    const bool quiet = ctx.step() == Step::q4;
    const std::int64_t p =
        quiet ? std::min<std::int64_t>(1000, checked_mul(tuning_.book_permille, ctx.years())) : tuning_.book_permille;
    const auto n = static_cast<std::int64_t>(figures_.size());
    for (std::int64_t id = 1; id <= n; ++id) {
        const Figure& f = figure(id);
        if (f.role != CASTER_ROLE || f.status != ALIVE || f.books >= tuning_.books_max || f.tower == 0) {
            continue;
        }
        if (!chance_pm(ctx.draw(keys_[BOOK], word(id), 0, 0), p) || (quiet && ctx.quiet_events_left(f.civ) <= 0)) {
            continue;
        }
        const auto k = static_cast<std::int64_t>(books_.size()) + 1;
        if (k > MAX_BOOKS) {
            fail("history: bound: books exceed 100000");
        }
        const CultureData& cu = culture(f.culture);
        const std::vector<std::string>& titles = cu.naming.book_titles;
        if (titles.empty()) {
            fail("history: figures: culture '" + cu.key + "' has no book titles");
        }
        const std::uint64_t nt = titles.size();
        const std::uint64_t start = pick(ctx.draw(keys_[NAMES_BOOK], word(k), 0, 0), nt);
        NameRegistry& registry = peoples_.mutable_name_registry();
        std::string title;
        for (std::uint64_t j = 0; j < nt && title.empty(); ++j) {
            const std::string& candidate = titles[static_cast<std::size_t>((start + j) % nt)];
            if (!registry.contains(candidate)) {
                title = candidate;
            }
        }
        for (std::uint64_t j = 2; title.empty(); ++j) {
            std::string candidate = titles[static_cast<std::size_t>(start)] + " " + roman(j);
            if (candidate.size() > NAME_MAX_BYTES) {
                fail("names: a name longer than 64 bytes");
            }
            if (!registry.contains(candidate)) {
                title = std::move(candidate);
            }
        }
        registry.add(title);
        const std::int64_t tower_event = f.tower_event;
        const std::int64_t last_book = f.last_book;
        const std::int64_t site = f.tower;
        const std::int64_t civ = f.civ;
        EventSpec e = point_event(ctx, "book_written", ctx.event_region(tower_event), 0);
        e.participants = {fig_role("figure", id), civ_role("civilization", civ),
                          Participant{"site", EntityType::site, site}};
        e.causes = {tower_event};
        if (last_book != 0) {
            e.causes.push_back(last_book);
        }
        const std::int64_t ev = ctx.append(e);
        books_.push_back(BookState{k, title, id, site, ctx.year(), ev});
        Figure& g = figure(id);
        g.books += 1;
        g.last_book = ev;
        stats_.books += 1;
    }
}

void FiguresSubsystem::blights(StepContext& ctx) {
    const auto n = static_cast<std::int64_t>(figures_.size());
    for (std::int64_t id = 1; id <= n; ++id) {
        const Figure f = figure(id);
        if (f.role != CASTER_ROLE || f.status != ALIVE || f.blights > 0 || f.tower == 0) {
            continue;
        }
        if (!chance_ppm(ctx.draw(keys_[BLIGHT], word(id), 0, 0), tuning_.blight_ppm)) {
            continue;
        }
        const std::int64_t region = ctx.event_region(f.tower_event);
        State& state = ctx.state();
        std::vector<std::pair<std::int64_t, std::int64_t>> struck;  // (settlement, dead)
        std::int64_t total = 0;
        const std::int64_t ns = settlement_count(state);
        for (std::int64_t u = 1; u <= ns; ++u) {
            const Settlement& s = state.settlement(u);
            if (s.region != region || !standing(s) || !ctx.visible(s)) {
                continue;
            }
            const std::int64_t dead = floor_div(checked_mul(s.population, tuning_.blight_toll_permille), 1000);
            if (dead > 0) {
                struck.emplace_back(u, dead);
                total = checked_add(total, dead);
            }
        }
        for (const auto& [u, dead] : struck) {
            state.set_population(u, state.settlement(u).population - dead);
        }
        EventSpec e = point_event(ctx, "blight", region, 0);
        e.participants = {fig_role("figure", id), civ_role("civilization", f.civ),
                          Participant{"site", EntityType::site, f.tower}};
        e.values = {total};
        e.causes = {f.tower_event};
        const std::int64_t ev = ctx.append(e);
        for (const auto& [u, dead] : struck) {
            (void)dead;
            peoples_.note_reduction(u, ev);
        }
        figure(id).blights += 1;
        stats_.blights += 1;
    }
}

// --- The present and the epithets (6.12, 6.10) -----------------------------------------------------------------

void FiguresSubsystem::present(StepContext& ctx) {
    if (!tuning_.name_rulers) {
        return;
    }
    const std::int64_t nc = civ_count(ctx.state());
    for (std::int64_t c = 1; c <= nc; ++c) {
        if (ctx.state().civ(c).ended_year || !line(c).has || line(c).reign.figure != 0) {
            continue;
        }
        const std::int64_t f = name_reign(ctx, c, 0);
        const Civilization& civ = ctx.state().civ(c);
        const std::int64_t region =
            civ.capital != 0 ? ctx.state().settlement(civ.capital).region : civ.home_region;
        EventSpec e = point_event(ctx, "reigned_at_end", region, 0);
        e.participants = {fig_role("figure", f), civ_role("civilization", c)};
        const std::int64_t ev = ctx.append(e);
        figure(f).start_event = ev;
        ctx.state().open_relation(figure_civ_key(f, c, "ruled"), ev);
    }
}

std::int64_t FiguresSubsystem::deed_value(const StepContext& ctx, const Figure& f, std::int64_t deed) const {
    switch (deed) {
        case 0: {  // founded_years
            if (f.role != RULER || f.reign_no != 1) {
                return 0;
            }
            const Civilization& c = ctx.state().civ(f.civ);
            const std::int64_t end = c.ended_year ? *c.ended_year : end_year_;
            return std::max<std::int64_t>(0, end - c.founded_year);
        }
        case 1: return f.tally.wars_won;
        case 2: return f.tally.conquests;
        case 3: return f.tally.betrayals;
        case 4: return f.realm_fell ? 1 : 0;
        case 5: {  // reign_permille
            if (f.role != RULER) {
                return 0;
            }
            const std::int64_t end = f.died ? *f.died : end_year_;
            return floor_div(checked_mul(std::max<std::int64_t>(0, end - f.office_from), 1000),
                             race(f.race).generation);
        }
        case 6: return f.battles_won;
        case 7: return f.battles_lost == 0 ? f.battles_won : 0;
        case 8: return f.slew_might;
        case 9: return f.exiled ? 1 : 0;
        case 10: return f.books;
        case 11: return f.blights;
        default: return 0;
    }
}

void FiguresSubsystem::epithets(const StepContext& ctx) {
    // Closing order: the dead by death event, then the living by id.
    std::vector<std::pair<std::int64_t, std::int64_t>> order;  // (key, id)
    for (const Figure& f : figures_) {
        order.emplace_back(f.died ? f.death_event : std::int64_t{0}, f.id);
    }
    std::stable_sort(order.begin(), order.end(), [](const auto& a, const auto& b) {
        const bool da = a.first != 0;
        const bool db = b.first != 0;
        if (da != db) {
            return da;  // the dead first
        }
        return da ? a.first < b.first : a.second < b.second;
    });
    std::map<std::string, std::int64_t> bearers;
    for (const auto& [key, id] : order) {
        (void)key;
        const Figure& f = figure(id);
        std::string chosen;
        for (const EpithetRule& rule : tuning_.epithets) {
            if (rule.role != f.role) {
                continue;
            }
            std::string text = rule.text;
            if (text == SLAYER) {
                if (f.slew_might == 0 || f.slew_kind >= monster_kinds_.size()) {
                    continue;
                }
                text = monster_kinds_[f.slew_kind].second;
                if (text.empty()) {
                    continue;
                }
            }
            const std::int64_t v = deed_value(ctx, f, rule.deed);
            const auto it = bearers.find(text);
            const std::int64_t b = it == bearers.end() ? 0 : it->second;
            if ((rule.substantial > 0 && v >= rule.substantial) || (v >= rule.min && b < tuning_.epithet_repeat_max)) {
                chosen = std::move(text);
                break;
            }
        }
        if (!chosen.empty()) {
            bearers[chosen] += 1;
            figure(id).epithet = std::move(chosen);
        }
    }
}

// --- Rows (9.1, 9.3) ---------------------------------------------------------------------------------------------

std::vector<db::FigureRow> FiguresSubsystem::figure_rows() const {
    std::vector<db::FigureRow> out;
    for (const Figure& f : figures_) {
        db::FigureRow r;
        r.id = f.id;
        r.name = f.name;
        r.epithet = f.epithet;
        r.role = f.role;
        r.race = f.race;
        r.culture = f.culture;
        r.civ = f.civ;
        r.born = f.born;
        r.office_from = f.office_from;
        r.start_event = f.start_event;
        r.died = f.died;
        r.death_event = f.death_event;
        r.death_region = f.death_region;
        r.death_settlement = f.death_settlement;
        r.status = f.status;
        r.reign_no = f.reign_no;
        r.regnal = f.regnal;
        r.temper = f.temper;
        r.skill = f.skill;
        out.push_back(std::move(r));
    }
    return out;
}

std::vector<db::BookRow> FiguresSubsystem::book_rows() const {
    std::vector<db::BookRow> out;
    for (const BookState& b : books_) {
        out.push_back(db::BookRow{b.id, b.title, b.author, b.site, b.year, b.event});
    }
    return out;
}

}  // namespace worldgen::history
