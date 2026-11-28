// Artifacts (spec docs/specs/p5-11-artifacts.md): data, the making of 6.3
// to 6.6, the holdings of 6.2, the view of 7.1 and the rows of 9. The moves
// of 6.7 are in artifacts_moves.cpp.
//
// Every collection is a vector indexed by id or a std::map keyed by ids;
// loops over State run by id up to a count taken before the loop; members
// hold ids only (6.10).

#include "worldgen/history/artifacts.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <utility>

#include "artifacts_internal.hpp"
#include "conflict_internal.hpp"
#include "worldgen/core/checked.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/streams.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/history/conflict.hpp"
#include "worldgen/history/draw.hpp"
#include "worldgen/history/event_catalogue.hpp"
#include "worldgen/history/figures.hpp"
#include "worldgen/history/monsters.hpp"
#include "worldgen/history/peoples.hpp"

namespace worldgen::history {

using namespace conflict_detail;
using namespace artifacts_detail;

namespace {

constexpr std::array<Step, 4> STEPS = {Step::t3, Step::t4, Step::t6, Step::q4};

constexpr std::array<std::string_view, 7> STREAM_NAMES = {
    streams::hist_artifacts_make, streams::hist_artifacts_class, streams::hist_artifacts_kind,
    streams::hist_artifacts_take, streams::hist_artifacts_bury,  streams::hist_artifacts_raid,
    streams::hist_artifacts_corrupt,
};

constexpr std::array<std::string_view, ARTIFACT_CLASS_COUNT> CLASS_KEYS = {"arms", "armour", "regalia", "relic",
                                                                           "staff"};
constexpr std::array<std::string_view, 4> BEARER_KEYS = {"realm", "general", "figure", "site"};
constexpr std::string_view TITLE_SLOT = "{title}";

// Site kinds (P5-02 to P5-08).
constexpr std::int64_t SITE_TOWER = 4;
constexpr std::int64_t SITE_TEMPLE = 6;

std::int64_t key_index(std::string_view value, std::span<const std::string_view> keys) {
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (keys[i] == value) {
            return static_cast<std::int64_t>(i);
        }
    }
    fail("history: artifacts: '" + std::string(value) + "' is not a known key");
}

std::string with_title(std::string form, std::string_view title) {
    const std::size_t at = form.find(TITLE_SLOT);
    if (at != std::string::npos) {
        form.replace(at, TITLE_SLOT.size(), title);
    }
    return form;
}

}  // namespace

// --- Data (4.1, 4.3, 4.4) --------------------------------------------------------------------------------

ArtifactsTuning artifacts_tuning_from(const data::ContentSet& content) {
    const data::Definition* d = content.find(data::HISTORY_KIND, data::HISTORY_ARTIFACTS_ID);
    if (d == nullptr) {
        fail("history: tuning: the content has no definition history.artifacts");
    }
    const data::Record& r = d->record;
    ArtifactsTuning t;
    for (const data::Value& v : r.get_array("occasions")) {
        const data::Record& o = v.as_record();
        ArtifactOccasion occ;
        occ.kind = o.get_string("kind");
        occ.role = o.get_string("role");
        occ.chance_permille = o.get_int("chance_permille");
        occ.bearer = key_index(o.get_string("bearer"), BEARER_KEYS);
        const data::Record& w = o.get_record("classes");
        for (std::size_t i = 0; i < ARTIFACT_CLASS_COUNT; ++i) {
            occ.classes[i] = w.get_int(CLASS_KEYS[i]);
        }
        t.occasions.push_back(std::move(occ));
    }
    t.forge_chance_permille = r.get_int("forge_chance_permille");
    t.realm_rest_years = r.get_int("realm_rest_years");
    t.world_max = r.get_int("world_max");
    t.deep_metal_depth = r.get_int("deep_metal_depth");
    t.deep_metal_permille = r.get_int("deep_metal_permille");
    t.force_max_permille = r.get_int("force_max_permille");
    t.hunt_max_permille = r.get_int("hunt_max_permille");
    t.claim_max_permille = r.get_int("claim_max_permille");
    t.burial_permille = r.get_int("burial_permille");
    t.battle_take_permille = r.get_int("battle_take_permille");
    t.sack_take_permille = r.get_int("sack_take_permille");
    t.capture_take_permille = r.get_int("capture_take_permille");
    t.raid_take_permille = r.get_int("raid_take_permille");
    t.hoard_hunt_permille = r.get_int("hoard_hunt_permille");
    t.corrupt_years = r.get_int("corrupt_years");
    t.corrupt_permille = r.get_int("corrupt_permille");
    const data::Record& camp = r.get_record("corrupt_camp_permille");
    t.corrupt_camp_permille = {camp.get_int("good"), camp.get_int("neutral"), camp.get_int("evil")};
    t.corrupt_warlike_permille = r.get_int("corrupt_warlike_permille");
    return t;
}

