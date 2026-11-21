// The simulation engine of the history (spec P3-04 sections 5 and 6).

#include "worldgen/history/engine.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <utility>

#include "worldgen/core/checked.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/data/settings.hpp"
#include "worldgen/db/schema.hpp"
#include "worldgen/history/draw.hpp"

namespace worldgen::history {

namespace {

[[noreturn]] void fail(const std::string& message) {
    throw FatalError(message);
}

std::string int_text(std::int64_t v) {
    return db::format_i64(v);
}

std::string size_text(std::size_t v) {
    return db::format_u64(static_cast<std::uint64_t>(v));
}

std::int64_t code(EntityType t) noexcept {
    return static_cast<std::int64_t>(t);
}

bool is_engine_step(Step s) noexcept {
    return s == Step::t5 || s == Step::t7 || s == Step::q2 || s == Step::q5 || s == Step::q6;
}

constexpr std::array<Step, 7> TICK_STEPS = {Step::t1, Step::t2, Step::t3, Step::t4, Step::t5, Step::t6, Step::t7};
constexpr std::array<Step, 5> QUIET_STEPS_BEFORE_TENSION = {Step::q1, Step::q2, Step::q3, Step::q4, Step::q5};

}  // namespace

std::span<const std::string_view> supported_intervention_kinds() noexcept {
    return {};  // P3: none (6.8); P5 fixes the list
}

// --- Entry checks (3.4 items 2, 4, 5) ------------------------------------------------

EngineTuning check_engine_inputs(const data::ContentSet& content, const HistoryInputs& in) {
    // Item 2: tuning.
    EngineTuning tuning = engine_tuning_from(content);
    check_engine_tuning(tuning);
    // Item 4: settings.
    if (!data::is_valid_history_span(in.params.span)) {
        fail("history: settings: history_span " + int_text(in.params.span) + " outside 2000 to 12000");
    }
    std::vector<const db::RaceRow*> present;
    for (const db::RaceRow& r : in.races) {
        if (r.present == 1) {
            present.push_back(&r);
        }
    }
    bool races_match = present.size() == in.params.races.size();
    for (std::size_t i = 0; races_match && i < present.size(); ++i) {
        races_match = present[i]->key == in.params.races[i];
    }
    if (!races_match) {
        fail("history: settings: param.races does not match the present race rows");
    }
    // Item 5: chronology rows (audit A1).
    const std::int64_t span = in.params.span;
    const auto p = static_cast<std::int64_t>(present.size());
    bool ids_ok = static_cast<std::int64_t>(in.ages.size()) == p;
    for (std::size_t i = 0; ids_ok && i < in.ages.size(); ++i) {
        ids_ok = in.ages[i].id == static_cast<std::int64_t>(i) + 1;
    }
    if (!ids_ok) {
        fail("history: chronology: the age rows are not ids 1 to " + int_text(p) + " in order");
    }
    for (std::size_t i = 0; i < in.ages.size(); ++i) {
        const db::AgeRow& a = in.ages[i];
        const std::string at = "history: chronology: age " + int_text(a.id) + ": ";
        if (i == 0 && a.start_year != 0) {
            fail(at + "the first start_year is " + int_text(a.start_year) + ", expected 0");
        }
        if (i > 0 && a.start_year != in.ages[i - 1].end_year + 1) {
            fail(at + "start_year " + int_text(a.start_year) + " is not the previous end_year + 1");
        }
        if (a.end_year < a.start_year) {
            fail(at + "end_year below start_year");
        }
        const db::RaceRow* race = nullptr;
        for (const db::RaceRow* r : present) {
            race = r->id == a.race ? r : race;
        }
        if (race == nullptr) {
            fail(at + "race " + int_text(a.race) + " is not a present race");
        }
        if (race->awakening_year != a.start_year) {
            fail(at + "race " + int_text(a.race) + " awakens in " +
                 (race->awakening_year ? int_text(*race->awakening_year) : std::string("no year")) +
                 ", expected its start_year " + int_text(a.start_year));
        }
    }
    if (!in.ages.empty() && in.ages.back().end_year != span - 1) {
        fail("history: chronology: the last end_year is " + int_text(in.ages.back().end_year) + ", expected " +
             int_text(span - 1));
    }
    for (const db::RaceRow* r : present) {
        bool found = false;
        for (const db::GodRow& g : in.gods) {
            found = found || g.id == r->creator_god;
        }
        if (!found) {
            fail("history: chronology: no god row for present race " + int_text(r->id));
        }
    }
    // Audit A4: the region layer the state checks settlement cells against.
    const db::LayerGrid& layer = in.region_layer;
    if (in.map_size < 1 || in.map_size > static_cast<std::int64_t>(db::MAX_LAYER_SIDE) ||
        static_cast<std::int64_t>(layer.width) != in.map_size ||
        static_cast<std::int64_t>(layer.height) != in.map_size || layer.get_if<std::uint16_t>() == nullptr ||
        !db::is_valid_layer_grid(layer)) {
        fail("history: engine: layer 'region' is not " + int_text(in.map_size) + " x " + int_text(in.map_size) +
             " u16");
    }
    return tuning;
}

// --- GeoView -----------------------------------------------------------------------------

GeoView::GeoView(const HistoryInputs& in, std::int64_t islet_max_dry_cells) {
    regions_.reserve(in.regions.size());
    for (std::size_t i = 0; i < in.regions.size(); ++i) {
        const db::RegionRow& r = in.regions[i];
        if (r.id != static_cast<std::int64_t>(i) + 1) {
            fail("history: input precondition: region ids are not 1 to " + size_text(in.regions.size()));
        }
        Region g;
        g.id = r.id;
        g.dry_cells = r.cells - r.lake_cells;
        g.islet = g.dry_cells < islet_max_dry_cells;
        regions_.push_back(std::move(g));
    }
    for (std::size_t k = 0; k < in.region_borders.size(); ++k) {
        const db::RegionBorderRow& b = in.region_borders[k];
        if (!has_region(b.a) || !has_region(b.b)) {
            fail("history: input precondition: a region_border row names a region that does not exist");
        }
        regions_[static_cast<std::size_t>(b.a - 1)].neighbours.push_back(Neighbour{b.b, k});
        regions_[static_cast<std::size_t>(b.b - 1)].neighbours.push_back(Neighbour{b.a, k});
    }
    for (Region& g : regions_) {
        std::stable_sort(g.neighbours.begin(), g.neighbours.end(),
                         [](const Neighbour& x, const Neighbour& y) { return x.region < y.region; });
    }
}

bool GeoView::has_region(std::int64_t id) const noexcept {
    return id >= 1 && static_cast<std::uint64_t>(id) <= regions_.size();
}

const GeoView::Region& GeoView::region(std::int64_t id) const {
    if (!has_region(id)) {
        fail("history: state: region " + int_text(id) + " does not exist");
    }
    return regions_[static_cast<std::size_t>(id - 1)];
}

// --- Engine --------------------------------------------------------------------------------

class Engine final : public ScheduleHooks {
public:
    Engine(const data::ContentSet& content, const HistoryInputs& inputs, const EngineConfig& config,
           const EngineTuning& tuning, const BaseSchedule& base)
        : content_(content),
          inputs_(inputs),
          config_(config),
          tuning_(tuning),
          base_(base),
          geo_(inputs, tuning.islet_max_dry_cells),
          state_(*this) {
        state_.regions_.resize(geo_.regions().size());
        recorded_owner_.assign(geo_.regions().size(), 0);
        open_control_.assign(geo_.regions().size(), NONE);
        for (Subsystem* s : config.subsystems) {
            if (s == nullptr) {
                fail("history: engine: a subsystem is null");
            }
            std::array<bool, 16> listed{};  // the steps this subsystem listed so far (N1)
            for (const Step step : s->steps()) {
                if (is_engine_step(step) || static_cast<int>(step) < 1 || static_cast<int>(step) > 13) {
                    fail("history: engine: a subsystem lists step " + int_text(static_cast<std::int64_t>(step)) +
                         ", which is not a subsystem step");
                }
                if (listed[static_cast<std::size_t>(step)]) {
                    fail("history: engine: a subsystem lists step " + int_text(static_cast<std::int64_t>(step)) +
                         " twice");
                }
                listed[static_cast<std::size_t>(step)] = true;
                by_step_[static_cast<std::size_t>(step)].push_back(s);
            }
        }
        for (const TensionSource* t : config.tension) {
            if (t == nullptr) {
                fail("history: engine: a tension source is null");
            }
        }
    }

