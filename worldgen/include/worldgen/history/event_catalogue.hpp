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
    // exist (or its type has no names: 6 and 7; P5-02 10.3: monster.name and
    // site.name for 8 and 9).
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

    // P5-02 10.3 (schema 9): monster_kind.noun of the monster's kind, and
    // beast.plural; nullptr when the row does not exist. The rows of the
    // tables beast, monster_kind, monster and site as read (primary-key
    // order).
    [[nodiscard]] const std::string* monster_noun(std::int64_t monster) const noexcept;
    [[nodiscard]] const std::string* beast_plural(std::int64_t beast) const noexcept;
    // P5-03a 7.1: the monster_kind row of the monster's kind, or nullptr.
    [[nodiscard]] const db::MonsterKindRow* monster_kind_row(std::int64_t monster) const noexcept;
    // P5-03 9.5, 13: the raid phrase of a monster's kind and the bold phrase
    // of a beast for event `event` (phrase number (event - 1) mod n of the
    // stored list); nullptr when the row does not exist.
    [[nodiscard]] const std::string* monster_raid_phrase(std::int64_t monster, std::int64_t event) const noexcept;
    [[nodiscard]] const std::string* beast_bold_phrase(std::int64_t beast, std::int64_t event) const noexcept;
    [[nodiscard]] std::span<const db::BeastRow> beast_rows() const noexcept { return beast_rows_; }
    [[nodiscard]] std::span<const db::MonsterKindRow> monster_kind_rows() const noexcept { return monster_kind_rows_; }
    [[nodiscard]] std::span<const db::MonsterRow> monster_rows() const noexcept { return monster_rows_; }
    [[nodiscard]] std::span<const db::SiteRow> site_rows() const noexcept { return site_rows_; }

    // P5-06 7.9 (schema 12): the rows of tables figure and book as read; the
    // recorded ruler of civilization `civ` at event `event` (the figure a of
    // the ruled row (a, civ) with since_event <= event and (until_event 0 or
    // event <= until_event); the first such row by since_event), 0 when none;
    // the title of the book row whose event is `event`, nullptr when none.
    [[nodiscard]] std::span<const db::FigureRow> figure_rows() const noexcept { return figure_rows_; }
    [[nodiscard]] std::span<const db::BookRow> book_rows() const noexcept { return book_rows_; }
    [[nodiscard]] std::int64_t ruler_at_event(std::int64_t civ, std::int64_t event) const noexcept;
    [[nodiscard]] const std::string* book_title_of_event(std::int64_t event) const noexcept;

    // P5-08 9 (schema 14): the god rows and faction rows as read; a god's
    // title (nullptr when the god does not exist).
    [[nodiscard]] std::span<const db::GodRow> god_rows() const noexcept { return god_rows_; }
    [[nodiscard]] std::span<const db::FactionRow> faction_rows() const noexcept { return faction_rows_; }
    [[nodiscard]] const std::string* god_title(std::int64_t id) const noexcept;

private:
    struct MonsterNames {
        std::string name;
        std::int64_t kind = 0;
    };
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
    // P5-02 (schema 9).
    std::vector<db::BeastRow> beast_rows_;
    std::vector<db::MonsterKindRow> monster_kind_rows_;
    std::vector<db::MonsterRow> monster_rows_;
    std::vector<db::SiteRow> site_rows_;
    std::vector<std::pair<std::int64_t, std::string>> beast_plurals_;       // ascending id
    std::vector<std::pair<std::int64_t, std::string>> monster_kind_nouns_;  // ascending id
    std::vector<std::pair<std::int64_t, MonsterNames>> monsters_;           // ascending id
    std::vector<std::pair<std::int64_t, std::string>> sites_;               // ascending id
    // P5-03 (schema 10): the phrase lists, ascending id.
    std::vector<std::pair<std::int64_t, std::vector<std::string>>> beast_phrases_;
    std::vector<std::pair<std::int64_t, std::vector<std::string>>> monster_kind_phrases_;
    // P5-06 (schema 12): the figure and book rows, the display names
    // (ascending id), the ruled rows as (civ, since_event, until_event,
    // figure) sorted, and (event, title) ascending event.
    std::vector<db::FigureRow> figure_rows_;
    std::vector<db::BookRow> book_rows_;
    std::vector<std::pair<std::int64_t, std::string>> figures_;
    struct Reign {
        std::int64_t civ = 0, since = 0, until = 0, figure = 0;
    };
    std::vector<Reign> reigns_;
    std::vector<std::pair<std::int64_t, std::string>> book_titles_;
    // P5-08 (schema 14).
    std::vector<db::GodRow> god_rows_;
    std::vector<std::pair<std::int64_t, std::string>> god_titles_;  // ascending id
    std::vector<db::FactionRow> faction_rows_;
    std::vector<std::pair<std::int64_t, std::string>> factions_;     // ascending id
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