std::vector<ArtifactKindDef> artifact_kind_defs_from(const data::ContentSet& content) {
    std::vector<ArtifactKindDef> out;
    const data::KindContent* defs = content.find_kind(data::ARTIFACT_KIND);
    if (defs == nullptr) {
        return out;
    }
    for (const data::Definition& d : defs->definitions) {  // canonical id order
        ArtifactKindDef k;
        k.key = d.id;
        k.artifact_class = key_index(d.record.get_string("class"), CLASS_KEYS) + 1;
        k.title = d.record.get_string("title");
        k.noun = d.record.get_string("noun");
        k.weight = d.record.get_int("weight");
        k.battle_permille = d.record.get_int("battle_permille");
        k.hunt_permille = d.record.get_int("hunt_permille");
        k.claim_permille = d.record.get_int("claim_permille");
        out.push_back(std::move(k));
    }
    return out;
}

// --- The subsystem ---------------------------------------------------------------------------------------

ArtifactsSubsystem::ArtifactsSubsystem(PeoplesSubsystem& peoples, const data::ContentSet& content)
    : peoples_(peoples), tuning_(artifacts_tuning_from(content)), kinds_(artifact_kind_defs_from(content)) {}

ArtifactsSubsystem::ArtifactsSubsystem(PeoplesSubsystem& peoples, ArtifactsTuning tuning,
                                       std::vector<ArtifactKindDef> kinds)
    : peoples_(peoples), tuning_(std::move(tuning)), kinds_(std::move(kinds)) {}

std::span<const Step> ArtifactsSubsystem::steps() const {
    return STEPS;
}

void ArtifactsSubsystem::prepare(const StepContext& ctx) {
    if (prepared_) {
        return;
    }
    const HistoryInputs& in = ctx.inputs();
    for (std::size_t i = 0; i < STREAM_NAMES.size(); ++i) {
        keys_[i] = stream_key(in.seed, STREAM_NAMES[i]);
    }
    for (const db::CultureRow& row : in.cultures) {
        const data::Definition* d = ctx.content().find(data::CULTURE_KIND, row.key);
        if (d == nullptr) {
            fail("history: artifacts: culture '" + row.key + "' is not in the content");
        }
        cultures_.push_back(CultureData{row.id, row.key, culture_naming_from(d->record.get_record("naming"))});
    }
    std::stable_sort(cultures_.begin(), cultures_.end(),
                     [](const CultureData& a, const CultureData& b) { return a.id < b.id; });
    for (const db::RegionRow& r : in.regions) {  // id order (GeoView checks ids 1, 2, ...)
        region_seeds_.emplace_back(r.seed_x, r.seed_y);
    }
    // r2 6.11: the camps of the gods and of the races.
    for (const db::GodRow& g : in.gods) {
        god_camps_.emplace_back(g.id, g.camp);
    }
    std::stable_sort(god_camps_.begin(), god_camps_.end());
    for (const db::RaceRow& r : in.races) {
        race_camps_.emplace_back(r.id, r.camp);
    }
    std::stable_sort(race_camps_.begin(), race_camps_.end());
    prepared_ = true;
}

const ArtifactsSubsystem::CultureData& ArtifactsSubsystem::culture(std::int64_t culture_id) const {
    const auto it = std::lower_bound(cultures_.begin(), cultures_.end(), culture_id,
                                     [](const CultureData& c, std::int64_t id) { return c.id < id; });
    if (it == cultures_.end() || it->id != culture_id) {
        fail("history: artifacts: culture " + int_text(culture_id) + " does not exist");
    }
    return *it;
}

ArtifactsSubsystem::Artifact& ArtifactsSubsystem::artifact(std::int64_t id) {
    if (id < 1 || static_cast<std::size_t>(id) > artifacts_.size()) {
        fail("history: artifacts: artifact " + int_text(id) + " does not exist");
    }
    return artifacts_[static_cast<std::size_t>(id - 1)];
}

const ArtifactsSubsystem::Artifact& ArtifactsSubsystem::artifact(std::int64_t id) const {
    if (id < 1 || static_cast<std::size_t>(id) > artifacts_.size()) {
        fail("history: artifacts: artifact " + int_text(id) + " does not exist");
    }
    return artifacts_[static_cast<std::size_t>(id - 1)];
}