    HistoryResult run() {
        HistoryResult out;
        out.phases = run_schedule(base_, *this);
        check_references();
        const CatalogueRows cat = catalogue_rows(*config_.catalogue);
        out.event_kinds = cat.kinds;
        out.event_kind_roles = cat.roles;
        out.event_kind_values = cat.values;
        for (StoredEvent& e : events_) {
            out.events.push_back(e.row);
            for (const db::EventParticipantRow& p : e.participants) {
                out.event_participants.push_back(p);
            }
            for (const std::int64_t c : e.causes) {
                out.event_causes.push_back(db::EventCauseRow{e.row.id, c});
            }
            for (std::size_t i = 0; i < e.values.size(); ++i) {
                out.event_values.push_back(db::EventValueRow{e.row.id, static_cast<std::int64_t>(i) + 1, e.values[i]});
            }
        }
        for (const Civilization& c : state_.civs_) {
            out.civilizations.push_back(db::CivilizationRow{c.id, c.name, c.race, c.culture, c.home_region, c.capital,
                                                            c.founded_year, c.founding_event, c.ended_year,
                                                            c.ending_event});
        }
        for (const Settlement& s : state_.settlements_) {
            out.settlements.push_back(db::SettlementRow{
                s.id, s.name, s.x, s.y, s.region, s.founder_civ, s.founded_year, s.founding_event, s.owner_civ,
                static_cast<std::int64_t>(s.kind), static_cast<std::int64_t>(s.status), s.population, s.ruined_year,
                s.ruin_event});
        }
        out.settlement_census = std::move(census_rows_);
        std::stable_sort(out.settlement_census.begin(), out.settlement_census.end(),
                         [](const db::SettlementCensusRow& a, const db::SettlementCensusRow& b) {
                             return std::pair(a.settlement, a.year) < std::pair(b.settlement, b.year);
                         });
        out.region_control = std::move(control_rows_);
        std::stable_sort(out.region_control.begin(), out.region_control.end(),
                         [](const db::RegionControlRow& a, const db::RegionControlRow& b) {
                             return std::pair(a.region, a.from_year) < std::pair(b.region, b.from_year);
                         });
        out.relations = std::move(relation_rows_);
        std::stable_sort(out.relations.begin(), out.relations.end(), [](const db::RelationRow& a, const db::RelationRow& b) {
            return std::tie(a.a_type, a.a_id, a.b_type, a.b_id, a.kind, a.since) <
                   std::tie(b.a_type, b.a_id, b.b_type, b.b_id, b.kind, b.since);
        });
        return out;
    }

    // --- Accessors for State and StepContext ---------------------------------------
    [[nodiscard]] std::uint64_t serial() const noexcept { return serial_; }
    [[nodiscard]] std::int64_t year() const noexcept { return year_; }
    [[nodiscard]] std::int64_t years() const noexcept { return years_; }
    [[nodiscard]] std::int64_t phase_id() const noexcept { return phase_id_; }
    [[nodiscard]] PhaseKind phase_kind() const noexcept { return phase_kind_; }
    [[nodiscard]] const HistoryInputs& inputs() const noexcept { return inputs_; }
    [[nodiscard]] const GeoView& geo() const noexcept { return geo_; }
    [[nodiscard]] const data::ContentSet& content() const noexcept { return content_; }
    [[nodiscard]] State& state() noexcept { return state_; }
    [[nodiscard]] const State& state() const noexcept { return state_; }
    [[nodiscard]] std::int64_t last_event() const noexcept { return static_cast<std::int64_t>(events_.size()); }

