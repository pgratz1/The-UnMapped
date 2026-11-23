#pragma once

// The event catalogue of the history (spec P3-04 section 8): the kinds of
// event, their roles and value keys (compiled code), their base salience
// and English templates (data, history.report), the template grammar
// (8.4), and the renderer every reader of the log uses (8.5).
//
// Kind ids are fixed: later specs append kinds, never renumber or edit one
// (8.2, "A shipped kind is never edited").

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "worldgen/data/content.hpp"
#include "worldgen/data/value.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/db/writer.hpp"

namespace worldgen::history {

// Entity type codes (P3-04 6.1, 9.2); 6 to 9 are reserved for P5 and P6.
enum class EntityType : std::int64_t {
    race = 1,
    god = 2,
    civilization = 3,
    settlement = 4,
    region = 5,
    figure = 6,
    faction = 7,
    monster = 8,
    site = 9,
};

// The name of a code: "race", "god", "civilization", "settlement",
// "region", "figure", "faction", "monster", "site".
[[nodiscard]] std::string_view entity_type_key(EntityType type) noexcept;

// The type of code `code`, or std::nullopt when it is not 1 to 9.
[[nodiscard]] std::optional<EntityType> entity_type_from_code(std::int64_t code) noexcept;

// One role of a kind (P3-04 8.2): seq 1, 2, ...; max_count 0 = unbounded.
struct RoleDef {
    std::int64_t seq = 0;
    std::string role;
    EntityType type = EntityType::race;
    std::int64_t min_count = 0;
    std::int64_t max_count = 0;

    friend bool operator==(const RoleDef&, const RoleDef&) = default;
};

// One kind of event (P3-04 8.2). region_use and settlement_use: 0 never,
// 1 always, 2 optional. values are the value keys in seq order (seq =
// index + 1). salience and templ come from history.report (0 and empty in
// the compiled catalogue).
struct KindDef {
    std::int64_t id = 0;
    std::string key;
    bool caused = false;
    std::int64_t region_use = 0;
    std::int64_t settlement_use = 0;
    std::vector<RoleDef> roles;
    std::vector<std::string> values;
    std::int64_t salience = 0;
    std::string templ;

    // The role of that key, or nullptr.
    [[nodiscard]] const RoleDef* find_role(std::string_view role) const noexcept;
    // The seq (1-based) of value key `key`, or 0 when the kind has none.
    [[nodiscard]] std::int64_t value_seq(std::string_view key) const noexcept;

    friend bool operator==(const KindDef&, const KindDef&) = default;
};

// The kinds in id order, ids 1, 2, ... (index id - 1).
struct EventCatalogue {
    std::vector<KindDef> kinds;

    // The kind of that key, or nullptr.
    [[nodiscard]] const KindDef* find(std::string_view key) const noexcept;
    // The kind of that id, or nullptr.
    [[nodiscard]] const KindDef* find_id(std::int64_t id) const noexcept;

    friend bool operator==(const EventCatalogue&, const EventCatalogue&) = default;
};

// The compiled catalogue of P3-04 8.2 (structure only: salience 0, empty
// templates). Its role and value keys are checked once, at first use
// (distinct within a kind, [a-z][a-z0-9_]*).
[[nodiscard]] const EventCatalogue& compiled_event_catalogue();

// The problems of a history.report definition's record (named check
// report_events, P3-04 4.2), every message in the order of 4.2. Empty when
// the record passes.
[[nodiscard]] std::vector<std::string> report_events_problems(const data::Record& report);

// The compiled catalogue with the base salience and template of each kind
// from the content's history.report (what the stage writes). FatalError
// "history: report: the content has no definition history.report" when it
// is absent, and "history: report: <message>" with the first message of
// report_events_problems when the record fails it.
[[nodiscard]] EventCatalogue event_catalogue(const data::ContentSet& content);

// The catalogue a world stores (tables event_kind, event_kind_role,
// event_kind_value). FatalError "read_event_catalogue: <reason>" when a
// table cannot be read or its rows do not form a catalogue (ids not 1, 2,
// ...; a role or value row naming no kind; seqs not 1, 2, ... per kind; an
// entity type code outside 1 to 9).
[[nodiscard]] EventCatalogue read_event_catalogue(const db::WorldReader& world);

// The rows of the three catalogue tables, in their write order.
struct CatalogueRows {
    std::vector<db::EventKindRow> kinds;
    std::vector<db::EventKindRoleRow> roles;
    std::vector<db::EventKindValueRow> values;
};
[[nodiscard]] CatalogueRows catalogue_rows(const EventCatalogue& catalogue);

// The first problem of `text` as a template of kind `kind` (P3-04 8.4),
// scanning left to right, or std::nullopt when it is a valid template.
[[nodiscard]] std::optional<std::string> template_problem(const KindDef& kind, std::string_view text);

// The history tables of a world in memory, read once (P3-04 8.5).
class HistoryView {
public:
    // One event with its participants (seq order), values (seq order) and
    // causes (ascending).
    struct Event {
        db::EventRow row;
        std::vector<db::EventParticipantRow> participants;
        std::vector<std::int64_t> values;
        std::vector<std::int64_t> causes;
    };

