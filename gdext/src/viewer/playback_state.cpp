#include "viewer/playback_state.hpp"

#include <algorithm>
#include <utility>

namespace unmapped {

namespace {

// A decimal integer: digits only, at most 18 of them.
bool parse_year(const std::string& text, std::int64_t& out) {
    if (text.empty() || text.size() > 18) {
        return false;
    }
    std::int64_t v = 0;
    for (const char c : text) {
        if (c < '0' || c > '9') {
            return false;
        }
        v = v * 10 + (c - '0');
    }
    out = v;
    return true;
}

std::string speeds_text() {
    std::string out;
    for (std::size_t i = 0; i < PLAYBACK_SPEEDS.size(); ++i) {
        out += (i > 0 ? ", " : "") + std::to_string(PLAYBACK_SPEEDS[i]);
    }
    return out;
}

bool is_option(const std::string& arg) {
    return arg.size() > 2 && arg[0] == '-' && arg[1] == '-';
}

}  // namespace

const char* playback_status_word(PlaybackStatus status) noexcept {
    switch (status) {
        case PlaybackStatus::playing: return "playing";
        case PlaybackStatus::paused: return "paused";
        case PlaybackStatus::ended: return "ended";
        case PlaybackStatus::stopped: break;
    }
    return "stopped";
}

std::string play_span_refusal(const std::string& from_text, const std::string& to_text, std::int64_t end_year) {
    return "--play " + from_text + " " + to_text + ": not a span of this world (0 to " + std::to_string(end_year) + ")";
}

std::variant<PlayOptions, std::string> parse_play_options(std::span<const std::string> args, bool has_history,
                                                          std::int64_t end_year) {
    PlayOptions o;
    bool screenshot = false;
    bool bench = false;
    bool play_values = false;  // --play had two values
    std::string from_text;
    std::string to_text;
    std::string speed_text;
    bool speed_given = false;
    std::string skip_text;
    bool skip_given = false;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "--play") {
            o.play = true;
            if (i + 2 < args.size() && !is_option(args[i + 1]) && !is_option(args[i + 2])) {
                from_text = args[i + 1];
                to_text = args[i + 2];
                play_values = true;
                i += 2;
            }
        } else if (a == "--speed") {
            speed_given = true;
            if (i + 1 < args.size()) {
                speed_text = args[++i];
            }
        } else if (a == "--skip-quiet") {
            skip_given = true;
            if (i + 1 < args.size()) {
                skip_text = args[++i];
            }
        } else if (a == "--quit-at-end") {
            o.quit_at_end = true;
        } else if (a == "--play-log") {
            o.log = true;
        } else if (a == "--no-pane") {
            o.no_pane = true;
        } else if (a == "--screenshot") {
            screenshot = true;
        } else if (a == "--bench") {
            bench = true;
        }
    }
    // 9.2, in the order of its table (the history before the span: without
    // history no span is one of the world's).
    if (o.play) {
        if (!play_values) {
            return std::string("--play needs two values: a year or - for each end");
        }
        const std::string head = "--play " + from_text + " " + to_text + ": not a year: ";
        if (from_text == "-") {
            o.from = -1;
        } else if (!parse_year(from_text, o.from)) {
            return head + from_text;
        }
        if (to_text == "-") {
            o.to = -1;
        } else if (!parse_year(to_text, o.to)) {
            return head + to_text;
        }
        if (!has_history) {
            return std::string("--play: this world has no history");
        }
        if ((o.from >= 0 && o.from > end_year) || (o.to >= 0 && o.to > end_year) ||
            (o.from >= 0 && o.to >= 0 && o.from > o.to)) {
            return play_span_refusal(from_text, to_text, end_year);
        }
    }
    if (speed_given) {
        std::int64_t s = 0;
        if (!parse_year(speed_text, s) ||
            std::find(PLAYBACK_SPEEDS.begin(), PLAYBACK_SPEEDS.end(), s) == PLAYBACK_SPEEDS.end()) {
            return "--speed " + speed_text + ": not one of " + speeds_text();
        }
        o.speed = s;
    }
    if (skip_given) {
        if (skip_text == "on") {
            o.skip_quiet = true;
        } else if (skip_text == "off") {
            o.skip_quiet = false;
        } else {
            return "--skip-quiet " + skip_text + ": on or off";
        }
    }
    if (o.quit_at_end && !o.play) {
        return std::string("--quit-at-end needs --play");
    }
    if (o.log && !o.play) {
        return std::string("--play-log needs --play");
    }
    if (o.play && (screenshot || bench)) {
        return std::string("--play cannot be combined with --screenshot or --bench");
    }
    return o;
}

