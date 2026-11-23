#pragma once

// The simulation engine of the history (spec P3-04 section 6): the
// in-memory state, the step seam that P3-05 (peoples), P3-06 (conflict) and
// P5 plug into, the checked event writer, quiet eras, the bookkeeping by
// year, and divine interventions.
//
// Single-threaded and deterministic: steps run in a fixed order, entities
// by ascending id, subsystems in configuration order. The engine itself
// makes no random decision; subsystems draw with keyed draws (H4) through
// StepContext::draw.

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "worldgen/data/content.hpp"
#include "worldgen/db/writer.hpp"
#include "worldgen/history/event_catalogue.hpp"
#include "worldgen/history/inputs.hpp"
#include "worldgen/history/schedule.hpp"

namespace worldgen::history {

// --- State (6.1) -----------------------------------------------------------------

// Settlement kinds in P3-01's race enum order, and statuses (P3-04 9.2).
enum class SettlementKind : std::int64_t { hamlet = 1, village = 2, town = 3, city = 4, fortress = 5 };
enum class SettlementStatus : std::int64_t { thriving = 1, declining = 2, ruined = 3, faded = 4 };

// The bounds of P3-04 6.1 (H6).
inline constexpr std::int64_t MAX_CIVILIZATIONS = 4096;
inline constexpr std::int64_t MAX_SETTLEMENTS = 1000000;
inline constexpr std::int64_t MAX_EVENTS = 10000000;
inline constexpr std::size_t MAX_PARTICIPANTS = 64;
inline constexpr std::size_t MAX_CAUSES = 64;
inline constexpr std::int64_t MAX_POPULATION = 2147483647;  // 2^31 - 1

// A civilization (index id - 1; ids 1, 2, ... in creation order, H2).
struct Civilization {
    std::int64_t id = 0;
    std::int64_t race = 0;
    std::int64_t culture = 0;
    std::int64_t home_region = 0;
    std::int64_t capital = 0;  // 0 none
    std::int64_t founded_year = 0;
    std::int64_t founding_event = 0;
    std::optional<std::int64_t> ended_year;
    std::int64_t ending_event = 0;
    std::string name;
    std::uint64_t created_step = 0;
};

// A settlement (index id - 1).
struct Settlement {
    std::int64_t id = 0;
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t region = 0;
    std::int64_t founder_civ = 0;
    std::int64_t founded_year = 0;
    std::int64_t founding_event = 0;
    std::int64_t owner_civ = 0;
    std::int64_t population = 0;
    SettlementKind kind = SettlementKind::hamlet;
    SettlementStatus status = SettlementStatus::thriving;
    std::optional<std::int64_t> ruined_year;
    std::int64_t ruin_event = 0;
    std::string name;
    std::uint64_t created_step = 0;
};

// The owner of a region (index region id - 1): 0 free.
struct RegionState {
    std::int64_t owner = 0;
    std::int64_t owner_event = 0;
    // P3-06 r14 21.8 (generator 0.19.0): the holding civilization, hold(G):
    // the owner, or while there is none the civilization that last owned the
    // region (0 if it was never owned). Not stored in the world.
    std::int64_t holder = 0;
    // The number of times a different civilization became the holder
    // (P3-06 21.8, the lead's ruling A: a change of holder clears the
    // weariness counts over the region). Not stored in the world.
    std::int64_t holder_changes = 0;
};

// A relation between two entities (6.1): a symmetric relation is stored with
// (a_type, a_id) < (b_type, b_id), an asymmetric one with the subject as a.
// Ordered by the integers, then the kind's bytes.
struct RelationKey {
    std::int64_t a_type = 0;
    std::int64_t a_id = 0;
    std::int64_t b_type = 0;
    std::int64_t b_id = 0;
    std::string kind;

    auto operator<=>(const RelationKey&) const = default;
    bool operator==(const RelationKey&) const = default;
};

// An open relation: the event that opened it.
struct RelationState {
    std::int64_t event = 0;
};

// The read-only geography the subsystems see, built once from the inputs.
class GeoView {
public:
    struct Neighbour {
        std::int64_t region = 0;     // ascending
        std::size_t border = 0;      // index into HistoryInputs::region_borders
    };
    struct Region {
        std::int64_t id = 0;
        std::int64_t dry_cells = 0;  // cells - lake_cells
        bool islet = false;          // dry_cells < islet_max_dry_cells
        std::vector<Neighbour> neighbours;
    };