    // Reads the stored catalogue, the log and the names of races, gods,
    // civilizations, settlements and regions. Works from the stored tables
    // only, so kinds this build does not compile are accepted. Never
    // throws for a bad file (P3-04 8.5 r1): an error, code corrupt_layer
    // with the message "corrupt table '<table>': <detail>" (or the reader's
    // own error when a table cannot be read), the catalogue rows do not form
    // a catalogue, a stored template has a problem (template_problem), or a
    // row refers to something that does not exist (an event kind, an event,
    // an entity, a role or value seq of the kind) or lacks a value of its
    // kind. A view that loaded renders every event without a FatalError.
    static db::Result<HistoryView> load(const db::WorldReader& world);

    [[nodiscard]] const EventCatalogue& catalogue() const noexcept { return catalogue_; }
    // Events in id order (index id - 1).
    [[nodiscard]] std::span<const Event> events() const noexcept { return events_; }
    // The event of that id, or nullptr.
    [[nodiscard]] const Event* event(std::int64_t id) const noexcept;
    // The display name of an entity (P3-04 8.3), or nullptr when it does not
    // exist (or its type has no names: 6 to 9).
    [[nodiscard]] const std::string* name(EntityType type, std::int64_t id) const noexcept;
    // The (id, name) pairs of each entity table, ascending id.
    [[nodiscard]] std::span<const std::pair<std::int64_t, std::string>> gods() const noexcept { return gods_; }
    [[nodiscard]] std::span<const std::pair<std::int64_t, std::string>> civilizations() const noexcept {
        return civilizations_;
    }
    [[nodiscard]] std::span<const std::pair<std::int64_t, std::string>> settlements() const noexcept {
        return settlements_;
    }
    [[nodiscard]] std::span<const std::pair<std::int64_t, std::string>> regions() const noexcept { return regions_; }
    // The race ids, ascending.
    [[nodiscard]] std::vector<std::int64_t> race_ids() const;
    // race.plural and race.adjective, or nullptr.
    [[nodiscard]] const std::string* race_plural(std::int64_t id) const noexcept;
    [[nodiscard]] const std::string* race_adjective(std::int64_t id) const noexcept;
    // race.generation_years, or 0 when the race does not exist (P3-07 r12
    // 22.2: the generations note).
    [[nodiscard]] std::int64_t race_generation_years(std::int64_t id) const noexcept;
    // civilization.race and civilization.founded_year, or std::nullopt when
    // the civilization does not exist.
    struct CivilizationInfo {
        std::int64_t race = 0;
        std::int64_t founded_year = 0;
    };
    [[nodiscard]] std::optional<CivilizationInfo> civilization_info(std::int64_t id) const noexcept;

    // P3-04 r21 23.1 (schema 8): the rows of table region_name, in primary-key
    // order; N(r, e), the name of row of r with the largest event <= e, else
    // region.name; and the name at year Y, the row with the largest
    // from_year <= Y, else region.name. nullptr when r is not a region.
    [[nodiscard]] std::span<const db::RegionNameRow> region_names() const noexcept { return region_names_; }
    [[nodiscard]] const std::string* region_name_at_event(std::int64_t region, std::int64_t event) const noexcept;
    [[nodiscard]] const std::string* region_name_at_year(std::int64_t region, std::int64_t year) const noexcept;

private:
    struct RaceNames {
        std::string name;
        std::string plural;
        std::string adjective;
        std::int64_t generation_years = 0;
    };
    EventCatalogue catalogue_;
    std::vector<Event> events_;
    std::vector<std::pair<std::int64_t, RaceNames>> races_;  // ascending id
    std::vector<std::pair<std::int64_t, std::string>> gods_;
    std::vector<std::pair<std::int64_t, std::string>> civilizations_;
    std::vector<std::pair<std::int64_t, CivilizationInfo>> civilization_info_;  // ascending id
    std::vector<std::pair<std::int64_t, std::string>> settlements_;
    std::vector<std::pair<std::int64_t, std::string>> regions_;
    std::vector<db::RegionNameRow> region_names_;  // primary-key order
    // Per region (index id - 1 of regions_ when ids are 1, 2, ...): the
    // range [first, last) of its rows in region_names_.
    std::vector<std::pair<std::size_t, std::size_t>> region_name_range_;
};

// P3-07 r21 26.6: the first problem of the region_name rows (primary-key
// order), as "corrupt table 'region_name': row (<region>, <from_year>):
// <reason>" or "corrupt table 'event': row <e>: region_renamed without a
// region_name row", or std::nullopt. region_ids and culture_ids ascending;
// events in id order with ids 1, 2, ...; renamed_kind the id of the stored
// kind region_renamed (0 when the catalogue has none); end_year E.
[[nodiscard]] std::optional<std::string> region_name_rows_problem(std::span<const db::RegionNameRow> rows,
                                                                  std::span<const std::int64_t> region_ids,
                                                                  std::span<const std::int64_t> culture_ids,
                                                                  std::int64_t end_year,
                                                                  std::span<const db::EventRow> events,
                                                                  std::int64_t renamed_kind);

// The sentence of event `event_id` (P3-04 8.5) from its kind's stored
// template. FatalError "render_event: event <id>: <reason>" when the id is
// not in the log or the stored template has a problem.
[[nodiscard]] std::string render_event(const HistoryView& view, std::int64_t event_id);

}  // namespace worldgen::history
