#pragma once

// The renaming subsystem of spec P5-12 (docs/specs/p5-12-renamed-regions.md):
// history gives a region its present-day name from its defining event. The
// rules are data (history.renaming, section 4); the subsystem runs in steps
// T6 and Q4 after conflict, reads the event log through StepContext, keeps
// its own members and appends kind 43 region_renamed (section 6). It changes
// no State. Keyed draws only (streams hist.renaming.chance and
// hist.renaming.name); integer arithmetic only; its members are vectors in
// a fixed order.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "worldgen/data/content.hpp"
#include "worldgen/db/writer.hpp"
#include "worldgen/history/engine.hpp"
#include "worldgen/history/event_catalogue.hpp"
#include "worldgen/history/names.hpp"
#include "worldgen/history/peoples.hpp"
#include "worldgen/stages/regions.hpp"

namespace worldgen::history {

// Record rename_rule (P5-12 4), fields in declaration order.
struct RenameRule {
    std::string key;
    std::vector<std::string> kinds;
    std::vector<std::string> values;
    std::int64_t min_magnitude = 1;
    std::int64_t window_years = 1;
    std::int64_t min_salience = 0;
    bool capital = false;
    std::int64_t chance_permille = 0;
    std::int64_t weight = 1;
    std::string name_role;
    std::string namer_role;
    std::vector<std::string> forms;

    friend bool operator==(const RenameRule&, const RenameRule&) = default;
};

// Definition history.renaming (P5-12 4).
struct RenamingTuning {
    std::int64_t region_max = 0;
    std::int64_t region_gap_years = 1;
    std::int64_t world_max_permille = 0;
    std::int64_t century_max_permille = 0;
    std::int64_t century_years = 1;
    std::vector<RenameRule> rules;  // rule number = index + 1

    friend bool operator==(const RenamingTuning&, const RenamingTuning&) = default;
};

// The tuning of loaded content. FatalError "history: tuning: the content has
// no definition history.renaming" when it is absent.
[[nodiscard]] RenamingTuning renaming_tuning_from(const data::ContentSet& content);

// The stage check renaming_rules (P3-01 r16 5.4): rules in order, for each
// rule its kinds in order and for each kind the problems in the order
// listed there; the first as "history: renaming rule '<key>': <problem>",
// or std::nullopt.
[[nodiscard]] std::optional<std::string> renaming_rules_problem(const RenamingTuning& tuning,
                                                                const EventCatalogue& catalogue);
// renaming_rules_problem as a FatalError.
void check_renaming_rules(const RenamingTuning& tuning, const EventCatalogue& catalogue);

// P5-12 5.3: the root of civilization name `name` for the culture's
// civilization_forms: the first form A{root}B (list order) such that the
// name starts with A, ends with B and is longer than |A| + |B|; the name
// without A and B. The whole name when no form matches.
[[nodiscard]] std::string realm_root(std::string_view name, const std::vector<std::string>& civilization_forms);

// P5-12 5.1: a form with {name} and {noun} replaced, and the byte offsets of
// the joins between its parts (the literal runs of the form and the two
// slot values, in order).
struct RenamedName {
    std::string name;
    std::vector<std::size_t> joins;
};
[[nodiscard]] RenamedName fill_rename_form(std::string_view form, std::string_view name_value,
                                           std::string_view noun_value);
// P5-02 21.2: the same with {stem} replaced by stem_value, and the
// possessive rule: after a slot value ending in s or S, a literal run
// starting with 's loses its s (the 3-argument form uses stem = name).
[[nodiscard]] RenamedName fill_rename_form(std::string_view form, std::string_view name_value,
                                           std::string_view noun_value, std::string_view stem_value);
// The general fill: each slot text (for example "{root}") replaced by its
// value, the joins between the parts, and the possessive rule of 21.2.
struct SlotValue {
    std::string_view slot;
    std::string_view value;
};
[[nodiscard]] RenamedName fill_slots(std::string_view form, std::span<const SlotValue> slots);
// P5-02 21.2: a settlement's name without a trailing separate-word suffix
// of its founder's culture: the first form {root}B of settlement_forms
// (list order) whose B starts with a space, such that the name ends with B
// and is longer than |B|; the whole name when none.
[[nodiscard]] std::string settlement_stem(std::string_view name, const std::vector<std::string>& settlement_forms);

// P5-02 9.3: what the renaming subsystem reads of the monsters (the root of
// a monster's name, 9.2). Implemented by MonstersSubsystem.
class MonsterNames {
public:
    virtual ~MonsterNames() = default;
    [[nodiscard]] virtual std::string monster_root(std::int64_t monster) const = 0;
};

// max(1, regions x permille div 1000) (P5-12 6 items 3 and 4), in int64.
[[nodiscard]] std::int64_t renaming_cap(std::int64_t regions, std::int64_t permille) noexcept;

// Counts for the validation of P5-12 11 (R4): candidates in total, those
// that reached item 2.6 (passed items 2.1 to 2.5), those dropped there for
// want of a name, and the renamings written.
struct RenamingStats {
    std::int64_t candidates = 0;
    std::int64_t reached_name = 0;
    std::int64_t no_name = 0;
    std::int64_t renamed = 0;
};

class RenamingSubsystem final : public Subsystem {
public:
    // The shipped configuration: the tuning of `content` and the stage's
    // catalogue (which the subsystem reads the roles and value keys of).
    RenamingSubsystem(PeoplesSubsystem& peoples, const data::ContentSet& content, const EventCatalogue& catalogue);
    // A test seam: hand-made tuning.
    RenamingSubsystem(PeoplesSubsystem& peoples, RenamingTuning tuning, const EventCatalogue& catalogue);
    RenamingSubsystem(const RenamingSubsystem&) = delete;
    RenamingSubsystem& operator=(const RenamingSubsystem&) = delete;

