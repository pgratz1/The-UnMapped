// The scheduler of the history (spec P3-04 section 5).

#include "worldgen/history/schedule.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

#include "worldgen/core/error.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/db/schema.hpp"

namespace worldgen::history {

namespace {

[[noreturn]] void fail(const std::string& message) {
    throw FatalError(message);
}

std::string int_text(std::int64_t v) {
    return db::format_i64(v);
}

constexpr std::int64_t REFERENCE_SPAN = 6000;  // the span the tuning's lengths are given at (4.1)

void field_in(bool ok, std::string_view field) {
    if (!ok) {
        fail("history: tuning: " + std::string(field) + " outside its schema range");
    }
}

bool in(std::int64_t v, std::int64_t lo, std::int64_t hi) noexcept {
    return lo <= v && v <= hi;
}

EngineLevel level_from(const data::Record& r) {
    return EngineLevel{r.get_int("active_years"), r.get_int("flare_years"), r.get_int("final_years"),
                       r.get_int("flare_threshold")};
}

// The age (1-based) containing year y: the last k with starts[k - 1] <= y.
std::int64_t age_of(std::span<const std::int64_t> starts, std::int64_t y) {
    std::int64_t age = 0;
    for (std::size_t k = 0; k < starts.size() && starts[k] <= y; ++k) {
        age = static_cast<std::int64_t>(k) + 1;
    }
    return age;
}

}  // namespace

const EngineLevel& EngineTuning::level(data::HistoryDetail detail) const noexcept {
    switch (detail) {
        case data::HistoryDetail::sparse: return sparse;
        case data::HistoryDetail::normal: return normal;
        case data::HistoryDetail::dense: return dense;
    }
    return normal;
}

EngineTuning engine_tuning_from(const data::ContentSet& content) {
    const data::Definition* d = content.find(data::HISTORY_KIND, data::HISTORY_ENGINE_ID);
    if (d == nullptr) {
        fail("history: tuning: the content has no definition history.engine");
    }
    const data::Record& r = d->record;
    EngineTuning t;
    t.quiet_step_years = r.get_int("quiet_step_years");
    t.quiet_events_max = r.get_int("quiet_events_max");
    t.quiet_min_years = r.get_int("quiet_min_years");  // r15 20.1 item 1a
    t.simulated_min_permille = r.get_int("simulated_min_permille");
    t.simulated_max_permille = r.get_int("simulated_max_permille");
    t.islet_max_dry_cells = r.get_int("islet_max_dry_cells");
    t.sparse = level_from(r.get_record("sparse"));
    t.normal = level_from(r.get_record("normal"));
    t.dense = level_from(r.get_record("dense"));
    return t;
}

void check_engine_tuning(const EngineTuning& t) {
    field_in(in(t.quiet_step_years, 1, 200), "quiet_step_years");
    field_in(in(t.quiet_events_max, 1, 100), "quiet_events_max");
    field_in(in(t.quiet_min_years, 0, 400), "quiet_min_years");
    field_in(in(t.simulated_min_permille, 100, 500), "simulated_min_permille");
    field_in(in(t.simulated_max_permille, 100, 500), "simulated_max_permille");
    field_in(in(t.islet_max_dry_cells, 0, 1000000), "islet_max_dry_cells");
    const std::pair<std::string_view, const EngineLevel*> levels[] = {
        {"sparse", &t.sparse}, {"normal", &t.normal}, {"dense", &t.dense}};
    for (const auto& [name, l] : levels) {
        const std::string prefix = std::string(name) + ".";
        field_in(in(l->active_years, 1, 2000), prefix + "active_years");
        field_in(in(l->flare_years, 1, 1000), prefix + "flare_years");
        field_in(in(l->final_years, 1, 3000), prefix + "final_years");
        field_in(in(l->flare_threshold, 1, 1000000), prefix + "flare_threshold");
    }
    // The named checks of 4.1, in order (the messages of the loader's checks).
    if (t.simulated_min_permille > t.simulated_max_permille) {
        fail("history: tuning: simulated_min_permille must not be above simulated_max_permille");
    }
    if (t.quiet_min_years > 2 * t.quiet_step_years) {  // r15 engine_quiet_min
        fail("history: tuning: quiet_min_years must not be above 2 x quiet_step_years");
    }
    for (const auto& [name, l] : levels) {
        if (6 * l->active_years + l->final_years > 6 * t.simulated_max_permille - 30) {
            fail("history: tuning: " + std::string(name) +
                 ": 6 x active_years + final_years must not be above 6 x simulated_max_permille "
                 "- 30");
        }
    }
}

std::int64_t scale_years(std::int64_t v, std::int64_t span) {
    if (v < 0 || span < 1) {
        fail("scale_years: negative length or span below 1");
    }
    // v <= 3000 and span <= 12000 with loaded data; v E < 2^43 (13).
    return std::max<std::int64_t>(1, (v * span + REFERENCE_SPAN / 2) / REFERENCE_SPAN);
}

ScheduleLengths schedule_lengths(std::int64_t span, const EngineTuning& tuning, data::HistoryDetail detail) {
    const EngineLevel& d = tuning.level(detail);
    ScheduleLengths l;
    l.active = scale_years(d.active_years, span);
    l.flare = scale_years(d.flare_years, span);
    l.final_years = scale_years(d.final_years, span);
    l.quiet_step = tuning.quiet_step_years;
    l.cap_years = (tuning.simulated_max_permille * span) / 1000;
    // Clamped to the cap (lead's ruling, 2026-11-19): with min = max permille
    // and E x permille not a multiple of 1000 the rounded-up floor would be
    // the cap + 1.
    l.floor_years = std::min((tuning.simulated_min_permille * span + 999) / 1000, l.cap_years);
    l.flare_threshold = d.flare_threshold;
    return l;
}

BaseSchedule base_schedule(std::int64_t span, std::span<const std::int64_t> starts, const EngineTuning& tuning,
                           data::HistoryDetail detail) {
    if (span < 2) {
        fail("history: schedule: span " + int_text(span) + " below 2");
    }
    if (starts.empty() || starts.front() != 0) {
        fail("history: schedule: the age start years must begin at 0");
    }
    for (std::size_t k = 1; k < starts.size(); ++k) {
        if (starts[k] <= starts[k - 1]) {
            fail("history: schedule: the age start years must increase");
        }
    }
    if (starts.back() >= span) {
        fail("history: schedule: an age starts at or after the span");
    }
    BaseSchedule b;
    b.span = span;
    b.lengths = schedule_lengths(span, tuning, detail);
    b.starts.assign(starts.begin(), starts.end());
    const std::int64_t a = b.lengths.active;
    const std::size_t p = starts.size();
    const std::int64_t y_last = starts[p - 1];
    // SC2: opening phases.
    b.opening_ends.resize(p);
    for (std::size_t k = 0; k + 1 < p; ++k) {
        b.opening_ends[k] = std::min(starts[k] + a, starts[k + 1]);  // E1
    }
    b.opening_ends[p - 1] = span - y_last >= 2 ? std::min(y_last + a, span - 1) : span;
    // SC3: final phase.
    b.final_start = span - y_last >= 2 ? std::max(span - b.lengths.final_years, b.opening_ends[p - 1]) : span;
    // SC4: floor.
    std::int64_t opened = 0;
    for (std::size_t k = 0; k < p; ++k) {
        opened += b.opening_ends[k] - starts[k];
    }
    std::int64_t missing = b.lengths.floor_years - (opened + (span - b.final_start));
    if (missing > 0 && b.final_start < span) {
        const std::int64_t s = std::max(b.opening_ends[p - 1], b.final_start - missing);
        missing -= b.final_start - s;
        b.final_start = s;
    }
    for (std::size_t k = 0; k + 1 < p && missing > 0; ++k) {
        const std::int64_t e = std::min(starts[k + 1], b.opening_ends[k] + missing);
        missing -= e - b.opening_ends[k];
        b.opening_ends[k] = e;
    }
    b.scheduled = span - b.final_start;
    for (std::size_t k = 0; k < p; ++k) {
        b.scheduled += b.opening_ends[k] - starts[k];
    }
    if (b.scheduled > b.lengths.cap_years) {
        fail("history: schedule: scheduled active years " + int_text(b.scheduled) + " above the cap " +
             int_text(b.lengths.cap_years));
    }
    return b;
}

namespace {

class Runner {
public:
    Runner(const BaseSchedule& base, ScheduleHooks& hooks) : base_(base), hooks_(hooks) {}