const ArtifactKindDef& ArtifactsSubsystem::kind_of(const Artifact& a) const {
    if (a.kind < 1 || static_cast<std::size_t>(a.kind) > kinds_.size()) {
        fail("history: artifacts: kind " + int_text(a.kind) + " does not exist");
    }
    return kinds_[static_cast<std::size_t>(a.kind - 1)];
}

bool ArtifactsSubsystem::civ_alive(const StepContext& ctx, std::int64_t civ) const {
    return civ >= 1 && civ <= civ_count(ctx.state()) && !ctx.state().civ(civ).ended_year;
}

bool ArtifactsSubsystem::alive_visible(const StepContext& ctx, std::int64_t civ) const {
    return civ_alive(ctx, civ) && ctx.visible(ctx.state().civ(civ));
}

std::int64_t ArtifactsSubsystem::seat_of(std::int64_t civ) const {
    if (civ < 1 || static_cast<std::size_t>(civ) > seat_.size()) {
        return 0;
    }
    return seat_[static_cast<std::size_t>(civ - 1)];
}

// 6.2: refreshed for every alive civilization ascending at the end of each
// step; the view's ended flags with it.
void ArtifactsSubsystem::refresh_seats(const StepContext& ctx) {
    const std::int64_t nc = civ_count(ctx.state());
    for (std::int64_t c = 1; c <= nc; ++c) {
        const Civilization& civ = ctx.state().civ(c);
        const auto i = static_cast<std::size_t>(c - 1);
        if (!civ.ended_year && civ.capital != 0) {
            seat_[i] = civ.capital;
        }
        ended_[i] = civ.ended_year ? 1 : 0;
    }
}

std::int64_t ArtifactsSubsystem::civ_region(const StepContext& ctx, std::int64_t civ) const {
    const Civilization& c = ctx.state().civ(civ);
    return c.capital != 0 ? ctx.state().settlement(c.capital).region : c.home_region;
}

void ArtifactsSubsystem::run(StepContext& ctx) {
    prepare(ctx);
    const auto nc = static_cast<std::size_t>(civ_count(ctx.state()));
    if (last_made_.size() < nc) {
        last_made_.resize(nc);
        seat_.resize(nc, 0);
        ended_.resize(nc, 0);
    }
    read_log(ctx);
    refresh_seats(ctx);
    corruption(ctx);  // r2 6.1: after the seats
}

// 6.1: for each event of the log, in ascending id, its moves, then its making.
void ArtifactsSubsystem::read_log(StepContext& ctx) {
    const std::int64_t last = ctx.last_event_id();
    for (std::int64_t e = cursor_; e <= last; ++e) {
        const std::string_view k = ctx.event_kind(e);
        moves(ctx, e, k);
        making(ctx, e, k);
    }
    cursor_ = last + 1;
}

// --- Making (6.3 to 6.6) ---------------------------------------------------------------------------------

std::int64_t ArtifactsSubsystem::first_of_type(const StepContext& ctx, std::int64_t event, std::string_view kind,
                                               EntityType type) {
    const KindDef* d = compiled_event_catalogue().find(kind);
    if (d == nullptr) {
        return 0;
    }
    for (const RoleDef& r : d->roles) {  // seq order
        if (r.type == type) {
            if (const std::int64_t id = ctx.event_participant(event, r.role); id != 0) {
                return id;
            }
        }
    }
    return 0;
}

bool ArtifactsSubsystem::open_site_of_kind(std::int64_t site, std::int64_t kind) const {
    if (sites_ == nullptr || site == 0) {
        return false;
    }
    const std::optional<MonstersSubsystem::SiteBrief> b = sites_->site_brief(site);
    return b && b->kind == kind && b->open;
}

std::int64_t ArtifactsSubsystem::caster_site(std::int64_t caster) const {
    if (figures_ == nullptr || caster == 0) {
        return 0;
    }
    const std::int64_t s = figures_->caster_site(caster);
    return open_site_of_kind(s, SITE_TOWER) ? s : 0;
}

// 6.4: c when it has an open mine (reading 1); else the alliance or trade
// partner with an open mine of the largest forge_depth, then the smaller id.
std::int64_t ArtifactsSubsystem::forge(std::int64_t civ) const {
    if (alliances_ == nullptr) {
        return 0;
    }
    if (alliances_->forged(civ)) {
        return civ;
    }
    std::int64_t best = 0;
    std::int64_t depth = -1;
    for (const auto& [p, ev] : alliances_->partners(civ)) {  // ascending id
        (void)ev;
        if (!alliances_->forged(p)) {
            continue;
        }
        const std::int64_t d = alliances_->forge_depth(p);
        if (d > depth) {
            best = p;
            depth = d;
        }
    }
    return best;
}

