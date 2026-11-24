// The renaming subsystem (spec docs/specs/p5-12-renamed-regions.md sections
// 3 to 9; P3-01 r16 5.4 for the data and the stage check).

#include "worldgen/history/renaming.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include "worldgen/core/checked.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/fx.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/streams.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/db/schema.hpp"
#include "worldgen/history/draw.hpp"
#include "worldgen/stages/biomes.hpp"

namespace worldgen::history {

namespace {

constexpr std::array<Step, 2> STEPS = {Step::t6, Step::q4};

constexpr std::string_view NAME_SLOT = "{name}";
constexpr std::string_view NOUN_SLOT = "{noun}";
constexpr std::string_view STEM_SLOT = "{stem}";  // P5-02 21.2
constexpr std::string_view ROOT_SLOT = "{root}";
constexpr std::string_view SETTLEMENT_ROLE = "settlement";
constexpr std::string_view RENAMED_KIND = "region_renamed";
constexpr std::string_view NAMER_ROLE = "namer";

[[noreturn]] void fail(const std::string& message) {
    throw FatalError(message);
}

std::uint64_t word(std::int64_t v) noexcept {
    return static_cast<std::uint64_t>(v);
}

// chance_pm of P3-05 3.3.
bool chance_pm(std::uint64_t u, std::int64_t p) {
    constexpr std::int64_t ONE_RAW = std::int64_t{1} << 32;
    const std::int64_t raw = std::min(ONE_RAW, floor_div(checked_mul(std::max<std::int64_t>(0, p), ONE_RAW), 1000));
    return chance(u, Fx::from_raw(raw));
}

std::vector<std::string> string_list(const data::Record& record, std::string_view field) {
    std::vector<std::string> out;
    for (const data::Value& v : record.get_array(field)) {
        out.push_back(v.as_string());
    }
    return out;
}

bool contains(const std::vector<std::string>& list, std::string_view s) {
    return std::find(list.begin(), list.end(), s) != list.end();
}

bool starts_with(std::string_view text, std::string_view prefix) noexcept {
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

bool ends_with(std::string_view text, std::string_view suffix) noexcept {
    return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
}

bool has_civilization_role(const KindDef& kind) {
    return std::any_of(kind.roles.begin(), kind.roles.end(),
                       [](const RoleDef& r) { return r.type == EntityType::civilization; });
}

// The first problem of one rule against one kind (P3-01 r16 5.4), or empty.
std::string kind_problem(const RenameRule& rule, std::string_view key, const EventCatalogue& catalogue) {
    const KindDef* kind = catalogue.find(key);
    const std::string k(key);
    if (kind == nullptr) {
        return "kind '" + k + "' is not in the catalogue";
    }
    if (kind->region_use != 1) {
        return "kind '" + k + "' has no region";
    }
    for (const std::string& v : rule.values) {
        if (kind->value_seq(v) == 0) {
            return "value '" + v + "' is not a value of kind '" + k + "'";
        }
    }
    if (!rule.name_role.empty()) {
        bool ok = false;
        if (rule.name_role == SETTLEMENT_ROLE) {
            ok = kind->settlement_use != 0;
        } else if (const RoleDef* r = kind->find_role(rule.name_role)) {
            // P5-02 9.3: a monster role names the monster's root.
            ok = r->type == EntityType::civilization || r->type == EntityType::settlement ||
                 r->type == EntityType::monster;
        }
        if (!ok) {
            return "name_role '" + rule.name_role + "' is not a civilization, settlement or monster role of kind '" +
                   k + "'";
        }
    }
    if (!rule.namer_role.empty()) {
        const RoleDef* r = kind->find_role(rule.namer_role);
        if (r == nullptr || r->type != EntityType::civilization) {
            return "namer_role '" + rule.namer_role + "' is not a civilization role of kind '" + k + "'";
        }
    }
    if (rule.capital && (kind->settlement_use == 0 || !has_civilization_role(*kind))) {
        return "capital needs a kind with a settlement and a civilization role ('" + k + "')";
    }
    return {};
}

}  // namespace

// --- Data --------------------------------------------------------------------------------------

RenamingTuning renaming_tuning_from(const data::ContentSet& content) {
    const data::Definition* d = content.find(data::HISTORY_KIND, data::HISTORY_RENAMING_ID);
    if (d == nullptr) {
        fail("history: tuning: the content has no definition history.renaming");
    }
    const data::Record& r = d->record;
    RenamingTuning t;
    t.region_max = r.get_int("region_max");
    t.region_gap_years = r.get_int("region_gap_years");
    t.world_max_permille = r.get_int("world_max_permille");
    t.century_max_permille = r.get_int("century_max_permille");
    t.century_years = r.get_int("century_years");
    for (const data::Value& v : r.get_array("rules")) {
        const data::Record& x = v.as_record();
        RenameRule rule;
        rule.key = x.get_string("key");
        rule.kinds = string_list(x, "kinds");
        rule.values = string_list(x, "values");
        rule.min_magnitude = x.get_int("min_magnitude");
        rule.window_years = x.get_int("window_years");
        rule.min_salience = x.get_int("min_salience");
        rule.capital = x.get_bool("capital");
        rule.chance_permille = x.get_int("chance_permille");
        rule.weight = x.get_int("weight");
        rule.name_role = x.get_string("name_role");
        rule.namer_role = x.get_string("namer_role");
        rule.forms = string_list(x, "forms");
        t.rules.push_back(std::move(rule));
    }
    return t;
}

std::optional<std::string> renaming_rules_problem(const RenamingTuning& tuning, const EventCatalogue& catalogue) {
    for (const RenameRule& rule : tuning.rules) {
        for (const std::string& k : rule.kinds) {
            std::string problem = kind_problem(rule, k, catalogue);
            if (!problem.empty()) {
                return "history: renaming rule '" + rule.key + "': " + problem;
            }
        }
    }
    return std::nullopt;
}

void check_renaming_rules(const RenamingTuning& tuning, const EventCatalogue& catalogue) {
    if (std::optional<std::string> problem = renaming_rules_problem(tuning, catalogue)) {
        fail(*problem);
    }
}

// --- Names (5.1, 5.3) ------------------------------------------------------------------------------

std::string RenamingSubsystem::name_now(const StepContext& ctx, std::int64_t region) const {
    for (auto it = rows_.rbegin(); it != rows_.rend(); ++it) {  // creation order: the last row wins
        if (it->region == region) {
            return it->name;
        }
    }
    const std::vector<db::RegionRow>& regions = ctx.inputs().regions;
    if (region < 1 || static_cast<std::size_t>(region) > regions.size()) {
        fail("history: renaming: region " + db::format_i64(region) + " does not exist");
    }
    return regions[static_cast<std::size_t>(region - 1)].name;
}

std::string realm_root(std::string_view name, const std::vector<std::string>& civilization_forms) {
    for (const std::string& form : civilization_forms) {
        const std::size_t at = form.find(ROOT_SLOT);
        if (at == std::string::npos) {
            continue;
        }
        const std::string_view f = form;
        const std::string_view a = f.substr(0, at);
        const std::string_view b = f.substr(at + ROOT_SLOT.size());
        if (name.size() > a.size() + b.size() && starts_with(name, a) && ends_with(name, b)) {
            return std::string(name.substr(a.size(), name.size() - a.size() - b.size()));
        }
    }
    return std::string(name);
}

RenamedName fill_slots(std::string_view form, std::span<const SlotValue> slots) {
    RenamedName out;
    std::size_t i = 0;
    bool literal_open = false;  // a literal run is being appended
    bool first_part = true;
    bool after_s_slot = false;  // the last part was a slot value ending in s or S (P5-02 21.2)
    // A join at every start of a part but the first.
    const auto start_part = [&out, &first_part]() {
        if (!first_part) {
            out.joins.push_back(out.name.size());
        }
        first_part = false;
    };
    while (i < form.size()) {
        const SlotValue* slot = nullptr;
        for (const SlotValue& v : slots) {
            if (form.substr(i, v.slot.size()) == v.slot) {
                slot = &v;
                break;
            }
        }
        if (slot != nullptr) {
            start_part();
            out.name += slot->value;
            i += slot->slot.size();
            literal_open = false;
            after_s_slot = !slot->value.empty() && (slot->value.back() == 's' || slot->value.back() == 'S');
            continue;
        }
        if (!literal_open) {
            start_part();
            literal_open = true;
            // P5-02 21.2, the possessive: after a value ending in s, a
            // literal run starting with 's keeps the ' alone.
            if (after_s_slot && form.substr(i, 2) == "'s") {
                out.name += '\'';
                i += 2;
                after_s_slot = false;
                continue;
            }
        }
        after_s_slot = false;
        out.name += form[i];
        ++i;
    }
    return out;
}

RenamedName fill_rename_form(std::string_view form, std::string_view name_value, std::string_view noun_value) {
    return fill_rename_form(form, name_value, noun_value, name_value);
}

RenamedName fill_rename_form(std::string_view form, std::string_view name_value, std::string_view noun_value,
                             std::string_view stem_value) {
    const std::array<SlotValue, 3> slots = {SlotValue{NAME_SLOT, name_value}, SlotValue{NOUN_SLOT, noun_value},
                                            SlotValue{STEM_SLOT, stem_value}};
    return fill_slots(form, slots);
}

std::string settlement_stem(std::string_view name, const std::vector<std::string>& settlement_forms) {
    for (const std::string& form : settlement_forms) {
        if (!starts_with(form, ROOT_SLOT)) {
            continue;
        }
        const std::string_view b = std::string_view(form).substr(ROOT_SLOT.size());
        if (b.empty() || b.front() != ' ') {
            continue;  // fused suffixes are never stripped
        }
        if (name.size() > b.size() && ends_with(name, b)) {
            return std::string(name.substr(0, name.size() - b.size()));
        }
    }
    return std::string(name);
}

std::int64_t renaming_cap(std::int64_t regions, std::int64_t permille) noexcept {
    return std::max<std::int64_t>(1, (regions * permille) / 1000);  // regions <= 65535, permille <= 1000
}

// --- The subsystem (6) -------------------------------------------------------------------------------

RenamingSubsystem::RenamingSubsystem(PeoplesSubsystem& peoples, const data::ContentSet& content,
                                     const EventCatalogue& catalogue)
    : RenamingSubsystem(peoples, renaming_tuning_from(content), catalogue) {}

RenamingSubsystem::RenamingSubsystem(PeoplesSubsystem& peoples, RenamingTuning tuning,
                                     const EventCatalogue& catalogue)
    : peoples_(peoples), tuning_(std::move(tuning)), catalogue_(catalogue) {}

std::span<const Step> RenamingSubsystem::steps() const {
    return STEPS;
}

std::vector<db::RegionNameRow> RenamingSubsystem::sorted_rows() const {
    std::vector<db::RegionNameRow> out = rows_;
    std::stable_sort(out.begin(), out.end(), [](const db::RegionNameRow& a, const db::RegionNameRow& b) {
        return std::pair(a.region, a.from_year) < std::pair(b.region, b.from_year);
    });
    return out;
}

void RenamingSubsystem::prepare(const StepContext& ctx) {
    if (prepared_) {
        return;
    }
    const HistoryInputs& in = ctx.inputs();
    chance_key_ = stream_key(in.seed, streams::hist_renaming_chance);
    name_key_ = stream_key(in.seed, streams::hist_renaming_name);
    region_count_ = static_cast<std::int64_t>(in.regions.size());
    // Section 3: the land noun recomputed from the stored rows (H8).
    nouns_ = stages::region_land_nouns(in.regions, stages::biome_definitions_from(ctx.content()),
                                       stages::region_tuning_from(ctx.content()));
    placenames_ = stages::placename_data_from(ctx.content());
    for (const db::CultureRow& c : in.cultures) {  // id order
        const data::Definition* d = ctx.content().find(data::CULTURE_KIND, c.key);
        if (d == nullptr) {
            fail("history: renaming: culture '" + c.key + "' is not in the content");
        }
        culture_forms_.emplace_back(c.id, string_list(d->record.get_record("naming"), "civilization_forms"));
        settlement_forms_.emplace_back(c.id, string_list(d->record.get_record("naming"), "settlement_forms"));
    }
    windows_.assign(tuning_.rules.size(),
                    std::vector<std::vector<WindowEntry>>(static_cast<std::size_t>(region_count_)));
    renamings_.assign(static_cast<std::size_t>(region_count_), {});
    prepared_ = true;
}

const std::vector<std::string>& RenamingSubsystem::civilization_forms(std::int64_t culture) const {
    for (const auto& [id, forms] : culture_forms_) {
        if (id == culture) {
            return forms;
        }
    }
    static const std::vector<std::string> none;
    return none;
}

const std::vector<std::string>& RenamingSubsystem::settlement_forms(std::int64_t culture) const {
    for (const auto& [id, forms] : settlement_forms_) {
        if (id == culture) {
            return forms;
        }
    }
    static const std::vector<std::string> none;
    return none;
}

std::int64_t RenamingSubsystem::first_civilization(const StepContext& ctx, std::int64_t event) const {
    const KindDef* kind = catalogue_.find(ctx.event_kind(event));
    if (kind == nullptr) {
        return 0;
    }
    // Participants are stored in ascending role seq (W4), so the first
    // civilization participant is the first one of the first civilization
    // role that has one.
    for (const RoleDef& r : kind->roles) {
        if (r.type != EntityType::civilization) {
            continue;
        }
        if (const std::int64_t id = ctx.event_participant(event, r.role); id != 0) {
            return id;
        }
    }
    return 0;
}

void RenamingSubsystem::read_event(const StepContext& ctx, std::int64_t event,
                                   std::vector<std::vector<Candidate>>& candidates) {
    const std::int64_t region = ctx.event_region(event);
    if (region < 1 || region > region_count_) {
        return;  // 0: no region
    }
    const std::string_view kind = ctx.event_kind(event);
    const std::int64_t year = ctx.event_year(event);
    for (std::size_t i = 0; i < tuning_.rules.size(); ++i) {
        const RenameRule& rule = tuning_.rules[i];
        if (!contains(rule.kinds, kind)) {
            continue;
        }
        std::int64_t m = 1;
        if (!rule.values.empty()) {
            m = 0;
            for (const std::string& v : rule.values) {
                m = checked_add(m, ctx.event_value(event, v));
            }
        }
        std::vector<WindowEntry>& w = windows_[i][static_cast<std::size_t>(region - 1)];
        w.push_back(WindowEntry{year, m});
        const std::int64_t oldest = year - rule.window_years;  // entries with year <= oldest drop
        std::size_t drop = 0;
        while (drop < w.size() && w[drop].year <= oldest) {
            ++drop;
        }
        w.erase(w.begin(), w.begin() + static_cast<std::ptrdiff_t>(drop));
        std::int64_t sum = 0;
        for (const WindowEntry& x : w) {
            sum = checked_add(sum, x.magnitude);
        }
        if (sum < rule.min_magnitude || ctx.event_salience(event) < rule.min_salience) {
            continue;
        }
        if (rule.capital) {
            const std::int64_t s = ctx.event_settlement(event);
            const std::int64_t civ = first_civilization(ctx, event);
            if (s == 0 || civ == 0 || ctx.state().civ(civ).capital != s) {
                continue;
            }
        }
        const std::uint64_t k = static_cast<std::uint64_t>(i) + 1U;  // the rule number
        if (chance_pm(ctx.draw(chance_key_, word(event), word(region), k), rule.chance_permille)) {
            candidates[static_cast<std::size_t>(region - 1)].push_back(Candidate{i, event});
            ++stats_.candidates;
        }
    }
}

std::optional<std::string> RenamingSubsystem::build_name(const StepContext& ctx, const RenameRule& rule,
                                                         std::int64_t event, std::int64_t region) const {
    std::optional<std::string> name_value;
    std::optional<std::string> stem_value;  // P5-02 21.2: {stem}
    const auto of_settlement = [&](std::int64_t s) {
        const Settlement& t = ctx.state().settlement(s);
        name_value = t.name;
        const std::int64_t founder = t.founder_civ;
        stem_value = founder != 0 ? settlement_stem(t.name, settlement_forms(ctx.state().civ(founder).culture))
                                  : t.name;
    };
    if (rule.name_role == SETTLEMENT_ROLE) {
        if (const std::int64_t s = ctx.event_settlement(event); s != 0) {
            of_settlement(s);
        }
    } else if (!rule.name_role.empty()) {
        const KindDef* kind = catalogue_.find(ctx.event_kind(event));
        const RoleDef* role = kind != nullptr ? kind->find_role(rule.name_role) : nullptr;
        const std::int64_t id = role != nullptr ? ctx.event_participant(event, rule.name_role) : 0;
        if (id != 0 && role->type == EntityType::civilization) {
            const Civilization& c = ctx.state().civ(id);
            name_value = realm_root(c.name, civilization_forms(c.culture));
        } else if (id != 0 && role->type == EntityType::settlement) {
            of_settlement(id);
        } else if (id != 0 && role->type == EntityType::monster && monsters_ != nullptr) {
            // P5-02 9.3. A host named for its place (P5-03 12.2) has no root:
            // no {name} then, so only the forms without one can be chosen
            // (P5-03a proposals entry; an empty root gave "'s Shadow").
            if (std::string root = monsters_->monster_root(id); !root.empty()) {
                name_value = std::move(root);
            }
        }
    }
    if (name_value && !stem_value) {
        stem_value = name_value;  // {stem} = {name} for any other role
    }
    const std::string& noun = nouns_[static_cast<std::size_t>(region - 1)];
    const std::vector<std::string>& forms = rule.forms;
    const std::uint64_t n = forms.size();
    const std::uint64_t start = pick(ctx.draw(name_key_, word(event), word(region), 0), n);
    const NameRegistry& registry = peoples_.name_registry();
    for (std::uint64_t j = 0; j < n; ++j) {
        const std::string& f = forms[static_cast<std::size_t>((start + j) % n)];
        const bool needs_name = f.find(NAME_SLOT) != std::string::npos || f.find(STEM_SLOT) != std::string::npos;
        if (needs_name && !name_value) {
            continue;  // no {name} for this event: the form cannot be filled
        }
        if (noun.empty() && f.find(NOUN_SLOT) != std::string::npos) {
            continue;  // a hand-made region without a land noun
        }
        RenamedName filled =
            fill_rename_form(f, needs_name ? std::string_view(*name_value) : std::string_view(), noun,
                             needs_name ? std::string_view(*stem_value) : std::string_view());
        if (stages::renamed_region_name_valid(filled.name, filled.joins, placenames_) &&
            !registry.contains(filled.name)) {
            return std::move(filled.name);
        }
    }
    return std::nullopt;
}

void RenamingSubsystem::choose(StepContext& ctx, std::int64_t region, const Candidate& c) {
    const RenameRule& rule = tuning_.rules[c.rule];
    const std::int64_t year = ctx.year();
    const std::vector<Renaming>& mine = renamings_[static_cast<std::size_t>(region - 1)];
    // 1. Fewer than region_max renamings.
    if (static_cast<std::int64_t>(mine.size()) >= tuning_.region_max) {
        return;
    }
    // 2. The gap, and a weightier deed for a second name.
    if (!mine.empty() &&
        (year - mine.back().year < tuning_.region_gap_years || rule.weight <= mine.back().weight)) {
        return;
    }
    // 3. The world cap.
    if (static_cast<std::int64_t>(renaming_years_.size()) >= renaming_cap(region_count_, tuning_.world_max_permille)) {
        return;
    }
    // 4. The century cap.
    std::int64_t recent = 0;
    for (const std::int64_t y : renaming_years_) {
        recent += y > year - tuning_.century_years ? 1 : 0;
    }
    if (recent >= renaming_cap(region_count_, tuning_.century_max_permille)) {
        return;
    }
    // 5. The namer, charged in a quiet step (W9; with no namer the engine
    // charges civilization 0, so its budget is tested).
    const std::int64_t namer = rule.namer_role.empty() ? 0 : ctx.event_participant(c.event, rule.namer_role);
    if (ctx.step() == Step::q4 && ctx.quiet_events_left(namer) <= 0) {
        return;
    }
    // 6. A name.
    ++stats_.reached_name;
    std::optional<std::string> name = build_name(ctx, rule, c.event, region);
    if (!name) {
        ++stats_.no_name;
        return;
    }
    // Write (6 item 3). Every value the event needs is read before the append.
    std::int64_t settlement = ctx.event_settlement(c.event);
    if (settlement != 0 && ctx.state().settlement(settlement).region != region) {
        settlement = 0;
    }
    const std::int64_t culture = namer != 0 ? ctx.state().civ(namer).culture : 0;
    EventSpec spec;
    spec.kind = RENAMED_KIND;
    spec.year = year;
    spec.end_year = year;
    spec.region = region;
    spec.settlement = settlement;
    if (namer != 0) {
        spec.participants.push_back(Participant{NAMER_ROLE, EntityType::civilization, namer});
    }
    spec.causes.push_back(c.event);
    const std::int64_t id = ctx.append(spec);
    peoples_.add_name(*name);
    rows_.push_back(db::RegionNameRow{region, year, std::move(*name), culture, id});
    renamings_[static_cast<std::size_t>(region - 1)].push_back(Renaming{year, rule.weight});
    renaming_years_.push_back(year);
    for (std::vector<std::vector<WindowEntry>>& by_region : windows_) {
        by_region[static_cast<std::size_t>(region - 1)].clear();
    }
    ++stats_.renamed;
}

void RenamingSubsystem::run(StepContext& ctx) {
    prepare(ctx);
    const std::int64_t last = ctx.last_event_id();
    std::vector<std::vector<Candidate>> candidates(static_cast<std::size_t>(region_count_));
    for (std::int64_t e = cursor_; e <= last; ++e) {
        read_event(ctx, e, candidates);
    }
    cursor_ = last + 1;
    // 2. Regions ascending; the candidate of the largest weight, then the
    // smallest event.
    for (std::int64_t r = 1; r <= region_count_; ++r) {
        const std::vector<Candidate>& list = candidates[static_cast<std::size_t>(r - 1)];
        if (list.empty()) {
            continue;
        }
        const Candidate* best = &list.front();
        for (const Candidate& c : list) {
            const std::int64_t wc = tuning_.rules[c.rule].weight;
            const std::int64_t wb = tuning_.rules[best->rule].weight;
            if (wc > wb || (wc == wb && c.event < best->event)) {
                best = &c;
            }
        }
        choose(ctx, r, *best);
    }
}

}  // namespace worldgen::history