    std::vector<db::PhaseRow> run() {
        const std::size_t p = base_.starts.size();
        for (std::size_t k = 0; k < p; ++k) {
            active(PhaseKind::opening, base_.starts[k], base_.opening_ends[k], 0);
            const std::int64_t q_e = k + 1 < p ? base_.starts[k + 1] : base_.final_start;
            if (base_.opening_ends[k] < q_e) {
                quiet_interval(base_.opening_ends[k], q_e);
            }
        }
        if (base_.final_start < base_.span) {
            active(PhaseKind::final, base_.final_start, base_.span, 0);
        }
        check_rows();
        return std::move(rows_);
    }

private:
    db::PhaseRow open_row(PhaseKind kind, std::int64_t start) {
        db::PhaseRow row;
        row.id = static_cast<std::int64_t>(rows_.size()) + 1;
        row.age = age_of(base_.starts, start);
        row.kind = static_cast<std::int64_t>(kind);
        row.start_year = start;
        row.end_year = start;
        return row;
    }

    void push(const db::PhaseRow& row) {
        if (row.end_year < row.start_year) {
            fail("history: schedule: phase " + int_text(row.id) + " ends before it starts");
        }
        rows_.push_back(row);
    }

    // [start, end) as one active phase.
    void active(PhaseKind kind, std::int64_t start, std::int64_t end, std::int64_t trigger) {
        db::PhaseRow row = open_row(kind, start);
        row.end_year = end - 1;
        row.trigger_event = trigger;
        if (row.end_year < row.start_year) {
            fail("history: schedule: phase " + int_text(row.id) + " ends before it starts");
        }
        hooks_.active_phase(row);
        push(row);
    }