std::int64_t ArtifactsSubsystem::forge_site(std::int64_t civ) const {
    return alliances_ != nullptr ? alliances_->forge_site(civ) : 0;
}

// 6.6: P3-02 4.3's name with the culture's artifact forms, {title} the kind's.
std::string ArtifactsSubsystem::draw_name(const StepContext& ctx, std::int64_t culture_id, std::int64_t id,
                                          const ArtifactKindDef& kind) {
    const CultureData& cu = culture(culture_id);
    CultureNaming naming = cu.naming;
    for (std::string& f : naming.artifact_forms) {
        f = with_title(std::move(f), kind.title);
    }
    if (naming.artifact_forms.empty()) {
        fail("history: artifacts: culture '" + cu.key + "' has no artifact_forms");
    }
    const NameChoice choice = choose_name(naming, peoples_.name_filter(), cu.key, NameKind::artifact,
                                          ctx.inputs().seed, word(cu.id), word(id), peoples_.mutable_name_registry());
    return choice.name;
}

void ArtifactsSubsystem::making(StepContext& ctx, std::int64_t e, std::string_view kind) {
    if (tuning_.occasions.empty() || static_cast<std::int64_t>(artifacts_.size()) >= tuning_.world_max) {
        return;
    }
    for (const ArtifactOccasion& o : tuning_.occasions) {  // list order
        if (o.kind != kind) {
            continue;
        }
        for (const std::int64_t c : ctx.event_participants(e, o.role)) {  // ascending seq
            if (c != 0 && try_make(ctx, e, o, c)) {
                return;  // at most one artifact per event
            }
        }
    }
}

