// The history settings as the pipeline sees them (spec P3-03 4.1, 4.2).

#include "worldgen/history/params.hpp"

#include "worldgen/core/error.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/db/schema.hpp"

namespace worldgen::history {

namespace {

bool is_lower_hex_64(std::string_view text) noexcept {
    if (text.size() != 64) {
        return false;
    }
    for (const char c : text) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
            return false;
        }
    }
    return true;
}

bool races_valid(const std::vector<std::string>& races) {
    if (races.empty()) {
        return false;
    }
    for (std::size_t i = 0; i < races.size(); ++i) {
        if (!data::is_valid_id(races[i])) {
            return false;
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (races[j] == races[i]) {
                return false;
            }
        }
    }
    return true;
}

std::string join_races(const std::vector<std::string>& races) {
    std::string out;
    for (const std::string& id : races) {
        if (!out.empty()) {
            out += ',';
        }
        out += id;
    }
    return out;
}

std::vector<std::string> split_races(std::string_view text) {
    std::vector<std::string> out;
    std::size_t start = 0;
    for (;;) {
        const std::size_t comma = text.find(',', start);
        out.emplace_back(text.substr(start, comma == std::string_view::npos ? std::string_view::npos : comma - start));
        if (comma == std::string_view::npos) {
            return out;
        }
        start = comma + 1;
    }
}

const std::string& meta_value(const db::WorldReader& world, std::string_view key, std::string& holder) {
    std::optional<std::string> v = world.meta(key);
    if (!v || !db::is_canonical_history_meta_value(key, *v)) {
        throw FatalError("history_params_from_meta: meta key '" + std::string(key) + "' is missing or not canonical");
    }
    holder = std::move(*v);
    return holder;
}

}  // namespace

HistoryParams history_params_from(const data::Settings& settings) {
    if (!settings.races || !settings.races_origins.empty()) {
        fatal("history_params_from: the races are not resolved");
    }
    HistoryParams p;
    p.span = settings.history_span;
    p.detail = settings.history_detail;
    p.civs_per_race = settings.civs_per_race;
    p.races = *settings.races;
    p.event_intensity = settings.event_intensity;
    p.monster_pressure = settings.monster_pressure;
    p.divine_activity = settings.divine_activity;
    p.divine_script = settings.divine_script;
    return p;
}

std::string_view history_params_problem(const HistoryParams& params) {
    if (!data::is_valid_history_span(params.span)) {
        return "span";
    }
    if (params.civs_per_race &&
        (*params.civs_per_race < data::MIN_CIVS_PER_RACE || *params.civs_per_race > data::MAX_CIVS_PER_RACE)) {
        return "civs_per_race";
    }
    if (!races_valid(params.races)) {
        return "races";
    }
    if (!params.divine_script.empty() && !is_lower_hex_64(params.divine_script)) {
        return "divine_script";
    }
    return {};
}

std::vector<std::pair<std::string_view, std::string>> history_meta_entries(const HistoryParams& params) {
    if (!history_params_problem(params).empty()) {
        throw FatalError("history_meta_entries: params." + std::string(history_params_problem(params)) + " is invalid");
    }
    std::vector<std::pair<std::string_view, std::string>> out;
    out.emplace_back(db::META_PARAM_CIVS_PER_RACE,
                     params.civs_per_race ? db::format_i64(*params.civs_per_race) : std::string("auto"));
    out.emplace_back(db::META_PARAM_DIVINE_ACTIVITY, std::string(data::divine_activity_name(params.divine_activity)));
    out.emplace_back(db::META_PARAM_DIVINE_SCRIPT, params.divine_script);
    out.emplace_back(db::META_PARAM_EVENT_INTENSITY, std::string(data::event_intensity_name(params.event_intensity)));
    out.emplace_back(db::META_PARAM_HISTORY_DETAIL, std::string(data::history_detail_name(params.detail)));
    out.emplace_back(db::META_PARAM_HISTORY_SPAN, db::format_i64(params.span));
    out.emplace_back(db::META_PARAM_MONSTER_PRESSURE,
                     std::string(data::monster_pressure_name(params.monster_pressure)));
    out.emplace_back(db::META_PARAM_RACES, join_races(params.races));
    return out;
}

HistoryParams history_params_from_meta(const db::WorldReader& world) {
    HistoryParams p;
    std::string v;
    const std::string& civs = meta_value(world, db::META_PARAM_CIVS_PER_RACE, v);
    if (civs == "auto") {
        p.civs_per_race = std::nullopt;
    } else {
        p.civs_per_race = static_cast<std::int64_t>(*db::parse_canonical_u64(civs));
    }
    p.divine_activity = *data::parse_divine_activity(meta_value(world, db::META_PARAM_DIVINE_ACTIVITY, v));
    p.divine_script = meta_value(world, db::META_PARAM_DIVINE_SCRIPT, v);
    p.event_intensity = *data::parse_event_intensity(meta_value(world, db::META_PARAM_EVENT_INTENSITY, v));
    p.detail = *data::parse_history_detail(meta_value(world, db::META_PARAM_HISTORY_DETAIL, v));
    p.span = static_cast<std::int64_t>(*db::parse_canonical_u64(meta_value(world, db::META_PARAM_HISTORY_SPAN, v)));
    p.monster_pressure = *data::parse_monster_pressure(meta_value(world, db::META_PARAM_MONSTER_PRESSURE, v));
    p.races = split_races(meta_value(world, db::META_PARAM_RACES, v));
    return p;
}

}  // namespace worldgen::history