    [[nodiscard]] bool has_race(std::int64_t id) const noexcept {
        return std::any_of(inputs_.races.begin(), inputs_.races.end(), [id](const db::RaceRow& r) { return r.id == id; });
    }
    [[nodiscard]] bool has_god(std::int64_t id) const noexcept {
        return std::any_of(inputs_.gods.begin(), inputs_.gods.end(), [id](const db::GodRow& g) { return g.id == id; });
    }
    [[nodiscard]] bool has_culture(std::int64_t id) const noexcept {
        return std::any_of(inputs_.cultures.begin(), inputs_.cultures.end(),
                           [id](const db::CultureRow& c) { return c.id == id; });
    }
    [[nodiscard]] bool entity_exists(EntityType type, std::int64_t id) const noexcept {
        switch (type) {
            case EntityType::race: return has_race(id);
            case EntityType::god: return has_god(id);
            case EntityType::civilization: return id >= 1 && static_cast<std::uint64_t>(id) <= state_.civs_.size();
            case EntityType::settlement:
                return id >= 1 && static_cast<std::uint64_t>(id) <= state_.settlements_.size();
            case EntityType::region: return geo_.has_region(id);
            case EntityType::figure:
            case EntityType::faction:
            case EntityType::monster:
            case EntityType::site: return false;
        }
        return false;
    }

    // W4, W7: an entity that exists, or the civilization or settlement the
    // emitter founds in the same operation, right after the event that names
    // it (the next id; 7.2 appends civ_founded before found_civilization).
    // check_references verifies, after each step, that it was founded.
    [[nodiscard]] bool entity_exists_or_next(EntityType type, std::int64_t id) const noexcept {
        if (type == EntityType::civilization) {
            return id >= 1 && static_cast<std::uint64_t>(id) <= state_.civs_.size() + 1U;
        }
        if (type == EntityType::settlement) {
            return id >= 1 && static_cast<std::uint64_t>(id) <= state_.settlements_.size() + 1U;
        }
        return entity_exists(type, id);
    }

    [[nodiscard]] std::int64_t quiet_events_left(std::int64_t civ) const {
        if (!in_quiet_) {
            return tuning_.quiet_events_max;
        }
        const auto it = charged_.find(civ);
        return tuning_.quiet_events_max - (it == charged_.end() ? 0 : it->second);
    }

    std::int64_t append(const EventSpec& spec) { return append_event(spec, false); }

    // P3-06 8.1 item 2: the stored log, read-only.
    [[nodiscard]] std::string_view event_kind(std::int64_t event) const {
        return config_.catalogue->find_id(stored(event).row.kind)->key;
    }
    [[nodiscard]] std::int64_t event_year(std::int64_t event) const { return stored(event).row.year; }
    [[nodiscard]] std::int64_t event_participant(std::int64_t event, std::string_view role) const {
        const StoredEvent& e = stored(event);
        const RoleDef* r = config_.catalogue->find_id(e.row.kind)->find_role(role);
        if (r == nullptr) {
            return 0;
        }
        for (const db::EventParticipantRow& p : e.participants) {
            if (p.role == r->seq) {
                return p.entity_id;
            }
        }
        return 0;
    }

private:
    static constexpr std::size_t NONE = std::numeric_limits<std::size_t>::max();

    struct StoredEvent {
        db::EventRow row;
        std::vector<db::EventParticipantRow> participants;
        std::vector<std::int64_t> values;
        std::vector<std::int64_t> causes;
    };

    [[nodiscard]] const StoredEvent& stored(std::int64_t event) const {
        if (event < 1 || event > last_event()) {
            fail("history: state: event " + int_text(event) + " does not exist");
        }
        return events_[static_cast<std::size_t>(event - 1)];
    }

    // --- Steps ------------------------------------------------------------------------

    void begin_step(Step step) {
        check_references();
        if (serial_ == std::numeric_limits<std::uint64_t>::max()) {
            fail("history: bound: steps exceed 2^64 - 1");
        }
        ++serial_;
        (void)step;
    }

    // The events appended since the last call name only entities that exist
    // now (W4, W7): a civilization or settlement named before its founding
    // was founded in the same step, and a settlement lies in the event's
    // region.
    void check_references() {
        for (; checked_events_ < events_.size(); ++checked_events_) {
            const StoredEvent& e = events_[checked_events_];
            const std::string at = "history: event " + int_text(e.row.id) + " (" +
                                   config_.catalogue->find_id(e.row.kind)->key + "): ";
            for (const db::EventParticipantRow& p : e.participants) {
                const auto type = static_cast<EntityType>(p.entity_type);
                if (!entity_exists(type, p.entity_id)) {
                    fail(at + std::string(entity_type_key(type)) + " " + int_text(p.entity_id) +
                         " was not founded in the step that named it");
                }
            }
            if (e.row.settlement != 0) {
                if (!entity_exists(EntityType::settlement, e.row.settlement)) {
                    fail(at + "settlement " + int_text(e.row.settlement) + " was not founded in the step that named it");
                }
                if (e.row.region != 0 && state_.settlement(e.row.settlement).region != e.row.region) {
                    fail(at + "settlement " + int_text(e.row.settlement) + " does not lie in region " +
                         int_text(e.row.region));
                }
            }
        }
    }

    void run_subsystems(Step step) {
        for (Subsystem* s : by_step_[static_cast<std::size_t>(step)]) {
            StepContext ctx(*this, step);
            s->run(ctx);
        }
    }

    // ScheduleHooks: an opening, flare-up or final phase.
    void active_phase(db::PhaseRow& row) override {
        phase_id_ = row.id;
        phase_kind_ = static_cast<PhaseKind>(row.kind);
        in_quiet_ = false;
        for (std::int64_t y = row.start_year; y <= row.end_year; ++y) {
            year_ = y;
            years_ = 1;
            if (phase_kind_ == PhaseKind::opening && y == row.start_year) {
                awakening(row);
            }
            if (phase_kind_ == PhaseKind::flare && y == row.start_year) {
                flare_start(row);
            }
            for (const Step step : TICK_STEPS) {
                begin_step(step);
                if (step == Step::t5) {
                    interventions();
                } else if (step == Step::t7) {
                    bookkeeping(y, y == row.end_year ? std::optional<std::int64_t>(row.end_year) : std::nullopt);
                } else {
                    run_subsystems(step);
                }
            }
        }
    }