bool ArtifactsSubsystem::try_make(StepContext& ctx, std::int64_t e, const ArtifactOccasion& o, std::int64_t c) {
    const std::int64_t y = ctx.year();
    const auto ci = static_cast<std::size_t>(c - 1);
    // Eligibility (pure; no draw).
    if (!alive_visible(ctx, c) || ci >= last_made_.size()) {
        return false;
    }
    if (last_made_[ci] && y - *last_made_[ci] < tuning_.realm_rest_years) {
        return false;
    }
    if (static_cast<std::int64_t>(artifacts_.size()) >= tuning_.world_max) {
        return false;
    }
    if (ctx.step() == Step::q4 && ctx.quiet_events_left(c) <= 0) {
        return false;
    }
    const std::int64_t forge_civ = forge(c);
    const std::int64_t p = std::min<std::int64_t>(
        1000, floor_div(checked_mul(o.chance_permille, forge_civ != 0 ? tuning_.forge_chance_permille : 1000), 1000));
    if (!chance_pm(ctx.draw(keys_[MAKE], word(e), word(c), 0), p)) {
        return false;
    }
    const std::string_view kind = ctx.event_kind(e);
    const std::int64_t god = first_of_type(ctx, e, kind, EntityType::god);
    const std::int64_t caster = figures_ != nullptr ? figures_->caster_of(c) : 0;
    const std::int64_t tower = caster_site(caster);
    std::array<std::int64_t, ARTIFACT_CLASS_COUNT> w = o.classes;
    if (god == 0) {
        w[static_cast<std::size_t>(RELIC) - 1] = 0;
    }
    if (caster == 0 || tower == 0) {
        w[static_cast<std::size_t>(STAFF) - 1] = 0;
    }
    if (std::all_of(w.begin(), w.end(), [](std::int64_t x) { return x == 0; })) {
        return false;
    }
    const auto cls = static_cast<std::int64_t>(weighted(ctx.draw(keys_[CLASS], word(e), word(c), 0), w)) + 1;
    const auto id = static_cast<std::int64_t>(artifacts_.size()) + 1;
    if (id > MAX_ARTIFACTS) {
        fail("history: bound: artifacts exceed 100000");
    }
    std::vector<std::int64_t> ids;
    std::vector<std::int64_t> weights;
    for (std::size_t k = 0; k < kinds_.size(); ++k) {  // id order
        if (kinds_[k].artifact_class == cls) {
            ids.push_back(static_cast<std::int64_t>(k) + 1);
            weights.push_back(kinds_[k].weight);
        }
    }
    if (ids.empty()) {
        return false;
    }
    const std::int64_t kind_id = ids[weighted(ctx.draw(keys_[KIND], word(id), 0, 0), weights)];
    const ArtifactKindDef& def = kinds_[static_cast<std::size_t>(kind_id - 1)];

    // 6.4: the maker.
    Artifact a;
    a.id = id;
    a.kind = kind_id;
    a.civ = c;
    a.made_year = y;
    a.occasion_event = e;
    std::int64_t name_culture = ctx.state().civ(c).culture;
    std::int64_t smith_realm = 0;  // the realm of a new smith, 0 none
    if (god != 0 && cls != STAFF) {
        a.maker_kind = 4;
        a.god = god;
        const std::int64_t s = first_of_type(ctx, e, kind, EntityType::site);
        a.maker_site = open_site_of_kind(s, SITE_TEMPLE) ? s : 0;
    } else if (cls == STAFF) {
        a.maker_kind = 3;
        a.maker = caster;
        a.maker_civ = c;
        a.maker_site = tower;
    } else {
        if (figures_ == nullptr) {
            return false;  // no smith can be raised (a test seam)
        }
        smith_realm = forge_civ != 0 ? forge_civ : c;
        a.maker_kind = forge_civ != 0 && forge_civ != c ? 2 : 1;
        a.maker = figures_->next_figure_id();
        a.maker_civ = smith_realm;
        if (forge_civ != 0) {
            a.maker_site = forge_site(forge_civ);
            a.deep = alliances_->forge_depth(forge_civ) >= tuning_.deep_metal_depth ? 1 : 0;
            name_culture = ctx.state().civ(forge_civ).culture;
        }
    }
    // 6.6: the name, before the event is appended (H2).
    a.name = draw_name(ctx, name_culture, id, def);

    // 8.1: artifact_made.
    std::int64_t region = civ_region(ctx, c);
    std::int64_t settlement = 0;
    std::int64_t deepened = 0;
    if (a.maker_site != 0 && sites_ != nullptr) {
        if (const std::optional<MonstersSubsystem::SiteBrief> b = sites_->site_brief(a.maker_site)) {
            region = b->region;
            settlement = b->settlement;
            deepened = b->deepen_event;
        }
    }
    if (settlement != 0) {
        region = ctx.state().settlement(settlement).region;  // reading 4: the settlement's region
    }
    EventSpec spec = point_event(ctx, "artifact_made", region, settlement);
    spec.participants = {artifact_role(id), civ_role("civilization", c)};
    if (a.maker_kind <= 3) {
        spec.participants.push_back(Participant{"maker", EntityType::figure, a.maker});
    }
    if (a.maker_kind == 4) {
        spec.participants.push_back(Participant{"god", EntityType::god, a.god});
    }
    if (a.maker_kind == 2) {
        spec.participants.push_back(civ_role("maker_realm", a.maker_civ));
    }
    if (a.maker_site != 0) {
        spec.participants.push_back(Participant{"site", EntityType::site, a.maker_site});
    }
    spec.values = {a.maker_kind, a.deep};
    spec.causes = {e};
    if (a.deep == 1 && deepened != 0 && deepened != e) {
        spec.causes.push_back(deepened);
        std::stable_sort(spec.causes.begin(), spec.causes.end());
    }
    const std::int64_t ev = ctx.append(spec);
    a.made_event = ev;
    if (smith_realm != 0) {
        figures_->raise_maker(ctx, a.maker, smith_realm, ev);
    }
    artifacts_.push_back(std::move(a));
    last_made_[ci] = y;
    stats_.made += 1;
    stats_.by_class[static_cast<std::size_t>(cls - 1)] += 1;
    const std::int64_t mk = artifacts_.back().maker_kind;
    stats_.smiths += mk <= 2 ? 1 : 0;
    stats_.commissioned += mk == 2 ? 1 : 0;
    stats_.casters += mk == 3 ? 1 : 0;
    stats_.gods += mk == 4 ? 1 : 0;
    stats_.deep += artifacts_.back().deep;

    // 6.5: the first holding.
    Holding h = holder(KEPT, CIV_TYPE, c);
    if (cls == STAFF) {
        if (!bears_class(caster, STAFF)) {  // reading 5: one staff per bearer
            h = holder(BORNE, FIGURE_TYPE, caster);
        }
    } else if (cls == RELIC) {
        const std::int64_t s = first_of_type(ctx, e, kind, EntityType::site);
        if (open_site_of_kind(s, SITE_TEMPLE)) {
            h = placed(ENSHRINED, site_place(s, true));
        }
    } else if (cls == ARMS || cls == ARMOUR) {
        std::int64_t bearer = 0;
        if (o.bearer == 1 && figures_ != nullptr) {
            bearer = figures_->general_of(e, c).figure;
        } else if (o.bearer == 2) {
            bearer = first_of_type(ctx, e, kind, EntityType::figure);
            if (bearer != 0 && figure_civ(bearer) != c) {
                bearer = 0;
            }
        }
        if (bearer != 0 && figure_alive(bearer) && !bears_class(bearer, cls)) {
            h = holder(BORNE, FIGURE_TYPE, bearer);
        }
    }
    h.event = ev;
    open(id, y, h);
    return true;
}

