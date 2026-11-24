// Data checks across kinds for the history (spec P3-03 section 6).

#include "worldgen/history/data_checks.hpp"

#include <string>

#include "worldgen/data/schema.hpp"
#include "worldgen/data/settings.hpp"
#include "worldgen/db/schema.hpp"

namespace worldgen::history {

namespace {

constexpr std::string_view PLURAL = "{plural}";
constexpr std::string_view ADJECTIVE = "{adjective}";

constexpr bool is_ascii_letter(char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

// D5: ASCII letters, space, ' and -; no leading or trailing space, no two
// spaces in a row. (An empty name cannot occur: the form holds a
// placeholder and the words are at least one byte.)
bool is_valid_age_name(std::string_view name) noexcept {
    if (name.empty() || name.front() == ' ' || name.back() == ' ') {
        return false;
    }
    for (std::size_t i = 0; i < name.size(); ++i) {
        const char c = name[i];
        if (c == ' ') {
            if (i + 1 < name.size() && name[i + 1] == ' ') {
                return false;
            }
        } else if (!is_ascii_letter(c) && c != '\'' && c != '-') {
            return false;
        }
    }
    return true;
}

data::LoadError race_error(std::string message) {
    data::LoadError e;
    e.path = std::string(data::RACE_DIRECTORY);
    e.message = std::move(message);
    return e;
}

// D1, D2: the biome preferences of one race, in list order.
void check_race_biomes(const data::ContentSet& content, const data::Definition& race,
                       std::vector<data::LoadError>& out) {
    for (const data::Value& entry : race.record.get_array("biomes")) {
        const std::string& biome_id = entry.as_record().get_string("biome");
        const data::Definition* biome = content.find(data::BIOME_KIND, biome_id);
        if (biome == nullptr) {
            continue;  // a broken ref is the loader's error
        }
        if (biome->record.get_bool("water")) {
            out.push_back(race_error("race '" + race.id + "' prefers water biome '" + biome_id + "'"));
            continue;
        }
        const bool natural = biome->record.get_bool("fallback") || !biome->record.get_array("rules").empty();
        if (!natural) {
            out.push_back(race_error("race '" + race.id + "' prefers biome '" + biome_id +
                                     "', which no geography stage assigns"));
        }
    }
}

// D3: the race's culture belongs to the race.
void check_race_culture(const data::ContentSet& content, const data::Definition& race,
                        std::vector<data::LoadError>& out) {
    const std::string& culture_id = race.record.get_string("culture");
    const data::Definition* culture = content.find(data::CULTURE_KIND, culture_id);
    if (culture == nullptr) {
        return;  // a broken ref is the loader's error
    }
    const std::string& owner = culture->record.get_string("race");
    if (owner != race.id) {
        out.push_back(race_error("race '" + race.id + "' has culture '" + culture_id + "' of race '" + owner + "'"));
    }
}

// D4, D5: the race's age name before any numeral.
void check_race_age_name(std::string_view form, const data::Definition& race, std::vector<data::LoadError>& out) {
    const std::string name =
        age_name_text(form, race.record.get_string("plural"), race.record.get_string("adjective"));
    if (name.size() > AGE_NAME_MAX_BYTES) {
        out.push_back(race_error("race '" + race.id + "' has the age name '" + name +
                                 "', which is longer than 48 bytes"));
    }
    if (!is_valid_age_name(name)) {
        out.push_back(race_error("race '" + race.id + "' has the age name '" + name +
                                 "', which is not ASCII letters with single spaces, ' and -"));
    }
}

// P5-02 4.1, 4.2 (reading 1): the checks of kinds beast and monster that
// need another kind, as "check '<name>' failed for '<kind>.<id>': <message>"
// at the kind's directory.
data::LoadError monster_error(std::string_view directory, std::string_view check, std::string_view kind,
                              const std::string& id, const std::string& message) {
    data::LoadError e;
    e.path = std::string(directory);
    e.message = "check '" + std::string(check) + "' failed for '" + std::string(kind) + "." + id + "': " + message;
    return e;
}

void check_habitat_biomes(const data::ContentSet& content, std::string_view directory, std::string_view kind,
                          std::string_view check, std::string_view field, std::vector<data::LoadError>& out) {
    const data::KindContent* defs = content.find_kind(kind);
    if (defs == nullptr) {
        return;
    }
    for (const data::Definition& d : defs->definitions) {
        const std::vector<data::Value>& list = d.record.get_array(field);
        for (std::size_t i = 0; i < list.size(); ++i) {
            const std::string& b = list[i].as_record().get_string("biome");
            if (content.find(data::BIOME_KIND, b) == nullptr) {
                out.push_back(monster_error(directory, check, kind, d.id,
                                            std::string(field) + "[" + db::format_u64(i) + "].biome '" + b +
                                                "' is not a biome"));
                break;  // the first failure of the definition
            }
        }
    }
}

void check_monster_cultures(const data::ContentSet& content, std::vector<data::LoadError>& out) {
    const data::KindContent* defs = content.find_kind(data::MONSTER_KIND);
    if (defs == nullptr) {
        return;
    }
    for (const data::Definition& d : defs->definitions) {
        const std::string& c = d.record.get_string("naming_culture");
        if (content.find(data::CULTURE_KIND, c) == nullptr) {
            out.push_back(monster_error(data::MONSTER_DIRECTORY, "monster_culture", data::MONSTER_KIND, d.id,
                                        "naming_culture '" + c + "' is not a culture"));
        }
    }
}

// P5-03 4.1: race_stances, the first failing entry of each race.
void check_race_stances(const data::ContentSet& content, const data::Definition& race,
                        std::vector<data::LoadError>& out) {
    const std::vector<data::Value>& list = race.record.get_array("stances");
    for (std::size_t i = 0; i < list.size(); ++i) {
        const std::string& r = list[i].as_record().get_string("race");
        const std::string at = "stances[" + db::format_u64(i) + "].race '" + r + "' ";
        std::string problem;
        if (content.find(data::RACE_KIND, r) == nullptr) {
            problem = at + "is not a race";
        } else if (r == race.id) {
            problem = at + "is the race itself";
        } else {
            for (std::size_t j = 0; j < i; ++j) {
                if (list[j].as_record().get_string("race") == r) {
                    problem = at + "is listed twice";
                    break;
                }
            }
        }
        if (!problem.empty()) {
            out.push_back(monster_error(data::RACE_DIRECTORY, "race_stances", data::RACE_KIND, race.id, problem));
            return;
        }
    }
}

}  // namespace

std::string age_name_text(std::string_view form, std::string_view plural, std::string_view adjective) {
    std::string out;
    std::size_t i = 0;
    while (i < form.size()) {
        if (form.substr(i, PLURAL.size()) == PLURAL) {
            out.append(plural);
            i += PLURAL.size();
        } else if (form.substr(i, ADJECTIVE.size()) == ADJECTIVE) {
            out.append(adjective);
            i += ADJECTIVE.size();
        } else {
            out += form[i];
            ++i;
        }
    }
    return out;
}

std::vector<data::LoadError> check_history_data(const data::ContentSet& content) {
    std::vector<data::LoadError> out;
    // D0: at least one race definition.
    const data::KindContent* races = content.find_kind(data::RACE_KIND);
    if (races == nullptr || races->definitions.empty()) {
        out.push_back(race_error("no race is defined; kind 'race' needs at least one definition"));
    }
    const data::Definition* tuning = content.find(data::HISTORY_KIND, data::HISTORY_CHRONOLOGY_ID);
    for (const std::string& id : data::race_ids_in_order(content)) {
        const data::Definition* race = content.find(data::RACE_KIND, id);
        check_race_biomes(content, *race, out);
        check_race_culture(content, *race, out);
        check_race_stances(content, *race, out);  // P5-03 4.1
        if (tuning != nullptr) {
            check_race_age_name(tuning->record.get_string("age_name_form"), *race, out);
        }
    }
    // P5-02 4.1, 4.2: beast_habitat_biomes, monster_biomes, monster_culture.
    check_habitat_biomes(content, data::BEAST_DIRECTORY, data::BEAST_KIND, "beast_habitat_biomes", "habitat", out);
    check_habitat_biomes(content, data::MONSTER_DIRECTORY, data::MONSTER_KIND, "monster_biomes", "biomes", out);
    check_monster_cultures(content, out);
    data::sort_errors(out);
    return out;
}

}  // namespace worldgen::history