    // 6.3: the awakening of the age's race at its start year.
    void awakening(db::PhaseRow& row) {
        begin_step(Step::awakening);
        const db::AgeRow& age = inputs_.ages[static_cast<std::size_t>(row.age - 1)];
        const db::RaceRow* race = nullptr;
        for (const db::RaceRow& r : inputs_.races) {
            race = r.id == age.race ? &r : race;
        }
        StepContext ctx(*this, Step::awakening);
        const std::int64_t place = config_.awakening->awakening_place(ctx, age.race);
        if (!geo_.has_region(place)) {
            fail("history: awakening: the place " + int_text(place) + " of race " + int_text(age.race) +
                 " is not a region");
        }
        EventSpec e;
        e.kind = "awakening";
        e.year = year_;
        e.end_year = year_;
        e.region = place;
        e.participants = {Participant{"race", EntityType::race, age.race},
                          Participant{"creator", EntityType::god, race->creator_god}};
        const std::int64_t id = append_event(e, false);
        row.trigger_event = id;
        config_.awakening->awaken(ctx, age.race, id);
    }

    // 6.3: the first year of a flare-up.
    void flare_start(const db::PhaseRow& row) {
        begin_step(Step::flare);
        EventSpec e;
        e.kind = "flare_up";
        e.year = year_;
        e.end_year = year_;
        e.values = {row.end_year - row.start_year + 1};
        e.causes = {row.trigger_event};
        append_event(e, false);
    }

    // T5 (6.8): every intervention is validated and, the supported list
    // being empty in P3, dropped without an event.
    void interventions() {
        StepContext ctx(*this, Step::t5);
        const std::vector<Intervention> list = config_.interventions->interventions(ctx);
        for (const Intervention& i : list) {
            (void)intervention_valid(i);  // dropped either way in P3
        }
    }

    [[nodiscard]] bool intervention_valid(const Intervention& i) const {
        if (i.year != year_ || !has_god(i.god)) {
            return false;
        }
        const std::span<const std::string_view> kinds = supported_intervention_kinds();
        if (std::find(kinds.begin(), kinds.end(), std::string_view(i.kind)) == kinds.end()) {
            return false;
        }
        if (!entity_exists(i.target_type, i.target_id)) {
            return false;
        }
        if (i.target_type == EntityType::civilization && state_.civ(i.target_id).ended_year) {
            return false;
        }
        if (i.target_type == EntityType::settlement &&
            state_.settlement(i.target_id).status == SettlementStatus::ruined) {
            return false;
        }
        return i.region == 0 || geo_.has_region(i.region);
    }

    // ScheduleHooks: a quiet phase row opens.
    void quiet_row(const db::PhaseRow& row) override {
        phase_id_ = row.id;
        phase_kind_ = PhaseKind::quiet;
        open_stretch_ = 0;
    }

    // ScheduleHooks: Q1 to Q5 of a quiet step.
    TensionReport quiet_step(std::int64_t start, std::int64_t length, bool asks_tension) override {
        in_quiet_ = true;
        year_ = start;
        years_ = length;
        charged_.clear();
        TensionReport report;
        for (const Step step : QUIET_STEPS_BEFORE_TENSION) {
            begin_step(step);
            if (step == Step::q2) {
                continue;  // generations are counted from years (6.6); no state
            }
            if (step == Step::q5) {
                if (asks_tension) {
                    report = tension();
                }
                continue;
            }
            run_subsystems(step);
        }
        return report;
    }

    // 6.5: the largest value of the sources, the trigger of the first one
    // reporting it.
    TensionReport tension() {
        TensionReport best;
        bool any = false;
        const StepContext ctx(*this, Step::q5);
        for (TensionSource* source : config_.tension) {
            const TensionReport r = source->tension(ctx);
            if (r.value < 0) {
                fail("history: tension: value " + int_text(r.value) + " below 0");
            }
            if (r.value >= base_.lengths.flare_threshold && (r.trigger_event < 1 || r.trigger_event > last_event())) {
                fail("history: tension: trigger event " + int_text(r.trigger_event) + " outside [1, " +
                     int_text(last_event()) + "]");
            }
            if (!any || r.value > best.value) {
                best = r;
                any = true;
            }
        }
        return best;
    }

    // ScheduleHooks: Q6 (quiet eras, then the bookkeeping).
    void quiet_step_end(bool ends_row) override {
        begin_step(Step::q6);
        const std::int64_t last = year_ + years_ - 1;
        quiet_stretch(last);
        bookkeeping(year_, ends_row ? std::optional<std::int64_t>(last) : std::nullopt);
        if (ends_row) {
            open_stretch_ = 0;
        }
        in_quiet_ = false;
        charged_.clear();
    }

    // r12 20.1 (generator 0.19.0; replaces 6.6's quiet eras): one
    // quiet_stretch per quiet phase row for the whole world, with every
    // civilization not ended at its first step as a realm; later steps of
    // the row lengthen it, its participants and realms never change.
    void quiet_stretch(std::int64_t last) {
        if (open_stretch_ != 0) {
            StoredEvent& e = events_[static_cast<std::size_t>(open_stretch_ - 1)];
            e.row.end_year = last;
            e.values[0] = last - e.row.year + 1;
            return;
        }
        EventSpec e;
        e.kind = "quiet_stretch";
        e.year = year_;
        e.end_year = last;
        // r13: realms counts every civilization not ended; the participants
        // are at most MAX_PARTICIPANTS of them, the most populous (people of
        // their standing settlements now), ties to the smaller id, listed in
        // ascending id.
        std::vector<std::pair<std::int64_t, std::int64_t>> alive;  // (-people, id)
        for (std::size_t i = 0; i < state_.civs_.size(); ++i) {
            const Civilization& c = state_.civs_[i];
            if (!c.ended_year) {
                alive.emplace_back(0, c.id);
            }
        }
        if (alive.empty()) {
            return;  // no civilization alive: no quiet line
        }
        for (std::pair<std::int64_t, std::int64_t>& a : alive) {
            for (const Settlement& t : state_.settlements_) {
                const bool up = t.status == SettlementStatus::thriving || t.status == SettlementStatus::declining;
                if (up && t.owner_civ == a.second) {
                    a.first = checked_sub(a.first, t.population);
                }
            }
        }
        const auto realms = static_cast<std::int64_t>(alive.size());
        if (alive.size() > MAX_PARTICIPANTS) {
            std::stable_sort(alive.begin(), alive.end());  // keys distinct (the id)
            alive.resize(MAX_PARTICIPANTS);
            std::stable_sort(alive.begin(), alive.end(),
                             [](const auto& x, const auto& y) { return x.second < y.second; });
        }
        for (const auto& a : alive) {
            e.participants.push_back(Participant{"realm", EntityType::civilization, a.second});
        }
        e.values = {last - year_ + 1, realms};
        open_stretch_ = append_event(e, true);
    }