// --- Holdings (6.2) --------------------------------------------------------------------------------------

const ArtifactsSubsystem::Holding& ArtifactsSubsystem::open_holding(std::int64_t id) const {
    const Artifact& a = artifact(id);
    if (a.holdings.empty()) {
        fail("history: artifacts: artifact " + int_text(id) + " has no holding");
    }
    return a.holdings.back();
}

std::vector<std::int64_t> ArtifactsSubsystem::held_by(std::int64_t state, std::int64_t who) const {
    const std::map<std::int64_t, std::vector<std::int64_t>>& index =
        state == KEPT ? kept_ : (state == BORNE ? borne_ : hoarded_);
    const auto it = index.find(who);
    return it == index.end() ? std::vector<std::int64_t>{} : it->second;
}

bool ArtifactsSubsystem::bears_class(std::int64_t figure, std::int64_t artifact_class) const {
    const auto it = borne_.find(figure);
    if (it == borne_.end()) {
        return false;
    }
    return std::any_of(it->second.begin(), it->second.end(),
                       [&](std::int64_t a) { return kind_of(artifact(a)).artifact_class == artifact_class; });
}

bool ArtifactsSubsystem::figure_alive(std::int64_t figure) const {
    return figures_ != nullptr && figure != 0 && figures_->brief(figure).alive;
}

std::int64_t ArtifactsSubsystem::figure_civ(std::int64_t figure) const {
    return figures_ != nullptr && figure != 0 ? figures_->brief(figure).civ : 0;
}

namespace {

void index_remove(std::map<std::int64_t, std::vector<std::int64_t>>& index, std::int64_t who, std::int64_t id) {
    const auto it = index.find(who);
    if (it == index.end()) {
        return;
    }
    std::vector<std::int64_t>& v = it->second;
    v.erase(std::remove(v.begin(), v.end(), id), v.end());
    if (v.empty()) {
        index.erase(it);
    }
}

void index_add(std::map<std::int64_t, std::vector<std::int64_t>>& index, std::int64_t who, std::int64_t id) {
    std::vector<std::int64_t>& v = index[who];
    v.insert(std::lower_bound(v.begin(), v.end(), id), id);
}

}  // namespace

void ArtifactsSubsystem::open(std::int64_t id, std::int64_t year, Holding h) {
    Artifact& a = artifact(id);
    if (!a.holdings.empty()) {
        fail("history: artifacts: artifact " + int_text(id) + " already has a holding");
    }
    h.from_year = year;
    h.to_year.reset();
    h.until_event = 0;
    a.holdings.push_back(h);
    if (h.state == KEPT) index_add(kept_, h.holder, id);
    if (h.state == BORNE) index_add(borne_, h.holder, id);
    if (h.state == HOARDED) index_add(hoarded_, h.holder, id);
}

// 6.2: the open holding closes with `event` in `year`; h opens.
void ArtifactsSubsystem::move(std::int64_t id, std::int64_t year, std::int64_t event, Holding h) {
    Artifact& a = artifact(id);
    Holding& last = a.holdings.back();
    if (year < last.from_year) {
        fail("history: artifacts: artifact " + int_text(id) + " moved in year " + int_text(year) +
             " before its holding of " + int_text(last.from_year));
    }
    last.to_year = year;
    last.until_event = event;
    if (last.state == KEPT) index_remove(kept_, last.holder, id);
    if (last.state == BORNE) index_remove(borne_, last.holder, id);
    if (last.state == HOARDED) index_remove(hoarded_, last.holder, id);
    h.from_year = year;
    h.to_year.reset();
    h.event = event;
    h.until_event = 0;
    a.holdings.push_back(h);
    if (h.state == KEPT) index_add(kept_, h.holder, id);
    if (h.state == BORNE) index_add(borne_, h.holder, id);
    if (h.state == HOARDED) index_add(hoarded_, h.holder, id);
}

ArtifactsSubsystem::Holding ArtifactsSubsystem::holder(std::int64_t state, std::int64_t type, std::int64_t id) {
    Holding h;
    h.state = state;
    h.holder_type = type;
    h.holder = id;
    return h;
}