    // FatalError "history: input precondition: region ids are not 1 to <n>"
    // when the region rows are not ids 1, 2, ... in order.
    GeoView(const HistoryInputs& inputs, std::int64_t islet_max_dry_cells);

    [[nodiscard]] std::span<const Region> regions() const noexcept { return regions_; }
    // FatalError if id is not a region id.
    [[nodiscard]] const Region& region(std::int64_t id) const;
    [[nodiscard]] bool has_region(std::int64_t id) const noexcept;

private:
    std::vector<Region> regions_;
};

class Engine;  // private (engine.cpp)

// The only mutable world state; subsystems change it through these calls
// (6.1). Every setter is fatal ("history: state: <what>") when it names a
// missing entity or event, an ended civilization as owner, or a value out
// of bounds ("history: bound: <what> exceeds <n>").
class State {
public:
    [[nodiscard]] const Civilization& civ(std::int64_t id) const;
    [[nodiscard]] std::span<const Civilization> civs() const noexcept { return civs_; }
    [[nodiscard]] const Settlement& settlement(std::int64_t id) const;
    [[nodiscard]] std::span<const Settlement> settlements() const noexcept { return settlements_; }
    [[nodiscard]] const RegionState& region(std::int64_t id) const;
    [[nodiscard]] const std::map<RelationKey, RelationState>& relations() const noexcept { return relations_; }

    // A new civilization founded now (the current year and step); returns
    // its id (the number of civilizations + 1).
    std::int64_t found_civilization(std::int64_t race, std::int64_t culture, std::int64_t home_region,
                                    std::string name, std::int64_t event);
    // A new settlement of founder_civ, owned by it, status thriving; the
    // cell must lie in the map and, when the region layer covers the map, in
    // `region`. Returns its id.
    std::int64_t found_settlement(std::int64_t x, std::int64_t y, std::int64_t region, std::int64_t founder_civ,
                                  SettlementKind kind, std::int64_t population, std::string name,
                                  std::int64_t event);
    void set_region_owner(std::int64_t region, std::int64_t civ /*0 frees*/, std::int64_t event);
    void set_settlement_owner(std::int64_t id, std::int64_t civ /*0 abandoned*/, std::int64_t event);
    void set_settlement_kind(std::int64_t id, SettlementKind kind, std::int64_t event);
    // Ruined sets ruined_year (now) and ruin_event; any other status clears them.
    void set_settlement_status(std::int64_t id, SettlementStatus status, std::int64_t event);
    void set_population(std::int64_t id, std::int64_t population);
    void set_capital(std::int64_t civ, std::int64_t settlement /*0 none*/);
    void end_civilization(std::int64_t civ, std::int64_t event);
    void open_relation(RelationKey key, std::int64_t event);
    void close_relation(const RelationKey& key, std::int64_t event);

private:
    friend class Engine;
    explicit State(const Engine& engine);

    void require_event(std::int64_t event, const char* what) const;
    void require_live_civ(std::int64_t civ, const char* what) const;
    Civilization& civ_mut(std::int64_t id, const char* what);
    Settlement& settlement_mut(std::int64_t id, const char* what);