    // --- The event writer (6.6) --------------------------------------------------------

    std::int64_t append_event(const EventSpec& spec, bool engine_era) {
        const std::int64_t id = last_event() + 1;
        if (id > MAX_EVENTS) {
            fail("history: bound: events exceeds " + int_text(MAX_EVENTS));
        }
        const std::string at = "history: event " + int_text(id) + " (" + std::string(spec.kind) + "): ";
        // W1.
        const KindDef* kind = config_.catalogue->find(spec.kind);
        if (kind == nullptr) {
            fail(at + "the kind is not in the catalogue");
        }
        // W2.
        const std::int64_t expected_year = year_;  // the tick year, or the quiet step's start
        if (spec.year != expected_year) {
            fail(at + "year " + int_text(spec.year) + ", expected " + int_text(expected_year));
        }
        const std::int64_t end_max = in_quiet_ ? year_ + years_ - 1 : inputs_.params.span - 1;
        if (spec.end_year < spec.year || spec.end_year > end_max) {
            fail(at + "end_year " + int_text(spec.end_year) + " outside [" + int_text(spec.year) + ", " +
                 int_text(end_max) + "]");
        }
        // W3.
        if (!events_.empty() && spec.year < events_.back().row.year) {
            fail(at + "year " + int_text(spec.year) + " below the previous event's " +
                 int_text(events_.back().row.year));
        }
        // W4.
        if (spec.participants.size() > MAX_PARTICIPANTS) {
            fail("history: bound: participants per event exceeds " + size_text(MAX_PARTICIPANTS));
        }
        std::vector<std::int64_t> counts(kind->roles.size(), 0);
        std::int64_t last_seq = 0;
        std::int64_t charged = 0;
        bool charged_found = false;
        StoredEvent stored;
        for (std::size_t i = 0; i < spec.participants.size(); ++i) {
            const Participant& p = spec.participants[i];
            const RoleDef* r = kind->find_role(p.role);
            if (r == nullptr) {
                fail(at + "'" + std::string(p.role) + "' is not a role of the kind");
            }
            if (r->type != p.type) {
                fail(at + "participant " + size_text(i + 1) + " of role '" + r->role + "' is a " +
                     std::string(entity_type_key(p.type)) + ", expected a " + std::string(entity_type_key(r->type)));
            }
            if (r->seq < last_seq) {
                fail(at + "participants are not in ascending role seq");
            }
            last_seq = r->seq;
            ++counts[static_cast<std::size_t>(r->seq - 1)];
            if (!entity_exists_or_next(p.type, p.id)) {
                fail(at + std::string(entity_type_key(p.type)) + " " + int_text(p.id) + " does not exist");
            }
            if (p.type == EntityType::civilization && !charged_found) {
                charged = p.id;
                charged_found = true;
            }
            stored.participants.push_back(db::EventParticipantRow{id, static_cast<std::int64_t>(i) + 1, r->seq,
                                                                  code(p.type), p.id});
        }
        for (const RoleDef& r : kind->roles) {
            const std::int64_t n = counts[static_cast<std::size_t>(r.seq - 1)];
            if (n < r.min_count || (r.max_count != 0 && n > r.max_count)) {
                fail(at + "role '" + r.role + "' has " + int_text(n) + " participants, expected " +
                     int_text(r.min_count) + " to " + (r.max_count == 0 ? std::string("any") : int_text(r.max_count)));
            }
        }
        // W5.
        if (spec.values.size() != kind->values.size()) {
            fail(at + size_text(spec.values.size()) + " values, expected " + size_text(kind->values.size()));
        }
        // W6.
        if (spec.causes.size() > MAX_CAUSES) {
            fail("history: bound: causes per event exceeds " + size_text(MAX_CAUSES));
        }
        for (std::size_t i = 0; i < spec.causes.size(); ++i) {
            const std::int64_t c = spec.causes[i];
            if (c < 1 || c > id - 1) {
                fail(at + "cause " + int_text(c) + " outside [1, " + int_text(id - 1) + "]");
            }
            if (i > 0 && c <= spec.causes[i - 1]) {
                fail(at + "causes are not strictly ascending");
            }
        }
        if (kind->caused && spec.causes.empty()) {
            fail(at + "the kind needs a cause");
        }
        // W7.
        check_use(at, "region", kind->region_use, spec.region, geo_.has_region(spec.region));
        check_use(at, "settlement", kind->settlement_use, spec.settlement,
                  entity_exists_or_next(EntityType::settlement, spec.settlement));
        if (spec.settlement != 0 && spec.region != 0 && entity_exists(EntityType::settlement, spec.settlement) &&
            state_.settlement(spec.settlement).region != spec.region) {
            fail(at + "settlement " + int_text(spec.settlement) + " does not lie in region " + int_text(spec.region));
        }
        // W8.
        const std::int64_t salience = spec.salience.value_or(kind->salience);
        if (salience < 0 || salience > 100) {
            fail(at + "salience " + int_text(salience) + " outside [0, 100]");
        }
        // W9.
        if (in_quiet_ && !engine_era) {
            if (quiet_events_left(charged) <= 0) {
                fail(at + "civilization " + int_text(charged) + " has no quiet events left in this step");
            }
            ++charged_[charged];
        }
        stored.row = db::EventRow{id, spec.year, spec.end_year, kind->id, phase_id_, spec.region, spec.settlement,
                                  salience};
        stored.values = spec.values;
        stored.causes = spec.causes;
        events_.push_back(std::move(stored));
        return id;
    }