ArtifactsSubsystem::Holding ArtifactsSubsystem::placed(std::int64_t state, const Place& p) {
    Holding h;
    h.state = state;
    h.holder_type = p.holder_type;
    h.holder = p.holder;
    h.region = p.region;
    h.settlement = p.settlement;
    h.site = p.site;
    h.x = p.x;
    h.y = p.y;
    return h;
}

ArtifactsSubsystem::Place ArtifactsSubsystem::field_place(std::int64_t region) const {
    Place p;
    p.holder_type = REGION_TYPE;
    p.holder = region;
    p.region = region;
    if (region >= 1 && static_cast<std::size_t>(region) <= region_seeds_.size()) {
        p.x = region_seeds_[static_cast<std::size_t>(region - 1)].first;
        p.y = region_seeds_[static_cast<std::size_t>(region - 1)].second;
    }
    return p;
}

ArtifactsSubsystem::Place ArtifactsSubsystem::ruin_place(const StepContext& ctx, std::int64_t settlement) const {
    const Settlement& s = ctx.state().settlement(settlement);
    Place p;
    p.holder_type = SETTLEMENT_TYPE;
    p.holder = settlement;
    p.region = s.region;
    p.settlement = settlement;
    p.x = s.x;
    p.y = s.y;
    return p;
}

ArtifactsSubsystem::Place ArtifactsSubsystem::site_place(std::int64_t site, bool with_settlement) const {
    Place p;
    p.holder_type = SITE_TYPE;
    p.holder = site;
    p.site = site;
    if (sites_ != nullptr) {
        if (const std::optional<MonstersSubsystem::SiteBrief> b = sites_->site_brief(site)) {
            p.region = b->region;
            p.settlement = with_settlement ? b->settlement : 0;
            p.x = b->x;
            p.y = b->y;
        }
    }
    return p;
}

ArtifactsSubsystem::Place ArtifactsSubsystem::grave_place(const StepContext& ctx, std::int64_t figure) const {
    const FigureView::Brief b = figures_ != nullptr ? figures_->brief(figure) : FigureView::Brief{};
    Place p = field_place(b.death_region);
    p.holder_type = FIGURE_TYPE;
    p.holder = figure;
    if (b.death_settlement != 0) {
        const Settlement& s = ctx.state().settlement(b.death_settlement);
        p.settlement = b.death_settlement;
        p.x = s.x;
        p.y = s.y;
    }
    return p;
}

std::int64_t ArtifactsSubsystem::monster_lair(std::int64_t monster) const {
    return sites_ != nullptr ? sites_->lair_of(monster) : 0;
}

// 6.8: a written artifact event, or a quiet move in Q4 without the budget of
// its first civilization participant (8.1's charged civilization).
std::int64_t ArtifactsSubsystem::emit(StepContext& ctx, const Emit& x, std::int64_t trigger) {
    std::int64_t charged = 0;
    for (const Participant& p : x.participants) {
        if (p.type == EntityType::civilization) {
            charged = p.id;
            break;
        }
    }
    if (ctx.step() == Step::q4 && ctx.quiet_events_left(charged) <= 0) {
        stats_.quiet_moves += 1;
        return trigger;
    }
    std::int64_t region = x.region;
    if (x.settlement != 0) {
        region = ctx.state().settlement(x.settlement).region;  // reading 4
    }
    EventSpec spec = point_event(ctx, x.kind, region, x.settlement);
    spec.participants = x.participants;
    spec.values = x.values;
    spec.causes = {trigger};
    const std::int64_t ev = ctx.append(spec);
    if (x.kind == "artifact_taken") stats_.taken += 1;
    if (x.kind == "artifact_lost") stats_.lost += 1;
    if (x.kind == "artifact_hoarded") stats_.hoarded += 1;
    if (x.kind == "artifact_recovered") stats_.recovered += 1;
    if (x.kind == "artifact_buried") stats_.buried += 1;
    return ev;
}

// --- The view (7.1) --------------------------------------------------------------------------------------

std::int64_t ArtifactsSubsystem::battle_permille(std::int64_t figure) const {
    std::int64_t sum = 0;
    for (const std::int64_t id : held_by(BORNE, figure)) {
        const Artifact& a = artifact(id);
        const std::int64_t b = kind_of(a).battle_permille;
        if (b > 0) {
            sum = checked_add(sum, checked_add(b, a.deep == 1 ? tuning_.deep_metal_permille : 0));
        }
    }
    return std::min(tuning_.force_max_permille, sum);
}

