// Stage P3-03 chronology (spec docs/specs/p3-03-chronology.md).

#include "worldgen/stages/chronology.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <utility>

#include "worldgen/core/decimal.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/streams.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/data/settings.hpp"
#include "worldgen/db/schema.hpp"
#include "worldgen/history/draw.hpp"

namespace worldgen::stages {

namespace {

[[noreturn]] void fail(const std::string& message) {
    throw FatalError(message);
}

std::string int_text(std::int64_t v) {
    return db::format_i64(v);
}

// --- C1 item 2: tuning ------------------------------------------------------------

void tuning_field(bool ok, std::string_view field) {
    if (!ok) {
        fail("chronology: tuning: " + std::string(field) + " outside its schema range");
    }
}

// The named check chronology_age_name_form (P3-01 5.1): at least one
// placeholder, every '{' starting {plural} or {adjective}.
bool age_form_placeholders_ok(std::string_view form) {
    static constexpr std::string_view PLACEHOLDERS[] = {"{plural}", "{adjective}"};
    std::size_t found = 0;
    std::size_t i = 0;
    while (i < form.size()) {
        if (form[i] != '{') {
            ++i;
            continue;
        }
        bool matched = false;
        for (const std::string_view p : PLACEHOLDERS) {
            if (form.substr(i, p.size()) == p) {
                matched = true;
                i += p.size();
                ++found;
                break;
            }
        }
        if (!matched) {
            return false;
        }
    }
    return found > 0;
}

// --- C1 item 3: settings -----------------------------------------------------------

// The race definitions in race order (ascending order; P3-01 makes orders
// 1 ... n, so this is also id order of table race).
std::vector<const data::Definition*> races_in_order(const data::ContentSet& content) {
    std::vector<const data::Definition*> out;
    for (const std::string& id : data::race_ids_in_order(content)) {
        out.push_back(content.find(data::RACE_KIND, id));
    }
    return out;
}

const data::Definition* find_race(const std::vector<const data::Definition*>& races, std::string_view id) {
    for (const data::Definition* d : races) {
        if (d->id == id) {
            return d;
        }
    }
    return nullptr;
}

// The present race definitions, in the order of params.races, after the
// checks of C1 item 3 in the order span, empty list, unknown ids (first in
// list order), race order.
std::vector<const data::Definition*> check_settings(const history::HistoryParams& params,
                                                    const std::vector<const data::Definition*>& races) {
    if (!data::is_valid_history_span(params.span)) {
        fail("chronology: settings: history_span " + int_text(params.span) + " outside 2000 to 12000");
    }
    if (params.races.empty()) {
        fail("chronology: settings: no race present");
    }
    std::vector<const data::Definition*> present;
    for (const std::string& id : params.races) {
        const data::Definition* d = find_race(races, id);
        if (d == nullptr) {
            fail("chronology: settings: unknown race '" + id + "'");
        }
        present.push_back(d);
    }
    for (std::size_t i = 1; i < present.size(); ++i) {
        if (present[i]->record.get_int("order") <= present[i - 1]->record.get_int("order")) {
            fail("chronology: settings: races not in race order at '" + present[i]->id + "'");
        }
    }
    return present;
}

// --- Rows ------------------------------------------------------------------------------

std::int64_t camp_code(const std::string& camp) {
    if (camp == "good") {
        return 0;
    }
    if (camp == "neutral") {
        return 1;
    }
    if (camp == "evil") {
        return 2;
    }
    fail("chronology: race camp '" + camp + "' is not good, neutral or evil");
}

// culture.id: 1, 2, ... in canonical id order (the kind's definition order).
std::int64_t culture_number(const data::ContentSet& content, std::string_view culture_id) {
    const data::KindContent* cultures = content.find_kind(data::CULTURE_KIND);
    if (cultures != nullptr) {
        for (std::size_t i = 0; i < cultures->definitions.size(); ++i) {
            if (cultures->definitions[i].id == culture_id) {
                return static_cast<std::int64_t>(i) + 1;
            }
        }
    }
    fail("chronology: culture '" + std::string(culture_id) + "' is not defined");
}

std::int64_t race_number(const std::vector<const data::Definition*>& races, std::string_view race_id) {
    const data::Definition* d = find_race(races, race_id);
    if (d == nullptr) {
        fail("chronology: race '" + std::string(race_id) + "' is not defined");
    }
    return d->record.get_int("order");
}

std::vector<db::RaceBiomeRow> race_biome_rows(const data::ContentSet& content, const data::Definition& race) {
    const std::int64_t id = race.record.get_int("order");
    std::vector<db::RaceBiomeRow> rows;
    for (const data::Value& entry : race.record.get_array("biomes")) {
        const data::Record& e = entry.as_record();
        const data::Definition* biome = content.find(data::BIOME_KIND, e.get_string("biome"));
        if (biome == nullptr) {
            fail("chronology: biome '" + e.get_string("biome") + "' is not defined");
        }
        rows.push_back(db::RaceBiomeRow{id, biome->record.get_int("code"), e.get_int("weight")});
    }
    // (race, biome) order: by biome code within the race. Codes are unique
    // (biome_code_unique) and so are the listed biomes (race_biomes_unique).
    std::stable_sort(rows.begin(), rows.end(),
                     [](const db::RaceBiomeRow& a, const db::RaceBiomeRow& b) { return a.biome < b.biome; });
    return rows;
}

}  // namespace

// --- Tuning ------------------------------------------------------------------------------

ChronologyTuning chronology_tuning_from(const data::ContentSet& content) {
    const data::Definition* d = content.find(data::HISTORY_KIND, data::HISTORY_CHRONOLOGY_ID);
    if (d == nullptr) {
        fail("chronology: the content has no definition history.chronology");
    }
    ChronologyTuning t;
    t.awakening_jitter_frac = d->record.get_fx("awakening_jitter_frac");
    t.awakening_min_gap_years = d->record.get_int("awakening_min_gap_years");
    t.age_name_form = d->record.get_string("age_name_form");
    return t;
}

void check_chronology_tuning(const ChronologyTuning& t) {
    const Fx jitter_max = parse_decimal("0.05").value;  // the schema bound, as the loader reads it
    tuning_field(Fx{} <= t.awakening_jitter_frac && t.awakening_jitter_frac <= jitter_max, "awakening_jitter_frac");
    tuning_field(1 <= t.awakening_min_gap_years && t.awakening_min_gap_years <= 2000, "awakening_min_gap_years");
    tuning_field(t.age_name_form.size() <= 64U, "age_name_form");
    if (!age_form_placeholders_ok(t.age_name_form)) {
        fail("chronology: tuning: age_name_form must contain {plural} or {adjective} and no other placeholder");
    }
}

// --- C2, C3 ---------------------------------------------------------------------------

AwakeningBounds awakening_bounds(std::int64_t span, std::int64_t present, const ChronologyTuning& tuning) {
    if (span < 1 || present < 1) {
        fail("awakening_bounds: span and present must be at least 1");
    }
    AwakeningBounds b;
    b.gap = std::min(tuning.awakening_min_gap_years, span / present);
    b.jmax = mul_int(tuning.awakening_jitter_frac, span).round_int();
    return b;
}

std::vector<std::int64_t> awakening_years(std::uint64_t seed, std::int64_t span,
                                          std::span<const std::int64_t> present_ids,
                                          std::span<const Fx> present_shares, const ChronologyTuning& tuning) {
    if (present_ids.empty() || present_ids.size() != present_shares.size()) {
        fail("awakening_years: present_ids and present_shares must be non-empty and of one length");
    }
    const auto p = static_cast<std::int64_t>(present_ids.size());  // at most a few races
    const AwakeningBounds b = awakening_bounds(span, p, tuning);
    if (b.jmax < 0) {
        fail("awakening_years: negative jitter");
    }
    // The schedule is strict only while G >= 1 (C3); with loaded data G >= 333
    // (span >= 2000, at most 6 races, gap >= 1).
    if (b.gap < 1) {
        fail("awakening_years: the least gap is below 1 year");
    }
    const std::uint64_t k_aw = stream_key(seed, streams::hist_chronology_awakening);
    const auto choices = static_cast<std::uint64_t>(2 * b.jmax + 1);
    std::vector<std::int64_t> years;
    years.reserve(present_ids.size());
    years.push_back(0);  // the first present race opens history
    for (std::size_t k = 1; k < present_ids.size(); ++k) {
        const std::int64_t i = static_cast<std::int64_t>(k) + 1;  // 1-based position of C3
        const std::int64_t base = mul_int(present_shares[k], span).round_int();
        const std::uint64_t u = history::draw(k_aw, 0, static_cast<std::uint64_t>(present_ids[k]), 0, 0);
        const std::int64_t d = static_cast<std::int64_t>(history::pick(u, choices)) - b.jmax;
        const std::int64_t lo = years[k - 1] + b.gap;
        const std::int64_t hi = span - b.gap * (p - i + 1);
        years.push_back(std::min(std::max(base + d, lo), hi));
    }
    return years;
}

// --- C1 to C5 --------------------------------------------------------------------------

ChronologyResult chronology_stage(const data::ContentSet& content, const history::HistoryParams& params,
                                  std::uint64_t seed, std::span<const std::string> region_names) {
    // C1 item 2: tuning.
    const ChronologyTuning tuning = chronology_tuning_from(content);
    check_chronology_tuning(tuning);
    // C1 item 3: settings.
    const std::vector<const data::Definition*> races = races_in_order(content);
    const std::vector<const data::Definition*> present = check_settings(params, races);
    // C1 item 4: data.
    if (const std::vector<data::LoadError> errors = history::check_history_data(content); !errors.empty()) {
        fail("chronology: data: " + errors.front().message);
    }
    // P3-02 r5 11.3 (P3-03 r3): the forms' fixed text holds no filtered
    // fragment, checked before any name is drawn.
    const history::NameFilter filter = history::name_filter_from(content);
    history::check_names_form_text(content, filter);

    // C2, C3.
    std::vector<std::int64_t> present_ids;
    std::vector<Fx> present_shares;
    for (const data::Definition* d : present) {
        present_ids.push_back(d->record.get_int("order"));
        present_shares.push_back(d->record.get_fx("awakening_share"));
    }
    const std::vector<std::int64_t> years = awakening_years(seed, params.span, present_ids, present_shares, tuning);

    ChronologyResult out;
    // 8.1 step 1: every region name, in id order.
    history::NameRegistry registry;
    for (const std::string& name : region_names) {
        registry.add(name);
    }

    // C4: ages, in age id order (8.1 step 2).
    for (std::size_t i = 0; i < present.size(); ++i) {
        const data::Definition& r = *present[i];
        db::AgeRow age;
        age.id = static_cast<std::int64_t>(i) + 1;
        age.race = present_ids[i];
        age.start_year = years[i];
        age.end_year = (i + 1 < present.size() ? years[i + 1] : params.span) - 1;
        std::string name =
            age_name_text(tuning.age_name_form, r.record.get_string("plural"), r.record.get_string("adjective"));
        if (registry.contains(name)) {
            std::uint64_t k = 2;
            while (registry.contains(name + " " + history::roman(k))) {
                ++k;
            }
            name += " " + history::roman(k);
        }
        registry.add(name);
        age.name = std::move(name);
        out.ages.push_back(std::move(age));
    }

    // Race, race_biome and culture rows; C5: gods in race id order (8.1 step 3).
    for (const data::Definition* r : races) {
        const std::int64_t id = r->record.get_int("order");
        const std::string& culture_id = r->record.get_string("culture");
        const std::int64_t culture = culture_number(content, culture_id);
        db::RaceRow row;
        row.id = id;
        row.key = r->id;
        row.name = r->record.get_string("name");
        row.plural = r->record.get_string("plural");
        row.adjective = r->record.get_string("adjective");
        row.camp = camp_code(r->record.get_string("camp"));
        row.lifespan_years = r->record.get_int("lifespan_years");
        row.generation_years = r->record.get_int("generation_years");
        row.culture = culture;
        row.present = 0;
        for (std::size_t i = 0; i < present.size(); ++i) {
            if (present[i]->id == r->id) {
                row.present = 1;
                row.awakening_year = years[i];
            }
        }
        row.creator_god = id;
        out.races.push_back(std::move(row));
        for (db::RaceBiomeRow& b : race_biome_rows(content, *r)) {
            out.race_biomes.push_back(b);
        }

        const data::Definition* culture_def = content.find(data::CULTURE_KIND, culture_id);
        const history::CultureNaming naming = history::culture_naming_from(culture_def->record.get_record("naming"));
        history::NameChoice choice =
            history::choose_name(naming, filter, culture_id, history::NameKind::god, seed,
                                 static_cast<std::uint64_t>(culture), static_cast<std::uint64_t>(id), registry);
        out.gods.push_back(db::GodRow{id, choice.name, camp_code(r->record.get_string("camp"))});
        out.god_names.push_back(std::move(choice));
    }

    if (const data::KindContent* cultures = content.find_kind(data::CULTURE_KIND)) {
        for (std::size_t i = 0; i < cultures->definitions.size(); ++i) {
            const data::Definition& c = cultures->definitions[i];
            out.cultures.push_back(db::CultureRow{static_cast<std::int64_t>(i) + 1, c.id, c.record.get_string("name"),
                                                  race_number(races, c.record.get_string("race"))});
        }
    }
    return out;
}

void run_chronology(StageContext& context) {
    const std::span<const db::RegionRow> regions = context.regions();
    if (regions.empty()) {
        fail("chronology: input precondition: no region rows");
    }
    std::vector<std::string> names;
    names.reserve(regions.size());
    for (const db::RegionRow& r : regions) {
        names.push_back(r.name);
    }
    const ChronologyResult result =
        chronology_stage(context.content(), context.history_params(), context.seed(), names);
    // C6: one transaction (the pipeline's), the five tables in this order.
    for (const db::RaceRow& row : result.races) {
        context.write_race(row);
    }
    for (const db::RaceBiomeRow& row : result.race_biomes) {
        context.write_race_biome(row);
    }
    for (const db::CultureRow& row : result.cultures) {
        context.write_culture(row);
    }
    for (const db::GodRow& row : result.gods) {
        context.write_god(row);
    }
    for (const db::AgeRow& row : result.ages) {
        context.write_age(row);
    }
}

}  // namespace worldgen::stages