void PlaybackState::reset() {
    *this = PlaybackState{};
}

void PlaybackState::set_source(PlaySource source) {
    source_ = std::move(source);
    by_year_ = source_.events;
    std::stable_sort(by_year_.begin(), by_year_.end(), [](const PlayEvent& a, const PlayEvent& b) {
        return a.year != b.year ? a.year < b.year : a.id < b.id;
    });
    if (active()) {
        recompute_stops();
    }
}

std::array<std::int64_t, 2> PlaybackState::default_span(std::int64_t end_year) const {
    if (!source_.scope_all && !source_.events.empty()) {
        std::int64_t last = 0;
        for (const PlayEvent& e : source_.events) {
            last = std::max(last, e.year);
        }
        return {source_.events.front().year, last};
    }
    return {0, end_year};
}

bool PlaybackState::set_speed(std::int64_t s) noexcept {
    if (std::find(PLAYBACK_SPEEDS.begin(), PLAYBACK_SPEEDS.end(), s) == PLAYBACK_SPEEDS.end()) {
        return false;
    }
    speed_ = s;
    return true;
}

bool PlaybackState::step_speed(std::int64_t d) noexcept {
    const auto it = std::find(PLAYBACK_SPEEDS.begin(), PLAYBACK_SPEEDS.end(), speed_);
    auto i = static_cast<std::int64_t>(it - PLAYBACK_SPEEDS.begin());
    const auto last = static_cast<std::int64_t>(PLAYBACK_SPEEDS.size()) - 1;
    const std::int64_t j = std::clamp<std::int64_t>(i + (d > 0 ? 1 : d < 0 ? -1 : 0), 0, last);
    if (j == i) {
        return false;
    }
    i = j;
    speed_ = PLAYBACK_SPEEDS[static_cast<std::size_t>(i)];
    return true;
}

bool PlaybackState::is_stop(std::int64_t y) const noexcept {
    return std::binary_search(stops_.begin(), stops_.end(), y);
}

std::int64_t PlaybackState::dwell(std::int64_t y) const noexcept {
    const std::int64_t d = dwell_full();
    return !skip_quiet_ || is_stop(y) ? d : d / PLAYBACK_QUIET_FACTOR;
}

void PlaybackState::recompute_stops() {
    stops_.clear();
    const auto in_span = [this](std::int64_t y) { return y >= from_ && y <= to_; };
    for (const PlayEvent& e : by_year_) {
        if (in_span(e.year) && (stops_.empty() || stops_.back() != e.year)) {
            stops_.push_back(e.year);
        }
    }
    if (source_.scope_all) {
        for (const std::int64_t y : source_.change_years) {
            if (in_span(y)) {
                stops_.push_back(y);
            }
        }
        for (const PlayPhase& p : source_.phases) {
            if (in_span(p.year)) {
                stops_.push_back(p.year);
            }
        }
    }
    stops_.push_back(from_);
    stops_.push_back(to_);
    std::sort(stops_.begin(), stops_.end());
    stops_.erase(std::unique(stops_.begin(), stops_.end()), stops_.end());
}

void PlaybackState::start(std::int64_t p0, std::int64_t p1, std::int64_t start) {
    status_ = PlaybackStatus::playing;
    from_ = p0;
    to_ = p1;
    y_ = start;
    r_ = 0;
    clock_ = 0;
    ticker_.clear();
    flashes_.clear();
    recompute_stops();
}

void PlaybackState::resume(std::int64_t y) noexcept {
    status_ = PlaybackStatus::playing;
    y_ = y;
    r_ = 0;
}

void PlaybackState::pause() noexcept {
    if (status_ == PlaybackStatus::playing) {
        status_ = PlaybackStatus::paused;
    }
}

