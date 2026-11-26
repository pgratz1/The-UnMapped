#include "viewer/viewer_check_playback.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <variant>
#include <vector>

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/check_box.hpp>
#include <godot_cpp/classes/container.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/h_slider.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/spin_box.hpp>
#include <godot_cpp/classes/tab_container.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/char_string.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include "viewer/playback_state.hpp"
#include "viewer/viewer_check_reference.hpp"
#include "viewer/viewer_screen.hpp"
#include "worldgen/inspect/history_browser.hpp"
#include "worldgen/inspect/markers.hpp"

namespace unmapped {

using namespace check_ref;

namespace {

godot::String to_godot(const std::string& text) {
    return godot::String::utf8(text.data(), static_cast<int64_t>(text.size()));
}

std::string to_utf8(const godot::String& text) {
    const godot::CharString utf8 = text.utf8();
    return {utf8.get_data(), static_cast<std::size_t>(utf8.length())};
}

godot::String num(int64_t n) {
    return godot::String::num_int64(n);
}

godot::String tail(const std::string& problem) {
    return problem.empty() ? godot::String() : ": " + to_godot(problem);
}

// --- Signal counters -----------------------------------------------------------

int64_t g_years = 0;
int64_t g_finished = 0;
void count_year(int64_t /*year*/) {
    ++g_years;
}
void count_finished() {
    ++g_finished;
}

void send_key(ViewerScreen& screen, godot::Key key, bool shift) {
    for (const bool pressed : {true, false}) {
        godot::Ref<godot::InputEventKey> event;
        event.instantiate();
        event->set_keycode(key);
        event->set_physical_keycode(key);
        event->set_shift_pressed(shift);
        event->set_pressed(pressed);
        screen.get_viewport()->push_input(event);
    }
}

// --- The timing reference (4.2, the check's own copy) ----------------------------

struct RefPlayer {
    std::vector<int64_t> stops;  // sorted
    int64_t p0 = 0, p1 = 0, speed = 10;
    bool skip = true;
    int64_t y = 0, r = 0;
    bool ended = false;

