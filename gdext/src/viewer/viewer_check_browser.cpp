#include "viewer/viewer_check_browser.hpp"
#include "viewer/viewer_check_reference.hpp"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <godot_cpp/classes/base_button.hpp>
#include <godot_cpp/classes/check_box.hpp>
#include <godot_cpp/classes/check_button.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/item_list.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/spin_box.hpp>
#include <godot_cpp/classes/tab_container.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/tree.hpp>
#include <godot_cpp/classes/tree_item.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/char_string.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#include "viewer/viewer_screen.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/history/timeline.hpp"
#include "worldgen/inspect/history_browser.hpp"
#include "worldgen/inspect/history_report.hpp"
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

// The report's trailing-space rule (P3-07 6.1): trailing spaces become \x20.
std::string report_line(std::string text) {
    std::size_t keep = text.size();
    while (keep > 0 && text[keep - 1] == ' ') {
        --keep;
    }
    std::string out = text.substr(0, keep);
    for (std::size_t i = keep; i < text.size(); ++i) {
        out += "\\x20";
    }
    return out;
}

std::string escape(const std::string& text) {
    return worldgen::inspect::escape_history_text(text);
}

// B4's transform of a display line back into the report form.
std::string report_form(const godot::Dictionary& line) {
    const godot::String text = line["text"];
    const godot::Array links = line["links"];
    std::string out;
    int64_t at = 0;
    for (int64_t i = 0; i < links.size(); ++i) {
        const godot::Dictionary link = links[i];
        const int64_t start = link["start"];
        const int64_t length = link["length"];
        out += escape(to_utf8(text.substr(at, start - at)));
        const std::string t = to_utf8(text.substr(start, length));
        if (bool(link["tagged"])) {
            out += escape(t) + " (" + to_utf8(link["target"]) + ")";
        } else {
            out += escape(t);
        }
        at = start + length;
    }
    out += escape(to_utf8(text.substr(at)));
    return report_line(out);
}

std::vector<int64_t> ids_of(const godot::PackedInt64Array& a) {
    return {a.ptr(), a.ptr() + a.size()};
}

std::string list_text(const std::vector<int64_t>& v) {
    std::string out;
    for (std::size_t i = 0; i < v.size() && i < 8; ++i) {
        out += (i > 0 ? " " : "") + std::to_string(v[i]);
    }
    return out + (v.size() > 8 ? " ..." : "");
}

std::string first_difference(const std::vector<int64_t>& got, const std::vector<int64_t>& want) {
    for (std::size_t i = 0; i < std::max(got.size(), want.size()); ++i) {
        if (i >= got.size() || i >= want.size() || got[i] != want[i]) {
            return "at " + std::to_string(i) + ": " + (i < got.size() ? std::to_string(got[i]) : "none") +
                   ", want " + (i < want.size() ? std::to_string(want[i]) : "none") + " (" +
                   std::to_string(got.size()) + " ids, want " + std::to_string(want.size()) + ")";
        }
    }
    return {};
}

int64_t g_year_changes = 0;
int64_t g_list_changes = 0;
int64_t g_highlight_changes = 0;
void count_year(int64_t /*year*/) {
    ++g_year_changes;
}
void count_list() {
    ++g_list_changes;
}
void count_highlight() {
    ++g_highlight_changes;
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

void send_button(ViewerScreen& screen, godot::Vector2 position, bool pressed) {
    godot::Ref<godot::InputEventMouseButton> button;
    button.instantiate();
    button->set_button_index(godot::MOUSE_BUTTON_LEFT);
    button->set_pressed(pressed);
    button->set_position(position);
    button->set_global_position(position);
    screen.map_view()->_gui_input(button);
}

void send_click(ViewerScreen& screen, godot::Vector2 position) {
    send_button(screen, position, true);
    send_button(screen, position, false);
}

void send_motion(ViewerScreen& screen, godot::Vector2 position, godot::Vector2 relative) {
    godot::Ref<godot::InputEventMouseMotion> motion;
    motion.instantiate();
    motion->set_position(position);
    motion->set_global_position(position);
    motion->set_relative(relative);
    screen.map_view()->_gui_input(motion);
}

godot::Dictionary selection(ViewerModel& model) {
    return model.get_selection();
}

int64_t selected_event(ViewerModel& model) {
    return model.get_selection().get("event", int64_t{0});
}

}  // namespace

namespace check_ref {

// --- The references, as the check reads them (8.1) ----------------------------

BrowserReport read_browser_report(const godot::String& path) {
    BrowserReport report;
    if (path.is_empty() || !godot::FileAccess::file_exists(path)) {
        report.problem = "no history report at " + to_utf8(path);
        return report;
    }
    const std::string text = to_utf8(godot::FileAccess::get_file_as_string(path));
    static const std::regex section_re(R"(^\[([a-z]+)\]$)");
    static const std::regex event_re(
        R"(^([0-9]+)(?:-([0-9]+))? e([0-9]+) ([a-z_]+)/([0-9]+): (.*) \{([^{}]*)\}(?: \[causes ([^\]]*)\])?$)");
    static const std::regex civ_re(R"(^c([0-9]+) (.+)$)");
    static const std::regex race_re(R"(^  race: .* \(ra([0-9]+)\)$)");
    static const std::regex founded_re(R"(^  founded: ([0-9]+) )");
    static const std::regex ended_re(R"(^  fate: ended in ([0-9]+))");
    static const std::regex events_re(R"(^  events: ([0-9]+)$)");
    static const std::regex settlement_re(R"(^  s([0-9]+) .*, at ([0-9]+) ([0-9]+) )");
    static const std::regex cause_re(R"(e([0-9]+))");
    std::string section;
    std::size_t start = 0;
    try {
        while (start < text.size()) {
            std::size_t stop = text.find('\n', start);
            if (stop == std::string::npos) {
                stop = text.size();
            }
            const std::string line = text.substr(start, stop - start);
            start = stop + 1;
            std::smatch m;
            if (std::regex_match(line, m, section_re)) {
                section = m[1];
            } else if (section == "chronicle" && std::regex_match(line, m, event_re)) {
                ChronicleLine e;
                e.start = std::stoll(m[1]);
                e.end = m[2].matched ? std::stoll(m[2]) : e.start;
                e.id = std::stoll(m[3]);
                e.kind = m[4];
                e.salience = std::stoll(m[5]);
                e.text = m[6];
                const std::string refs = m[7];
                std::size_t at = 0;
                while (at < refs.size()) {
                    std::size_t space = refs.find(' ', at);
                    if (space == std::string::npos) {
                        space = refs.size();
                    }
                    if (space > at) {
                        e.refs.push_back(refs.substr(at, space - at));
                    }
                    at = space + 1;
                }
                for (const std::string& r : e.refs) {
                    if (r.rfind("@rg", 0) == 0) {
                        e.region = std::stoll(r.substr(3));
                    } else if (r.rfind("@s", 0) == 0) {
                        e.settlement = std::stoll(r.substr(2));
                    }
                }
                if (m[8].matched) {
                    const std::string causes = m[8];
                    for (auto it = std::sregex_iterator(causes.begin(), causes.end(), cause_re);
                         it != std::sregex_iterator(); ++it) {
                        e.causes.push_back(std::stoll((*it)[1]));
                    }
                }
                report.chronicle.push_back(std::move(e));
            } else if (section == "civilizations") {
                if (std::regex_match(line, m, civ_re)) {
                    BlockText b;
                    b.id = std::stoll(m[1]);
                    b.name = m[2];
                    b.lines.push_back(line);
                    report.blocks.push_back(std::move(b));
                } else if (!report.blocks.empty()) {
                    BlockText& b = report.blocks.back();
                    b.lines.push_back(line);
                    if (std::regex_match(line, m, race_re)) {
                        b.race = std::stoll(m[1]);
                    } else if (std::regex_search(line, m, founded_re)) {
                        b.founded = std::stoll(m[1]);
                    } else if (std::regex_search(line, m, ended_re)) {
                        b.ended = std::stoll(m[1]);
                    } else if (std::regex_match(line, m, events_re)) {
                        b.events = std::stoll(m[1]);
                    }
                }
            } else if (section == "present" && std::regex_search(line, m, settlement_re)) {
                report.settlement_cells[std::stoll(m[1])] = {std::stoll(m[2]), std::stoll(m[3])};
            }
        }
    } catch (const std::exception& e) {
        report.problem = std::string("cannot read ") + to_utf8(path) + ": " + e.what();
    }
    std::sort(report.chronicle.begin(), report.chronicle.end(),
              [](const ChronicleLine& a, const ChronicleLine& b) { return a.id < b.id; });
    if (report.problem.empty() && (report.chronicle.empty() || report.blocks.empty())) {
        report.problem = to_utf8(path) + " has no chronicle lines or no civilization blocks";
    }
    return report;
}

// The check's own read of the world.
OwnWorld open_own_world(const godot::String& path) {
    OwnWorld w;
    try {
        auto reader = worldgen::db::WorldReader::open(to_utf8(path));
        if (!reader.ok()) {
            w.problem = reader.error().message;
            return w;
        }
        w.reader.emplace(std::move(reader).value());
        auto loaded = worldgen::history::load_history(*w.reader);
        if (!loaded.ok()) {
            w.problem = loaded.error().message;
            return w;
        }
        w.history.emplace(std::move(loaded).value());
        auto markers = worldgen::inspect::MarkerIndex::load(*w.reader, *w.history);
        if (!markers.ok()) {
            w.problem = markers.error().message;
            return w;
        }
        w.markers.emplace(std::move(markers).value());
        auto browser = worldgen::inspect::BrowserIndex::build(*w.history);
        if (!browser.ok()) {
            w.problem = browser.error().message;
            return w;
        }
        w.browser.emplace(std::move(browser).value());
        auto regions = worldgen::db::read_region_rows(*w.reader);
        if (!regions.ok()) {
            w.problem = regions.error().message;
            return w;
        }
        for (const auto& r : regions.value()) {
            w.seeds[r.id] = {r.seed_x, r.seed_y};
        }
    } catch (const std::exception& e) {
        w.problem = e.what();
    }
    return w;
}

}  // namespace check_ref

