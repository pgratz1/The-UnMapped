#pragma once

// The scheduler of the history (spec P3-04 section 5): the tuning
// history.engine (4.1), the lengths SC1, the base schedule SC2 to SC4 and
// the run of the phase rows SC5 and SC6, which interleaves quiet steps with
// flare-ups as the tension asks.
//
// Integers only (H1): every division is a div of non-negative int64.

#include <cstdint>
#include <functional>
#include <span>
#include <vector>

#include "worldgen/data/content.hpp"
#include "worldgen/data/settings.hpp"
#include "worldgen/db/writer.hpp"

namespace worldgen::history {

// Record engine_detail of history.engine (P3-04 4.1), fields in order.
struct EngineLevel {
    std::int64_t active_years = 0;     // int(1, 2000)
    std::int64_t flare_years = 0;      // int(1, 1000)
    std::int64_t final_years = 0;      // int(1, 3000)
    std::int64_t flare_threshold = 0;  // int(1, 1000000)

    friend bool operator==(const EngineLevel&, const EngineLevel&) = default;
};

// The fields of definition history.engine (P3-04 4.1), in declaration order.
struct EngineTuning {
    std::int64_t quiet_step_years = 0;        // int(1, 200)
    std::int64_t quiet_events_max = 0;        // int(1, 100)
    std::int64_t simulated_min_permille = 0;  // int(100, 500)
    std::int64_t simulated_max_permille = 0;  // int(100, 500)
    std::int64_t islet_max_dry_cells = 0;     // int(0, 1000000)
    EngineLevel sparse;
    EngineLevel normal;
    EngineLevel dense;

    // The record of a detail level.
    [[nodiscard]] const EngineLevel& level(data::HistoryDetail detail) const noexcept;

    friend bool operator==(const EngineTuning&, const EngineTuning&) = default;
};

// The tuning of the content's history.engine. FatalError ("history: tuning:
// the content has no definition history.engine") if it is absent.
[[nodiscard]] EngineTuning engine_tuning_from(const data::ContentSet& content);

// C1 item 2 (P3-04 3.4): each field inside its schema range ("history:
// tuning: <field> outside its schema range", declaration order, the level
// records' fields as "<level>.<field>"), then the named checks
// engine_simulated_bounds and engine_phase_budget ("history: tuning:
// <message>", the messages of 4.1).
void check_engine_tuning(const EngineTuning& tuning);

// SC1: max(1, (v E + 3000) div 6000). FatalError if v < 0 or span < 1.
[[nodiscard]] std::int64_t scale_years(std::int64_t v, std::int64_t span);

// SC1: the lengths of a span and level.
struct ScheduleLengths {
    std::int64_t active = 0;       // A
    std::int64_t flare = 0;        // F
    std::int64_t final_years = 0;  // L
    std::int64_t quiet_step = 0;   // Q (not scaled)
    std::int64_t floor_years = 0;  // min(ceil(simulated_min_permille E / 1000), cap_years)
    std::int64_t cap_years = 0;    // floor(simulated_max_permille E / 1000)
    std::int64_t flare_threshold = 0;

    friend bool operator==(const ScheduleLengths&, const ScheduleLengths&) = default;
};
[[nodiscard]] ScheduleLengths schedule_lengths(std::int64_t span, const EngineTuning& tuning,
                                               data::HistoryDetail detail);

// SC2 to SC4: the opening phases [starts[k], opening_ends[k]) and the final
// phase [final_start, span) (empty when final_start = span, E5); scheduled =
// the active years of both.
struct BaseSchedule {
    ScheduleLengths lengths;
    std::int64_t span = 0;
    std::vector<std::int64_t> starts;
    std::vector<std::int64_t> opening_ends;
    std::int64_t final_start = 0;
    std::int64_t scheduled = 0;

    friend bool operator==(const BaseSchedule&, const BaseSchedule&) = default;
};

// SC1 to SC4 for the age start years `starts` (0 = y_1 < y_2 < ... < y_P <
// span). FatalError "history: schedule: ..." if starts is empty, does not
// begin at 0, is not strictly increasing below span, or (A1) the scheduled
// active years exceed the cap ("history: schedule: scheduled active years
// <sched> above the cap <cap_years>").
[[nodiscard]] BaseSchedule base_schedule(std::int64_t span, std::span<const std::int64_t> starts,
                                         const EngineTuning& tuning, data::HistoryDetail detail);

// The tension of one quiet step (P3-04 6.5).
struct TensionReport {
    std::int64_t value = 0;
    std::int64_t trigger_event = 0;

    friend bool operator==(const TensionReport&, const TensionReport&) = default;
};

// Phase kinds (P3-00 4.1, P3-04 5.7).
enum class PhaseKind : std::int64_t { opening = 1, quiet = 2, flare = 3, final = 4 };

// What run_schedule calls as it runs the phases, in time order.
class ScheduleHooks {
public:
    virtual ~ScheduleHooks() = default;
    // An opening, flare-up or final phase, its row complete (end_year
    // inclusive; trigger_event the flare-up's trigger, 0 for the others).
    // Runs its years; may set row.trigger_event (an opening's awakening).
    virtual void active_phase(db::PhaseRow& row) = 0;
    // A quiet phase row opens: id, age, kind 2 and start_year are set,
    // end_year is not known yet.
    virtual void quiet_row(const db::PhaseRow& row) = 0;
    // A quiet step [start, start + length) of the open row: Q1 to Q5.
    // asks_tension is false for a step that ends its interval (SC6, E3);
    // the result is then ignored.
    virtual TensionReport quiet_step(std::int64_t start, std::int64_t length, bool asks_tension) = 0;
    // Q6 of that step: ends_row is true when the step closes its quiet row
    // (the interval ends, or a flare-up follows).
    virtual void quiet_step_end(bool ends_row) = 0;
};

// SC5 and SC6 over a base schedule: the phase rows, ids 1, 2, ... in time
// order, after the guards of 5.5 (fatal "history: schedule: <what>"): a step
// length below 1, a flare length outside [1, q_e - s], a row ending before
// it starts, and after the run rows not tiling [0, E) or simulated years
// outside [floor_years, cap_years].
[[nodiscard]] std::vector<db::PhaseRow> run_schedule(const BaseSchedule& base, ScheduleHooks& hooks);

// The phase rows alone, for a tension function of the quiet step (start,
// length); triggers of opening phases stay 0. For tests and tools.
[[nodiscard]] std::vector<db::PhaseRow> plan_phases(
    const BaseSchedule& base, const std::function<TensionReport(std::int64_t start, std::int64_t length)>& tension);

// Simulated years of phase rows: the years of kinds 1, 3 and 4.
[[nodiscard]] std::int64_t simulated_years(std::span<const db::PhaseRow> rows) noexcept;

}  // namespace worldgen::history
