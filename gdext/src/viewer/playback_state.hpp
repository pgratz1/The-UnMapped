#pragma once

// PlaybackState: the history movie of the viewer (spec P4-06 3 to 7). C++
// only, not registered with Godot, no Godot types; owned by ViewerModel
// beside HistoryState and BrowserState.
//
// It holds the playback session: the state (stopped, playing, paused,
// ended), the span [P0, P1], the speed and the skip-quiet flag, the stop
// years S_stop, the year y shown with the microseconds r already spent at
// it, the clock of the session, the ticker entries and the flashes. Its
// timing rule is 4.2: advance(dt) moves at most one year per call and never
// passes a stop year. It knows nothing of where its list comes from: the
// model hands it the playback list L (ids and years), the holder-change
// years and the phase starts (set_source), and builds the ticker entries
// and flashes of a year from worldgen's row texts and event places.
//
// No wall-clock time enters it: the only time source is the dt given to
// advance (P4-06 4.1).

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace unmapped {

// 3: speeds in years per second; each divides 1,000,000.
inline constexpr std::array<std::int64_t, 9> PLAYBACK_SPEEDS{1, 2, 5, 10, 20, 50, 100, 200, 500};
inline constexpr std::int64_t PLAYBACK_DEFAULT_SPEED = 10;
// 3: a quiet year takes D / Q with skip quiet years on (question 1).
inline constexpr std::int64_t PLAYBACK_QUIET_FACTOR = 10;
// 4.1: the largest dt the screen hands to advance.
inline constexpr std::int64_t PLAYBACK_MAX_DT = 250000;
// 6.1, 7.
inline constexpr std::size_t TICKER_LIMIT = 6;
inline constexpr std::size_t FLASH_LIMIT = 32;
inline constexpr std::int64_t FLASH_LIFE = 1200000;

enum class PlaybackStatus : std::uint8_t { stopped = 0, playing = 1, paused = 2, ended = 3 };

[[nodiscard]] const char* playback_status_word(PlaybackStatus status) noexcept;

// One event of the playback list L.
struct PlayEvent {
    std::int64_t id = 0;
    std::int64_t year = 0;
};

// One phase start (scope all only): its year and the year mark's label.
struct PlayPhase {
    std::int64_t year = 0;
    std::string label;
};

// What playback walks (3): L in ascending id, whether the scope is `all`
// (holder changes and phase starts are stops then), and the holder-change
// years and phase starts of the world.
struct PlaySource {
    std::vector<PlayEvent> events;
    bool scope_all = true;
    std::vector<std::int64_t> change_years;  // ascending
    std::vector<PlayPhase> phases;           // ascending year
};

// 6.1. kind: "phase", "event" or "more"; event 0 for phase and more.
struct TickerEntry {
    std::int64_t year = 0;
    std::int64_t event = 0;
    std::string kind;
    std::string text;
    friend bool operator==(const TickerEntry&, const TickerEntry&) = default;
};

// 7: a flash at a cell; born is the clock when it was added.
struct Flash {
    std::int64_t event = 0;
    std::int64_t x = 0, y = 0;
    std::uint32_t colour = 0xFFFFFF;  // 0xRRGGBB
    std::int64_t born = 0;
};

struct AdvanceResult {
    bool moved = false;     // the year changed (to year())
    bool finished = false;  // the call ended playback (ended)
    bool flashes_changed = false;
};

// 9.1, 5.1.
struct PlayOptions {
    bool play = false;
    std::int64_t from = -1, to = -1;
    std::int64_t speed = PLAYBACK_DEFAULT_SPEED;
    bool skip_quiet = true;
    bool quit_at_end = false;
    bool log = false;
    bool no_pane = false;
};

// 9.2. args: the user arguments after "--"; has_history, end_year: the open
// world's. Returns the options, or the first refusal text of 9.2. A span
// with a default end ("-") is checked against [0, E] only here; the screen
// checks it again once the default span is known.
[[nodiscard]] std::variant<PlayOptions, std::string> parse_play_options(std::span<const std::string> args,
                                                                        bool has_history, std::int64_t end_year);
// The refusal text of a span outside [0, E] or with from > to (9.2), with the
// values as given on the command line.
[[nodiscard]] std::string play_span_refusal(const std::string& from_text, const std::string& to_text,
                                            std::int64_t end_year);

class PlaybackState {
public:
    // Back to stopped with the default speed and skip, nothing held.
    void reset();

    [[nodiscard]] PlaybackStatus status() const noexcept { return status_; }
    [[nodiscard]] bool active() const noexcept { return status_ != PlaybackStatus::stopped; }
    [[nodiscard]] std::int64_t from() const noexcept { return from_; }
    [[nodiscard]] std::int64_t to() const noexcept { return to_; }
    [[nodiscard]] std::int64_t speed() const noexcept { return speed_; }
    [[nodiscard]] bool skip_quiet() const noexcept { return skip_quiet_; }
    [[nodiscard]] std::int64_t clock() const noexcept { return clock_; }
    [[nodiscard]] std::int64_t year() const noexcept { return y_; }
    [[nodiscard]] std::int64_t spent() const noexcept { return r_; }
    [[nodiscard]] const std::vector<std::int64_t>& stops() const noexcept { return stops_; }
    [[nodiscard]] const PlaySource& source() const noexcept { return source_; }

    // The list, holder changes and phases; the stops are recomputed for the
    // current span (the span is kept, 5.4).
    void set_source(PlaySource source);
    // 3: the default span for end year E.
    [[nodiscard]] std::array<std::int64_t, 2> default_span(std::int64_t end_year) const;

    // s in PLAYBACK_SPEEDS; false otherwise. r is kept (5.2).
    bool set_speed(std::int64_t s) noexcept;
    // d = +1 / -1: one place in PLAYBACK_SPEEDS, clamped; true when it moved.
    bool step_speed(std::int64_t d) noexcept;
    void set_skip_quiet(bool on) noexcept { skip_quiet_ = on; }

    // D and the dwell of year y (3).
    [[nodiscard]] std::int64_t dwell_full() const noexcept { return 1000000 / speed_; }
    [[nodiscard]] std::int64_t dwell(std::int64_t y) const noexcept;
    [[nodiscard]] bool is_stop(std::int64_t y) const noexcept;

    // A new session over [p0, p1] starting at `start` (4.4): playing, r = 0,
    // clock 0, ticker and flashes cleared, stops recomputed. The caller shows
    // the year and calls arrive.
    void start(std::int64_t p0, std::int64_t p1, std::int64_t start);
    // paused -> playing at the year shown, r = 0.
    void resume(std::int64_t y) noexcept;
    void pause() noexcept;
    // 5.3: a year change not made by playback: paused at y, r = 0, flashes
    // cleared (the caller refills the ticker).
    void pause_at(std::int64_t y);
    // stopped, everything of the session cleared (speed and skip kept).
    void stop();

    // 4.2. dt in microseconds (>= 0).
    AdvanceResult advance(std::int64_t dt);

    // The events of L with year y, ascending id.
    [[nodiscard]] std::span<const PlayEvent> events_of_year(std::int64_t y) const;
    // Whether y is the start year of a phase (with its labels) and the scope
    // is all.
    [[nodiscard]] std::vector<const PlayPhase*> phases_of_year(std::int64_t y) const;

    // --- Ticker (6.1) --------------------------------------------------------
    // Appends the entries of one year (given in order: phase lines, then the
    // events), applying the 5 + "... and <n> more" cut of a year with more
    // than TICKER_LIMIT, then dropping the oldest beyond TICKER_LIMIT. An
    // empty list leaves the ticker as it was. Returns whether it changed.
    bool add_ticker_year(std::int64_t y, std::vector<TickerEntry> entries);
    void clear_ticker() noexcept { ticker_.clear(); }
    [[nodiscard]] const std::vector<TickerEntry>& ticker() const noexcept { return ticker_; }

    // --- Flashes (7) ---------------------------------------------------------
    void add_flash(Flash flash);
    void clear_flashes() noexcept { flashes_.clear(); }
    [[nodiscard]] const std::vector<Flash>& flashes() const noexcept { return flashes_; }
    [[nodiscard]] std::int64_t flash_age(const Flash& flash) const noexcept { return clock_ - flash.born; }

private:
    void recompute_stops();
    bool prune_flashes();

    PlaybackStatus status_ = PlaybackStatus::stopped;
    std::int64_t from_ = 0, to_ = 0;
    std::int64_t speed_ = PLAYBACK_DEFAULT_SPEED;
    bool skip_quiet_ = true;
    std::int64_t y_ = 0;
    std::int64_t r_ = 0;
    std::int64_t clock_ = 0;
    PlaySource source_;
    std::vector<PlayEvent> by_year_;  // L sorted by (year, id)
    std::vector<std::int64_t> stops_;
    std::vector<TickerEntry> ticker_;
    std::vector<Flash> flashes_;
};

}  // namespace unmapped