// --- B0 ---------------------------------------------------------------------------

void check_browser_without_history(ViewerScreen& screen, const CheckExpect& expect) {
    godot::Ref<ViewerModel> model = screen.get_model();
    expect(!model->get_browser_available(), "B0 without history the browser is not available");
    bool disabled = true;
    godot::String which;
    for (godot::Control* c : screen.browser_controls()) {
        bool off = false;
        if (auto* b = godot::Object::cast_to<godot::BaseButton>(c)) {
            off = b->is_disabled();
        } else if (auto* l = godot::Object::cast_to<godot::LineEdit>(c)) {
            off = !l->is_editable();
        } else if (auto* s = godot::Object::cast_to<godot::SpinBox>(c)) {
            off = !s->is_editable();
        }
        if (!off) {
            disabled = false;
            which += " " + godot::String(c->get_name());
        }
    }
    expect(disabled && screen.civ_list()->get_root() == nullptr && screen.event_list()->get_item_count() == 0,
           "B0 every browser control is disabled and the lists are empty" + (which.is_empty() ? which : ":" + which));
    expect(screen.right_pane()->is_tab_disabled(0) && screen.right_pane()->get_current_tab() == 1,
           "B0 the Browser tab is disabled and Legend is shown");
    const bool empty = model->get_civilization_list().is_empty() && model->get_civilization_block(1).is_empty() &&
                       model->get_event_ids().is_empty() && model->get_event_rows().is_empty() &&
                       model->get_event_detail(1).is_empty() && !model->set_browser_scope("all") &&
                       !model->set_browser_group("all") && !model->set_browser_level("full") &&
                       !model->set_browser_years(-1, -1) && !model->set_browser_this_year(true) &&
                       !model->select_civilization(1) && !model->select_event(1) && !model->select_place(5, 1) &&
                       !model->follow_link("c1") && !model->browser_back() && !model->browser_forward() &&
                       !model->can_browser_back() && !model->can_browser_forward() && !model->set_follow(true) &&
                       !model->get_follow() && !model->follow_first() && model->search_names("a").is_empty() &&
                       model->get_selection().is_empty() && model->get_highlight().is_empty() &&
                       model->get_event_groups().is_empty() && model->get_place_text().is_empty() &&
                       !model->clear_selection();
    expect(empty, "B0 every browser method answers empty or false");
}

// --- B1 to B12 --------------------------------------------------------------------