    [[nodiscard]] std::span<const Step> steps() const override;
    void run(StepContext& context) override;

    // The region_name rows in creation order (ascending year), and sorted by
    // (region, from_year) for the writer (P3-04 r21 23.4).
    [[nodiscard]] std::span<const db::RegionNameRow> rows() const noexcept { return rows_; }
    [[nodiscard]] std::vector<db::RegionNameRow> sorted_rows() const;
    [[nodiscard]] const RenamingStats& stats() const noexcept { return stats_; }
    [[nodiscard]] const RenamingTuning& tuning() const noexcept { return tuning_; }
    // P5-02 9.3: the monsters whose roots a monster name_role reads (the
    // history stage sets it; without it such a role gives no {name}).
    void set_monsters(const MonsterNames* monsters) noexcept { monsters_ = monsters; }

private:
    struct WindowEntry {
        std::int64_t year = 0;
        std::int64_t magnitude = 0;
    };
    struct Candidate {
        std::size_t rule = 0;  // index into tuning_.rules
        std::int64_t event = 0;
    };
    struct Renaming {
        std::int64_t year = 0;
        std::int64_t weight = 0;
    };

    void prepare(const StepContext& context);
    // 6 item 1 for one event.
    void read_event(const StepContext& context, std::int64_t event, std::vector<std::vector<Candidate>>& candidates);
    // The first civilization participant of an event, 0 when none.
    [[nodiscard]] std::int64_t first_civilization(const StepContext& context, std::int64_t event) const;
    // 5.1: the name, or std::nullopt.
    [[nodiscard]] std::optional<std::string> build_name(const StepContext& context, const RenameRule& rule,
                                                        std::int64_t event, std::int64_t region) const;
    // 6 item 2 for the chosen candidate of a region.
    void choose(StepContext& context, std::int64_t region, const Candidate& c);
    [[nodiscard]] const std::vector<std::string>& civilization_forms(std::int64_t culture) const;
    [[nodiscard]] const std::vector<std::string>& settlement_forms(std::int64_t culture) const;

    PeoplesSubsystem& peoples_;
    RenamingTuning tuning_;
    const EventCatalogue& catalogue_;
    bool prepared_ = false;
    std::uint64_t chance_key_ = 0;
    std::uint64_t name_key_ = 0;
    std::int64_t region_count_ = 0;
    std::int64_t cursor_ = 1;
    std::vector<std::string> nouns_;                          // per region (index id - 1)
    stages::PlacenameData placenames_;                        // the lists of 5.2 item 3 and the filter
    std::vector<std::pair<std::int64_t, std::vector<std::string>>> culture_forms_;  // (culture id, forms), ascending
    std::vector<std::pair<std::int64_t, std::vector<std::string>>> settlement_forms_;  // P5-02 21.2, ascending
    const MonsterNames* monsters_ = nullptr;
    std::vector<std::vector<std::vector<WindowEntry>>> windows_;  // [rule][region - 1]
    std::vector<std::vector<Renaming>> renamings_;               // per region (index id - 1)
    std::vector<std::int64_t> renaming_years_;                   // every renaming of the world, ascending
    std::vector<db::RegionNameRow> rows_;                        // creation order
    RenamingStats stats_;
};

}  // namespace worldgen::history