    static void check_use(const std::string& at, const char* what, std::int64_t use, std::int64_t value,
                          bool exists) {
        if (use == 0 && value != 0) {
            fail(at + "the kind has no " + what + ", but " + what + " " + int_text(value) + " is given");
        }
        if (use == 1 && !exists) {
            fail(at + "the kind needs a " + what + "; " + int_text(value) + " is not one");
        }
        if (use == 2 && value != 0 && !exists) {
            fail(at + what + " " + int_text(value) + " does not exist");
        }
    }

    // --- Bookkeeping (6.7) -----------------------------------------------------------------

    void bookkeeping(std::int64_t y, std::optional<std::int64_t> phase_end) {
        // 1. Regions.
        for (std::size_t r = 0; r < state_.regions_.size(); ++r) {
            const RegionState& now = state_.regions_[r];
            if (now.owner == recorded_owner_[r]) {
                continue;
            }
            if (open_control_[r] != NONE) {
                control_rows_[open_control_[r]].to_year = y;
                open_control_[r] = NONE;
            }
            if (now.owner != 0) {
                open_control_[r] = control_rows_.size();
                control_rows_.push_back(db::RegionControlRow{static_cast<std::int64_t>(r) + 1, y, std::nullopt,
                                                             now.owner, now.owner_event});
            }
            recorded_owner_[r] = now.owner;
        }
        // 2. Relations: every opening and closing since the last bookkeeping,
        // in call order. A relation opened and closed again before this
        // bookkeeping gets a row with since = until = y (P3-06 ruling); one
        // opened twice would need two rows with one primary key (a_type,
        // a_id, b_type, b_id, kind, since) and is fatal.
        std::set<RelationKey> opened_now;
        for (const State::RelationOp& op : state_.relation_ops_) {
            const RelationKey& k = op.key;
            if (op.open) {
                if (!opened_now.insert(k).second) {
                    fail("history: state: relation '" + k.kind + "' of " + int_text(k.a_type) + " " +
                         int_text(k.a_id) + " and " + int_text(k.b_type) + " " + int_text(k.b_id) +
                         " opened twice before one bookkeeping");
                }
                open_relations_[k] = relation_rows_.size();
                relation_rows_.push_back(
                    db::RelationRow{k.a_type, k.a_id, k.b_type, k.b_id, k.kind, y, std::nullopt, op.event, 0});
            } else {
                const auto rec = open_relations_.find(k);
                if (rec == open_relations_.end()) {
                    fail("history: engine: a relation was closed that has no row");
                }
                db::RelationRow& row = relation_rows_[rec->second];
                row.until = y;
                row.until_event = op.event;
                open_relations_.erase(rec);
            }
        }
        state_.relation_ops_.clear();
        if (open_relations_.size() != state_.relations_.size() ||
            !std::equal(open_relations_.begin(), open_relations_.end(), state_.relations_.begin(),
                        [](const auto& a, const auto& b) { return a.first == b.first; })) {
            fail("history: engine: a relation vanished without a closing event");
        }
        // 3. Census.
        last_census_.resize(state_.settlements_.size());
        for (std::size_t i = 0; i < state_.settlements_.size(); ++i) {
            const db::SettlementCensusRow now = census_of(state_.settlements_[i], y);
            const std::optional<db::SettlementCensusRow>& last = last_census_[i];
            if (!last || last->owner_civ != now.owner_civ || last->kind != now.kind || last->status != now.status) {
                census_rows_.push_back(now);
                last_census_[i] = now;
            }
        }
        // 4. Phase end.
        if (phase_end) {
            for (std::size_t i = 0; i < state_.settlements_.size(); ++i) {
                const db::SettlementCensusRow now = census_of(state_.settlements_[i], *phase_end);
                const db::SettlementCensusRow& last = *last_census_[i];
                if (last.owner_civ != now.owner_civ || last.kind != now.kind || last.status != now.status ||
                    last.population != now.population) {
                    if (last.year == *phase_end) {
                        fail("history: engine: two census rows of settlement " + size_text(i + 1) + " in year " +
                             int_text(*phase_end));
                    }
                    census_rows_.push_back(now);
                    last_census_[i] = now;
                }
            }
        }
    }

    static db::SettlementCensusRow census_of(const Settlement& s, std::int64_t y) {
        return db::SettlementCensusRow{s.id, y, s.owner_civ, static_cast<std::int64_t>(s.kind),
                                       static_cast<std::int64_t>(s.status), s.population};
    }

    const data::ContentSet& content_;
    const HistoryInputs& inputs_;
    const EngineConfig& config_;
    const EngineTuning& tuning_;
    const BaseSchedule& base_;
    GeoView geo_;
    State state_;
    std::array<std::vector<Subsystem*>, 16> by_step_{};

    // The clock.
    std::uint64_t serial_ = 0;
    std::int64_t year_ = 0;
    std::int64_t years_ = 1;
    std::int64_t phase_id_ = 0;
    PhaseKind phase_kind_ = PhaseKind::opening;
    bool in_quiet_ = false;

    std::vector<StoredEvent> events_;
    std::size_t checked_events_ = 0;  // events_ before this index passed check_references
    std::map<std::int64_t, std::int64_t> charged_;    // civilization (0 none) -> events charged in this quiet step
    std::int64_t open_stretch_ = 0;  // r12 20.1: the quiet_stretch of this quiet row, 0 none