    const Engine& engine_;
    std::vector<Civilization> civs_;
    std::vector<Settlement> settlements_;
    std::vector<RegionState> regions_;
    std::map<RelationKey, RelationState> relations_;
    // Every open_relation and close_relation since the last bookkeeping, in
    // call order (6.7: each opening gets a row, also one closed again before
    // the bookkeeping).
    struct RelationOp {
        bool open = false;
        RelationKey key;
        std::int64_t event = 0;
    };
    std::vector<RelationOp> relation_ops_;
};

// --- The step seam (6.4) -------------------------------------------------------------

// The steps of P3-04 6.2: T1 to T7 of a tick year, Q1 to Q6 of a quiet step,
// and the engine's own steps of 6.3 (the awakening, the flare-up start),
// which StepContext::step() returns to the awakening handler. Subsystems
// list only T1 to T4, T6, Q1, Q3 and Q4.
enum class Step : std::uint8_t { t1 = 1, t2, t3, t4, t5, t6, t7, q1, q2, q3, q4, q5, q6, awakening, flare };

struct Participant {
    std::string_view role;
    EntityType type = EntityType::race;
    std::int64_t id = 0;
};

struct EventSpec {
    std::string_view kind;                       // a key of the catalogue
    std::int64_t year = 0, end_year = 0;         // 6.6 rules; end_year = year for a point event
    std::int64_t region = 0, settlement = 0;
    std::vector<Participant> participants;       // ascending role seq, then the emitter's order
    std::vector<std::int64_t> values;            // one per value key of the kind, in seq order
    std::vector<std::int64_t> causes;            // strictly ascending
    std::optional<std::int64_t> salience;        // std::nullopt: the kind's base salience
};

// What a subsystem sees in one step. Valid only during the call it is given to.
class StepContext {
public:
    [[nodiscard]] Step step() const noexcept { return step_; }
    [[nodiscard]] std::uint64_t serial() const noexcept;
    // The tick year, or the quiet step's start.
    [[nodiscard]] std::int64_t year() const noexcept;
    // 1, or the quiet step's length.
    [[nodiscard]] std::int64_t years() const noexcept;
    [[nodiscard]] std::int64_t phase_id() const noexcept;
    [[nodiscard]] PhaseKind phase_kind() const noexcept;
    // P5-02 7.4: the current phase row's trigger_event, 0 when none.
    [[nodiscard]] std::int64_t phase_trigger_event() const noexcept;
    [[nodiscard]] const HistoryInputs& inputs() const noexcept;
    [[nodiscard]] const GeoView& geo() const noexcept;
    [[nodiscard]] const data::ContentSet& content() const noexcept;
    [[nodiscard]] const State& state() const noexcept;
    [[nodiscard]] State& state() noexcept;  // mutations through the State API only
    // created_step < serial(): entities created in this step are not seen.
    [[nodiscard]] bool visible(const Civilization& c) const noexcept;
    [[nodiscard]] bool visible(const Settlement& s) const noexcept;
    // The event writer (6.6); returns the new id.
    std::int64_t append(const EventSpec& spec);
    // quiet_events_max minus the events charged to civ in this quiet step;
    // quiet_events_max outside quiet steps.
    [[nodiscard]] std::int64_t quiet_events_left(std::int64_t civ) const;
    // draw(stream_key, year(), a, b, k) (P3-02 4.1).
    [[nodiscard]] std::uint64_t draw(std::uint64_t stream_key, std::uint64_t a, std::uint64_t b,
                                     std::uint64_t k) const noexcept;
    // Read-only access to the log (P3-06 8.1 item 2: the parent of a
    // secession is the `from` of its civ_seceded). The key of event `event`'s
    // kind; the entity id of its first participant in role `role`, 0 when it
    // has none. FatalError "history: state: event <e> does not exist".
    [[nodiscard]] std::string_view event_kind(std::int64_t event) const;
    [[nodiscard]] std::int64_t event_participant(std::int64_t event, std::string_view role) const;
    // The `year` of event `event` (P3-05 r12 22.2: the cause memory).
    [[nodiscard]] std::int64_t event_year(std::int64_t event) const;
    // P3-04 r21 23.3 (P5-12): pure reads of the events appended so far. The
    // id of the last event (0 when none); an event's region, settlement (0
    // none) and salience; the value of key `key` (FatalError "history: state:
    // event <e> has no value '<key>'" when the kind has no such key). Each is
    // fatal "history: state: event <e> does not exist" for an id outside
    // [1, last_event_id()].
    [[nodiscard]] std::int64_t last_event_id() const;
    [[nodiscard]] std::int64_t event_region(std::int64_t event) const;
    [[nodiscard]] std::int64_t event_settlement(std::int64_t event) const;
    [[nodiscard]] std::int64_t event_salience(std::int64_t event) const;
    [[nodiscard]] std::int64_t event_value(std::int64_t event, std::string_view key) const;

private:
    friend class Engine;
    StepContext(Engine& engine, Step step) : engine_(engine), step_(step) {}
    Engine& engine_;
    Step step_;
};

class Subsystem {  // P3-05, P3-06, P5
public:
    virtual ~Subsystem() = default;
    // The steps it runs in: T1 to T4, T6, Q1, Q3, Q4 (the others are the engine's).
    [[nodiscard]] virtual std::span<const Step> steps() const = 0;
    // Its private state is its own members.
    virtual void run(StepContext& context) = 0;
};

class AwakeningHandler {  // exactly one: the stub (7), then P3-05
public:
    virtual ~AwakeningHandler() = default;
    // A region id; no state change.
    virtual std::int64_t awakening_place(const StepContext& context, std::int64_t race) = 0;
    virtual void awaken(StepContext& context, std::int64_t race, std::int64_t awakening_event) = 0;
};

class TensionSource {  // P3-06
public:
    virtual ~TensionSource() = default;
    // Called in Q5; must not change state or append.
    virtual TensionReport tension(const StepContext& context) = 0;
};

// --- Divine interventions (6.8) --------------------------------------------------------

struct Intervention {
    std::int64_t year = 0;
    std::int64_t god = 0;
    std::string kind;
    EntityType target_type = EntityType::civilization;
    std::int64_t target_id = 0;
    std::int64_t region = 0;
    bool scripted = false;
};

class InterventionSource {
public:
    virtual ~InterventionSource() = default;
    // In T5 of each tick year; read-only view.
    virtual std::vector<Intervention> interventions(const StepContext& context) = 0;
};

class EmptyInterventionSource final : public InterventionSource {
public:
    std::vector<Intervention> interventions(const StepContext&) override { return {}; }
};

// The intervention kinds the engine supports: none in P3 (6.8), so every
// intervention is dropped.
[[nodiscard]] std::span<const std::string_view> supported_intervention_kinds() noexcept;

// --- The engine --------------------------------------------------------------------------

struct EngineConfig {
    AwakeningHandler* awakening = nullptr;     // not null
    std::vector<Subsystem*> subsystems;        // run order within each step
    std::vector<TensionSource*> tension;       // empty: tension 0
    InterventionSource* interventions = nullptr;  // not null; the empty source in P3
    const EventCatalogue* catalogue = nullptr;    // the stage's catalogue (8.2); tests may extend it
};

// The rows of the thirteen tables of 9.1, each in its write order (9.3).
struct HistoryResult {
    std::vector<db::EventKindRow> event_kinds;
    std::vector<db::EventKindRoleRow> event_kind_roles;
    std::vector<db::EventKindValueRow> event_kind_values;
    std::vector<db::PhaseRow> phases;
    std::vector<db::EventRow> events;
    std::vector<db::EventParticipantRow> event_participants;
    std::vector<db::EventCauseRow> event_causes;
    std::vector<db::EventValueRow> event_values;
    std::vector<db::CivilizationRow> civilizations;
    std::vector<db::SettlementRow> settlements;
    std::vector<db::SettlementCensusRow> settlement_census;
    std::vector<db::RegionControlRow> region_control;
    std::vector<db::RelationRow> relations;
    // P3-04 r21 23.4 (schema 8): the renaming subsystem's rows, sorted by
    // (region, from_year); filled by the history stage, empty from
    // run_engine.
    std::vector<db::RegionNameRow> region_names;
    // P5-02 11.1 (schema 9): the monsters subsystem's rows, filled by the
    // history stage, empty from run_engine.
    std::vector<db::BeastRow> beasts;
    std::vector<db::MonsterKindRow> monster_kinds;
    std::vector<db::BeastPopulationRow> beast_populations;
    std::vector<db::MonsterRow> monsters;
    std::vector<db::SiteRow> sites;

    friend bool operator==(const HistoryResult&, const HistoryResult&) = default;
};

// The entry checks the engine depends on (P3-04 3.4 items 2, 4 and 5), in
// that order, with their messages: the tuning of history.engine, the
// settings (span, param.races against the present race rows) and the
// chronology rows; then (audit A4) "history: engine: layer 'region' is not
// <N> x <N> u16" unless the region layer is a valid N x N u16 grid, N =
// map_size. Returns the checked tuning.
EngineTuning check_engine_inputs(const data::ContentSet& content, const HistoryInputs& inputs);

// Runs the schedule (section 5) and the engine (section 6) with the given
// configuration. FatalError on a null awakening handler, intervention
// source or catalogue, on a subsystem that lists an engine step (T5, T7,
// Q2, Q5, Q6) or one step twice ("history: engine: a subsystem lists step
// <n> twice"), and on every check of 3.4 items 2, 4, 5, 5.4 to 5.6 and 6.
[[nodiscard]] HistoryResult run_engine(const data::ContentSet& content, const HistoryInputs& inputs,
                                       const EngineConfig& config);

}  // namespace worldgen::history