void check_browser(ViewerScreen& screen, const ViewerCheckOptions& options, const CheckExpect& expect) {
    godot::Ref<ViewerModel> model = screen.get_model();
    expect(model->get_browser_available(), "B the browser is available" +
                                               (model->get_browser_available()
                                                    ? godot::String()
                                                    : ": " + model->get_browser_error_code() + ": " +
                                                          model->get_browser_error_message()));
    const BrowserReport full = read_browser_report(options.history_report);
    const BrowserReport summary = read_browser_report(options.history_summary);
    expect(full.problem.empty() && summary.problem.empty(),
           "B the history reports at both levels read" +
               (full.problem.empty() ? godot::String() : ": " + to_godot(full.problem)) +
               (summary.problem.empty() ? godot::String() : ": " + to_godot(summary.problem)));
    OwnWorld own = open_own_world(model->get_world_path());
    expect(own.problem.empty(), "B the check's own read of the world" +
                                    (own.problem.empty() ? godot::String() : ": " + to_godot(own.problem)));
    if (!model->get_browser_available() || !full.problem.empty() || !summary.problem.empty() || !own.problem.empty()) {
        return;
    }
    const int64_t end = model->get_end_year();
    const int64_t territory = model->get_territory_view();
    screen.choose_view(territory);
    std::map<int64_t, const ChronicleLine*> line_of;
    for (const auto& e : full.chronicle) {
        line_of[e.id] = &e;
    }
    const godot::Callable on_year = callable_mp_static(&count_year);
    const godot::Callable on_list = callable_mp_static(&count_list);
    const godot::Callable on_highlight = callable_mp_static(&count_highlight);
    model->connect("year_changed", on_year);
    model->connect("event_list_changed", on_list);
    model->connect("highlight_changed", on_highlight);

    // B1: the civilization list against the [civilizations] blocks.
    {
        const godot::Array civs = model->get_civilization_list();
        std::string problem;
        if (civs.size() != static_cast<int64_t>(full.blocks.size())) {
            problem = std::to_string(civs.size()) + " rows, " + std::to_string(full.blocks.size()) + " blocks";
        }
        godot::TreeItem* root = screen.civ_list()->get_root();
        godot::TreeItem* row = root != nullptr ? root->get_first_child() : nullptr;
        for (int64_t i = 0; i < civs.size() && problem.empty(); ++i, row = row != nullptr ? row->get_next() : nullptr) {
            const godot::Dictionary c = civs[i];
            const BlockText& b = full.blocks[static_cast<std::size_t>(i)];
            const int64_t ended = c["ended_year"];
            const bool same = int64_t(c["id"]) == b.id && to_utf8(c["name"]) == b.name && int64_t(c["race"]) == b.race &&
                              int64_t(c["founded_year"]) == b.founded && ended == b.ended &&
                              godot::String(c["fate"]) == (b.ended >= 0 ? "ended" : "alive") &&
                              int64_t(c["events"]) == b.events &&
                              int64_t(c["colour"]) == int64_t(model->get_civilization_info(b.id)["colour"]);
            if (!same) {
                problem = "c" + std::to_string(b.id) + " " + to_utf8(c["name"]) + " differs from its block";
                continue;
            }
            // The row shows the name and the colour's swatch.
            const godot::Ref<godot::Texture2D> icon = row != nullptr ? row->get_icon(0) : godot::Ref<godot::Texture2D>();
            const godot::Ref<godot::Image> image = icon.is_valid() ? icon->get_image() : godot::Ref<godot::Image>();
            const int64_t colour = c["colour"];
            const bool swatch = image.is_valid() &&
                                image->get_pixel(1, 1).to_rgba32() ==
                                    static_cast<uint32_t>(((colour & 0xFFFFFF) << 8) | 0xFF);
            if (row == nullptr || row->get_text(0) != godot::String(c["name"]) || !swatch) {
                problem = "the list row of c" + std::to_string(b.id) + " lacks its name or swatch";
            }
        }
        expect(problem.empty(), "B1 " + num(civs.size()) +
                                    " civilizations: id, name, race, founded, fate, events and swatch as the blocks of "
                                    "the report" +
                                    (problem.empty() ? godot::String() : ": " + to_godot(problem)));
    }

    // B2: every event's detail against its chronicle line at full.
    {
        std::set<int64_t> summary_ids;
        for (const auto& e : summary.chronicle) {
            summary_ids.insert(e.id);
        }
        std::map<int64_t, std::vector<int64_t>> effects;
        for (const auto& e : full.chronicle) {
            for (const int64_t c : e.causes) {
                effects[c].push_back(e.id);
            }
        }
        std::string problem;
        for (const auto& e : full.chronicle) {
            if (!problem.empty()) {
                break;
            }
            const godot::Dictionary d = model->get_event_detail(e.id);
            if (d.is_empty()) {
                problem = "e" + std::to_string(e.id) + ": no detail";
                break;
            }
            const godot::Dictionary sentence = d["sentence"];
            const std::string note = to_utf8(d["note"]);
            const std::string text =
                escape(to_utf8(sentence["text"])) + (note.empty() ? std::string() : " (" + escape(note) + ")");
            std::vector<std::string> refs;
            if (int64_t(d["region"]) != 0) {
                refs.push_back("@rg" + std::to_string(int64_t(d["region"])));
            }
            if (int64_t(d["settlement"]) != 0) {
                refs.push_back("@s" + std::to_string(int64_t(d["settlement"])));
            }
            const godot::Array participants = d["participants"];
            for (int64_t i = 0; i < participants.size(); ++i) {
                const godot::Dictionary p = participants[i];
                refs.push_back(escape(to_utf8(p["role"])) + "=" + to_utf8(p["target"]));
            }
            const godot::Array values = d["values"];
            for (int64_t i = 0; i < values.size(); ++i) {
                const godot::Dictionary v = values[i];
                refs.push_back(escape(to_utf8(v["key"])) + "=" + std::to_string(int64_t(v["value"])));
            }
            std::vector<std::string> want_refs = e.refs;
            std::sort(refs.begin(), refs.end());
            std::sort(want_refs.begin(), want_refs.end());
            const auto targets = [](const godot::Array& rows) {
                std::vector<int64_t> out;
                for (int64_t i = 0; i < rows.size(); ++i) {
                    const std::string t = to_utf8(godot::Dictionary(rows[i])["target"]);
                    out.push_back(t.size() > 1 && t[0] == 'e' ? std::stoll(t.substr(1)) : -1);
                }
                return out;
            };
            const std::vector<int64_t> want_effects = effects[e.id];
            const std::string where = "e" + std::to_string(e.id) + ": ";
            if (text != e.text) {
                problem = where + "text '" + text + "', the line's '" + e.text + "'";
            } else if (to_utf8(d["kind_key"]) != e.kind || int64_t(d["salience"]) != e.salience ||
                       int64_t(d["year"]) != e.start || int64_t(d["end_year"]) != e.end) {
                problem = where + "kind, salience or span differ";
            } else if (targets(d["causes"]) != e.causes) {
                problem = where + "causes differ";
            } else if (refs != want_refs) {
                problem = where + "participants and values differ from the line's refs";
            } else if (targets(d["effects"]) != want_effects) {
                problem = where + "effects are not the inverse of the causes";
            } else if (bool(d["summary"]) != (summary_ids.count(e.id) != 0)) {
                problem = where + "the summary flag differs from history-summary.txt";
            }
        }
        expect(problem.empty(), "B2 " + num(static_cast<int64_t>(full.chronicle.size())) +
                                    " events: sentence, note, kind, salience, span, causes, refs, effects and summary "
                                    "flag equal the chronicle lines" +
                                    (problem.empty() ? godot::String() : ": " + to_godot(problem)));
    }

    // B3: lists.
    std::map<int64_t, std::vector<int64_t>> civ_events;  // at full, from the chronicle
    {
        model->clear_selection();
        model->set_browser_group("all");
        model->set_browser_level("full");
        std::vector<int64_t> all;
        for (const auto& e : full.chronicle) {
            all.push_back(e.id);
        }
        std::vector<int64_t> summary_ids;
        for (const auto& e : summary.chronicle) {
            summary_ids.push_back(e.id);
        }
        const std::string d_full = first_difference(ids_of(model->get_event_ids()), all);
        const bool rows_ok = model->get_event_rows().size() == static_cast<int64_t>(all.size()) &&
                             screen.event_list()->get_item_count() == static_cast<int64_t>(all.size());
        model->set_browser_level("summary");
        const std::string d_summary = first_difference(ids_of(model->get_event_ids()), summary_ids);
        expect(d_full.empty() && d_summary.empty() && rows_ok,
               "B3 scope all: level full lists every chronicle id (" + num(static_cast<int64_t>(all.size())) +
                   "), summary the summary chronicle's (" + num(static_cast<int64_t>(summary_ids.size())) + ")" +
                   to_godot(d_full.empty() ? "" : ": full " + d_full) +
                   to_godot(d_summary.empty() ? "" : ": summary " + d_summary));
        model->set_browser_level("full");
        std::string problem;
        for (const auto& b : full.blocks) {
            std::vector<int64_t> want;
            const std::string tag = "=c" + std::to_string(b.id);
            for (const auto& e : full.chronicle) {
                for (const auto& r : e.refs) {
                    if (r.size() > tag.size() && r.compare(r.size() - tag.size(), tag.size(), tag) == 0 &&
                        r[0] != '@') {
                        want.push_back(e.id);
                        break;
                    }
                }
            }
            civ_events[b.id] = want;
            model->select_civilization(b.id);
            const std::string d = first_difference(ids_of(model->get_event_ids()), want);
            if (problem.empty() && (!d.empty() || static_cast<int64_t>(want.size()) != b.events)) {
                problem = "c" + std::to_string(b.id) + ": " + (d.empty() ? "events line " + std::to_string(b.events) : d);
            }
        }
        expect(problem.empty(), "B3 each civilization at full lists the chronicle lines with a participant =c<id>, "
                                "as many as its events line" +
                                    (problem.empty() ? godot::String() : ": " + to_godot(problem)));
    }

    // B4: blocks at both levels against the reports.
    {
        std::string problem;
        int64_t lines = 0;
        for (const char* level : {"summary", "full"}) {
            model->set_browser_level(level);
            const BrowserReport& ref = std::string(level) == "full" ? full : summary;
            for (const auto& b : ref.blocks) {
                const godot::Array block = model->get_civilization_block(b.id);
                if (block.size() != static_cast<int64_t>(b.lines.size()) && problem.empty()) {
                    problem = std::string(level) + " c" + std::to_string(b.id) + ": " + std::to_string(block.size()) +
                              " lines, the report " + std::to_string(b.lines.size());
                }
                for (int64_t i = 0; i < block.size() && i < static_cast<int64_t>(b.lines.size()); ++i) {
                    ++lines;
                    const std::string got = report_form(block[i]);
                    if (got != b.lines[static_cast<std::size_t>(i)] && problem.empty()) {
                        problem = std::string(level) + " c" + std::to_string(b.id) + " line " + std::to_string(i) +
                                  ": '" + got + "', the report '" + b.lines[static_cast<std::size_t>(i)] + "'";
                    }
                }
            }
        }
        model->set_browser_level("full");
        expect(problem.empty(), "B4 " + num(lines) +
                                    " block lines at both levels, turned back into report form, equal the reports" +
                                    (problem.empty() ? godot::String() : ": " + to_godot(problem)));
    }

    // B5: follow, the walkthrough.
    int64_t followed_civ = 0;
    {
        for (const auto& b : full.blocks) {
            if (b.ended >= 0 && followed_civ == 0) {
                followed_civ = b.id;
            }
        }
        model->set_browser_group("all");
        model->set_browser_level("full");
        model->select_civilization(followed_civ);
        const bool follow_on = model->get_follow() && godot::String(selection(*model.ptr())["scope"]) == "entity";
        g_year_changes = 0;
        int64_t want_year_changes = 0;
        int64_t previous_year = model->get_year();
        std::vector<int64_t> visited;
        std::string problem;
        if (model->follow_first()) {
            do {
                const int64_t e = selected_event(*model.ptr());
                visited.push_back(e);
                const auto it = line_of.find(e);
                if (it == line_of.end() || model->get_year() != it->second->start) {
                    problem = problem.empty() ? "e" + std::to_string(e) + ": year " + std::to_string(model->get_year())
                                              : problem;
                }
                want_year_changes += model->get_year() != previous_year ? 1 : 0;
                previous_year = model->get_year();
            } while (model->step_event(1) && visited.size() < 100000);
        }
        const std::vector<int64_t>& want = civ_events[followed_civ];
        const std::string d = first_difference(visited, want);
        const bool years_counted = g_year_changes == want_year_changes;
        bool back_ok = false;
        if (want.size() >= 2) {
            back_ok = model->step_event(-1) && selected_event(*model.ptr()) == want[want.size() - 2];
        }
        expect(follow_on && d.empty() && problem.empty() && years_counted && back_ok,
               "B5 follow c" + num(followed_civ) + ": First event and event > visit its " +
                   num(static_cast<int64_t>(want.size())) + " events in order at their years, " + num(g_year_changes) +
                   " year_changed for " + num(want_year_changes) + " year changes, < event goes back one" +
                   to_godot(d.empty() ? "" : ": " + d) + to_godot(problem.empty() ? "" : ": " + problem));
        model->set_follow(false);
        const int64_t from = model->get_year();
        const auto next = own.history->timeline.next_event_year(from);
        model->step_event(1);
        expect(next.has_value() && model->get_year() == *next && model->get_follow() == false,
               "B5 with follow off the step from " + num(from) + " goes to the next event year " +
                   num(next.value_or(-1)) + ": " + num(model->get_year()));
    }

    // B6: links and the path.
    {
        int64_t start = 0;
        int64_t c1 = 0;
        int64_t c2 = 0;
        for (const auto& e : full.chronicle) {
            if (start != 0) {
                break;
            }
            for (const int64_t c : e.causes) {
                const auto it = line_of.find(c);
                if (it != line_of.end() && !it->second->causes.empty() && start == 0) {
                    start = e.id;
                    c1 = c;
                    c2 = it->second->causes.front();
                }
            }
        }
        const auto year_of = [&line_of](int64_t e) {
            const auto it = line_of.find(e);
            return it != line_of.end() ? it->second->start : int64_t{-1};
        };
        model->clear_selection();
        model->select_event(start);
        const bool at_start = selected_event(*model.ptr()) == start && model->get_year() == year_of(start);
        model->follow_link("e" + num(c1));
        const bool at_c1 = selected_event(*model.ptr()) == c1 && model->get_year() == year_of(c1);
        model->follow_link("e" + num(c2));
        const bool at_c2 = selected_event(*model.ptr()) == c2 && model->get_year() == year_of(c2);
        model->browser_back();
        model->browser_back();
        const bool back = selected_event(*model.ptr()) == start && model->get_year() == year_of(start);
        const bool forward = model->can_browser_forward() && model->browser_forward() &&
                             selected_event(*model.ptr()) == c1 && model->get_year() == year_of(c1);
        // A c link of the start event's participants.
        int64_t civ = 0;
        const godot::Array participants = model->get_event_detail(start)["participants"];
        for (int64_t i = 0; i < participants.size() && civ == 0; ++i) {
            const std::string t = to_utf8(godot::Dictionary(participants[i])["target"]);
            if (t.size() > 1 && t[0] == 'c' && t[1] >= '0' && t[1] <= '9') {
                civ = std::stoll(t.substr(1));
            }
        }
        civ = civ != 0 ? civ : full.blocks.front().id;
        const bool linked = model->follow_link("c" + num(civ)) &&
                            int64_t(selection(*model.ptr())["entity_id"]) == civ &&
                            int64_t(selection(*model.ptr())["entity_type"]) == 3;
        const bool inert = !model->follow_link("ra1") && !model->follow_link("g1") && !model->follow_link("m1") &&
                           !model->follow_link("si1") && !model->follow_link("x");
        expect(start != 0 && at_start && at_c1 && at_c2 && back && forward && linked && inert,
               "B6 e" + num(start) + ": its cause e" + num(c1) + " and that one's cause e" + num(c2) +
                   " by link, each at its year; Back twice returns to e" + num(start) + ", Forward to e" + num(c1) +
                   "; a c link selects c" + num(civ) + "; ra, g, m, si links are not followed");
    }

    // B7: an event's place.
    {
        std::string problem;
        const std::size_t n = full.chronicle.size();
        for (const std::size_t i : {std::size_t{0}, n / 2, n - 1}) {
            const ChronicleLine& e = full.chronicle[i];
            model->clear_selection();
            model->select_event(e.id);
            std::pair<int64_t, int64_t> want{-1, -1};
            if (e.settlement != 0 && full.settlement_cells.count(e.settlement) != 0) {
                want = full.settlement_cells.at(e.settlement);
            } else if (e.settlement == 0 && e.region != 0 && own.seeds.count(e.region) != 0) {
                want = own.seeds.at(e.region);
            }
            const godot::Vector2i place = model->get_highlight()["place"];
            const bool at = model->get_year() == e.start && place.x == want.first && place.y == want.second &&
                            screen.map_view()->get_event_place() == place;
            const int64_t outside = e.start > 0 ? e.start - 1 : e.end + 1;
            bool cleared = true;
            if (outside >= 0 && outside <= end) {
                model->set_year(outside);
                const godot::Vector2i gone = model->get_highlight()["place"];
                cleared = gone == godot::Vector2i(-1, -1) && selected_event(*model.ptr()) == e.id &&
                          screen.map_view()->get_event_place() == gone;
            }
            if ((!at || !cleared) && problem.empty()) {
                problem = "e" + std::to_string(e.id) + " place " + std::to_string(place.x) + " " +
                          std::to_string(place.y) + ", want " + std::to_string(want.first) + " " +
                          std::to_string(want.second) + (cleared ? "" : "; not cleared outside its span");
            }
        }
        expect(problem.empty(), "B7 the first, a middle and the last event: Y its year, the place its settlement's or "
                                "region seed's cell, none outside its span" +
                                    (problem.empty() ? godot::String() : ": " + to_godot(problem)));
    }

    // B8: map clicks.
    {
        model->clear_selection();
        model->set_browser_group("all");
        model->set_browser_level("full");
        model->set_year(end);
        screen.map_view()->fit_to_view();
        LayerMapView* view = screen.map_view();
        const auto click_marker = [&](const char* type) -> std::string {
            const godot::Array markers = model->get_markers();
            for (int64_t i = 0; i < markers.size(); ++i) {
                const godot::Dictionary m = markers[i];
                if (godot::String(m["type"]) != type) {
                    continue;
                }
                const godot::Vector2i c = view->get_marker_screen_position(i);
                const godot::Vector2 at(static_cast<float>(c.x) + 0.5F, static_cast<float>(c.y) + 0.5F);
                if (c.x < 0 || view->marker_at(at) != i) {
                    continue;
                }
                send_click(screen, at);
                const int64_t id = m["id"];
                const godot::Dictionary sel = model->get_selection();
                const auto list = own.history->timeline.events_of_settlement(id);
                const std::vector<int64_t> want(list.begin(), list.end());
                const std::string d = first_difference(ids_of(model->get_event_ids()), want);
                if (int64_t(sel["place_type"]) != 4 || int64_t(sel["place_id"]) != id || !d.empty()) {
                    return std::string(type) + " s" + std::to_string(id) + ": place " +
                           std::to_string(int64_t(sel["place_type"])) + " " + std::to_string(int64_t(sel["place_id"])) +
                           (d.empty() ? "" : ", list " + d);
                }
                return {};
            }
            return std::string("no ") + type + " marker to click";
        };
        const std::string standing = click_marker("standing");
        const std::string ruin = click_marker("ruin");
        expect(standing.empty() && ruin.empty(),
               "B8 a click on a standing and a ruin marker selects the settlement, the list its events" +
                   to_godot(standing.empty() ? "" : ": " + standing) + to_godot(ruin.empty() ? "" : ": " + ruin));
        // A ring: a year from E down with a ring whose band is hit.
        std::string ring = "no ring to click";
        for (int64_t y = end; y >= 0 && !ring.empty() && ring.rfind("no ring", 0) == 0; --y) {
            model->set_year(y);
            const godot::Array markers = model->get_markers();
            for (int64_t i = 0; i < markers.size(); ++i) {
                const godot::Dictionary m = markers[i];
                if (godot::String(m["type"]) != "place") {
                    continue;
                }
                const godot::Dictionary glyph = model->get_marker_glyph("ring");
                const int64_t r = glyph.get("radius", int64_t{0});
                const int64_t g = view->glyph_cell_size();
                const godot::Vector2i c = view->get_marker_screen_position(i);
                const godot::Vector2 at(static_cast<float>(c.x - r * g - g / 2) + 0.5F, static_cast<float>(c.y) + 0.5F);
                if (c.x < 0 || view->marker_at(at) != i) {
                    continue;
                }
                const godot::PackedInt64Array events = m["events"];
                int64_t smallest = events.is_empty() ? 0 : events[0];
                for (int64_t k = 0; k < events.size(); ++k) {
                    smallest = std::min(smallest, events[k]);
                }
                send_click(screen, at);
                ring = selected_event(*model.ptr()) == smallest
                           ? std::string()
                           : "ring in " + std::to_string(y) + ": selected e" +
                                 std::to_string(selected_event(*model.ptr())) + ", want e" + std::to_string(smallest);
                break;
            }
        }
        expect(ring.empty(), "B8 a click on a ring selects its smallest event" + to_godot(ring.empty() ? "" : ": " + ring));
        // A region cell away from every marker.
        model->set_year(end);
        view->fit_to_view();
        const godot::Rect2 shown = view->get_shown_rect();
        const godot::Ref<godot::Image> image = view->get_image();
        const float cell = image.is_valid() ? shown.size.x / static_cast<float>(image->get_width()) : 1.0F;
        int64_t region = 0;
        godot::Vector2 at(-1.0F, -1.0F);
        for (float y = shown.position.y + 3.0F; y < shown.position.y + shown.size.y && region == 0; y += 9.0F) {
            for (float x = shown.position.x + 3.0F; x < shown.position.x + shown.size.x && region == 0; x += 9.0F) {
                const auto cx = static_cast<int64_t>((x - shown.position.x) / cell);
                const auto cy = static_cast<int64_t>((y - shown.position.y) / cell);
                const int64_t r = model->get_region_at(cx, cy);
                if (r > 0 && view->marker_at(godot::Vector2(x, y)) < 0 && !model->get_region_holding(r).is_empty()) {
                    region = r;
                    at = godot::Vector2(x, y);
                }
            }
        }
        send_click(screen, at);
        const godot::Dictionary sel = model->get_selection();
        const auto list = own.history->timeline.events_of_region(region);
        const std::string d = first_difference(ids_of(model->get_event_ids()), {list.begin(), list.end()});
        expect(region > 0 && int64_t(sel["place_type"]) == 5 && int64_t(sel["place_id"]) == region && d.empty(),
               "B8 a click on a cell of region " + num(region) + " away from every marker selects the region, the "
               "list its events" + to_godot(d.empty() ? "" : ": " + d));
        // A drag pans and selects nothing.
        const godot::Dictionary before = model->get_selection();
        const godot::Vector2 position_before = view->get_shown_rect().position;
        send_button(screen, at, true);
        send_motion(screen, at + godot::Vector2(10.0F, 0.0F), godot::Vector2(10.0F, 0.0F));
        send_motion(screen, at + godot::Vector2(20.0F, 0.0F), godot::Vector2(10.0F, 0.0F));
        send_button(screen, at + godot::Vector2(20.0F, 0.0F), false);
        const godot::Vector2 moved = view->get_shown_rect().position - position_before;
        expect(model->get_selection() == before && moved.is_equal_approx(godot::Vector2(20.0F, 0.0F)),
               "B8 a press, a 20 px drag and a release selects nothing and pans by " + godot::Variant(moved).stringify());
        view->fit_to_view();
    }

    // B9: the highlight of the civilization of B5.
    {
        model->clear_selection();
        model->select_civilization(followed_civ);
        const auto expected = [&](int64_t year, std::vector<int64_t>& regions, std::vector<int64_t>& settlements) {
            regions.clear();
            settlements.clear();
            for (const auto& h : own.history->timeline.year_state(year).regions) {
                if (h.civ == followed_civ) {
                    regions.push_back(h.region);
                }
            }
            for (const auto& s :
                 own.markers->markers_at(*own.history, year, worldgen::inspect::MarkerOptions{}).settlements) {
                if ((s.status == 1 || s.status == 2) && s.owner == followed_civ) {
                    settlements.push_back(s.settlement);
                }
            }
            std::sort(settlements.begin(), settlements.end());
        };
        std::string problem;
        for (const int64_t y : options.marker_years) {
            model->set_year(y);
            const godot::Dictionary h = model->get_highlight();
            // From the model's own answers: the holdings and the markers.
            std::vector<int64_t> regions;
            for (const int64_t r : own.history->timeline.region_ids()) {
                if (int64_t(model->get_region_holding(r).get("civilization", int64_t{-1})) == followed_civ) {
                    regions.push_back(r);
                }
            }
            std::vector<int64_t> settlements;
            const godot::Array markers = model->get_markers();
            for (const auto& s : model->history_state().markers().settlements) {
                if ((s.status == 1 || s.status == 2) && s.owner == followed_civ) {
                    settlements.push_back(s.settlement);
                }
            }
            std::sort(settlements.begin(), settlements.end());
            int64_t standing_markers = 0;
            for (int64_t i = 0; i < markers.size(); ++i) {
                const godot::Dictionary m = markers[i];
                standing_markers += godot::String(m["type"]) == "standing" &&
                                            std::binary_search(settlements.begin(), settlements.end(), int64_t(m["id"]))
                                        ? 1
                                        : 0;
            }
            if ((ids_of(h["regions"]) != regions || ids_of(h["settlements"]) != settlements ||
                 standing_markers != static_cast<int64_t>(settlements.size()) ||
                 static_cast<int64_t>(screen.map_view()->gold_frames().size()) != standing_markers) &&
                problem.empty()) {
                problem = "year " + std::to_string(y) + ": regions " + list_text(ids_of(h["regions"])) + " want " +
                          list_text(regions) + ", settlements " + list_text(ids_of(h["settlements"])) + " want " +
                          list_text(settlements);
            }
        }
        {
            // The mask: dimmed exactly where the region layer is not a held region.
            const std::vector<uint8_t>& mask = model->highlight_mask();
            const godot::PackedInt64Array held = model->get_highlight()["regions"];
            int64_t wrong = 0;
            int64_t dimmed = 0;
            for (std::size_t i = 0; i < mask.size(); ++i) {
                const int64_t x = static_cast<int64_t>(i % model->highlight_mask_width());
                const int64_t y = static_cast<int64_t>(i / model->highlight_mask_width());
                const bool keep = held.has(model->get_region_at(x, y));
                dimmed += mask[i] != 0 ? 1 : 0;
                wrong += (mask[i] != 0) == keep ? 1 : 0;
            }
            expect(!mask.empty() && wrong == 0 && dimmed > 0 && screen.map_view()->has_mask(),
                   "B9 the dimming mask covers the " + num(dimmed) + " cells outside the held regions (" + num(wrong) +
                       " wrong), and the map view has it");
        }
        expect(problem.empty(), "B9 c" + num(followed_civ) + " at the marker years: the regions it holds and its "
                                "standing settlements, framed on the map" +
                                    (problem.empty() ? godot::String() : ": " + to_godot(problem)));
        model->set_year(0);
        std::vector<int64_t> prev_regions;
        std::vector<int64_t> prev_settlements;
        expected(0, prev_regions, prev_settlements);
        int64_t emitted = 0;
        int64_t want = 0;
        problem.clear();
        for (int64_t y = 1; y <= end; ++y) {
            g_highlight_changes = 0;
            model->set_year(y);
            std::vector<int64_t> regions;
            std::vector<int64_t> settlements;
            expected(y, regions, settlements);
            const bool differs = regions != prev_regions || settlements != prev_settlements;
            emitted += g_highlight_changes;
            want += differs ? 1 : 0;
            if ((g_highlight_changes == 1) != differs && problem.empty()) {
                problem = "year " + std::to_string(y) + ": " + std::to_string(g_highlight_changes) +
                          " highlight_changed, the lists " + (differs ? "differ" : "are the same");
            }
            prev_regions = std::move(regions);
            prev_settlements = std::move(settlements);
        }
        expect(problem.empty(), "B9 stepping 0 to " + num(end) + ": " + num(emitted) +
                                    " highlight_changed, in exactly the " + num(want) +
                                    " years whose lists differ from the year before" +
                                    (problem.empty() ? godot::String() : ": " + to_godot(problem)));
    }

    // B10: no rebuilds on a year change, except with This year on.
    {
        model->set_year(0);
        g_list_changes = 0;
        for (int64_t y = 1; y <= end; ++y) {
            model->set_year(y);
        }
        const int64_t without = g_list_changes;
        model->set_year(0);
        model->set_browser_this_year(true);
        worldgen::inspect::EventFilter f;
        f.civilization = followed_civ;
        f.summary_only = false;
        std::vector<int64_t> previous;
        f.from_year = 0;
        f.to_year = 0;
        own.browser->select(*own.history, f, previous);
        std::string problem;
        int64_t emitted = 0;
        int64_t want = 0;
        std::vector<int64_t> now;
        for (int64_t y = 1; y <= end; ++y) {
            g_list_changes = 0;
            model->set_year(y);
            f.from_year = y;
            f.to_year = y;
            own.browser->select(*own.history, f, now);
            const bool differs = now != previous;
            emitted += g_list_changes;
            want += differs ? 1 : 0;
            if (((g_list_changes == 1) != differs || ids_of(model->get_event_ids()) != now) && problem.empty()) {
                problem = "year " + std::to_string(y) + ": " + std::to_string(g_list_changes) +
                          " event_list_changed, the ids " + (differs ? "differ" : "are the same");
            }
            std::swap(previous, now);
        }
        model->set_browser_this_year(false);
        expect(without == 0 && problem.empty(),
               "B10 stepping 0 to " + num(end) + " with c" + num(followed_civ) + " selected: " + num(without) +
                   " event_list_changed; with This year on " + num(emitted) + ", in exactly the " + num(want) +
                   " years whose ids differ" + (problem.empty() ? godot::String() : ": " + to_godot(problem)));
    }

    // B11: search.
    {
        std::string problem;
        for (const auto& b : full.blocks) {
            std::string q = b.name.substr(0, 3);
            for (char& ch : q) {
                ch = ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch - 'A' + 'a') : ch;
            }
            const godot::String query = to_godot(q);
            screen.browser_search()->set_text(query);
            screen.browser_search()->emit_signal("text_changed", query);
            const godot::Array rows = model->search_names(query);
            int64_t found = -1;
            for (int64_t i = 0; i < rows.size() && found < 0; ++i) {
                if (godot::String(godot::Dictionary(rows[i])["target"]) == "c" + num(b.id)) {
                    found = i;
                }
            }
            if (found < 0 || screen.search_results()->get_item_count() != rows.size() ||
                !screen.search_results()->is_visible() || screen.civ_list()->is_visible()) {
                problem = problem.empty() ? "'" + q + "' does not list c" + std::to_string(b.id) : problem;
                continue;
            }
            screen.search_results()->emit_signal("item_selected", found);
            const godot::Dictionary sel = model->get_selection();
            if ((int64_t(sel["entity_type"]) != 3 || int64_t(sel["entity_id"]) != b.id) && problem.empty()) {
                problem = "selecting the result of '" + q + "' does not select c" + std::to_string(b.id);
            }
        }
        screen.browser_search()->set_text("");
        screen.browser_search()->emit_signal("text_changed", godot::String());
        const bool empty = model->search_names("").is_empty() && screen.search_results()->get_item_count() == 0 &&
                           !screen.search_results()->is_visible() && screen.civ_list()->is_visible();
        expect(problem.empty() && empty, "B11 the first three letters of each civilization's name find it, and "
                                         "selecting the result selects it; an empty query gives no rows" +
                                             (problem.empty() ? godot::String() : ": " + to_godot(problem)));
    }

    // B12: keys with no control focused.
    {
        screen.get_viewport()->gui_release_focus();
        model->clear_selection();
        model->select_civilization(followed_civ);
        const bool follow_before = model->get_follow();
        send_key(screen, godot::KEY_F, false);
        const bool toggled = model->get_follow() != follow_before;
        send_key(screen, godot::KEY_F, false);
        const bool restored = model->get_follow() == follow_before;
        const int64_t first = full.chronicle.front().id;
        model->select_event(first);
        send_key(screen, godot::KEY_BACKSPACE, false);
        const godot::Dictionary after_back = model->get_selection();
        const bool back = int64_t(after_back["event"]) == 0 && int64_t(after_back["entity_id"]) == followed_civ;
        send_key(screen, godot::KEY_ESCAPE, false);
        const godot::Dictionary cleared = model->get_selection();
        const bool escape = int64_t(cleared["entity_id"]) == 0 && int64_t(cleared["event"]) == 0 &&
                            int64_t(cleared["place_id"]) == 0 && godot::String(cleared["scope"]) == "all";
        send_key(screen, godot::KEY_B, false);
        const bool hidden = !screen.right_pane()->is_visible();
        send_key(screen, godot::KEY_B, false);
        const bool shown = screen.right_pane()->is_visible();
        expect(toggled && restored && back && escape && hidden && shown,
               godot::String("B12 keys: F toggles follow (") + (toggled && restored ? "ok" : "no") + "), Backspace goes "
               "back (" + (back ? "ok" : "no") + "), Escape clears the selection (" + (escape ? "ok" : "no") +
                   "), B hides and shows the right pane (" + (hidden && shown ? "ok" : "no") + ")");
    }

    // B13 (P5-04 11.2): the kind group alliances, last in the group menu,
    // filters the list to its kinds; allied_battle stays in war and
    // mine_opened in settlements; a realm's block shows its allies lines.
    {
        model->clear_selection();
        model->set_browser_level("full");
        const godot::PackedStringArray groups = model->get_event_groups();
        std::vector<std::string> want_groups;
        for (std::uint32_t code = 0; code < 32U; ++code) {
            if ((worldgen::inspect::ALL_GROUPS & (std::uint32_t{1} << code)) != 0U) {
                want_groups.emplace_back(
                    worldgen::inspect::event_group_word(static_cast<worldgen::inspect::EventGroup>(code)));
            }
        }
        std::vector<std::string> got_groups;
        for (int64_t i = 0; i < groups.size(); ++i) {
            got_groups.push_back(to_utf8(groups[i]));
        }
        godot::OptionButton* menu = screen.event_group();
        // P5-06 11.3: nine groups, alliances (7) before figures, the last.
        const int32_t last = 8;
        const bool listed = got_groups == want_groups && got_groups.size() == 9 && got_groups[7] == "alliances" &&
                            got_groups.back() == "figures" && menu->get_item_count() == groups.size() + 1 &&
                            menu->get_item_text(last) == "alliances" && menu->get_item_text(9) == "figures";
        menu->select(last);
        menu->emit_signal("item_selected", last);  // as a pick in the menu
        std::vector<int64_t> want;
        std::vector<int64_t> war_kinds;
        std::vector<int64_t> settlement_kinds;
        for (const auto& e : full.chronicle) {
            const auto group = worldgen::inspect::event_group(e.kind);
            if (group == worldgen::inspect::EventGroup::alliances) {
                want.push_back(e.id);
            }
            if (e.kind == "allied_battle" && group != worldgen::inspect::EventGroup::war) {
                war_kinds.push_back(e.id);
            }
            if (e.kind == "mine_opened" && group != worldgen::inspect::EventGroup::settlements) {
                settlement_kinds.push_back(e.id);
            }
        }
        const std::string d = first_difference(ids_of(model->get_event_ids()), want);
        const bool rows = screen.event_list()->get_item_count() == static_cast<int64_t>(want.size());
        expect(listed && d.empty() && rows && !want.empty() && war_kinds.empty() && settlement_kinds.empty(),
               "B13 the group menu lists " + num(static_cast<int64_t>(got_groups.size())) +
                   " groups, alliances before figures (last); picking alliances lists the " + num(static_cast<int64_t>(want.size())) +
                   " chronicle lines of its kinds" + to_godot(d.empty() ? "" : ": " + d) +
                   (war_kinds.empty() && settlement_kinds.empty()
                        ? godot::String()
                        : godot::String(": allied_battle outside war or mine_opened outside settlements")));
        menu->select(0);
        menu->emit_signal("item_selected", 0);
        // The allies lines of the first block that has an ally, at full.
        std::string problem = "no block with 'allies: <n>' above 0";
        int64_t shown = 0;
        for (const auto& b : full.blocks) {
            std::size_t at = b.lines.size();
            for (std::size_t i = 0; i < b.lines.size() && at == b.lines.size(); ++i) {
                if (b.lines[i].rfind("  allies: ", 0) == 0 && b.lines[i] != "  allies: 0") {
                    at = i;
                }
            }
            if (at == b.lines.size()) {
                continue;
            }
            const godot::Array block = model->get_civilization_block(b.id);
            problem.clear();
            for (std::size_t i = at; i < b.lines.size() && (i == at || b.lines[i].rfind("    c", 0) == 0); ++i) {
                const std::string got = static_cast<int64_t>(i) < block.size() ? report_form(block[static_cast<int64_t>(i)]) : std::string();
                if (got != b.lines[i] && problem.empty()) {
                    problem = "c" + std::to_string(b.id) + " line " + std::to_string(i) + ": '" + got + "', the report '" +
                              b.lines[i] + "'";
                }
                ++shown;
            }
            break;
        }
        expect(problem.empty(), "B13 a realm's block shows its " + num(shown) + " allies lines as the report" +
                                    (problem.empty() ? godot::String() : ": " + to_godot(problem)));
    }

    // B14 (P5-06 11.3): figures. The group figures, last in the menu, lists
    // the chronicle lines of its kinds; the Figures tab lists the index's
    // figures; an f link in an event's sentence is a link and following it
    // selects the figure (tab, row, follow, its events, its block); search
    // finds a figure by its display name.
    {
        model->clear_selection();
        model->set_browser_level("full");
        godot::OptionButton* menu = screen.event_group();
        const int32_t last = menu->get_item_count() - 1;
        menu->select(last);
        menu->emit_signal("item_selected", last);
        std::vector<int64_t> want;
        for (const auto& e : full.chronicle) {
            if (worldgen::inspect::event_group(e.kind) == worldgen::inspect::EventGroup::figures) {
                want.push_back(e.id);
            }
        }
        const std::string d = first_difference(ids_of(model->get_event_ids()), want);
        const bool group_word = godot::String(model->get_selection()["group"]) == "figures";
        expect(d.empty() && group_word && !want.empty() &&
                   screen.event_list()->get_item_count() == static_cast<int64_t>(want.size()),
               "B14 picking figures, the last group, lists the " + num(static_cast<int64_t>(want.size())) +
                   " chronicle lines of its kinds" + to_godot(d.empty() ? "" : ": " + d));
        menu->select(0);
        menu->emit_signal("item_selected", 0);

        // The Figures tab.
        const auto figures = own.browser->figures();
        std::string problem;
        godot::TreeItem* root = screen.figure_list()->get_root();
        int64_t rows = 0;
        for (godot::TreeItem* row = root != nullptr ? root->get_first_child() : nullptr; row != nullptr;
             row = row->get_next(), ++rows) {
            const auto i = static_cast<std::size_t>(rows);
            if (i >= figures.size() || int64_t(row->get_metadata(0)) != figures[i].id ||
                to_utf8(row->get_text(0)) != figures[i].display_name) {
                problem = problem.empty() ? "row " + std::to_string(rows) + " '" + to_utf8(row->get_text(0)) + "'" : problem;
            }
        }
        const bool tabs = screen.entity_tabs()->get_tab_count() == 2 && screen.entity_tabs()->get_tab_title(1) == "Figures";
        expect(problem.empty() && rows == static_cast<int64_t>(figures.size()) && rows > 0 && tabs,
               "B14 the Figures tab lists the " + num(rows) + " figures of the index (" +
                   num(static_cast<int64_t>(figures.size())) + "), ascending id, by display name" +
                   (problem.empty() ? godot::String() : ": " + to_godot(problem)));

        // An f link: the first event of the chronicle whose sentence links a figure.
        int64_t event = 0;
        int64_t figure = 0;
        for (const auto& e : full.chronicle) {
            const auto detail = own.browser->event_detail(*own.history, e.id);
            for (const auto& link : detail.sentence.links) {
                if (link.type == 6 && figure == 0) {
                    figure = link.id;
                    event = e.id;
                }
            }
            if (figure != 0) {
                break;
            }
        }
        const godot::String target = "f" + num(figure);
        bool linked = false;
        if (event != 0) {
            model->select_event(event);
            const godot::Array links = godot::Dictionary(model->get_event_detail(event)["sentence"])["links"];
            for (int64_t i = 0; i < links.size(); ++i) {
                linked = linked || godot::String(godot::Dictionary(links[i])["target"]) == target;
            }
            screen.browser_detail()->emit_signal("meta_clicked", target);  // as a click on the link
        }
        const godot::Dictionary sel = model->get_selection();
        const bool selected = int64_t(sel["entity_type"]) == 6 && int64_t(sel["entity_id"]) == figure &&
                              int64_t(sel["event"]) == 0 && godot::String(sel["scope"]) == "entity" &&
                              bool(sel["follow"]) && model->can_follow_link(target);
        godot::TreeItem* chosen = screen.figure_list()->get_selected();
        const bool shown = screen.entity_tabs()->get_current_tab() == 1 && screen.figure_list()->is_visible() &&
                           !screen.civ_list()->is_visible() && chosen != nullptr &&
                           int64_t(chosen->get_metadata(0)) == figure;
        worldgen::inspect::EventFilter filter;
        filter.figure = figure;
        filter.summary_only = false;
        std::vector<int64_t> want_events;
        if (figure != 0) {
            own.browser->select(*own.history, filter, want_events);
        }
        const std::string de = first_difference(ids_of(model->get_event_ids()), want_events);
        std::string block_problem;
        if (figure != 0) {
            const auto lines = own.browser->figure_block(*own.history, figure, worldgen::inspect::HistoryLevel::full);
            const godot::Array block = model->get_figure_block(figure);
            for (std::size_t i = 0; i < lines.size() || static_cast<int64_t>(i) < block.size(); ++i) {
                const std::string got = static_cast<int64_t>(i) < block.size()
                                            ? to_utf8(godot::Dictionary(block[static_cast<int64_t>(i)])["text"])
                                            : std::string("none");
                const std::string w = i < lines.size() ? lines[i].text : std::string("none");
                if (got != w && block_problem.empty()) {
                    block_problem = "line " + std::to_string(i) + ": '" + got + "', expected '" + w + "'";
                }
            }
            const godot::String detail = screen.browser_detail()->get_parsed_text();
            if (block_problem.empty() && (lines.empty() || !detail.begins_with(to_godot(lines.front().text)))) {
                block_problem = "the detail does not start with the block: " + to_utf8(detail.substr(0, 80));
            }
        }
        expect(event != 0 && linked && selected && shown && de.empty() && !want_events.empty() && block_problem.empty(),
               "B14 e" + num(event) + "'s sentence links " + target + "; following it selects the figure (" +
                   (selected ? "ok" : "no") + "), shows the Figures tab with its row (" + (shown ? "ok" : "no") +
                   "), lists its " + num(static_cast<int64_t>(want_events.size())) + " events" +
                   to_godot(de.empty() ? "" : ": " + de) + " and shows its block" +
                   to_godot(block_problem.empty() ? "" : ": " + block_problem));

        // Search by display name, and a click on a row of the list.
        std::string search_problem = figure == 0 ? "no figure" : "";
        if (figure != 0) {
            model->clear_selection();
            std::string name;
            for (const auto& f : figures) {
                name = f.id == figure ? f.display_name : name;
            }
            const godot::String query = to_godot(name);
            screen.browser_search()->set_text(query);
            screen.browser_search()->emit_signal("text_changed", query);
            const godot::Array found = model->search_names(query);
            int64_t at = -1;
            for (int64_t i = 0; i < found.size() && at < 0; ++i) {
                const godot::Dictionary r = found[i];
                at = godot::String(r["target"]) == target && godot::String(r["kind"]) == "figure" ? i : -1;
            }
            if (at < 0 || !screen.search_results()->is_visible() || screen.figure_list()->is_visible()) {
                search_problem = "'" + name + "' does not list " + to_utf8(target);
            } else {
                screen.search_results()->emit_signal("item_selected", at);
                const godot::Dictionary s = model->get_selection();
                if (int64_t(s["entity_type"]) != 6 || int64_t(s["entity_id"]) != figure) {
                    search_problem = "selecting the result does not select " + to_utf8(target);
                }
            }
            screen.browser_search()->set_text("");
            screen.browser_search()->emit_signal("text_changed", godot::String());
            // A click on the first row of the list selects that figure.
            model->clear_selection();
            godot::TreeItem* first = root != nullptr ? root->get_first_child() : nullptr;
            if (first != nullptr) {
                first->select(0);
                screen.figure_list()->emit_signal("item_selected");  // as a click on the row
                const godot::Dictionary s = model->get_selection();
                if ((int64_t(s["entity_type"]) != 6 || int64_t(s["entity_id"]) != figures.front().id) &&
                    search_problem.empty()) {
                    search_problem = "a click on the first row does not select f" + std::to_string(figures.front().id);
                }
            }
        }
        expect(search_problem.empty() && screen.figure_list()->is_visible(),
               "B14 searching a figure's display name lists it (kind figure) and selecting the result selects it; a "
               "click on a row of the Figures tab selects that figure" +
                   (search_problem.empty() ? godot::String() : ": " + to_godot(search_problem)));
        model->clear_selection();
        screen.entity_tabs()->set_current_tab(0);
        model->set_browser_level("summary");
    }

    model->disconnect("year_changed", on_year);
    model->disconnect("event_list_changed", on_list);
    model->disconnect("highlight_changed", on_highlight);
    model->clear_selection();
    model->set_year(end);
}

}  // namespace unmapped