    // SC5, SC6 over [q_s, q_e).
    void quiet_interval(std::int64_t q_s, std::int64_t q_e) {
        const ScheduleLengths& l = base_.lengths;
        std::int64_t cur = q_s;
        while (cur < q_e) {
            db::PhaseRow row = open_row(PhaseKind::quiet, cur);
            hooks_.quiet_row(row);
            std::int64_t s = cur;
            for (;;) {
                const std::int64_t len = std::min(l.quiet_step, q_e - s);  // E2
                if (len < 1) {
                    fail("history: schedule: quiet step length " + int_text(len) + " below 1");
                }
                const bool ends_interval = s + len == q_e;
                const TensionReport t = hooks_.quiet_step(s, len, !ends_interval);
                s += len;
                std::int64_t f = 0;
                if (!ends_interval && t.value >= l.flare_threshold) {
                    f = std::min({l.flare, q_e - s, l.cap_years - base_.scheduled - flare_total_});  // E3, E8
                }
                const bool flare = f >= 1;
                const bool ends_row = ends_interval || flare;
                hooks_.quiet_step_end(ends_row, ends_row ? 0 : std::min(l.quiet_step, q_e - s));
                if (ends_interval) {
                    row.end_year = s - 1;
                    push(row);
                    cur = q_e;
                    break;
                }
                if (flare) {
                    if (f > q_e - s) {
                        fail("history: schedule: flare-up length " + int_text(f) + " outside [1, " +
                             int_text(q_e - s) + "]");
                    }
                    row.end_year = s - 1;
                    push(row);
                    active(PhaseKind::flare, s, s + f, t.trigger_event);
                    flare_total_ += f;
                    cur = s + f;
                    break;
                }
            }
        }
    }

    void check_rows() const {
        std::int64_t next = 0;
        for (const db::PhaseRow& r : rows_) {
            if (r.start_year != next || r.end_year < r.start_year) {
                fail("history: schedule: phase rows do not tile [0, " + int_text(base_.span) + ") at phase " +
                     int_text(r.id));
            }
            next = r.end_year + 1;
        }
        if (next != base_.span) {
            fail("history: schedule: phase rows do not tile [0, " + int_text(base_.span) + ") at the end");
        }
        const std::int64_t sim = simulated_years(rows_);
        if (sim < base_.lengths.floor_years || sim > base_.lengths.cap_years) {
            fail("history: schedule: simulated years " + int_text(sim) + " outside [" +
                 int_text(base_.lengths.floor_years) + ", " + int_text(base_.lengths.cap_years) + "]");
        }
    }

    const BaseSchedule& base_;
    ScheduleHooks& hooks_;
    std::vector<db::PhaseRow> rows_;
    std::int64_t flare_total_ = 0;
};

class PlanHooks final : public ScheduleHooks {
public:
    explicit PlanHooks(const std::function<TensionReport(std::int64_t, std::int64_t)>& tension) : tension_(tension) {}
    void active_phase(db::PhaseRow&) override {}
    void quiet_row(const db::PhaseRow&) override {}
    TensionReport quiet_step(std::int64_t start, std::int64_t length, bool asks_tension) override {
        return asks_tension && tension_ ? tension_(start, length) : TensionReport{};
    }
    void quiet_step_end(bool, std::int64_t) override {}

private:
    const std::function<TensionReport(std::int64_t, std::int64_t)>& tension_;
};

}  // namespace

std::vector<db::PhaseRow> run_schedule(const BaseSchedule& base, ScheduleHooks& hooks) {
    return Runner(base, hooks).run();
}

std::vector<db::PhaseRow> plan_phases(const BaseSchedule& base,
                                      const std::function<TensionReport(std::int64_t, std::int64_t)>& tension) {
    PlanHooks hooks(tension);
    return run_schedule(base, hooks);
}

std::int64_t simulated_years(std::span<const db::PhaseRow> rows) noexcept {
    std::int64_t n = 0;
    for (const db::PhaseRow& r : rows) {
        if (r.kind != static_cast<std::int64_t>(PhaseKind::quiet)) {
            n += r.end_year - r.start_year + 1;
        }
    }
    return n;
}

}  // namespace worldgen::history
