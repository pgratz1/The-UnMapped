// ViewerScreen: the history movie (spec P4-06 6.2, 7, 8, 9, 10). The
// controls of the year bar's first row, the ticker over the map, the frame
// clock handed to the model, the command-line options and the bench's
// playback lines.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <variant>
#include <vector>

#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/scene_tree_timer.hpp>
#include <godot_cpp/classes/style_box.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/char_string.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "viewer/playback_state.hpp"
#include "viewer/viewer_screen.hpp"

namespace unmapped {

namespace {

int64_t now_nsec() {
    return static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

std::string to_utf8(const godot::String& text) {
    const godot::CharString utf8 = text.utf8();
    return {utf8.get_data(), static_cast<std::size_t>(utf8.length())};
}

godot::String to_godot(const std::string& text) {
    return godot::String::utf8(text.data(), static_cast<int64_t>(text.size()));
}

const godot::Color TICKER_CURRENT(1.0F, 1.0F, 1.0F);
const godot::Color TICKER_OLDER(170.0F / 255.0F, 170.0F / 255.0F, 170.0F / 255.0F);
constexpr float TICKER_MARGIN = 8.0F;     // from the map view's edges (6.2)
constexpr double TICKER_MAX_SHARE = 0.4;  // of the map view's height

}  // namespace

void ViewerScreen::wire_playback() {
    play_speed_->clear();
    for (std::size_t i = 0; i < PLAYBACK_SPEEDS.size(); ++i) {
        play_speed_->add_item(godot::String::num_int64(PLAYBACK_SPEEDS[i]) + " y/s", static_cast<int32_t>(i));
    }
    play_button_->connect("pressed", callable_mp(this, &ViewerScreen::on_play_pressed));
    play_speed_->connect("item_selected", callable_mp(this, &ViewerScreen::on_play_speed_selected));
    play_skip_quiet_->connect("toggled", callable_mp(this, &ViewerScreen::on_play_skip_toggled));
    model_->connect("playback_changed", callable_mp(this, &ViewerScreen::on_playback_changed));
    model_->connect("ticker_changed", callable_mp(this, &ViewerScreen::on_ticker_changed));
    model_->connect("playback_finished", callable_mp(this, &ViewerScreen::on_playback_finished));
    map_view_->connect("resized", callable_mp(this, &ViewerScreen::layout_ticker));
    ticker_->set_mouse_filter(MOUSE_FILTER_IGNORE);
    ticker_lines_->set_mouse_filter(MOUSE_FILTER_IGNORE);
    update_play_controls();
    set_process(true);
}

void ViewerScreen::update_play_controls() {
    if (!wired_) {
        return;
    }
    const bool history = model_->has_history();
    const godot::Dictionary p = model_->get_playback();
    const godot::String state = p.get("state", "stopped");
    play_button_->set_disabled(!history);
    play_speed_->set_disabled(!history);
    play_skip_quiet_->set_disabled(!history);
    play_button_->set_text(state == "playing" ? "Pause" : "Play");
    const int64_t speed = p.get("speed", PLAYBACK_DEFAULT_SPEED);
    for (std::size_t i = 0; i < PLAYBACK_SPEEDS.size(); ++i) {
        if (PLAYBACK_SPEEDS[i] == speed && play_speed_->get_selected() != static_cast<int32_t>(i)) {
            play_speed_->select(static_cast<int32_t>(i));
        }
    }
    play_skip_quiet_->set_pressed_no_signal(bool(p.get("skip_quiet", true)));
}

void ViewerScreen::on_playback_changed() {
    update_play_controls();
    const godot::Dictionary p = model_->get_playback();
    if (godot::String(p.get("state", "stopped")) == "stopped") {
        map_view_->set_flashes(godot::Array());
    }
}

void ViewerScreen::on_play_pressed() {
    model_->toggle_playback();
}

void ViewerScreen::on_play_speed_selected(int64_t index) {
    if (index >= 0 && static_cast<std::size_t>(index) < PLAYBACK_SPEEDS.size()) {
        model_->set_playback_speed(PLAYBACK_SPEEDS[static_cast<std::size_t>(index)]);
    }
    update_play_controls();
}

void ViewerScreen::on_play_skip_toggled(bool on) {
    model_->set_playback_skip_quiet(on);
    update_play_controls();
}

void ViewerScreen::on_ticker_changed() {
    if (!wired_) {
        return;
    }
    rebuild_ticker();
    layout_ticker();
    // Flashes cleared by the model (an outside year change, a new session)
    // are cleared on the map at once.
    map_view_->set_flashes(model_->get_flashes());
}

void ViewerScreen::rebuild_ticker() {
    const godot::Array lines = model_->get_ticker_lines();
    // One Label per entry, newest at the bottom; reused where it exists.
    while (ticker_lines_->get_child_count() > lines.size()) {
        godot::Node* last = ticker_lines_->get_child(ticker_lines_->get_child_count() - 1);
        ticker_lines_->remove_child(last);
        last->queue_free();
    }
    while (ticker_lines_->get_child_count() < lines.size()) {
        auto* label = memnew(godot::Label);
        label->set_autowrap_mode(godot::TextServer::AUTOWRAP_WORD);
        label->set_mouse_filter(MOUSE_FILTER_IGNORE);
        label->set_h_size_flags(SIZE_EXPAND_FILL);
        ticker_lines_->add_child(label);
    }
    for (int64_t i = 0; i < lines.size(); ++i) {
        const godot::Dictionary line = lines[i];
        auto* label = godot::Object::cast_to<godot::Label>(ticker_lines_->get_child(static_cast<int32_t>(i)));
        if (label == nullptr) {
            continue;
        }
        label->set_text(line["text"]);
        label->add_theme_color_override("font_color", bool(line["current"]) ? TICKER_CURRENT : TICKER_OLDER);
        label->set_visible(true);
    }
}

void ViewerScreen::layout_ticker() {
    if (!wired_) {
        return;
    }
    const godot::Dictionary p = model_->get_playback();
    const bool active = godot::String(p.get("state", "stopped")) != "stopped";
    const int64_t count = ticker_lines_->get_child_count();
    const godot::Vector2 view = map_view_->get_size();
    if (!ticker_shown_ || !active || count == 0 || view.x < 4.0F * TICKER_MARGIN) {
        ticker_->set_visible(false);
        return;
    }
    // 6.2: at most 40 % of the map view's height; the oldest entries that do
    // not fit are hidden. Each label is given the width it gets in the panel
    // first, so that its wrapped height is known now, not after a frame.
    float pad_x = 0.0F;
    float pad_y = 0.0F;
    const godot::Ref<godot::StyleBox> panel = ticker_->get_theme_stylebox("panel");
    if (panel.is_valid()) {
        const godot::Size2 m = panel->get_minimum_size();
        pad_x = m.x;
        pad_y = m.y;
    }
    const float width = std::max(view.x - 2.0F * TICKER_MARGIN - pad_x, 1.0F);
    const float budget = static_cast<float>(TICKER_MAX_SHARE * static_cast<double>(view.y)) - pad_y;
    const int32_t separation = ticker_lines_->get_theme_constant("separation");
    float used = 0.0F;
    bool full = false;
    for (int64_t i = count - 1; i >= 0; --i) {
        auto* label = godot::Object::cast_to<godot::Label>(ticker_lines_->get_child(static_cast<int32_t>(i)));
        if (label == nullptr) {
            continue;
        }
        label->set_custom_minimum_size(godot::Vector2(width, 0.0F));
        label->set_size(godot::Vector2(width, label->get_size().y));
        const float h = label->get_minimum_size().y;
        const float need = h + (used > 0.0F ? static_cast<float>(separation) : 0.0F);
        // The newest entry is always shown.
        if (full || (used > 0.0F && used + need > budget)) {
            full = true;
            label->set_visible(false);
            continue;
        }
        label->set_visible(true);
        used += need;
    }
    const float height = std::min(used + pad_y, static_cast<float>(TICKER_MAX_SHARE * static_cast<double>(view.y)));
    ticker_->set_anchor(godot::SIDE_LEFT, 0.0F);
    ticker_->set_anchor(godot::SIDE_RIGHT, 1.0F);
    ticker_->set_anchor(godot::SIDE_TOP, 1.0F);
    ticker_->set_anchor(godot::SIDE_BOTTOM, 1.0F);
    ticker_->set_offset(godot::SIDE_LEFT, TICKER_MARGIN);
    ticker_->set_offset(godot::SIDE_RIGHT, -TICKER_MARGIN);
    ticker_->set_offset(godot::SIDE_TOP, -TICKER_MARGIN - std::ceil(height));
    ticker_->set_offset(godot::SIDE_BOTTOM, -TICKER_MARGIN);
    ticker_->set_visible(true);
}

void ViewerScreen::_process(double delta) {
    if (!wired_ || !model_.is_valid() || !model_->has_history()) {
        return;
    }
    const PlaybackStatus status = model_->playback_state().status();
    if (status != PlaybackStatus::playing && status != PlaybackStatus::ended) {
        return;
    }
    // 4.1: whole microseconds, clamped; the bench's real-frame pass uses a
    // fixed step whatever the frame took.
    const int64_t dt = bench_play_frames_ ? int64_t{33333}
                                          : std::min<int64_t>(std::llround(delta * 1000000.0), PLAYBACK_MAX_DT);
    if (play_logging_) {
        ++play_frame_;
    }
    const bool moved = model_->playback_advance(dt);
    if (moved && play_logging_ && play_log_) {
        godot::UtilityFunctions::print("UNMAPPED-PLAY: ", play_frame_, " ", model_->get_year());
    }
    if (map_view_->flash_count() > 0 || !model_->playback_state().flashes().empty()) {
        map_view_->set_flashes(model_->get_flashes());
    }
}

void ViewerScreen::on_playback_finished() {
    if (play_logging_ && play_log_) {
        godot::UtilityFunctions::print("UNMAPPED-PLAY: finished at ", model_->playback_state().to(), " after ",
                                       play_frame_, " frames");
    }
    if (play_logging_ && quit_at_end_) {
        get_tree()->create_timer(2.0)->connect("timeout", callable_mp(this, &ViewerScreen::quit_after_play));
    }
    if (bench_play_frames_) {
        finish_bench_playback();
    }
}

void ViewerScreen::quit_after_play() {
    get_tree()->quit(0);
}

// --- Command line (9.1, 9.2) ----------------------------------------------------

void ViewerScreen::refuse_play(const godot::String& text) {
    if (status_label_ != nullptr) {
        show_status(text, true);
    }
    godot::UtilityFunctions::printerr(text);
}

bool ViewerScreen::apply_play_options(const godot::PackedStringArray& args) {
    std::vector<std::string> list;
    list.reserve(static_cast<std::size_t>(args.size()));
    for (int64_t i = 0; i < args.size(); ++i) {
        list.push_back(to_utf8(args[i]));
    }
    bool mentioned = false;
    for (const std::string& a : list) {
        mentioned = mentioned || a == "--play" || a == "--speed" || a == "--skip-quiet" || a == "--quit-at-end" ||
                    a == "--play-log" || a == "--no-pane";
    }
    if (!mentioned) {
        return true;
    }
    const auto parsed = parse_play_options(list, model_->has_history(), model_->get_end_year());
    if (const auto* error = std::get_if<std::string>(&parsed)) {
        refuse_play(to_godot(*error));
        // 9.2: with --quit-at-end the viewer quits with status 1.
        for (const std::string& a : list) {
            if (a == "--quit-at-end") {
                get_tree()->quit(1);
            }
        }
        return false;
    }
    const PlayOptions& o = std::get<PlayOptions>(parsed);
    if (model_->has_history()) {
        model_->set_playback_speed(o.speed);
        model_->set_playback_skip_quiet(o.skip_quiet);
    }
    if (o.no_pane) {
        right_pane_->set_visible(false);
    }
    play_pending_ = o.play;
    play_from_ = o.from;
    play_to_ = o.to;
    play_log_ = o.log;
    quit_at_end_ = o.quit_at_end;
    return true;
}

void ViewerScreen::prepare_play() {
    // The default span of the list once --select and the filters apply.
    const godot::Dictionary p = model_->get_playback();
    const int64_t end = model_->get_end_year();
    play_p0_ = play_from_ < 0 ? int64_t(p["from"]) : play_from_;
    play_p1_ = play_to_ < 0 ? int64_t(p["to"]) : play_to_;
    if (play_p0_ < 0 || play_p1_ > end || play_p0_ > play_p1_) {
        const std::string from = play_from_ < 0 ? "-" : std::to_string(play_from_);
        const std::string to = play_to_ < 0 ? "-" : std::to_string(play_to_);
        refuse_play(to_godot(play_span_refusal(from, to, end)));
        play_pending_ = false;
        if (quit_at_end_) {
            get_tree()->quit(1);
        }
        return;
    }
    model_->set_year(play_p0_);
}

void ViewerScreen::start_play() {
    if (!play_pending_) {
        return;
    }
    play_pending_ = false;
    if (!model_->play(play_p0_, play_p1_)) {
        refuse_play("--play: playback could not start");
        if (quit_at_end_) {
            get_tree()->quit(1);
        }
        return;
    }
    play_logging_ = true;
    play_frame_ = 0;
    if (play_log_) {
        godot::UtilityFunctions::print("UNMAPPED-PLAY: 0 ", model_->get_year());
    }
}

// --- Bench (10) ------------------------------------------------------------------

void ViewerScreen::bench_playback() {
    const auto ms = [](int64_t nsec, int digits) {
        return godot::String::num(static_cast<double>(nsec) / 1000000.0, digits) + " ms";
    };
    if (!model_->has_history()) {
        get_tree()->quit(0);
        return;
    }
    const int64_t end = model_->get_end_year();
    struct Run {
        const char* name;
        int64_t speed;
        bool skip;
        bool story;
    };
    const Run runs[] = {{"i", 200, false, false}, {"ii", 10, true, false}, {"iii", 5, true, true}};
    for (const Run& run : runs) {
        model_->stop_playback();
        if (model_->get_browser_available()) {
            model_->clear_selection();
            if (run.story) {
                const godot::Array civs = model_->get_civilization_list();
                int64_t lowest = 0;
                for (int64_t i = 0; i < civs.size(); ++i) {
                    const int64_t id = godot::Dictionary(civs[i])["id"];
                    lowest = lowest == 0 || id < lowest ? id : lowest;
                }
                model_->select_civilization(lowest);
            }
        }
        model_->set_playback_speed(run.speed);
        model_->set_playback_skip_quiet(run.skip);
        model_->set_year(end);  // outside [P0, P1): the session starts at P0 (4.4)
        const bool started = run.story ? model_->play(-1, -1) : model_->play(0, end);
        if (!started) {
            godot::UtilityFunctions::print("UNMAPPED-BENCH: playback ", run.name, ": could not start");
            continue;
        }
        int64_t frames = 0;
        int64_t changes = 0;
        int64_t max = 0;
        int64_t max_year = -1;
        int64_t total = 0;
        int64_t ticker_max = 0;
        while (model_->playback_state().status() == PlaybackStatus::playing && frames < 10000000) {
            const bool moved = model_->playback_advance(33333);
            ++frames;
            changes += moved ? 1 : 0;
            const ViewerModel::PlaybackTimings& t = model_->playback_timings();
            total += t.advance;
            ticker_max = std::max(ticker_max, t.ticker);
            if (t.advance > max) {
                max = t.advance;
                max_year = model_->get_year();
            }
        }
        const int64_t stops = model_->get_playback()["stops"];
        godot::UtilityFunctions::print(
            "UNMAPPED-BENCH: playback ", run.name, ": ", frames, " frames, ", changes, " year changes, ", stops,
            " stops, movie ", godot::String::num(static_cast<double>(frames) / 30.0, 1), " s at 30 fps, advance max ",
            ms(max, 3), " (year ", max_year, ") mean ", ms(total / std::max<int64_t>(frames, 1), 4), ", ticker max ",
            ms(ticker_max, 3));
    }
    model_->stop_playback();
    godot::UtilityFunctions::print("UNMAPPED-BENCH: playback list and stop years recomputed (play, filter changes): "
                                   "slowest ",
                                   ms(model_->playback_timings().slowest_source, 3), " (",
                                   model_->get_browser_available()
                                       ? static_cast<int64_t>(model_->browser_state().index().events().size())
                                       : int64_t{0},
                                   " events)");
    if (godot::DisplayServer::get_singleton()->get_name() == "headless") {
        get_tree()->quit(0);
        return;
    }
    // Windowed: run (i) on real frames, dt fixed, each frame timed from one
    // frame_post_draw to the next.
    if (model_->get_browser_available()) {
        model_->clear_selection();
    }
    model_->set_playback_speed(200);
    model_->set_playback_skip_quiet(false);
    bench_play_times_.clear();
    bench_play_last_ = -1;
    model_->set_year(end);
    bench_play_frames_ = true;
    godot::RenderingServer::get_singleton()->connect("frame_post_draw", callable_mp(this, &ViewerScreen::bench_play_frame));
    model_->play(0, end);
}

void ViewerScreen::bench_play_frame() {
    const int64_t now = now_nsec();
    if (bench_play_last_ >= 0) {
        bench_play_times_.push_back(now - bench_play_last_);
    }
    bench_play_last_ = now;
}

void ViewerScreen::finish_bench_playback() {
    bench_play_frames_ = false;
    godot::RenderingServer::get_singleton()->disconnect("frame_post_draw",
                                                        callable_mp(this, &ViewerScreen::bench_play_frame));
    int64_t max = 0;
    int64_t total = 0;
    int64_t over33 = 0;
    int64_t over50 = 0;
    for (const int64_t t : bench_play_times_) {
        max = std::max(max, t);
        total += t;
        over33 += t > 33000000 ? 1 : 0;
        over50 += t > 50000000 ? 1 : 0;
    }
    const auto n = static_cast<int64_t>(bench_play_times_.size());
    const auto ms = [](int64_t nsec) { return godot::String::num(static_cast<double>(nsec) / 1000000.0, 2) + " ms"; };
    godot::UtilityFunctions::print("UNMAPPED-BENCH: playback frames: mean ", ms(total / std::max<int64_t>(n, 1)),
                                   ", max ", ms(max), ", ", over33, " over 33 ms, ", over50, " over 50 ms (", n,
                                   " frames)");
    get_tree()->quit(0);
}

}  // namespace unmapped