void PlaybackState::pause_at(std::int64_t y) {
    status_ = PlaybackStatus::paused;
    y_ = y;
    r_ = 0;
    flashes_.clear();
}

void PlaybackState::stop() {
    status_ = PlaybackStatus::stopped;
    r_ = 0;
    clock_ = 0;
    ticker_.clear();
    flashes_.clear();
    stops_.clear();
}

bool PlaybackState::prune_flashes() {
    const auto before = flashes_.size();
    std::erase_if(flashes_, [this](const Flash& f) { return clock_ - f.born >= FLASH_LIFE; });
    return flashes_.size() != before;
}

AdvanceResult PlaybackState::advance(std::int64_t dt) {
    AdvanceResult out;
    dt = std::max<std::int64_t>(dt, 0);
    if (status_ == PlaybackStatus::ended) {
        clock_ += dt;
        out.flashes_changed = prune_flashes();
        return out;
    }
    if (status_ != PlaybackStatus::playing) {
        return out;
    }
    clock_ += dt;
    r_ += dt;
    out.flashes_changed = prune_flashes();
    // 4.2, with each run of quiet years passed arithmetically (4.2 item 4):
    // a quiet year costs q = D / Q (or D with skip off) and is never a stop,
    // so k whole quiet years cost k q and the loop's result is unchanged.
    for (;;) {
        const std::int64_t d = dwell(y_);
        if (r_ < d) {
            break;
        }
        if (y_ == to_) {
            status_ = PlaybackStatus::ended;
            out.finished = true;
            break;
        }
        r_ -= d;
        ++y_;
        out.moved = true;
        if (is_stop(y_)) {
            r_ = 0;
            break;
        }
        // y_ is quiet; the next stop s > y_ exists (P1 is one).
        const auto next = std::upper_bound(stops_.begin(), stops_.end(), y_);
        const std::int64_t s = next != stops_.end() ? *next : to_;
        const std::int64_t q = dwell(y_);
        const std::int64_t k = std::min(r_ / q, s - y_);
        r_ -= k * q;
        y_ += k;
        if (y_ == s) {
            r_ = 0;
            break;
        }
    }
    return out;
}

std::span<const PlayEvent> PlaybackState::events_of_year(std::int64_t y) const {
    const auto lo = std::lower_bound(by_year_.begin(), by_year_.end(), y,
                                     [](const PlayEvent& e, std::int64_t key) { return e.year < key; });
    const auto hi = std::upper_bound(lo, by_year_.end(), y,
                                     [](std::int64_t key, const PlayEvent& e) { return key < e.year; });
    return {by_year_.data() + (lo - by_year_.begin()), static_cast<std::size_t>(hi - lo)};
}

std::vector<const PlayPhase*> PlaybackState::phases_of_year(std::int64_t y) const {
    std::vector<const PlayPhase*> out;
    if (source_.scope_all) {
        for (const PlayPhase& p : source_.phases) {
            if (p.year == y) {
                out.push_back(&p);
            }
        }
    }
    return out;
}

bool PlaybackState::add_ticker_year(std::int64_t y, std::vector<TickerEntry> entries) {
    if (entries.empty()) {
        return false;
    }
    if (entries.size() > TICKER_LIMIT) {
        const std::size_t keep = TICKER_LIMIT - 1;
        const std::size_t left = entries.size() - keep;
        entries.resize(keep);
        entries.push_back(TickerEntry{y, 0, "more", std::to_string(y) + "  ... and " + std::to_string(left) + " more"});
    }
    for (auto& e : entries) {
        ticker_.push_back(std::move(e));
    }
    if (ticker_.size() > TICKER_LIMIT) {
        ticker_.erase(ticker_.begin(), ticker_.begin() + static_cast<std::ptrdiff_t>(ticker_.size() - TICKER_LIMIT));
    }
    return true;
}

void PlaybackState::add_flash(Flash flash) {
    flash.born = clock_;
    flashes_.push_back(flash);
    if (flashes_.size() > FLASH_LIMIT) {
        flashes_.erase(flashes_.begin(), flashes_.begin() + static_cast<std::ptrdiff_t>(flashes_.size() - FLASH_LIMIT));
    }
}

}  // namespace unmapped