// P5-02 11.4 (schema 9): the first problem of the tables beast,
// monster_kind, beast_population, site and monster, of settlement.ending,
// and of the events of kinds 44 to 55, in the order of 11.4, as
// "corrupt table '<table>': row <key>: <reason>", or std::nullopt. Every
// row span is in primary-key order as read; region_ids and civ_ids
// ascending; events in id order (ids are checked to be 1, 2, ... before);
// participants, values and causes in primary-key order; end_year E and the
// map side. Linear in the rows (bounds-checked index lookups).
struct MonsterTables {
    std::span<const db::BeastRow> beasts;
    std::span<const db::MonsterKindRow> monster_kinds;
    std::span<const db::BeastPopulationRow> beast_populations;
    std::span<const db::MonsterRow> monsters;
    std::span<const db::SiteRow> sites;
    std::span<const db::SettlementRow> settlements;
    std::span<const std::int64_t> region_ids;
    std::span<const std::int64_t> civ_ids;
    std::int64_t end_year = 0;
    std::int64_t map_size = 0;
    std::span<const db::EventRow> events;
    std::span<const db::EventParticipantRow> participants;
    std::span<const db::EventValueRow> values;
    std::span<const db::EventCauseRow> causes;
    const EventCatalogue* catalogue = nullptr;
    // P5-03 16.4 (schema 10): checked after the tables of P5-02, except the
    // race participants, checked first with HistoryView::load's message;
    // race_ids ascending.
    std::span<const db::RaceRelationRow> race_relations;
    std::span<const db::SettlementRaceRow> settlement_races;
    std::span<const std::int64_t> race_ids;
    // P5-04 9.1 (schema 11): the relation rows and, per civilization of
    // civ_ids (same order), its ending event (0 none).
    std::span<const db::RelationRow> relations;
    std::span<const std::int64_t> civ_endings;
};
[[nodiscard]] std::optional<std::string> monster_tables_problem(const MonsterTables& t);

// P5-06 9.4 (schema 12): the first problem of the tables figure and book, of
// the relation rows of the kinds ruled, served, rival and parent, and of the
// events of kinds 79 to 92, in the order of 9.4, or std::nullopt. Called by
// both readers after their monster checks, so both give the same first
// error. Spans as for MonsterTables; races in primary-key order (their
// awakening years), culture_ids ascending.
struct FigureTables {
    std::span<const db::FigureRow> figures;
    std::span<const db::BookRow> books;
    std::span<const db::RelationRow> relations;
    std::span<const db::RaceRow> races;
    std::span<const std::int64_t> culture_ids;
    std::span<const std::int64_t> civ_ids;
    std::span<const std::int64_t> civ_endings;  // per civ_ids entry, 0 none
    std::span<const std::int64_t> region_ids;
    std::span<const db::SettlementRow> settlements;
    std::span<const db::SiteRow> sites;
    std::span<const db::EventRow> events;
    std::span<const db::EventParticipantRow> participants;
    std::span<const db::EventValueRow> values;
    const EventCatalogue* catalogue = nullptr;
    std::int64_t end_year = 0;  // P5-04a 14.4: E, for hero_rose's quiet
};
[[nodiscard]] std::optional<std::string> figure_tables_problem(const FigureTables& t);

// P5-06 9.4: the role words ruler, claimant, general, hero, caster (roles 1
// to 5), "?" otherwise; the temper words steady, warlike, peaceable,
// grasping, just (0 to 4).
[[nodiscard]] std::string_view figure_role_word(std::int64_t role) noexcept;
[[nodiscard]] std::string_view figure_temper_word(std::int64_t temper) noexcept;

// P5-06 6.10, 6.14: the display name of a figure row: name, " " + the roman
// numeral of regnal when it is 2 or more, " " + the epithet when not empty.
[[nodiscard]] std::string figure_display_name(const db::FigureRow& row);

// P5-02 8.3: the ending code of a settlement whose ruin event is `ruin` (0
// when unknown: 5), given the kind key of an event and its causes.
[[nodiscard]] std::int64_t settlement_ending_code(std::string_view ruin_kind,
                                                  std::span<const std::string_view> cause_kinds) noexcept;

// The sentence of event `event_id` (P3-04 8.5) from its kind's stored
// template. FatalError "render_event: event <id>: <reason>" when the id is
// not in the log or the stored template has a problem. render_event_linked's
// text: one code path.
[[nodiscard]] std::string render_event(const HistoryView& view, std::int64_t event_id);

// A link over part of a text (P4-05 4.1): byte offsets into the UTF-8 text,
// type 0 for an event or an EntityType code, the id; tagged only for the
// references of a block line (4.2), always false in a sentence.
struct TextLink {
    std::uint32_t start = 0, length = 0;
    std::int64_t type = 0;
    std::int64_t id = 0;
    bool tagged = false;
    friend bool operator==(const TextLink&, const TextLink&) = default;
};
struct LinkedText {
    std::string text;
    std::vector<TextLink> links;  // ascending start, never overlapping
    friend bool operator==(const LinkedText&, const LinkedText&) = default;
};

// P4-05 4.1: the sentence of render_event with a link over every name the
// renderer appends: a role in form name or all, each participant's name as
// written (a lower-cased "the " included) with its type and id; forms plural
// and adjective, the race word (type 1); form kind, the monster noun (type
// 8); {@region} and {@region_former}, the region name (type 5);
// {@settlement} (type 4). Values, years, choices, beasts and cause words have
// none. FatalErrors those of render_event.
[[nodiscard]] LinkedText render_event_linked(const HistoryView& view, std::int64_t event_id);

}  // namespace worldgen::history