    [[nodiscard]] bool stop(int64_t v) const { return std::binary_search(stops.begin(), stops.end(), v); }
    [[nodiscard]] int64_t dwell(int64_t v) const {
        const int64_t d = 1000000 / speed;
        return skip && !stop(v) ? d / 10 : d;
    }
    // 0: nothing, 1: moved, 2: finished.
    int advance(int64_t dt) {
        if (ended) {
            return 0;
        }
        r += dt;
        bool moved = false;
        for (;;) {
            if (r < dwell(y)) {
                break;
            }
            if (y == p1) {
                ended = true;
                return 2;
            }
            r -= dwell(y);
            ++y;
            moved = true;
            if (stop(y)) {
                r = 0;
                break;
            }
        }
        return moved ? 1 : 0;
    }
};

std::vector<int64_t> ref_stops(const std::vector<int64_t>& event_years, bool scope_all,
                               const std::vector<int64_t>& changes, const std::vector<int64_t>& phases, int64_t p0,
                               int64_t p1) {
    std::set<int64_t> s{p0, p1};
    const auto add = [&](const std::vector<int64_t>& v) {
        for (const int64_t y : v) {
            if (y >= p0 && y <= p1) {
                s.insert(y);
            }
        }
    };
    add(event_years);
    if (scope_all) {
        add(changes);
        add(phases);
    }
    return {s.begin(), s.end()};
}

std::vector<int64_t> years_of(const std::vector<const ChronicleLine*>& lines) {
    std::vector<int64_t> out;
    for (const ChronicleLine* e : lines) {
        out.push_back(e->start);
    }
    return out;
}

// [phases] of history.txt: phase id -> start year.
std::map<int64_t, int64_t> read_phase_starts(const godot::String& path) {
    std::map<int64_t, int64_t> out;
    const std::string text = to_utf8(godot::FileAccess::get_file_as_string(path));
    static const std::regex phase_re(R"(^p([0-9]+) a[0-9]+ [a-z_]+ ([0-9]+))");
    std::string section;
    std::size_t start = 0;
    while (start < text.size()) {
        std::size_t stop = text.find('\n', start);
        if (stop == std::string::npos) {
            stop = text.size();
        }
        const std::string line = text.substr(start, stop - start);
        start = stop + 1;
        if (line.size() > 2 && line.front() == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            continue;
        }
        std::smatch m;
        if (section == "phases" && std::regex_search(line, m, phase_re)) {
            out[std::stoll(m[1])] = std::stoll(m[2]);
        }
    }
    return out;
}

// The ticker entries of the year shown (current), in order.
std::vector<godot::Dictionary> current_entries(ViewerModel& model) {
    std::vector<godot::Dictionary> out;
    const godot::Array lines = model.get_ticker_lines();
    for (int64_t i = 0; i < lines.size(); ++i) {
        const godot::Dictionary d = lines[i];
        if (bool(d["current"])) {
            out.push_back(d);
        }
    }
    return out;
}

// What the check expects at an arrival at year y (6.1): the phase lines
// (scope all) and the events of y, ascending id, cut to 5 + "more" above 6.
struct WantEntry {
    std::string kind;
    int64_t event = 0;
    std::string text;    // exact for events and "more"
    std::string prefix;  // for a phase line: "<y>  phase p<id> "
};

std::vector<WantEntry> want_entries(int64_t y, const std::vector<int64_t>& phase_ids, const std::vector<int64_t>& events,
                                    const std::map<int64_t, std::string>& rows) {
    std::vector<WantEntry> out;
    for (const int64_t p : phase_ids) {
        out.push_back(WantEntry{"phase", 0, "", std::to_string(y) + "  phase p" + std::to_string(p) + " "});
    }
    for (const int64_t e : events) {
        const auto it = rows.find(e);
        out.push_back(WantEntry{"event", e, it != rows.end() ? it->second : std::string("?"), ""});
    }
    if (out.size() > TICKER_LIMIT) {
        const std::size_t left = out.size() - (TICKER_LIMIT - 1);
        out.resize(TICKER_LIMIT - 1);
        out.push_back(WantEntry{"more", 0, std::to_string(y) + "  ... and " + std::to_string(left) + " more", ""});
    }
    return out;
}

std::string compare_entries(int64_t y, const std::vector<godot::Dictionary>& got, const std::vector<WantEntry>& want) {
    if (got.size() != want.size()) {
        return "year " + std::to_string(y) + ": " + std::to_string(got.size()) + " entries, want " +
               std::to_string(want.size());
    }
    for (std::size_t i = 0; i < got.size(); ++i) {
        const std::string kind = to_utf8(got[i]["kind"]);
        const int64_t event = got[i]["event"];
        const std::string text = to_utf8(got[i]["text"]);
        const int64_t year = got[i]["year"];
        const WantEntry& w = want[i];
        const bool same = kind == w.kind && event == w.event && year == y &&
                          (w.kind == "phase" ? text.rfind(w.prefix, 0) == 0 : text == w.text);
        if (!same) {
            return "year " + std::to_string(y) + " entry " + std::to_string(i) + ": " + kind + " e" +
                   std::to_string(event) + " '" + text + "', want " + w.kind + " e" + std::to_string(w.event) + " '" +
                   (w.kind == "phase" ? w.prefix : w.text) + "'";
        }
    }
    return {};
}

// Sends NOTIFICATION_SORT_CHILDREN to every container, parents first, so that
// a new screen size is laid out without waiting for a frame.
void settle(godot::Node* node) {
    if (auto* c = godot::Object::cast_to<godot::Container>(node)) {
        c->notification(godot::Container::NOTIFICATION_SORT_CHILDREN);
    }
    for (int32_t i = 0; i < node->get_child_count(); ++i) {
        settle(node->get_child(i));
    }
}

bool inside(const godot::Rect2& outer, const godot::Rect2& inner) {
    constexpr float slack = 0.5F;
    return inner.position.x >= outer.position.x - slack && inner.position.y >= outer.position.y - slack &&
           inner.position.x + inner.size.x <= outer.position.x + outer.size.x + slack &&
           inner.position.y + inner.size.y <= outer.position.y + outer.size.y + slack;
}

}  // namespace

// --- P0 -------------------------------------------------------------------------

void check_playback_without_history(ViewerScreen& screen, const CheckExpect& expect) {
    godot::Ref<ViewerModel> model = screen.get_model();
    const bool disabled = screen.play_button()->is_disabled() && screen.play_speed()->is_disabled() &&
                          screen.play_skip_quiet()->is_disabled();
    const bool methods = !model->play(-1, -1) && !model->toggle_playback() && !model->stop_playback() &&
                         !model->playback_advance(100000) && !model->set_playback_speed(20) &&
                         godot::String(model->get_playback()["state"]) == "stopped" &&
                         model->get_ticker_lines().is_empty() && model->get_flashes().is_empty() &&
                         model->get_playback_stop_years().is_empty();
    screen.get_viewport()->gui_release_focus();
    const godot::Dictionary before = model->get_playback();
    const bool shown_before = screen.ticker_shown();
    for (const godot::Key key : {godot::KEY_SPACE, godot::KEY_Q, godot::KEY_MINUS, godot::KEY_EQUAL, godot::KEY_T}) {
        send_key(screen, key, false);
    }
    const godot::Dictionary after = model->get_playback();
    const bool keys = before == after && screen.ticker_shown() == shown_before;
    expect(disabled && methods && keys && !screen.ticker()->is_visible(),
           godot::String("P0 without history: the play controls are disabled (") + (disabled ? "ok" : "no") +
               "), play, toggle and the other methods answer false or empty and the state is stopped (" +
               (methods ? "ok" : "no") + "), Space, Q, -, = and T do nothing (" + (keys ? "ok" : "no") +
               "), the ticker is hidden");
}

// --- P1 -------------------------------------------------------------------------

void check_playback_at_open(ViewerScreen& screen, const CheckExpect& expect) {
    godot::Ref<ViewerModel> model = screen.get_model();
    const godot::Dictionary p = model->get_playback();
    const godot::PackedInt64Array speeds = model->get_playback_speeds();
    bool speeds_ok = speeds.size() == static_cast<int64_t>(PLAYBACK_SPEEDS.size());
    const int64_t want_speeds[] = {1, 2, 5, 10, 20, 50, 100, 200, 500};
    for (int64_t i = 0; i < speeds.size() && speeds_ok; ++i) {
        speeds_ok = speeds[i] == want_speeds[i];
    }
    const int64_t end = model->get_end_year();
    const bool ok = godot::String(p["state"]) == "stopped" && int64_t(p["speed"]) == 10 && bool(p["skip_quiet"]) &&
                    screen.ticker_shown() && speeds_ok && int64_t(p["from"]) == 0 && int64_t(p["to"]) == end &&
                    !screen.play_button()->is_disabled() && screen.play_button()->get_text() == "Play" &&
                    screen.play_speed()->get_item_text(screen.play_speed()->get_selected()) == "10 y/s" &&
                    screen.play_skip_quiet()->is_pressed() && !screen.ticker()->is_visible();
    expect(ok, "P1 at open: stopped, speed 10, skip on, ticker toggle on, speeds 1 to 500, default span [0, " +
                   num(end) + "] (" + godot::Variant(p).stringify() + ")");
}

// --- P2 to P10 ------------------------------------------------------------------

void check_playback(ViewerScreen& screen, const ViewerCheckOptions& options, const CheckExpect& expect) {
    godot::Ref<ViewerModel> model = screen.get_model();
    const BrowserReport full = read_browser_report(options.history_report, model->get_world_path());
    const BrowserReport summary = read_browser_report(options.history_summary, model->get_world_path());
    OwnWorld own = open_own_world(model->get_world_path());
    const std::map<int64_t, int64_t> phase_starts = read_phase_starts(options.history_report);
    if (!full.problem.empty() || !summary.problem.empty() || !own.problem.empty() || phase_starts.empty() ||
        !model->get_browser_available()) {
        expect(false, "P the references: " + to_godot(full.problem + " " + summary.problem + " " + own.problem) +
                          (phase_starts.empty() ? " (no [phases])" : ""));
        return;
    }
    const int64_t end = model->get_end_year();
    const auto changes_span = own.history->timeline.holder_change_years();
    const std::vector<int64_t> changes(changes_span.begin(), changes_span.end());
    std::vector<int64_t> phase_years;
    std::map<int64_t, std::vector<int64_t>> phases_at;  // year -> phase ids
    for (const auto& [id, year] : phase_starts) {
        phase_years.push_back(year);
        phases_at[year].push_back(id);
    }
    std::vector<const ChronicleLine*> summary_lines;
    for (const auto& e : summary.chronicle) {
        summary_lines.push_back(&e);
    }
    std::vector<const ChronicleLine*> full_lines;
    std::map<int64_t, const ChronicleLine*> line_of;
    for (const auto& e : full.chronicle) {
        full_lines.push_back(&e);
        line_of[e.id] = &e;
    }
    const godot::Callable on_year = callable_mp_static(&count_year);
    const godot::Callable on_finished = callable_mp_static(&count_finished);
    model->connect("year_changed", on_year);
    model->connect("playback_finished", on_finished);
    screen.choose_view(model->get_territory_view());
    screen.right_pane()->set_visible(true);

    // A run of the model against the reference: same (call, Y) changes, at
    // most one year change per call, the same ending call. stop_call: stop
    // after this many calls (0: to the end). on_move: called after each
    // year change (and once at the start with call 0).
    struct RunOut {
        std::string problem;
        std::vector<std::pair<int64_t, int64_t>> changes;
        int64_t end_call = -1;
        int64_t calls = 0;
    };
    const auto run = [&](RefPlayer& ref, int64_t dt, int64_t stop_call, const auto& on_move) {
        RunOut out;
        for (int64_t call = 1; call <= 400000; ++call) {
            if (stop_call > 0 && call > stop_call) {
                break;
            }
            g_years = 0;
            const int64_t finished_before = g_finished;
            const int got = ref.advance(dt);
            const bool moved = model->playback_advance(dt);
            out.calls = call;
            if (g_years > 1 && out.problem.empty()) {
                out.problem = "call " + std::to_string(call) + ": " + std::to_string(g_years) + " year changes";
            }
            if (moved != (got == 1) || model->get_year() != ref.y) {
                if (out.problem.empty()) {
                    out.problem = "call " + std::to_string(call) + ": Y " + std::to_string(model->get_year()) +
                                  (moved ? " (moved)" : "") + ", the reference " + std::to_string(ref.y) +
                                  (got == 1 ? " (moved)" : "");
                }
                break;
            }
            if (moved) {
                out.changes.emplace_back(call, ref.y);
                on_move(call);
            }
            const bool ended = godot::String(model->get_playback()["state"]) == "ended";
            if (got == 2 || ended) {
                if ((got == 2) != ended || g_finished != finished_before + 1) {
                    out.problem = out.problem.empty() ? "call " + std::to_string(call) + ": ended " +
                                                            (ended ? "yes" : "no") + ", the reference " +
                                                            (got == 2 ? "yes" : "no")
                                                      : out.problem;
                }
                out.end_call = call;
                break;
            }
        }
        return out;
    };
    const auto no_move = [](int64_t) {};
    const auto stops_equal = [&](const std::vector<int64_t>& want) -> std::string {
        const godot::PackedInt64Array got = model->get_playback_stop_years();
        std::vector<int64_t> g(got.ptr(), got.ptr() + got.size());
        if (g == want) {
            return {};
        }
        for (std::size_t i = 0; i < std::max(g.size(), want.size()); ++i) {
            if (i >= g.size() || i >= want.size() || g[i] != want[i]) {
                return "stop " + std::to_string(i) + ": " + (i < g.size() ? std::to_string(g[i]) : "none") +
                       ", want " + (i < want.size() ? std::to_string(want[i]) : "none") + " (" +
                       std::to_string(g.size()) + " stops, want " + std::to_string(want.size()) + ")";
            }
        }
        return "stops differ";
    };
    // Sets up scope all at a level and starts a session over [p0, p1] from p0.
    const auto start_all = [&](const char* level, int64_t speed, bool skip, int64_t p0, int64_t p1) {
        model->stop_playback();
        model->clear_selection();
        model->set_browser_group("all");
        model->set_browser_level(level);
        model->set_year(p1 < end ? p1 : end);  // outside [p0, p1): the session starts at p0
        model->set_playback_speed(speed);
        model->set_playback_skip_quiet(skip);
        return model->play(p0, p1);
    };

    // P2: plain years.
    {
        const int64_t p1 = std::min<int64_t>(120, end);
        const bool started = start_all("summary", 10, false, 0, p1);
        bool ok = started && model->get_year() == 0;
        std::string problem;
        g_finished = 0;
        for (int64_t k = 1; k <= p1 && ok; ++k) {
            g_years = 0;
            const bool moved = model->playback_advance(100000);
            if (!moved || model->get_year() != k || g_years != 1) {
                ok = false;
                problem = "call " + std::to_string(k) + ": Y " + std::to_string(model->get_year());
            }
        }
        g_years = 0;
        const bool last = model->playback_advance(100000);
        const bool ended = godot::String(model->get_playback()["state"]) == "ended";
        const bool later = !model->playback_advance(100000) && !model->playback_advance(100000);
        ok = ok && !last && ended && g_finished == 1 && later && g_years == 0 && model->get_year() == p1;
        expect(ok, "P2 play(0, " + num(p1) + ") at speed 10, skip off: call k sets Y = k with one year_changed, call " +
                       num(p1 + 1) + " ends playback with one playback_finished, later calls change nothing" +
                       tail(problem));
    }

    // P3: timing against the reference, scope all, level summary.
    struct Case {
        const char* name;
        int64_t speed;
        bool skip;
        int64_t p1;
        int64_t dt;
    };
    const Case cases[] = {{"C1", 10, false, std::min<int64_t>(300, end), 33333},
                          {"C2", 50, true, end, 33333},
                          {"C3", 500, true, end, 16667},
                          {"C4", 500, false, end, 33333}};
    const std::vector<int64_t> summary_years = years_of(summary_lines);
    for (const Case& c : cases) {
        std::string problem;
        if (!start_all("summary", c.speed, c.skip, 0, c.p1)) {
            problem = "play failed";
        }
        RefPlayer ref;
        ref.stops = ref_stops(summary_years, true, changes, phase_years, 0, c.p1);
        ref.p0 = 0;
        ref.p1 = c.p1;
        ref.speed = c.speed;
        ref.skip = c.skip;
        if (problem.empty()) {
            problem = stops_equal(ref.stops);
        }
        RunOut out;
        if (problem.empty()) {
            out = run(ref, c.dt, 0, no_move);
            problem = out.problem;
        }
        if (problem.empty()) {
            std::set<int64_t> seen{0};
            for (const auto& [call, y] : out.changes) {
                seen.insert(y);
            }
            for (const int64_t s : ref.stops) {
                if (seen.count(s) == 0 && problem.empty()) {
                    problem = "stop year " + std::to_string(s) + " never shown";
                }
            }
            if (out.end_call < 0 && problem.empty()) {
                problem = "playback did not end";
            }
        }
        expect(problem.empty(), godot::String("P3 ") + c.name + " (speed " + num(c.speed) + ", skip " +
                                    (c.skip ? "on" : "off") + ", [0, " + num(c.p1) + "], dt " + num(c.dt) + "): " +
                                    num(static_cast<int64_t>(ref.stops.size())) + " stops, " +
                                    num(static_cast<int64_t>(out.changes.size())) + " year changes, ended at call " +
                                    num(out.end_call) + " as the reference" + tail(problem));
    }

    // P4: a civilization's story (the civilization of B5: the first that
    // ended), level full; then its wars.
    {
        int64_t civ = 0;
        for (const auto& b : full.blocks) {
            if (b.ended >= 0 && civ == 0) {
                civ = b.id;
            }
        }
        std::vector<int64_t> story;  // B3's list: a participant =c<civ>
        const std::string tag = "=c" + std::to_string(civ);
        for (const auto& e : full.chronicle) {
            for (const auto& r : e.refs) {
                if (r.size() > tag.size() && r.compare(r.size() - tag.size(), tag.size(), tag) == 0 && r[0] != '@') {
                    story.push_back(e.id);
                    break;
                }
            }
        }
        // The war events of the story, from the check's own index.
        int war = -1;
        for (std::uint32_t g = 0; g < 32U; ++g) {
            if ((worldgen::inspect::ALL_GROUPS & (std::uint32_t{1} << g)) != 0U &&
                worldgen::inspect::event_group_word(static_cast<worldgen::inspect::EventGroup>(g)) == "war") {
                war = static_cast<int>(g);
            }
        }
        std::vector<int64_t> wars;
        if (war >= 0) {
            worldgen::inspect::EventFilter f;
            f.civilization = civ;
            f.groups = std::uint32_t{1} << war;
            f.summary_only = false;
            own.browser->select(*own.history, f, wars);
        }
        for (const bool war_only : {false, true}) {
            const std::vector<int64_t>& list = war_only ? wars : story;
            std::string problem;
            model->stop_playback();
            model->clear_selection();
            model->set_browser_group("all");
            model->set_browser_level("full");
            model->select_civilization(civ);
            if (war_only) {
                model->set_browser_group("war");
            }
            model->set_playback_speed(50);
            model->set_playback_skip_quiet(true);
            model->set_year(end);
            if (list.empty()) {
                expect(war_only, "P4 c" + num(civ) + " has events");
                continue;
            }
            int64_t first = line_of.count(list.front()) != 0 ? line_of.at(list.front())->start : -1;
            int64_t last = 0;
            std::map<int64_t, std::vector<int64_t>> by_year;
            std::vector<int64_t> years;
            for (const int64_t id : list) {
                const int64_t y = line_of.count(id) != 0 ? line_of.at(id)->start : -1;
                last = std::max(last, y);
                by_year[y].push_back(id);
                years.push_back(y);
            }
            const bool started = model->play(-1, -1);
            const godot::Dictionary p = model->get_playback();
            if (!started || int64_t(p["from"]) != first || int64_t(p["to"]) != last || model->get_year() != first) {
                problem = "span [" + std::to_string(int64_t(p["from"])) + ", " + std::to_string(int64_t(p["to"])) +
                          "], want [" + std::to_string(first) + ", " + std::to_string(last) + "]";
            }
            RefPlayer ref;
            ref.stops = ref_stops(years, false, changes, phase_years, first, last);
            ref.p0 = first;
            ref.p1 = last;
            ref.speed = 50;
            ref.skip = true;
            ref.y = first;
            if (problem.empty()) {
                problem = stops_equal(ref.stops);
            }
            std::map<int64_t, std::string> rows;
            const godot::PackedInt64Array ids = model->get_event_ids();
            const godot::PackedStringArray texts = model->get_event_rows();
            for (int64_t i = 0; i < ids.size() && i < texts.size(); ++i) {
                rows[ids[i]] = to_utf8(texts[i]);
            }
            std::vector<int64_t> want_seen;
            std::vector<int64_t> seen;
            const auto arrival = [&](int64_t /*call*/) {
                const int64_t y = model->get_year();
                const auto it = by_year.find(y);
                const std::vector<WantEntry> want =
                    want_entries(y, {}, it != by_year.end() ? it->second : std::vector<int64_t>{}, rows);
                for (const WantEntry& w : want) {
                    if (w.kind == "event") {
                        want_seen.push_back(w.event);
                    }
                }
                const std::vector<godot::Dictionary> got = current_entries(*model.ptr());
                for (const auto& d : got) {
                    if (godot::String(d["kind"]) == "event") {
                        seen.push_back(d["event"]);
                    }
                }
                if (problem.empty()) {
                    problem = compare_entries(y, got, want);
                }
            };
            arrival(0);
            RunOut out;
            if (problem.empty()) {
                out = run(ref, 33333, 0, arrival);
                problem = problem.empty() ? out.problem : problem;
            }
            if (problem.empty() && (seen != want_seen || out.end_call < 0)) {
                problem = "the ticker's events differ from the list (" + std::to_string(seen.size()) + " seen, " +
                          std::to_string(want_seen.size()) + " wanted)";
            }
            std::set<int64_t> distinct(seen.begin(), seen.end());
            if (problem.empty() && distinct.size() != seen.size()) {
                problem = "an event appears twice";
            }
            expect(problem.empty(),
                   "P4 c" + num(civ) + (war_only ? godot::String(" with group war") : godot::String()) +
                       ": play(-1, -1) spans [" + num(first) + ", " + num(last) + "], stops are the list's " +
                       num(static_cast<int64_t>(ref.stops.size())) + " years and the span ends, the ticker shows its " +
                       num(static_cast<int64_t>(list.size())) + " events in order, once each" + tail(problem));
        }
    }

    // P5 and P6: the ticker at both levels and the flashes (summary), C2.
    bool cut_seen = false;
    for (const char* level : {"summary", "full"}) {
        const bool is_full = std::string(level) == "full";
        const std::vector<const ChronicleLine*>& lines = is_full ? full_lines : summary_lines;
        std::map<int64_t, std::vector<int64_t>> by_year;
        for (const ChronicleLine* e : lines) {
            by_year[e->start].push_back(e->id);
        }
        std::string problem;
        std::string flash_problem;
        if (!start_all(level, 50, true, 0, end)) {
            problem = "play failed";
        }
        std::map<int64_t, std::string> rows;
        const godot::PackedInt64Array ids = model->get_event_ids();
        const godot::PackedStringArray texts = model->get_event_rows();
        for (int64_t i = 0; i < ids.size() && i < texts.size(); ++i) {
            rows[ids[i]] = to_utf8(texts[i]);
        }
        RefPlayer ref;
        ref.stops = ref_stops(years_of(lines), true, changes, phase_years, 0, end);
        ref.p0 = 0;
        ref.p1 = end;
        ref.speed = 50;
        ref.skip = true;
        int64_t arrivals = 0;
        int64_t flashes_added = 0;
        const auto arrival = [&](int64_t /*call*/) {
            ++arrivals;
            const int64_t y = model->get_year();
            const auto ev = by_year.find(y);
            const auto ph = phases_at.find(y);
            const std::vector<int64_t> events = ev != by_year.end() ? ev->second : std::vector<int64_t>{};
            const std::vector<WantEntry> want =
                want_entries(y, ph != phases_at.end() ? ph->second : std::vector<int64_t>{}, events, rows);
            cut_seen = cut_seen || (is_full && !want.empty() && want.back().kind == "more");
            const godot::Array all = model->get_ticker_lines();
            if (problem.empty() && all.size() > static_cast<int64_t>(TICKER_LIMIT)) {
                problem = "year " + std::to_string(y) + ": " + std::to_string(all.size()) + " entries";
            }
            for (int64_t i = 0; i < all.size() && problem.empty(); ++i) {
                const godot::Dictionary d = all[i];
                if (bool(d["current"]) != (int64_t(d["year"]) == y)) {
                    problem = "year " + std::to_string(y) + ": entry " + std::to_string(i) + " current flag wrong";
                }
            }
            if (problem.empty()) {
                problem = compare_entries(y, current_entries(*model.ptr()), want);
            }
            if (is_full) {
                return;
            }
            // P6: the flashes added now (age 0) are the year's events with a
            // place, at the check's own place, in the ring colour of the kind.
            std::vector<std::pair<int64_t, std::array<int64_t, 3>>> want_flashes;
            for (const int64_t e : events) {
                const auto place = own.markers->event_place(*own.history, e);
                if (!place) {
                    continue;
                }
                const auto it = line_of.find(e);
                const worldgen::inspect::Rgb c = worldgen::inspect::event_symbol_colour(
                    worldgen::inspect::event_symbol(it != line_of.end() ? it->second->kind : std::string()));
                const std::array<int64_t, 3> cell{(*place)[0], (*place)[1],
                                                  (int64_t{c.r} << 16) | (int64_t{c.g} << 8) | int64_t{c.b}};
                want_flashes.emplace_back(e, cell);
            }
            if (want_flashes.size() > FLASH_LIMIT) {
                want_flashes.erase(want_flashes.begin(),
                                   want_flashes.begin() + static_cast<std::ptrdiff_t>(want_flashes.size() - FLASH_LIMIT));
            }
            const godot::Array flashes = model->get_flashes();
            std::vector<std::pair<int64_t, std::array<int64_t, 3>>> got_flashes;
            for (int64_t i = 0; i < flashes.size(); ++i) {
                const godot::Dictionary f = flashes[i];
                if (int64_t(f["age"]) >= int64_t(f["life"]) && flash_problem.empty()) {
                    flash_problem = "a flash of age " + std::to_string(int64_t(f["age"])) + " is still there";
                }
                if (int64_t(f["life"]) != 1200000 && flash_problem.empty()) {
                    flash_problem = "life " + std::to_string(int64_t(f["life"]));
                }
                if (int64_t(f["age"]) == 0) {
                    const std::array<int64_t, 3> cell{int64_t(f["x"]), int64_t(f["y"]), int64_t(f["colour"])};
                    got_flashes.emplace_back(int64_t(f["event"]), cell);
                }
            }
            if (flashes.size() > static_cast<int64_t>(FLASH_LIMIT) && flash_problem.empty()) {
                flash_problem = std::to_string(flashes.size()) + " flashes";
            }
            if (got_flashes != want_flashes && flash_problem.empty()) {
                flash_problem = "year " + std::to_string(y) + ": " + std::to_string(got_flashes.size()) +
                                " flashes added, want " + std::to_string(want_flashes.size());
            }
            flashes_added += static_cast<int64_t>(got_flashes.size());
        };
        arrival(0);
        RunOut out;
        if (problem.empty()) {
            out = run(ref, 33333, 0, arrival);
            problem = problem.empty() ? out.problem : problem;
        }
        expect(problem.empty(), godot::String("P5 ticker at ") + level + ", scope all, C2: at each of " +
                                    num(arrivals) + " arrivals the new entries are the phase line and the chronicle's "
                                    "rows of the year, at most 6, current exactly for Y" + tail(problem));
        if (!is_full) {
            expect(flash_problem.empty(), "P6 flashes, C2: " + num(flashes_added) +
                                              " added at arrivals, each a placed event of the year at its place in "
                                              "its ring colour, gone at 1,200,000, never more than 32" +
                                              tail(flash_problem));
        }
    }
    {
        std::string problem;
        if (!cut_seen) {
            // No year with more than six entries: the rule on a ticker fed
            // directly.
            PlaybackState direct;
            std::vector<TickerEntry> entries;
            for (int64_t i = 1; i <= 8; ++i) {
                entries.push_back(TickerEntry{7, i, "event", "7  line " + std::to_string(i)});
            }
            direct.add_ticker_year(5, {TickerEntry{5, 99, "event", "5  older"}});
            direct.add_ticker_year(7, entries);
            const auto& t = direct.ticker();
            const bool ok = t.size() == 6 && t[0].event == 1 && t[4].event == 5 && t[5].kind == "more" &&
                            t[5].text == "7  ... and 4 more";
            problem = ok ? "" : "the direct ticker has " + std::to_string(t.size()) + " entries";
        }
        expect(problem.empty(), godot::String("P5 a year with more than six entries shows its first five and '... and "
                                              "<n> more'") +
                                    (cut_seen ? " (seen at level full)" : " (on a ticker fed directly)") +
                                    tail(problem));
    }

    // P7: interaction during C2.
    {
        std::string problem;
        start_all("summary", 50, true, 0, end);
        RefPlayer ref;
        ref.stops = ref_stops(summary_years, true, changes, phase_years, 0, end);
        ref.p1 = end;
        ref.speed = 50;
        ref.skip = true;
        const int64_t third = ref.stops.size() > 2 ? ref.stops[2] : end;
        for (int64_t call = 0; call < 100000 && model->get_year() < third; ++call) {
            model->playback_advance(33333);
        }
        const int64_t y7 = model->get_year() + 7;
        std::map<int64_t, std::vector<int64_t>> by_year;
        for (const ChronicleLine* e : summary_lines) {
            by_year[e->start].push_back(e->id);
        }
        std::map<int64_t, std::string> rows;
        const godot::PackedInt64Array ids = model->get_event_ids();
        const godot::PackedStringArray texts = model->get_event_rows();
        for (int64_t i = 0; i < ids.size() && i < texts.size(); ++i) {
            rows[ids[i]] = to_utf8(texts[i]);
        }
        if (model->get_year() != third || y7 > end) {
            problem = "did not reach the third stop year " + std::to_string(third);
        }
        model->set_year(y7);
        const godot::Dictionary p = model->get_playback();
        if (problem.empty() && (godot::String(p["state"]) != "paused" || model->get_year() != y7)) {
            problem = "set_year(Y + 7): state " + to_utf8(p["state"]);
        }
        if (problem.empty()) {
            const auto ev = by_year.find(y7);
            const auto ph = phases_at.find(y7);
            const std::vector<WantEntry> want = want_entries(
                y7, ph != phases_at.end() ? ph->second : std::vector<int64_t>{},
                ev != by_year.end() ? ev->second : std::vector<int64_t>{}, rows);
            const godot::Array all = model->get_ticker_lines();
            if (all.size() != static_cast<int64_t>(want.size())) {
                problem = "the ticker holds " + std::to_string(all.size()) + " entries after the outside change, want " +
                          std::to_string(want.size());
            } else {
                problem = compare_entries(y7, current_entries(*model.ptr()), want);
            }
            if (problem.empty() && !model->get_flashes().is_empty()) {
                problem = "flashes remain after the outside change";
            }
        }
        if (problem.empty() && (model->playback_advance(33333) || model->playback_advance(250000) ||
                                model->get_year() != y7)) {
            problem = "advance moved while paused";
        }
        // Resume: the reference restarted at Y + 7 with r = 0.
        model->toggle_playback();
        ref.y = y7;
        ref.r = 0;
        if (problem.empty() && godot::String(model->get_playback()["state"]) != "playing") {
            problem = "toggle_playback did not resume";
        }
        if (problem.empty()) {
            problem = run(ref, 33333, 40, no_move).problem;
        }
        // A speed change keeps r: no year change in that call, then the
        // dwell at the new speed (D = 10,000 at 100).
        if (problem.empty()) {
            const int64_t year_before = model->get_year();
            g_years = 0;
            model->set_playback_speed(100);
            ref.speed = 100;
            if (g_years != 0 || model->get_year() != year_before) {
                problem = "set_playback_speed changed the year";
            }
            // Finish the current year: one microsecond short, then the rest.
            const int64_t rest = ref.dwell(ref.y) - ref.r;
            if (problem.empty() && rest > 1) {
                const int a = ref.advance(rest - 1);
                const bool moved_a = model->playback_advance(rest - 1);
                const int b = ref.advance(1);
                const bool moved_b = model->playback_advance(1);
                if (a != 0 || moved_a || b != 1 || !moved_b || model->get_year() != ref.y) {
                    problem = "after the speed change the dwell is not " + std::to_string(ref.dwell(year_before)) +
                              " (" + std::to_string(rest) + " left at year " + std::to_string(year_before) + ")";
                }
            }
            if (problem.empty()) {
                problem = run(ref, 33333, 20, no_move).problem;
            }
        }
        // select_event pauses at the event's year.
        if (problem.empty()) {
            const ChronicleLine* target = nullptr;
            for (const ChronicleLine* e : summary_lines) {
                if (e->start != model->get_year() && target == nullptr && e->start > 0) {
                    target = e;
                }
            }
            model->select_event(target != nullptr ? target->id : 1);
            if (target == nullptr || godot::String(model->get_playback()["state"]) != "paused" ||
                model->get_year() != target->start) {
                problem = "select_event did not pause at its year";
            }
        }
        model->toggle_playback();
        model->playback_advance(33333);
        model->stop_playback();
        if (problem.empty() && (!model->get_ticker_lines().is_empty() || !model->get_flashes().is_empty() ||
                                godot::String(model->get_playback()["state"]) != "stopped")) {
            problem = "stop_playback left the ticker or flashes";
        }
        model->play(0, end);
        const godot::String path = model->get_world_path();
        screen.open_world(path);
        if (problem.empty() && godot::String(model->get_playback()["state"]) != "stopped") {
            problem = "reopening the world did not stop playback";
        }
        expect(problem.empty(), "P7 at the third stop year " + num(third) + " set_year(Y + 7) pauses with the "
                                "ticker of Y + 7 and no flashes; paused calls change nothing; Play resumes as the "
                                "reference from Y + 7; a speed change keeps the year and takes the new dwell; "
                                "select_event pauses; stop clears; reopening stops" +
                                    tail(problem));
        screen.choose_view(model->get_territory_view());
    }

    // P8: keys, with focus released.
    {
        std::string problem;
        screen.get_viewport()->gui_release_focus();
        model->clear_selection();
        model->set_year(end);
        const auto state = [&]() { return to_utf8(model->get_playback()["state"]); };
        send_key(screen, godot::KEY_SPACE, false);
        const bool playing = state() == "playing" && screen.play_button()->get_text() == "Pause";
        send_key(screen, godot::KEY_SPACE, false);
        const bool paused = state() == "paused" && screen.play_button()->get_text() == "Play";
        send_key(screen, godot::KEY_SPACE, false);
        const bool again = state() == "playing";
        send_key(screen, godot::KEY_SPACE, true);
        const bool stopped = state() == "stopped" && screen.play_button()->get_text() == "Play";
        if (!(playing && paused && again && stopped)) {
            problem = "Space / Shift+Space";
        }
        bool speeds = true;
        for (int i = 0; i < 10; ++i) {
            send_key(screen, godot::KEY_MINUS, false);
        }
        speeds = speeds && int64_t(model->get_playback()["speed"]) == 1 && screen.play_speed()->get_selected() == 0;
        for (std::size_t i = 1; i < PLAYBACK_SPEEDS.size(); ++i) {
            send_key(screen, godot::KEY_EQUAL, false);
            speeds = speeds && int64_t(model->get_playback()["speed"]) == PLAYBACK_SPEEDS[i] &&
                     screen.play_speed()->get_selected() == static_cast<int32_t>(i);
        }
        send_key(screen, godot::KEY_EQUAL, false);
        speeds = speeds && int64_t(model->get_playback()["speed"]) == 500;
        if (!speeds && problem.empty()) {
            problem = "- and =";
        }
        const bool skip_before = model->get_playback()["skip_quiet"];
        send_key(screen, godot::KEY_Q, false);
        const bool q1 = bool(model->get_playback()["skip_quiet"]) != skip_before &&
                        screen.play_skip_quiet()->is_pressed() != skip_before;
        send_key(screen, godot::KEY_Q, false);
        const bool q2 = bool(model->get_playback()["skip_quiet"]) == skip_before &&
                        screen.play_skip_quiet()->is_pressed() == skip_before;
        if (!(q1 && q2) && problem.empty()) {
            problem = "Q";
        }
        // T: start a session at a year with entries so that the ticker shows.
        model->set_playback_speed(10);
        model->play(0, end);
        int64_t guard = 0;
        while (model->get_ticker_lines().is_empty() && guard++ < 100000 &&
               godot::String(model->get_playback()["state"]) == "playing") {
            model->playback_advance(33333);
        }
        screen.layout_ticker();
        const bool shown = screen.ticker()->is_visible();
        send_key(screen, godot::KEY_T, false);
        const bool hidden = !screen.ticker()->is_visible() && !screen.ticker_shown();
        send_key(screen, godot::KEY_T, false);
        const bool back = screen.ticker()->is_visible() && screen.ticker_shown();
        if (!(shown && hidden && back) && problem.empty()) {
            problem = std::string("T (shown ") + (shown ? "yes" : "no") + ", hidden " + (hidden ? "yes" : "no") +
                      ", back " + (back ? "yes" : "no") + ")";
        }
        model->set_playback_speed(10);
        expect(problem.empty(), "P8 keys: Space plays and pauses, Shift+Space stops, - and = step through the speeds "
                                "and stop at both ends, Q toggles skip, T the ticker; the play controls follow" +
                                    tail(problem));
    }

    // P9: parse_play_options.
    {
        std::string problem;
        struct Row {
            std::vector<std::string> args;
            bool history;
            std::string error;  // empty: valid
            int64_t from, to, speed;
        };
        const std::string e = std::to_string(end);
        const std::string e1 = std::to_string(end + 1);
        const std::string span_err = ": not a span of this world (0 to " + e + ")";
        const std::string speeds = "not one of 1, 2, 5, 10, 20, 50, 100, 200, 500";
        std::vector<Row> rows = {
            {{"w.db", "--play", "0", "200"}, true, "", 0, 200, 10},
            {{"w.db", "--play", "-", "-"}, true, "", -1, -1, 10},
            {{"w.db", "--play", "-", "100"}, true, "", -1, 100, 10},
            {{"w.db", "--play", "100", "-"}, true, "", 100, -1, 10},
            {{"w.db", "--play", "0", e}, true, "", 0, end, 10},
            {{"w.db", "--play", "200", "100"}, true, "--play 200 100" + span_err, 0, 0, 0},
            {{"w.db", "--play", "0", e1}, true, "--play 0 " + e1 + span_err, 0, 0, 0},
            {{"w.db", "--play", "0", "1.5"}, true, "--play 0 1.5: not a year: 1.5", 0, 0, 0},
            {{"w.db", "--play", "x", "5"}, true, "--play x 5: not a year: x", 0, 0, 0},
            {{"w.db", "--play", "5"}, true, "--play needs two values: a year or - for each end", 0, 0, 0},
            {{"w.db", "--play"}, true, "--play needs two values: a year or - for each end", 0, 0, 0},
            {{"w.db", "--play", "0", "10"}, false, "--play: this world has no history", 0, 0, 0},
            {{"w.db", "--play", "0", "10", "--speed", "0"}, true, "--speed 0: " + speeds, 0, 0, 0},
            {{"w.db", "--play", "0", "10", "--speed", "3"}, true, "--speed 3: " + speeds, 0, 0, 0},
            {{"w.db", "--play", "0", "10", "--speed", "1000"}, true, "--speed 1000: " + speeds, 0, 0, 0},
            {{"w.db", "--play", "0", "10", "--skip-quiet", "maybe"}, true, "--skip-quiet maybe: on or off", 0, 0, 0},
            {{"w.db", "--quit-at-end"}, true, "--quit-at-end needs --play", 0, 0, 0},
            {{"w.db", "--play-log"}, true, "--play-log needs --play", 0, 0, 0},
            {{"w.db", "--play", "0", "10", "--screenshot", "a.png"}, true,
             "--play cannot be combined with --screenshot or --bench", 0, 0, 0},
            {{"w.db", "--play", "0", "10", "--bench"}, true, "--play cannot be combined with --screenshot or --bench", 0,
             0, 0},
        };
        for (const int64_t s : PLAYBACK_SPEEDS) {
            rows.push_back({{"w.db", "--play", "0", "10", "--speed", std::to_string(s), "--skip-quiet", "off",
                             "--quit-at-end", "--play-log", "--no-pane"},
                            true, "", 0, 10, s});
        }
        int64_t n = 0;
        for (const Row& row : rows) {
            ++n;
            const auto got = parse_play_options(row.args, row.history, end);
            std::string args;
            for (const auto& a : row.args) {
                args += " " + a;
            }
            if (row.error.empty()) {
                const auto* o = std::get_if<PlayOptions>(&got);
                const bool all = row.args.size() > 6;
                if (o == nullptr || !o->play || o->from != row.from || o->to != row.to || o->speed != row.speed ||
                    (all && (o->skip_quiet || !o->quit_at_end || !o->log || !o->no_pane))) {
                    problem = problem.empty() ? args + ": " +
                                                    (o == nullptr ? std::get<std::string>(got) : "wrong options")
                                              : problem;
                }
            } else {
                const auto* text = std::get_if<std::string>(&got);
                if (text == nullptr || *text != row.error) {
                    problem = problem.empty() ? args + ": '" + (text != nullptr ? *text : "accepted") + "', want '" +
                                                    row.error + "'"
                                              : problem;
                }
            }
        }
        expect(problem.empty(), "P9 parse_play_options over " + num(n) + " argument lists gives the options or the "
                                "exact refusal texts of 9.2" + tail(problem));
    }

    // P10: layout at 1280 x 800 and 1600 x 1000, right pane shown.
    {
        std::string problem;
        auto* map_area = godot::Object::cast_to<godot::Control>(screen.get_node_or_null(godot::NodePath("Split/MapArea")));
        auto* split = godot::Object::cast_to<godot::Control>(screen.get_node_or_null(godot::NodePath("Split")));
        // A session with entries, so that the ticker shows.
        model->stop_playback();
        model->clear_selection();
        model->set_browser_level("full");
        model->set_playback_speed(10);
        model->play(0, end);
        int64_t guard = 0;
        while (model->get_ticker_lines().size() < 4 && guard++ < 100000 &&
               godot::String(model->get_playback()["state"]) == "playing") {
            model->playback_advance(33333);
        }
        model->toggle_playback();  // paused: the ticker stays
        const godot::Vector2 original = screen.get_size();
        godot::String widths;
        for (const auto& size : {godot::Vector2(1280, 800), godot::Vector2(1600, 1000)}) {
            screen.right_pane()->set_visible(true);
            screen.set_size(size);
            if (split != nullptr) {
                split->set_position(godot::Vector2());
                split->set_size(size);
            }
            for (int i = 0; i < 4; ++i) {
                settle(&screen);
            }
            screen.layout_ticker();
            const float slider = screen.year_slider()->get_size().x;
            widths += " " + num(static_cast<int64_t>(size.x)) + "x" + num(static_cast<int64_t>(size.y)) + ": slider " +
                      num(static_cast<int64_t>(slider)) + " px, map " +
                      num(static_cast<int64_t>(screen.map_view()->get_size().x)) + " x " +
                      num(static_cast<int64_t>(screen.map_view()->get_size().y)) + ";";
            const float want = size.x < 1500.0F ? 240.0F : 400.0F;
            if (map_area == nullptr) {
                problem = "no Split/MapArea";
                break;
            }
            if (slider < want && problem.empty()) {
                problem = "slider " + std::to_string(static_cast<int64_t>(slider)) + " px at " +
                          std::to_string(static_cast<int64_t>(size.x));
            }
            std::vector<godot::Control*> bar = {screen.play_button(), screen.year_slider(), screen.play_speed(),
                                                screen.play_skip_quiet(), screen.year_field(), screen.year_label(),
                                                screen.year_counts()};
            for (godot::Button* b : screen.year_buttons()) {
                bar.push_back(b);
            }
            const godot::Rect2 area = map_area->get_global_rect();
            for (godot::Control* c : bar) {
                if (!inside(area, c->get_global_rect()) && problem.empty()) {
                    problem = to_utf8(c->get_name()) + " lies outside MapArea at " +
                              std::to_string(static_cast<int64_t>(size.x));
                }
            }
            const godot::Rect2 view = screen.map_view()->get_global_rect();
            const godot::Rect2 ticker = screen.ticker()->get_global_rect();
            if (problem.empty() && (!screen.ticker()->is_visible() || !inside(view, ticker) ||
                                    ticker.size.y > 0.4F * view.size.y + 0.5F)) {
                problem = "the ticker (" + to_utf8(godot::Variant(ticker).stringify()) + ") is not inside the map view (" +
                          to_utf8(godot::Variant(view).stringify()) + ") within 40 % of its height";
            }
        }
        screen.set_size(original);
        if (split != nullptr) {
            split->set_position(godot::Vector2());
            split->set_size(original);
        }
        for (int i = 0; i < 4; ++i) {
            settle(&screen);
        }
        model->stop_playback();
        model->set_browser_level("summary");
        expect(problem.empty(), "P10 layout with the right pane shown:" + widths +
                                    " the slider at least 240 / 400 px, the year bar inside MapArea, the ticker inside "
                                    "the map view within 40 % of its height" +
                                    tail(problem));
    }

    model->disconnect("year_changed", on_year);
    model->disconnect("playback_finished", on_finished);
    model->stop_playback();
    model->clear_selection();
    model->set_year(end);
}

}  // namespace unmapped