    // Bookkeeping.
    std::vector<std::int64_t> recorded_owner_;  // per region
    std::vector<std::size_t> open_control_;     // per region: index of its open region_control row, or NONE
    std::vector<db::RegionControlRow> control_rows_;
    std::map<RelationKey, std::size_t> open_relations_;  // recorded open relations: index in relation_rows_
    std::vector<db::RelationRow> relation_rows_;
    std::vector<std::optional<db::SettlementCensusRow>> last_census_;
    std::vector<db::SettlementCensusRow> census_rows_;
};

// --- State ----------------------------------------------------------------------------------

namespace {

bool is_relation_kind(std::string_view kind) noexcept {
    return data::is_valid_id(kind);
}

}  // namespace

State::State(const Engine& engine) : engine_(engine) {}

void State::require_event(std::int64_t event, const char* what) const {
    if (event < 1 || event > engine_.last_event()) {
        fail(std::string("history: state: ") + what + ": event " + int_text(event) + " does not exist");
    }
}

void State::require_live_civ(std::int64_t id, const char* what) const {
    if (id < 1 || static_cast<std::uint64_t>(id) > civs_.size()) {
        fail(std::string("history: state: ") + what + ": civilization " + int_text(id) + " does not exist");
    }
    if (civs_[static_cast<std::size_t>(id - 1)].ended_year) {
        fail(std::string("history: state: ") + what + ": civilization " + int_text(id) + " has ended");
    }
}

const Civilization& State::civ(std::int64_t id) const {
    if (id < 1 || static_cast<std::uint64_t>(id) > civs_.size()) {
        fail("history: state: civilization " + int_text(id) + " does not exist");
    }
    return civs_[static_cast<std::size_t>(id - 1)];
}

const Settlement& State::settlement(std::int64_t id) const {
    if (id < 1 || static_cast<std::uint64_t>(id) > settlements_.size()) {
        fail("history: state: settlement " + int_text(id) + " does not exist");
    }
    return settlements_[static_cast<std::size_t>(id - 1)];
}

const RegionState& State::region(std::int64_t id) const {
    if (id < 1 || static_cast<std::uint64_t>(id) > regions_.size()) {
        fail("history: state: region " + int_text(id) + " does not exist");
    }
    return regions_[static_cast<std::size_t>(id - 1)];
}

Civilization& State::civ_mut(std::int64_t id, const char* what) {
    if (id < 1 || static_cast<std::uint64_t>(id) > civs_.size()) {
        fail(std::string("history: state: ") + what + ": civilization " + int_text(id) + " does not exist");
    }
    return civs_[static_cast<std::size_t>(id - 1)];
}

Settlement& State::settlement_mut(std::int64_t id, const char* what) {
    if (id < 1 || static_cast<std::uint64_t>(id) > settlements_.size()) {
        fail(std::string("history: state: ") + what + ": settlement " + int_text(id) + " does not exist");
    }
    return settlements_[static_cast<std::size_t>(id - 1)];
}

std::int64_t State::found_civilization(std::int64_t race, std::int64_t culture, std::int64_t home_region,
                                       std::string name, std::int64_t event) {
    const char* what = "found_civilization";
    if (static_cast<std::int64_t>(civs_.size()) >= MAX_CIVILIZATIONS) {
        fail("history: bound: civilizations exceeds " + int_text(MAX_CIVILIZATIONS));
    }
    if (!engine_.has_race(race)) {
        fail("history: state: found_civilization: race " + int_text(race) + " does not exist");
    }
    if (!engine_.has_culture(culture)) {
        fail("history: state: found_civilization: culture " + int_text(culture) + " does not exist");
    }
    if (!engine_.geo().has_region(home_region)) {
        fail("history: state: found_civilization: region " + int_text(home_region) + " does not exist");
    }
    require_event(event, what);
    Civilization c;
    c.id = static_cast<std::int64_t>(civs_.size()) + 1;
    c.race = race;
    c.culture = culture;
    c.home_region = home_region;
    c.founded_year = engine_.year();
    c.founding_event = event;
    c.name = std::move(name);
    c.created_step = engine_.serial();
    civs_.push_back(std::move(c));
    return civs_.back().id;
}

std::int64_t State::found_settlement(std::int64_t x, std::int64_t y, std::int64_t region, std::int64_t founder_civ,
                                     SettlementKind kind, std::int64_t population, std::string name,
                                     std::int64_t event) {
    const char* what = "found_settlement";
    if (static_cast<std::int64_t>(settlements_.size()) >= MAX_SETTLEMENTS) {
        fail("history: bound: settlements exceeds " + int_text(MAX_SETTLEMENTS));
    }
    const std::int64_t n = engine_.inputs().map_size;
    if (x < 0 || y < 0 || x >= n || y >= n) {
        fail("history: state: found_settlement: cell (" + int_text(x) + ", " + int_text(y) + ") is outside the map");
    }
    if (!engine_.geo().has_region(region)) {
        fail("history: state: found_settlement: region " + int_text(region) + " does not exist");
    }
    // The region layer is N x N u16: a precondition of run_engine (audit A4).
    const std::vector<std::uint16_t>& cells = *engine_.inputs().region_layer.get_if<std::uint16_t>();
    const std::uint16_t at = cells[static_cast<std::size_t>(y * n + x)];
    if (static_cast<std::int64_t>(at) != region) {
        fail("history: state: found_settlement: cell (" + int_text(x) + ", " + int_text(y) + ") lies in region " +
             int_text(at) + ", not " + int_text(region));
    }
    require_live_civ(founder_civ, what);
    if (population < 0 || population > MAX_POPULATION) {
        fail("history: bound: population exceeds " + int_text(MAX_POPULATION));
    }
    if (static_cast<std::int64_t>(kind) < 1 || static_cast<std::int64_t>(kind) > 5) {
        fail("history: state: found_settlement: kind is not a settlement kind");
    }
    require_event(event, what);
    Settlement s;
    s.id = static_cast<std::int64_t>(settlements_.size()) + 1;
    s.x = x;
    s.y = y;
    s.region = region;
    s.founder_civ = founder_civ;
    s.founded_year = engine_.year();
    s.founding_event = event;
    s.owner_civ = founder_civ;
    s.population = population;
    s.kind = kind;
    s.status = SettlementStatus::thriving;
    s.name = std::move(name);
    s.created_step = engine_.serial();
    settlements_.push_back(std::move(s));
    return settlements_.back().id;
}

void State::set_region_owner(std::int64_t region, std::int64_t civ, std::int64_t event) {
    const char* what = "set_region_owner";
    if (!engine_.geo().has_region(region)) {
        fail("history: state: set_region_owner: region " + int_text(region) + " does not exist");
    }
    if (civ != 0) {
        require_live_civ(civ, what);
    }
    require_event(event, what);
    RegionState& r = regions_[static_cast<std::size_t>(region - 1)];
    const bool new_holder = civ != 0 && civ != r.holder;
    r = RegionState{civ, event, civ != 0 ? civ : r.holder, r.holder_changes + (new_holder ? 1 : 0)};
}

void State::set_settlement_owner(std::int64_t id, std::int64_t civ, std::int64_t event) {
    const char* what = "set_settlement_owner";
    Settlement& s = settlement_mut(id, what);
    if (civ != 0) {
        require_live_civ(civ, what);
    }
    require_event(event, what);
    s.owner_civ = civ;
}

void State::set_settlement_kind(std::int64_t id, SettlementKind kind, std::int64_t event) {
    const char* what = "set_settlement_kind";
    Settlement& s = settlement_mut(id, what);
    if (static_cast<std::int64_t>(kind) < 1 || static_cast<std::int64_t>(kind) > 5) {
        fail("history: state: set_settlement_kind: kind is not a settlement kind");
    }
    require_event(event, what);
    s.kind = kind;
}

void State::set_settlement_status(std::int64_t id, SettlementStatus status, std::int64_t event) {
    const char* what = "set_settlement_status";
    Settlement& s = settlement_mut(id, what);
    if (static_cast<std::int64_t>(status) < 1 || static_cast<std::int64_t>(status) > 4) {
        fail("history: state: set_settlement_status: status is not a settlement status");
    }
    require_event(event, what);
    s.status = status;
    if (status == SettlementStatus::ruined) {
        s.ruined_year = engine_.year();
        s.ruin_event = event;
    } else {
        s.ruined_year.reset();
        s.ruin_event = 0;
    }
}

void State::set_population(std::int64_t id, std::int64_t population) {
    Settlement& s = settlement_mut(id, "set_population");
    if (population < 0 || population > MAX_POPULATION) {
        fail("history: bound: population exceeds " + int_text(MAX_POPULATION));
    }
    s.population = population;
}

void State::set_capital(std::int64_t civ, std::int64_t settlement) {
    Civilization& c = civ_mut(civ, "set_capital");
    if (settlement != 0) {
        (void)settlement_mut(settlement, "set_capital");
    }
    c.capital = settlement;
}

void State::end_civilization(std::int64_t civ, std::int64_t event) {
    const char* what = "end_civilization";
    require_live_civ(civ, what);
    require_event(event, what);
    Civilization& c = civs_[static_cast<std::size_t>(civ - 1)];
    c.ended_year = engine_.year();
    c.ending_event = event;
}

void State::open_relation(RelationKey key, std::int64_t event) {
    const char* what = "open_relation";
    if (!is_relation_kind(key.kind)) {
        fail("history: state: open_relation: kind '" + key.kind + "' is not of the definition id grammar");
    }
    const std::optional<EntityType> a = entity_type_from_code(key.a_type);
    const std::optional<EntityType> b = entity_type_from_code(key.b_type);
    if (!a || !b || !engine_.entity_exists(*a, key.a_id) || !engine_.entity_exists(*b, key.b_id)) {
        fail("history: state: open_relation: an entity of the relation does not exist");
    }
    require_event(event, what);
    if (relations_.find(key) != relations_.end()) {
        fail("history: state: open_relation: relation '" + key.kind + "' is already open");
    }
    relation_ops_.push_back(RelationOp{true, key, event});
    relations_.emplace(std::move(key), RelationState{event});
}

void State::close_relation(const RelationKey& key, std::int64_t event) {
    require_event(event, "close_relation");
    const auto it = relations_.find(key);
    if (it == relations_.end()) {
        fail("history: state: close_relation: relation '" + key.kind + "' is not open");
    }
    relations_.erase(it);
    relation_ops_.push_back(RelationOp{false, key, event});
}

// --- StepContext ---------------------------------------------------------------------------

std::uint64_t StepContext::serial() const noexcept {
    return engine_.serial();
}

std::int64_t StepContext::year() const noexcept {
    return engine_.year();
}

std::int64_t StepContext::years() const noexcept {
    return engine_.years();
}

std::int64_t StepContext::phase_id() const noexcept {
    return engine_.phase_id();
}

PhaseKind StepContext::phase_kind() const noexcept {
    return engine_.phase_kind();
}

const HistoryInputs& StepContext::inputs() const noexcept {
    return engine_.inputs();
}

const GeoView& StepContext::geo() const noexcept {
    return engine_.geo();
}

const data::ContentSet& StepContext::content() const noexcept {
    return engine_.content();
}

const State& StepContext::state() const noexcept {
    return engine_.state();
}

State& StepContext::state() noexcept {
    return engine_.state();
}

bool StepContext::visible(const Civilization& c) const noexcept {
    return c.created_step < engine_.serial();
}

bool StepContext::visible(const Settlement& s) const noexcept {
    return s.created_step < engine_.serial();
}

std::int64_t StepContext::append(const EventSpec& spec) {
    return engine_.append(spec);
}

std::string_view StepContext::event_kind(std::int64_t event) const {
    return engine_.event_kind(event);
}

std::int64_t StepContext::event_year(std::int64_t event) const {
    return engine_.event_year(event);
}

std::int64_t StepContext::event_participant(std::int64_t event, std::string_view role) const {
    return engine_.event_participant(event, role);
}

std::int64_t StepContext::quiet_events_left(std::int64_t civ) const {
    return engine_.quiet_events_left(civ);
}

std::uint64_t StepContext::draw(std::uint64_t stream_key, std::uint64_t a, std::uint64_t b,
                                std::uint64_t k) const noexcept {
    return history::draw(stream_key, static_cast<std::uint64_t>(engine_.year()), a, b, k);
}

// --- run_engine ------------------------------------------------------------------------------

HistoryResult run_engine(const data::ContentSet& content, const HistoryInputs& inputs, const EngineConfig& config) {
    if (config.awakening == nullptr || config.interventions == nullptr || config.catalogue == nullptr) {
        fail("history: engine: the configuration needs an awakening handler, an intervention source and a catalogue");
    }
    const EngineTuning tuning = check_engine_inputs(content, inputs);
    std::vector<std::int64_t> starts;
    for (const db::AgeRow& a : inputs.ages) {
        starts.push_back(a.start_year);
    }
    const BaseSchedule base = base_schedule(inputs.params.span, starts, tuning, inputs.params.detail);
    Engine engine(content, inputs, config, tuning, base);
    return engine.run();
}

}  // namespace worldgen::history