std::int64_t ArtifactsSubsystem::battle_artifact(std::int64_t figure) const {
    for (const std::int64_t id : held_by(BORNE, figure)) {  // ascending id
        const Artifact& a = artifact(id);
        if (kind_of(a).battle_permille > 0) {
            return a.made_event;
        }
    }
    return 0;
}

std::int64_t ArtifactsSubsystem::hunt_permille(std::int64_t figure) const {
    std::int64_t sum = 0;
    for (const std::int64_t id : held_by(BORNE, figure)) {
        const Artifact& a = artifact(id);
        const std::int64_t h = kind_of(a).hunt_permille;
        if (h > 0) {
            sum = checked_add(sum, checked_add(h, a.deep == 1 ? tuning_.deep_metal_permille : 0));
        }
    }
    return std::min(tuning_.hunt_max_permille, sum);
}

std::int64_t ArtifactsSubsystem::claim_permille(std::int64_t home, std::int64_t who) const {
    if (home == who || home < 1 || who < 1) {
        return 0;
    }
    const auto ended = [this](std::int64_t c) {
        return static_cast<std::size_t>(c) <= ended_.size() && ended_[static_cast<std::size_t>(c - 1)] != 0;
    };
    if (ended(home) || ended(who)) {
        return 0;
    }
    std::int64_t sum = 0;
    for (const Artifact& a : artifacts_) {
        if (a.civ != home) {
            continue;
        }
        const Holding& h = a.holdings.back();
        if ((h.state == KEPT && h.holder == who) || (h.state == BORNE && figure_civ(h.holder) == who)) {
            sum = checked_add(sum, kind_of(a).claim_permille);
        }
    }
    return std::min(tuning_.claim_max_permille, sum);
}

std::int64_t ArtifactsSubsystem::claim_event(std::int64_t home, std::int64_t who) const {
    if (claim_permille(home, who) <= 0) {
        return 0;
    }
    std::int64_t best = 0;
    for (const Artifact& a : artifacts_) {
        if (a.civ != home) {
            continue;
        }
        const Holding& h = a.holdings.back();
        if ((h.state == KEPT && h.holder == who) || (h.state == BORNE && figure_civ(h.holder) == who)) {
            best = std::max(best, h.event);
        }
    }
    return best;
}

std::int64_t ArtifactsSubsystem::hoard_motive_permille(std::int64_t monster, std::int64_t civ) const {
    for (const std::int64_t id : held_by(HOARDED, monster)) {
        const Artifact& a = artifact(id);
        if (a.civ == civ) {
            return tuning_.hoard_hunt_permille;
        }
        if (a.holdings.size() >= 2) {
            const Holding& before = a.holdings[a.holdings.size() - 2];
            if ((before.state == KEPT && before.holder == civ) ||
                (before.state == BORNE && figure_civ(before.holder) == civ)) {
                return tuning_.hoard_hunt_permille;
            }
        }
    }
    return 0;
}

// --- Rows (9) --------------------------------------------------------------------------------------------

std::vector<db::ArtifactKindRow> ArtifactsSubsystem::artifact_kind_rows() const {
    std::vector<db::ArtifactKindRow> out;
    for (std::size_t k = 0; k < kinds_.size(); ++k) {
        const ArtifactKindDef& d = kinds_[k];
        out.push_back(db::ArtifactKindRow{static_cast<std::int64_t>(k) + 1, d.key, d.title, d.noun, d.artifact_class,
                                          d.battle_permille, d.hunt_permille, d.claim_permille});
    }
    return out;
}

std::vector<db::ArtifactRow> ArtifactsSubsystem::artifact_rows() const {
    std::vector<db::ArtifactRow> out;
    for (const Artifact& a : artifacts_) {
        out.push_back(db::ArtifactRow{a.id, a.name, a.kind, a.civ, a.maker_kind, a.maker, a.god, a.maker_civ,
                                      a.maker_site, a.made_year, a.made_event, a.occasion_event, a.deep,
                                      a.holdings.back().state});
    }
    return out;
}

std::vector<db::ArtifactHoldingRow> ArtifactsSubsystem::artifact_holding_rows() const {
    std::vector<db::ArtifactHoldingRow> out;
    for (const Artifact& a : artifacts_) {
        for (std::size_t i = 0; i < a.holdings.size(); ++i) {
            const Holding& h = a.holdings[i];
            out.push_back(db::ArtifactHoldingRow{a.id, static_cast<std::int64_t>(i) + 1, h.from_year, h.to_year, h.state,
                                                 h.holder_type, h.holder, h.region, h.settlement, h.site, h.x, h.y,
                                                 h.event, h.until_event});
        }
    }
    return out;
}

}  // namespace worldgen::history
