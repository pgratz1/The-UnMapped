// ViewerModel: the history movie (spec P4-06 3 to 7). PlaybackState keeps
// the timing rule, the ticker and the flashes; this file hands it the
// browser's list (BrowserIndex::select without the year span), the
// timeline's holder-change years and the phase marks, builds the ticker
// entries from the browser's row texts and the flashes from MarkerIndex's
// event places, and converts everything into Godot values.

#include <algorithm>
#include <array>
#include <chrono>
#include <exception>
#include <string>
#include <string_view>
#include <optional>
#include <utility>
#include <vector>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/core/property_info.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "viewer/viewer_model.hpp"
#include "worldgen/history/event_catalogue.hpp"
#include "worldgen/inspect/markers.hpp"

namespace unmapped {

namespace {

godot::String to_godot(std::string_view text) {
    return godot::String::utf8(text.data(), static_cast<int64_t>(text.size()));
}

int64_t now_nsec() {
    return static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

}  // namespace

PlaySource ViewerModel::playback_source() const {
    PlaySource source;
    if (!history_.loaded()) {
        return source;
    }
    try {
        const auto& history = history_.history();
        if (browser_.available()) {
            // 3: the browser's filter with its year span open and This year
            // off.
            source.scope_all = browser_.filter.scope == BrowserScope::all;
            worldgen::inspect::EventFilter f;
            std::vector<std::int64_t> ids;
            if (browser_.event_filter(history_.year(), f)) {
                f.from_year = -1;
                f.to_year = -1;
                browser_.index().select(history, f, ids);
            }
            source.events.reserve(ids.size());
            for (const std::int64_t id : ids) {
                if (const auto* e = browser_.event(id)) {
                    source.events.push_back(PlayEvent{id, e->year});
                }
            }
        } else {
            // Without the browser: every event.
            for (const auto& e : history.view.events()) {
                source.events.push_back(PlayEvent{e.row.id, e.row.year});
            }
        }
        const auto changes = history.timeline.holder_change_years();
        source.change_years.assign(changes.begin(), changes.end());
        for (const YearMark& mark : history_.year_marks()) {
            if (mark.kind == "phase") {
                source.phases.push_back(PlayPhase{mark.year, mark.label});
            }
        }
    } catch (const std::exception& e) {
        godot::UtilityFunctions::push_error(godot::String("ViewerModel: playback: ") + e.what());
        source = {};
    }
    return source;
}

void ViewerModel::refresh_playback_source() {
    const int64_t start = now_nsec();
    playback_.set_source(playback_source());
    playback_timings_.source = now_nsec() - start;
    playback_timings_.slowest_source = std::max(playback_timings_.slowest_source, playback_timings_.source);
}

void ViewerModel::playback_set_year(int64_t year) {
    playback_year_ = true;
    set_year(year);
    playback_year_ = false;
}

bool ViewerModel::playback_arrive(int64_t year) {
    const int64_t start = now_nsec();
    std::vector<TickerEntry> entries;
    for (const PlayPhase* p : playback_.phases_of_year(year)) {
        entries.push_back(TickerEntry{year, 0, "phase", std::to_string(year) + "  " + p->label});
    }
    const auto events = playback_.events_of_year(year);
    for (const PlayEvent& e : events) {
        std::string text = browser_.available() ? browser_.row(e.id) : std::string();
        if (text.empty()) {
            // Without the browser (or a row it lacks): worldgen's sentence.
            try {
                text = std::to_string(year) + "  " + worldgen::history::render_event(history_.history().view, e.id);
            } catch (const std::exception&) {
                text = std::to_string(year) + "  e" + std::to_string(e.id);
            }
        }
        entries.push_back(TickerEntry{year, e.id, "event", std::move(text)});
    }
    const bool changed = playback_.add_ticker_year(year, std::move(entries));
    // 7: a flash at the place of each event of the year that has one.
    const worldgen::inspect::MarkerIndex* markers = history_.marker_index();
    if (markers != nullptr) {
        const auto& history = history_.history();
        const auto& view = history.view;
        for (const PlayEvent& e : events) {
            std::optional<std::array<std::int64_t, 2>> place;
            try {
                place = markers->event_place(history, e.id);
            } catch (const std::exception&) {
                place.reset();
            }
            if (!place) {
                continue;
            }
            std::uint32_t colour = 0xFFFFFF;
            if (e.id >= 1 && static_cast<std::size_t>(e.id) <= view.events().size()) {
                const auto& row = view.events()[static_cast<std::size_t>(e.id - 1)].row;
                if (const auto* kind = view.catalogue().find_id(row.kind)) {
                    const worldgen::inspect::Rgb c =
                        worldgen::inspect::event_symbol_colour(worldgen::inspect::event_symbol(kind->key));
                    colour = (std::uint32_t{c.r} << 16) | (std::uint32_t{c.g} << 8) | std::uint32_t{c.b};
                }
            }
            playback_.add_flash(Flash{e.id, (*place)[0], (*place)[1], colour, 0});
        }
    }
    playback_timings_.ticker = now_nsec() - start;
    return changed;
}

void ViewerModel::playback_outside_change() {
    const int64_t y = history_.year();
    playback_.pause_at(y);
    // A path step restores the selection before the year: the list of the
    // new selection.
    refresh_playback_source();
    playback_.clear_ticker();
    // 5.3: the ticker holds the entries of the new Y alone (no flashes).
    playback_arrive(y);
    playback_.clear_flashes();
    emit_signal("playback_changed");
    emit_signal("ticker_changed");
}

void ViewerModel::playback_start(int64_t p0, int64_t p1, int64_t start) {
    playback_.start(p0, p1, start);
    playback_set_year(start);
    playback_arrive(start);
    emit_signal("playback_changed");
    emit_signal("ticker_changed");
}

bool ViewerModel::play(int64_t from, int64_t to) {
    if (!history_.loaded()) {
        return false;
    }
    refresh_playback_source();
    const int64_t end = history_.end_year();
    const auto span = playback_.default_span(end);
    const int64_t p0 = from < 0 ? span[0] : from;
    const int64_t p1 = to < 0 ? span[1] : to;
    if (from < -1 || to < -1 || p0 < 0 || p1 > end || p0 > p1) {
        return false;
    }
    const int64_t y = history_.year();
    playback_start(p0, p1, p0 <= y && y < p1 ? y : p0);
    return true;
}

bool ViewerModel::toggle_playback() {
    if (!history_.loaded()) {
        return false;
    }
    const int64_t y = history_.year();
    switch (playback_.status()) {
        case PlaybackStatus::stopped:
            return play(-1, -1);
        case PlaybackStatus::playing:
            playback_.pause();
            emit_signal("playback_changed");
            return true;
        case PlaybackStatus::paused:
            if (playback_.from() <= y && y < playback_.to()) {
                playback_.resume(y);
                emit_signal("playback_changed");
            } else {
                playback_start(playback_.from(), playback_.to(), playback_.from());
            }
            return true;
        case PlaybackStatus::ended:
            playback_start(playback_.from(), playback_.to(), playback_.from());
            return true;
    }
    return false;
}

bool ViewerModel::stop_playback() {
    if (!history_.loaded()) {
        return false;
    }
    const bool was = playback_.active();
    playback_.stop();
    if (was) {
        emit_signal("playback_changed");
        emit_signal("ticker_changed");
    }
    return true;
}

bool ViewerModel::playback_advance(int64_t usec) {
    if (!history_.loaded()) {
        return false;
    }
    const int64_t start = now_nsec();
    const AdvanceResult result = playback_.advance(usec);
    int64_t year_nsec = 0;
    bool ticker = false;
    if (result.moved) {
        const int64_t before = now_nsec();
        playback_set_year(playback_.year());
        year_nsec = now_nsec() - before;
        ticker = playback_arrive(playback_.year());
    } else {
        playback_timings_.ticker = 0;
    }
    playback_timings_.advance = now_nsec() - start - year_nsec;
    // The entries' `current` flags follow Y, so a year change with entries
    // shown changes the ticker too.
    if (ticker || (result.moved && !playback_.ticker().empty())) {
        emit_signal("ticker_changed");
    }
    if (result.finished) {
        emit_signal("playback_changed");
        emit_signal("playback_finished");
    }
    return result.moved;
}

godot::Dictionary ViewerModel::get_playback() const {
    godot::Dictionary out;
    const bool loaded = history_.loaded();
    std::array<std::int64_t, 2> span{-1, -1};
    if (loaded) {
        if (playback_.active()) {
            span = {playback_.from(), playback_.to()};
        } else {
            // The default span of the current filter (3), without touching
            // the session.
            PlaybackState probe;
            probe.set_source(playback_source());
            span = probe.default_span(history_.end_year());
        }
    }
    out["state"] = playback_status_word(loaded ? playback_.status() : PlaybackStatus::stopped);
    out["from"] = span[0];
    out["to"] = span[1];
    out["speed"] = playback_.speed();
    out["skip_quiet"] = playback_.skip_quiet();
    out["clock"] = playback_.clock();
    out["stops"] = static_cast<int64_t>(playback_.stops().size());
    return out;
}

godot::PackedInt64Array ViewerModel::get_playback_speeds() const {
    godot::PackedInt64Array out;
    for (const std::int64_t s : PLAYBACK_SPEEDS) {
        out.push_back(s);
    }
    return out;
}

bool ViewerModel::set_playback_speed(int64_t speed) {
    if (!history_.loaded()) {
        return false;
    }
    const int64_t before = playback_.speed();
    if (!playback_.set_speed(speed)) {
        return false;
    }
    if (before != speed) {
        emit_signal("playback_changed");
    }
    return true;
}

bool ViewerModel::step_playback_speed(int64_t delta) {
    if (!history_.loaded()) {
        return false;
    }
    if (playback_.step_speed(delta)) {
        emit_signal("playback_changed");
        return true;
    }
    return false;
}

bool ViewerModel::set_playback_skip_quiet(bool on) {
    if (!history_.loaded()) {
        return false;
    }
    if (playback_.skip_quiet() != on) {
        playback_.set_skip_quiet(on);
        emit_signal("playback_changed");
    }
    return true;
}

godot::PackedInt64Array ViewerModel::get_playback_stop_years() const {
    godot::PackedInt64Array out;
    if (history_.loaded()) {
        for (const std::int64_t y : playback_.stops()) {
            out.push_back(y);
        }
    }
    return out;
}

godot::Array ViewerModel::get_ticker_lines() const {
    godot::Array out;
    if (!history_.loaded() || !playback_.active()) {
        return out;
    }
    const int64_t y = history_.year();
    for (const TickerEntry& t : playback_.ticker()) {
        godot::Dictionary row;
        row["year"] = t.year;
        row["event"] = t.event;
        row["kind"] = to_godot(t.kind);
        row["text"] = to_godot(t.text);
        row["current"] = t.year == y;
        out.push_back(row);
    }
    return out;
}

godot::Array ViewerModel::get_flashes() const {
    godot::Array out;
    if (!history_.loaded()) {
        return out;
    }
    for (const Flash& f : playback_.flashes()) {
        godot::Dictionary row;
        row["event"] = f.event;
        row["x"] = f.x;
        row["y"] = f.y;
        row["colour"] = static_cast<int64_t>(f.colour);
        row["age"] = playback_.flash_age(f);
        row["life"] = FLASH_LIFE;
        out.push_back(row);
    }
    return out;
}

void ViewerModel::bind_playback_methods() {
    using godot::ClassDB;
    using godot::D_METHOD;
    using godot::MethodInfo;
    ClassDB::bind_method(D_METHOD("play", "from", "to"), &ViewerModel::play);
    ClassDB::bind_method(D_METHOD("toggle_playback"), &ViewerModel::toggle_playback);
    ClassDB::bind_method(D_METHOD("stop_playback"), &ViewerModel::stop_playback);
    ClassDB::bind_method(D_METHOD("playback_advance", "usec"), &ViewerModel::playback_advance);
    ClassDB::bind_method(D_METHOD("get_playback"), &ViewerModel::get_playback);
    ClassDB::bind_method(D_METHOD("get_playback_speeds"), &ViewerModel::get_playback_speeds);
    ClassDB::bind_method(D_METHOD("set_playback_speed", "speed"), &ViewerModel::set_playback_speed);
    ClassDB::bind_method(D_METHOD("step_playback_speed", "delta"), &ViewerModel::step_playback_speed);
    ClassDB::bind_method(D_METHOD("set_playback_skip_quiet", "on"), &ViewerModel::set_playback_skip_quiet);
    ClassDB::bind_method(D_METHOD("get_playback_stop_years"), &ViewerModel::get_playback_stop_years);
    ClassDB::bind_method(D_METHOD("get_ticker_lines"), &ViewerModel::get_ticker_lines);
    ClassDB::bind_method(D_METHOD("get_flashes"), &ViewerModel::get_flashes);
    ADD_SIGNAL(MethodInfo("playback_changed"));
    ADD_SIGNAL(MethodInfo("playback_finished"));
    ADD_SIGNAL(MethodInfo("ticker_changed"));
}

}  // namespace unmapped
