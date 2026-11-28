#include "viewer/viewer_check.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <optional>
#include <span>
#include <exception>
#include <map>
#include <regex>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/h_slider.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/check_box.hpp>
#include <godot_cpp/classes/check_button.hpp>
#include <godot_cpp/classes/item_list.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/spin_box.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/tree_item.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/char_string.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "viewer/viewer_check_browser.hpp"
#include "viewer/viewer_check_playback.hpp"
#include "viewer/viewer_check_reference.hpp"
#include "viewer/viewer_screen.hpp"
#include "world_file.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/history/digest.hpp"
#include "worldgen/history/event_catalogue.hpp"
#include "worldgen/history/timeline.hpp"
#include "worldgen/inspect/markers.hpp"
#include "worldgen/inspect/layer_catalogue.hpp"
#include "worldgen/inspect/territory.hpp"

namespace unmapped {

namespace {

class Checker {
public:
    void expect(bool ok, const godot::String& what) {
        godot::UtilityFunctions::print(godot::String(ok ? "  ok    " : "  FAIL  ") + what);
        if (!ok) {
            failed_ = true;
        }
    }
    [[nodiscard]] bool failed() const { return failed_; }

private:
    bool failed_ = false;
};

int64_t tree_row_count(godot::Tree* tree) {
    godot::TreeItem* root = tree->get_root();
    return root == nullptr ? 0 : root->get_child_count();
}

// Compares the viewer's image with the PNG that worldgen-cli wrote.
godot::String compare_images(const godot::Ref<godot::Image>& shown, const godot::String& png_path) {
    if (shown.is_null()) {
        return "the map view has no image";
    }
    godot::Ref<godot::Image> reference = godot::Image::load_from_file(png_path);
    if (reference.is_null()) {
        return "cannot load " + png_path;
    }
    if (reference->get_format() != godot::Image::FORMAT_RGB8) {
        reference->convert(godot::Image::FORMAT_RGB8);
    }
    if (shown->get_format() != godot::Image::FORMAT_RGB8) {
        return "the map view's image is not RGB8";
    }
    if (shown->get_size() != reference->get_size()) {
        return godot::String("size {0} differs from the reference {1}")
            .format(godot::Array::make(shown->get_size(), reference->get_size()));
    }
    const godot::PackedByteArray a = shown->get_data();
    const godot::PackedByteArray b = reference->get_data();
    if (a.size() != b.size()) {
        return "pixel buffers differ in size";
    }
    for (int64_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) {
            const int64_t pixel = i / 3;
            const int64_t width = shown->get_width();
            return godot::String("first difference at pixel ({0}, {1}), channel {2}: {3} vs reference {4}")
                .format(godot::Array::make(pixel % width, pixel / width, i % 3, a[i], b[i]));
        }
    }
    return {};
}

// Pixels of an RGB8 image equal to (r, g, b); -1 without an RGB8 image.
int64_t count_rgb(const godot::Ref<godot::Image>& image, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    if (image.is_null() || image->get_format() != godot::Image::FORMAT_RGB8) {
        return -1;
    }
    const godot::PackedByteArray data = image->get_data();
    const std::uint8_t* bytes = data.ptr();
    int64_t count = 0;
    for (int64_t i = 0; i + 2 < data.size(); i += 3) {
        if (bytes[i] == r && bytes[i + 1] == g && bytes[i + 2] == b) {
            ++count;
        }
    }
    return count;
}

// Whether the legend has a row labelled label with the single colour colour.
bool legend_has(const godot::Array& legend, const godot::String& label, int64_t colour) {
    for (int64_t i = 0; i < legend.size(); ++i) {
        const godot::Dictionary row = legend[i];
        const godot::PackedInt32Array colours = row["colours"];
        if (godot::String(row["label"]) == label && colours.size() == 1 && colours[0] == colour) {
            return true;
        }
    }
    return false;
}

// Every biome row (land rows only for a composite) is in the legend with
// its name and colour; "" or the first one missing.
godot::String missing_biome(const godot::Array& legend, const godot::Array& biomes, bool land_only) {
    for (int64_t i = 0; i < biomes.size(); ++i) {
        const godot::Dictionary biome = biomes[i];
        if (land_only && int64_t(biome["water"]) != 0) {
            continue;
        }
        if (!legend_has(legend, biome["name"], int64_t(biome["colour"]) & 0xFFFFFF)) {
            return biome["name"];
        }
    }
    return {};
}

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

// --- The history report, as the check reads it (P4-03 10.2) -------------------

struct ReportAge {
    int64_t id = 0, start = 0, end = 0, race = 0;
    std::string name;
};
struct ReportPhase {
    int64_t id = 0, start = 0, end = 0;
    std::string kind;
};
struct ReportCiv {
    int64_t id = 0, race = 0, founded = -1, ended = -1;
    std::string name;
};
// A settlement line of [present] (P4-04 9.2 M2).
struct ReportSettlement {
    int64_t id = 0, population = 0, owner = 0, x = 0, y = 0, founded = -1, ruined = -1, ruin_event = 0;
    std::string name, kind, status;
};
// A chronicle line (P4-04 9.2 M3, M4, M7).
struct ReportEvent {
    int64_t id = 0, start = 0, end = 0, settlement = 0, region = 0;
    std::string kind, sentence;
};
struct HistoryReport {
    std::string problem;  // empty when read
    std::vector<ReportSettlement> settlements;  // [present], ascending id
    std::vector<ReportEvent> events;            // [chronicle], ascending id
    std::map<int64_t, int64_t> capitals;        // civ -> settlement ([civilizations] capital lines)
    int64_t end_year = -1;
    int64_t last_event_year = -1;
    int64_t first_ceded = -1;  // F: the start year of the first region_ceded event
    std::vector<ReportAge> ages;
    std::vector<ReportPhase> phases;
    std::vector<ReportCiv> civs;                     // ascending id
    std::map<int64_t, std::string> race_plural;      // from [ages]
    std::vector<std::pair<int64_t, int64_t>> present;  // (civ, regions) of [present]

    [[nodiscard]] const ReportCiv* civ(int64_t id) const {
        for (const auto& c : civs) {
            if (c.id == id) {
                return &c;
            }
        }
        return nullptr;
    }
    [[nodiscard]] bool alive(const ReportCiv& c, int64_t year) const {
        return c.founded <= year && (c.ended < 0 || c.ended > year);
    }
};

int64_t to_i64(const std::string& text) {
    return std::stoll(text);
}

// world_path: the world, read for the events of same-day group lines
// (P5-04a 12.4), which print no {...}.
HistoryReport read_history_report(const godot::String& path, const godot::String& world_path) {
    HistoryReport report;
    if (path.is_empty() || !godot::FileAccess::file_exists(path)) {
        report.problem = "no history report at " + to_utf8(path);
        return report;
    }
    const std::string text = to_utf8(godot::FileAccess::get_file_as_string(path));
    static const std::regex section_re(R"(^\[([a-z]+)\]$)");
    static const std::regex age_re(R"(^a([0-9]+) (.+): ([0-9]+)-([0-9]+), [0-9]+ years?; (.+) \(ra([0-9]+)\) made by)");
    static const std::regex phase_re(R"(^p([0-9]+) a[0-9]+ ([a-z_]+) ([0-9]+)-([0-9]+),)");
    static const std::regex event_re(R"(^([0-9]+)(-[0-9]+)? e[0-9]+ ([a-z_]+)/)");
    static const std::regex civ_re(R"(^c([0-9]+) (.+)$)");
    static const std::regex race_re(R"(^  race: .* \(ra([0-9]+)\)$)");
    static const std::regex founded_re(R"(^  founded: ([0-9]+) )");
    static const std::regex ended_re(R"(^  fate: ended in ([0-9]+))");
    static const std::regex present_re(R"(^c([0-9]+) .*: ([0-9]+) regions?, )");
    static const std::regex end_re(R"(^end_year = ([0-9]+)$)");
    static const std::regex full_event_re(
        R"(^([0-9]+)(?:-([0-9]+))? e([0-9]+) ([a-z_]+)/[0-9]+: (.*) \{([^{}]*)\}( \[causes [^\]]*\])?$)");
    static const std::regex tag_s_re(R"((?:^| )@s([0-9]+)(?: |$))");
    static const std::regex tag_rg_re(R"((?:^| )@rg([0-9]+)(?: |$))");
    static const std::regex capital_re(R"(^  capital: .* \(s([0-9]+)\)$)");
    static const std::regex settlement_re(
        R"(^  s([0-9]+) (.+?): ([a-z]+), ([a-z]+), population ([0-9]+), (?:no owner|.+? \(c([0-9]+)\)), at ([0-9]+) ([0-9]+) .*, founded ([0-9]+) by)");
    static const std::regex ruined_re(R"(, ruined ([0-9]+)(?:, e([0-9]+))?,)");
    std::string section;
    std::size_t start = 0;
    std::vector<int64_t> grouped;
    try {
        while (start < text.size()) {
            std::size_t stop = text.find('\n', start);
            if (stop == std::string::npos) {
                stop = text.size();
            }
            const std::string line = text.substr(start, stop - start);
            start = stop + 1;
            std::smatch m;
            check_ref::GroupLine group;
            if (std::regex_match(line, m, section_re)) {
                section = m[1];
            } else if (section == "chronicle" && check_ref::parse_group_line(line, group)) {
                report.last_event_year = std::max(report.last_event_year, group.start);
                for (const int64_t id : group.ids) {
                    ReportEvent e;
                    e.id = id;
                    e.start = group.start;
                    e.end = group.end;
                    e.kind = group.kind;
                    e.sentence = group.text;
                    grouped.push_back(id);
                    report.events.push_back(std::move(e));
                }
            } else if (section == "ages" && std::regex_search(line, m, age_re)) {
                report.ages.push_back(ReportAge{to_i64(m[1]), to_i64(m[3]), to_i64(m[4]), to_i64(m[6]), m[2]});
                report.race_plural[to_i64(m[6])] = m[5];
            } else if (section == "phases" && std::regex_search(line, m, phase_re)) {
                report.phases.push_back(ReportPhase{to_i64(m[1]), to_i64(m[3]), to_i64(m[4]), m[2]});
            } else if (section == "chronicle" && std::regex_search(line, m, event_re)) {
                report.last_event_year = std::max(report.last_event_year, to_i64(m[1]));
                if (report.first_ceded < 0 && m[3] == "region_ceded") {
                    report.first_ceded = to_i64(m[1]);
                }
                std::smatch f;
                if (std::regex_match(line, f, full_event_re)) {
                    ReportEvent e;
                    e.start = to_i64(f[1]);
                    e.end = f[2].matched ? to_i64(f[2]) : e.start;
                    e.id = to_i64(f[3]);
                    e.kind = f[4];
                    e.sentence = f[5];
                    const std::string tags = f[6];
                    std::smatch t;
                    if (std::regex_search(tags, t, tag_s_re)) {
                        e.settlement = to_i64(t[1]);
                    }
                    if (std::regex_search(tags, t, tag_rg_re)) {
                        e.region = to_i64(t[1]);
                    }
                    report.events.push_back(std::move(e));
                }
            } else if (section == "civilizations") {
                if (std::regex_match(line, m, civ_re)) {
                    report.civs.push_back(ReportCiv{to_i64(m[1]), 0, -1, -1, m[2]});
                } else if (!report.civs.empty() && std::regex_match(line, m, race_re)) {
                    report.civs.back().race = to_i64(m[1]);
                } else if (!report.civs.empty() && std::regex_search(line, m, founded_re)) {
                    report.civs.back().founded = to_i64(m[1]);
                } else if (!report.civs.empty() && std::regex_search(line, m, ended_re)) {
                    report.civs.back().ended = to_i64(m[1]);
                } else if (!report.civs.empty() && std::regex_match(line, m, capital_re)) {
                    report.capitals[report.civs.back().id] = to_i64(m[1]);
                }
            } else if (section == "present" && std::regex_search(line, m, present_re)) {
                report.present.emplace_back(to_i64(m[1]), to_i64(m[2]));
            } else if (section == "present" && std::regex_search(line, m, settlement_re)) {
                ReportSettlement r;
                r.id = to_i64(m[1]);
                r.name = m[2];
                r.kind = m[3];
                r.status = m[4];
                r.population = to_i64(m[5]);
                r.owner = m[6].matched ? to_i64(m[6]) : 0;
                r.x = to_i64(m[7]);
                r.y = to_i64(m[8]);
                r.founded = to_i64(m[9]);
                std::smatch ruin;
                if (std::regex_search(line, ruin, ruined_re)) {
                    r.ruined = to_i64(ruin[1]);
                    r.ruin_event = ruin[2].matched ? to_i64(ruin[2]) : 0;
                }
                report.settlements.push_back(std::move(r));
            } else if (section == "stats" && std::regex_match(line, m, end_re)) {
                report.end_year = to_i64(m[1]);
            }
        }
    } catch (const std::exception& e) {
        report.problem = std::string("cannot read the history report: ") + e.what();
    }
    std::sort(report.events.begin(), report.events.end(),
              [](const ReportEvent& a, const ReportEvent& b) { return a.id < b.id; });
    if (report.problem.empty() && !grouped.empty()) {
        std::string problem;
        const std::map<int64_t, check_ref::EventRefs> refs = check_ref::event_refs(world_path, grouped, problem);
        if (!problem.empty()) {
            report.problem = "the events of the history report's group lines: " + problem;
        }
        for (ReportEvent& e : report.events) {
            const auto it = refs.find(e.id);
            if (it != refs.end()) {
                e.region = it->second.region;
                e.settlement = it->second.settlement;
            }
        }
    }
    if (report.problem.empty() && (report.end_year < 0 || report.ages.empty() || report.phases.empty() ||
                                   report.civs.empty() || report.last_event_year < 0 ||
                                   report.first_ceded < 0)) {
        report.problem =
            "the history report lacks [stats] end_year, [ages], [phases], [civilizations], events or region_ceded";
    }
    return report;
}

// The %YearLabel text of P4-03 4.4 from the report's ages and phases.
godot::String expected_year_label(const HistoryReport& report, int64_t year) {
    std::string text = "Year " + std::to_string(year) + " of " + std::to_string(report.end_year);
    if (year == report.end_year) {
        text += " (the present)";
    }
    // The present belongs to the last age (P4-01 4: the ages tile [0, E - 1]).
    for (const auto& age : report.ages) {
        if ((age.start <= year && year <= age.end) || (year == report.end_year && &age == &report.ages.back())) {
            text += "   |   " + age.name;
        }
    }
    bool phase = false;
    for (const auto& p : report.phases) {
        if (p.start <= year && year <= p.end) {
            text += "   |   phase p" + std::to_string(p.id) + " " + p.kind + ", " + std::to_string(p.start) + "-" +
                    std::to_string(p.end);
            phase = true;
        }
    }
    if (!phase) {
        text += "   |   no phase";
    }
    return to_godot(text);
}

// The expected head of the territory legend at a year (6.4 item 1): race
// headings (plural, no colours) and civilization names.
std::vector<std::pair<std::string, bool>> expected_legend_head(const HistoryReport& report, int64_t year) {
    std::vector<std::pair<std::string, bool>> rows;  // (label, is heading)
    std::set<int64_t> races;
    for (const auto& c : report.civs) {
        if (report.alive(c, year)) {
            races.insert(c.race);
        }
    }
    for (const int64_t race : races) {
        const auto plural = report.race_plural.find(race);
        rows.emplace_back(plural != report.race_plural.end() ? plural->second : "ra" + std::to_string(race), true);
        for (const auto& c : report.civs) {
            if (c.race == race && report.alive(c, year)) {
                rows.emplace_back(c.name, false);
            }
        }
    }
    return rows;
}

// The rows of the legend before "border between holders", as (label, is
// heading); "" when it matches, else the first difference.
std::string legend_head_difference(const godot::Array& legend, const std::vector<std::pair<std::string, bool>>& want) {
    std::vector<std::pair<std::string, bool>> got;
    for (int64_t i = 0; i < legend.size(); ++i) {
        const godot::Dictionary row = legend[i];
        const std::string label = to_utf8(row["label"]);
        if (label == "border between holders") {
            break;
        }
        got.emplace_back(label, godot::PackedInt32Array(row["colours"]).is_empty());
    }
    for (std::size_t i = 0; i < std::max(got.size(), want.size()); ++i) {
        if (i >= got.size() || i >= want.size() || got[i] != want[i]) {
            return "row " + std::to_string(i) + ": " +
                   (i < got.size() ? "'" + got[i].first + "'" + (got[i].second ? " (heading)" : "") : "none") +
                   ", expected " +
                   (i < want.size() ? "'" + want[i].first + "'" + (want[i].second ? " (heading)" : "") : "none");
        }
    }
    return {};
}

int64_t g_year_changed = 0;
void count_year_changed(int64_t /*year*/) {
    ++g_year_changed;
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

int64_t usec_now() {
    return static_cast<int64_t>(godot::Time::get_singleton()->get_ticks_usec());
}

// --- N1 to N3 -------------------------------------------------------------------

void check_without_history(Checker& check, ViewerScreen& screen, const godot::String& hover_text) {
    godot::Ref<ViewerModel> model = screen.get_model();
    bool no_territory = true;
    for (int64_t i = 0; i < model->get_view_count(); ++i) {
        no_territory = no_territory && model->get_view_info(i)["name"] != godot::Variant("territory");
    }
    check.expect(model->get_history_status() == "none" && no_territory && model->get_territory_view() == -1,
                 "N1 history status " + model->get_history_status() + ", no territory view");
    bool disabled = !screen.year_slider()->is_editable() && !screen.year_field()->is_editable();
    for (const godot::Button* button : screen.year_buttons()) {
        disabled = disabled && button->is_disabled();
    }
    check.expect(disabled, "N2 every control of the year row is disabled");
    check.expect(screen.year_label()->get_text() == "No history in this world." &&
                     screen.year_counts()->get_text().is_empty(),
                 "N2 year label '" + screen.year_label()->get_text() + "', counts '" +
                     screen.year_counts()->get_text() + "'");
    const bool set = model->set_year(0);
    screen.get_viewport()->gui_release_focus();
    send_key(screen, godot::KEY_RIGHT, false);
    check.expect(!set && model->get_year() == -1, "N2 set_year(0) refused and the Right key leaves the year at -1");
    check.expect(!hover_text.contains("held by") && !hover_text.contains("free"),
                 "N3 the hover text names no holding: " + hover_text);
}

// --- H1 to H9 -------------------------------------------------------------------

void check_with_history(Checker& check, ViewerScreen& screen, const ViewerCheckOptions& options) {
    godot::Ref<ViewerModel> model = screen.get_model();
    const godot::Ref<WorldFile> world = model->get_world();
    const HistoryReport report = read_history_report(options.history_report, model->get_world_path());
    check.expect(report.problem.empty(), "history report read: " + options.history_report +
                                             (report.problem.empty() ? godot::String() : ": " + to_godot(report.problem)));
    if (!report.problem.empty()) {
        return;
    }
    const int64_t end = model->get_end_year();
    const int64_t territory = model->get_territory_view();

    // H1.
    check.expect(model->get_history_status() == "loaded" && end == options.expect_end_year && end == report.end_year,
                 "H1 history " + model->get_history_status() + ", end year " + num(end) + ", expected " +
                     num(options.expect_end_year));
    check.expect(screen.year_slider()->get_min() == 0.0 && screen.year_slider()->get_max() == static_cast<double>(end),
                 "H1 slider range 0 to " + num(static_cast<int64_t>(screen.year_slider()->get_max())));
    check.expect(model->get_year() == end && screen.year_slider()->get_value() == static_cast<double>(end) &&
                     screen.year_field()->get_value() == static_cast<double>(end),
                 "H1 the world opened at the present: year " + num(model->get_year()));
    check.expect(territory >= 0 && screen.view_of_item(1) == territory && model->get_selected_view() == territory,
                 "H1 territory is the first composite and selected at opening (view " + num(territory) + ")");

    // H2: each year through the slider.
    const godot::Callable counter = callable_mp_static(&count_year_changed);
    model->connect("year_changed", counter);
    screen.choose_view(territory);
    int64_t slowest = 0;
    int64_t slowest_year = -1;
    for (const int64_t year : options.expect_years) {
        const int64_t t = usec_now();
        screen.year_slider()->set_value(static_cast<double>(year));
        const int64_t took = usec_now() - t;
        if (took > slowest) {
            slowest = took;
            slowest_year = year;
        }
        const godot::String png = options.reference_dir.path_join("territory-" + num(year) + ".png");
        const godot::String difference = compare_images(screen.map_view()->get_image(), png);
        check.expect(model->get_year() == year && difference.is_empty(),
                     "H2 year " + num(year) + " set through the slider: year " + num(model->get_year()) +
                         ", map view pixels equal worldgen-cli render --composite territory --year " + num(year) +
                         (difference.is_empty() ? godot::String() : ": " + difference));
        const godot::String want_label = expected_year_label(report, year);
        check.expect(screen.year_label()->get_text() == want_label,
                     "H2 year label '" + screen.year_label()->get_text() + "'" +
                         (screen.year_label()->get_text() == want_label ? godot::String()
                                                                        : ", expected '" + want_label + "'"));
        const std::string legend_diff = legend_head_difference(model->get_legend(), expected_legend_head(report, year));
        check.expect(legend_diff.empty() && screen.legend_list()->get_item_count() == model->get_legend().size(),
                     "H2 year " + num(year) + ": the legend lists the realms alive by race" +
                         (legend_diff.empty() ? godot::String() : ": " + to_godot(legend_diff)));
    }

    // H3: at E, the regions of each realm of [present].
    model->set_year(end);
    std::vector<int64_t> regions;
    const godot::Array region_rows = world->get_region_entries();
    for (int64_t i = 0; i < region_rows.size(); ++i) {
        regions.push_back(int64_t(godot::Dictionary(region_rows[i])["id"]));
    }
    std::map<int64_t, int64_t> held;
    for (const int64_t r : regions) {
        const godot::Dictionary h = model->get_region_holding(r);
        held[h.get("civilization", int64_t{-1})] += 1;
    }
    bool present_ok = !report.present.empty();
    int64_t present_regions = 0;
    for (const auto& [civ, n] : report.present) {
        present_ok = present_ok && held[civ] == n;
        present_regions += n;
    }
    check.expect(present_ok && held[0] + present_regions == static_cast<int64_t>(regions.size()),
                 "H3 at the present every realm holds the regions of [present], the rest are free (" +
                     num(present_regions) + " held, " + num(held[0]) + " free)");

    // H4: steps.
    {
        g_year_changed = 0;
        const bool next_event = model->step_event(1);
        check.expect(!next_event && model->get_year() == end && g_year_changed == 0, "H4 no event after the present");
        model->step_event(-1);
        check.expect(model->get_year() == report.last_event_year && g_year_changed == 1,
                     "H4 the previous event from the present is in " + num(model->get_year()) + ", expected " +
                         num(report.last_event_year));
        // The first change of holder after year 0, by stepping year by year.
        model->set_year(0);
        std::vector<int64_t> before;
        for (const int64_t r : regions) {
            before.push_back(model->get_region_holding(r).get("civilization", int64_t{-1}));
        }
        int64_t first_change = -1;
        for (int64_t y = 1; y <= end && first_change < 0; ++y) {
            model->set_year(y);
            for (std::size_t i = 0; i < regions.size(); ++i) {
                const int64_t now = model->get_region_holding(regions[i]).get("civilization", int64_t{-1});
                if (now != before[i]) {
                    first_change = y;
                }
                before[i] = now;
            }
        }
        model->set_year(0);
        g_year_changed = 0;
        model->step_holder_change(1);
        const int64_t f = report.first_ceded;
        check.expect(first_change > 0 && first_change <= f && model->get_year() == first_change &&
                         g_year_changed == 1,
                     "H4 the first change of holder after year 0 is in " + num(model->get_year()) +
                         ", found by stepping " + num(first_change) + ", first region_ceded in " + num(f));
        model->set_year(f + 1);
        g_year_changed = 0;
        model->step_holder_change(-1);
        check.expect(model->get_year() == f && g_year_changed == 1,
                     "H4 the previous change from " + num(f + 1) + " is in " + num(model->get_year()));
        model->set_year(end - 10);
        g_year_changed = 0;
        model->step_year(25);
        check.expect(model->get_year() == end && g_year_changed == 1, "H4 +25 from E - 10 gives E");
        model->set_year(10);
        g_year_changed = 0;
        model->step_year(-25);
        check.expect(model->get_year() == 0 && g_year_changed == 1, "H4 -25 from 10 gives 0");
        g_year_changed = 0;
        const bool same = model->set_year(0);
        check.expect(same && g_year_changed == 0, "H4 setting the current year emits nothing");
    }

    // H5: keys, with no control focused.
    {
        screen.get_viewport()->gui_release_focus();
        model->set_year(end - 1);
        send_key(screen, godot::KEY_RIGHT, false);
        const int64_t right = model->get_year();
        send_key(screen, godot::KEY_LEFT, true);
        const int64_t shift_left = model->get_year();
        send_key(screen, godot::KEY_HOME, false);
        const int64_t home = model->get_year();
        send_key(screen, godot::KEY_END, false);
        const int64_t end_key = model->get_year();
        send_key(screen, godot::KEY_PAGEUP, false);
        const int64_t page_up = model->get_year();
        check.expect(right == end && shift_left == end - 25 && home == 0 && end_key == end &&
                         page_up == report.last_event_year,
                     "H5 keys: Right " + num(right) + ", Shift+Left " + num(shift_left) + ", Home " + num(home) +
                         ", End " + num(end_key) + ", Page Up " + num(page_up));
    }

    // H6: hover.
    {
        model->set_year(end);
        const godot::Ref<WorldLayer> region_layer = world->read_layer("region");
        int64_t held_x = -1, held_y = -1, free_x = -1, free_y = -1;
        if (region_layer.is_valid()) {
            const auto& grid = region_layer->layer().grid;
            for (std::uint32_t y = 0; y < grid.height && (held_x < 0 || free_x < 0); ++y) {
                for (std::uint32_t x = 0; x < grid.width && (held_x < 0 || free_x < 0); ++x) {
                    std::int64_t v = 0;
                    if (!worldgen::inspect::cell_value(grid, x, y, v) || v <= 0) {
                        continue;
                    }
                    const godot::Dictionary h = model->get_region_holding(v);
                    if (h.is_empty()) {
                        continue;
                    }
                    if (int64_t(h["civilization"]) != 0 && held_x < 0) {
                        held_x = x;
                        held_y = y;
                    } else if (int64_t(h["civilization"]) == 0 && free_x < 0) {
                        free_x = x;
                        free_y = y;
                    }
                }
            }
        }
        const auto expected_suffix = [&](int64_t x, int64_t y) -> godot::String {
            const godot::String value = region_layer->get_cell_text(x, y);
            const godot::Dictionary h = model->get_region_holding(value.to_int());
            const int64_t civ = h["civilization"];
            const int64_t since = h["since"];
            if (civ != 0) {
                const ReportCiv* c = report.civ(civ);
                // P5-06 12: the realm's recorded ruler at Y, named from the
                // browser's figure list.
                const int64_t ruler = model->history_state().history().timeline.ruler_at(civ, model->get_year());
                godot::String ruled = ", ruler unrecorded";
                if (ruler != 0) {
                    ruled = ", ruled by ?";
                    const godot::Array figures = model->get_figure_list();
                    for (int64_t i = 0; i < figures.size(); ++i) {
                        const godot::Dictionary f = figures[i];
                        if (int64_t(f["id"]) == ruler) {
                            ruled = ", ruled by " + godot::String(f["name"]);
                        }
                    }
                }
                // P5-08 11.2 item 2: the realm's faith, named from the
                // check's own faction rows.
                godot::String faith;
                const auto& history = model->history_state().history();
                const int64_t cult = history.timeline.faith_at(civ, model->get_year());
                if (cult != 0) {
                    faith = ", faith: cult " + num(cult);
                    for (const auto& k : history.tables.factions) {
                        if (k.id == cult) {
                            faith = ", faith: " + to_godot(k.name);
                        }
                    }
                }
                return "   |   held by " + (c != nullptr ? to_godot(c->name) : godot::String("?")) + " since " +
                       num(since) + ruled + faith;
            }
            return since > 0 ? "   |   free since " + num(since) : godot::String("   |   free");
        };
        check.expect(held_x >= 0 && free_x >= 0, "H6 a held and a free cell at the present: (" + num(held_x) + ", " +
                                                     num(held_y) + ") and (" + num(free_x) + ", " + num(free_y) + ")");
        if (held_x >= 0 && free_x >= 0) {
            screen.hover_cell(held_x, held_y);
            const godot::String held_text = screen.cell_label()->get_text();
            check.expect(held_text.ends_with(expected_suffix(held_x, held_y)), "H6 held cell: " + held_text);
            screen.hover_cell(free_x, free_y);
            const godot::String free_text = screen.cell_label()->get_text();
            check.expect(free_text.ends_with(expected_suffix(free_x, free_y)), "H6 free cell: " + free_text);
            // P5-06 12: a year before E names the realm's recorded ruler
            // (Timeline::ruler_at counts a reign to E exclusive, so at E
            // itself every reign reads unrecorded).
            screen.hover_cell(held_x, held_y);
            model->set_year(model->get_end_year() - 1);
            const godot::String ruled_text = screen.cell_label()->get_text();
            check.expect(ruled_text.contains(", ruled by ") && ruled_text.ends_with(expected_suffix(held_x, held_y)),
                         "H6 held cell at E - 1 names its ruler: " + ruled_text);
            model->set_year(0);  // the screen refreshes the description of the hovered cell
            const godot::String at_zero = screen.cell_label()->get_text();
            check.expect(at_zero.ends_with(expected_suffix(held_x, held_y)), "H6 held cell at year 0: " + at_zero);
            screen.hover_cell(-1, -1);
            check.expect(screen.cell_label()->get_text() == "Point at the map to inspect a cell.",
                         "H6 leaving the map shows the prompt again");
        }
    }

    // H7: colours, against the library on the check's own read of the file.
    {
        model->set_year(end);
        bool colours_ok = false;
        godot::String detail;
        try {
            auto reader = worldgen::db::WorldReader::open(to_utf8(model->get_world_path()));
            if (reader.ok()) {
                auto loaded = worldgen::history::load_history(reader.value());
                if (loaded.ok()) {
                    const auto colours = worldgen::inspect::civilization_colours(loaded.value().tables);
                    colours_ok = !colours.empty();
                    for (std::size_t i = 0; i < colours.size(); ++i) {
                        const int64_t want = (int64_t{colours[i].r} << 16) | (int64_t{colours[i].g} << 8) |
                                             int64_t{colours[i].b};
                        const godot::Dictionary info = model->get_civilization_info(static_cast<int64_t>(i) + 1);
                        if (int64_t(info.get("colour", int64_t{-1})) != want) {
                            colours_ok = false;
                            detail = ", civilization " + num(static_cast<int64_t>(i) + 1) + " differs";
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            detail = godot::String(", ") + e.what();
        }
        check.expect(colours_ok, "H7 every civilization's colour is the library's" + detail);
        bool swatches_ok = true;
        const godot::Array legend = model->get_legend();
        for (int64_t i = 0; i < legend.size(); ++i) {
            const godot::Dictionary row = legend[i];
            const std::string label = to_utf8(row["label"]);
            if (label == "border between holders") {
                break;
            }
            const godot::PackedInt32Array colours = row["colours"];
            if (colours.is_empty()) {
                continue;
            }
            bool found = false;
            for (const auto& c : report.civs) {
                if (c.name == label) {
                    found = int64_t(model->get_civilization_info(c.id)["colour"]) == int64_t{colours[0]};
                }
            }
            swatches_ok = swatches_ok && found;
        }
        check.expect(swatches_ok, "H7 every swatch of the territory legend is its civilization's colour");

        // H9 (P4-03 r4 10.2): the rows after the last civilization row, at E,
        // against the regions legend of the same world.
        std::vector<std::pair<std::string, std::vector<int64_t>>> want = {
            {"border between holders", {0}},
            {"border inside a realm", {}},
            {"border in free land", {}},
            {"free land", {}},
        };
        std::vector<worldgen::inspect::LegendEntry> regions_rows;
        if (world.is_valid()) {
            regions_rows =
                worldgen::inspect::composite_legend(worldgen::inspect::Composite::regions, world->biome_records());
        }
        constexpr std::size_t water_rows = 3;
        const bool regions_shape = regions_rows.size() >= 1U + water_rows && regions_rows[0].label == "region border";
        if (regions_shape) {
            const auto add = [&want](const worldgen::inspect::LegendEntry& row) {
                std::vector<int64_t> colours;
                for (const std::uint32_t c : row.colours) {
                    colours.push_back(int64_t{c});
                }
                want.emplace_back(row.label, std::move(colours));
            };
            const std::size_t water = regions_rows.size() - water_rows;
            for (std::size_t i = 1; i < water; ++i) {
                add(regions_rows[i]);
            }
            want.emplace_back("water", std::vector<int64_t>{});
            for (std::size_t i = water; i < regions_rows.size(); ++i) {
                add(regions_rows[i]);
            }
        }
        int64_t tail = -1;
        for (int64_t i = 0; i < legend.size(); ++i) {
            const godot::Dictionary row = legend[i];
            if (to_utf8(row["label"]) == "border between holders") {
                tail = i;
                break;
            }
        }
        std::string tail_diff;
        if (!regions_shape) {
            tail_diff = "the regions legend does not have its border row and three water rows";
        } else if (tail < 0) {
            tail_diff = "no row 'border between holders'";
        } else if (legend.size() - tail != static_cast<int64_t>(want.size())) {
            tail_diff = std::to_string(legend.size() - tail) + " rows, want " + std::to_string(want.size());
        } else {
            for (std::size_t k = 0; k < want.size() && tail_diff.empty(); ++k) {
                const godot::Dictionary row = legend[tail + static_cast<int64_t>(k)];
                const std::string label = to_utf8(row["label"]);
                const godot::PackedInt32Array colours = row["colours"];
                bool same = label == want[k].first && colours.size() == static_cast<int64_t>(want[k].second.size());
                for (int64_t j = 0; same && j < colours.size(); ++j) {
                    same = (int64_t{colours[j]} & 0xFFFFFF) == want[k].second[static_cast<std::size_t>(j)];
                }
                if (!same) {
                    tail_diff = "row " + std::to_string(k) + " '" + label + "', want '" + want[k].first + "'";
                } else {
                    // The border rows (k < 3) carry their explanation as a
                    // tooltip; every other row has none (P4-03 r4 6.4).
                    static constexpr const char* tips[] = {"black, between regions of different holders",
                                                           "the realm's colour, darker", "the land's colour, darker"};
                    const std::string tip = to_utf8(row.get("tooltip", godot::String()));
                    const std::string want_tip = k < 3U ? tips[k] : "";
                    if (tip != want_tip) {
                        tail_diff = "row " + std::to_string(k) + " '" + label + "' tooltip '" + tip + "', want '" +
                                    want_tip + "'";
                    }
                }
            }
        }
        check.expect(tail_diff.empty(), "H9 the territory legend ends with the border, free land and water rows" +
                                            (tail_diff.empty() ? godot::String() : ": " + to_godot(tail_diff)));
    }

    // H8: year marks.
    {
        std::vector<std::pair<int64_t, std::string>> want;
        for (const auto& age : report.ages) {
            want.emplace_back(age.start, "awakening");
        }
        for (const auto& p : report.phases) {
            want.emplace_back(p.start, "phase");
        }
        want.emplace_back(end, "present");
        const auto rank = [](const std::string& kind) { return kind == "awakening" ? 0 : kind == "phase" ? 1 : 2; };
        std::stable_sort(want.begin(), want.end(), [&](const auto& a, const auto& b) {
            return a.first != b.first ? a.first < b.first : rank(a.second) < rank(b.second);
        });
        const godot::Array marks = model->get_year_marks();
        bool same = marks.size() == static_cast<int64_t>(want.size());
        for (int64_t i = 0; same && i < marks.size(); ++i) {
            const godot::Dictionary mark = marks[i];
            same = int64_t(mark["year"]) == want[static_cast<std::size_t>(i)].first &&
                   to_utf8(mark["kind"]) == want[static_cast<std::size_t>(i)].second;
        }
        check.expect(same && screen.year_marks()->get_marks().size() == marks.size(),
                     "H8 " + num(marks.size()) + " year marks: one per awakening, phase start and the present");
    }

    model->disconnect("year_changed", counter);
    godot::UtilityFunctions::print("UNMAPPED-CHECK: info: open ", num(screen.last_open_usec() / 1000), " ms, first territory image ",
                                   num(model->territory_timings().first_image_usec / 1000), " ms, slowest H2 year change ",
                                   godot::String::num(static_cast<double>(slowest) / 1000.0, 2), " ms (year ",
                                   num(slowest_year), ")");
}

// --- M0 to M9 (P4-04 9.2) -------------------------------------------------------

int64_t g_markers_changed = 0;
void count_markers_changed() {
    ++g_markers_changed;
}

// The labels of the key list of 8.6 for the given options.
std::vector<std::string> expected_marker_legend(bool faded, bool events, bool passes, bool mines = true) {
    std::vector<std::string> rows = {"hamlet", "village", "town", "city", "fortress", "capital", "declining", "ruin"};
    if (faded) {
        rows.emplace_back("old ruin");  // P5-06 25.1 (e)
    }
    if (events) {
        for (const char* label : {"sack or capture", "siege", "battle", "plague", "any other event"}) {
            rows.emplace_back(label);
        }
    }
    if (passes) {
        rows.emplace_back("pass");
    }
    if (mines) {
        rows.emplace_back("mine");   // P5-04 11.3
        rows.emplace_back("tower");  // P5-06 11.4
        for (const char* label : {"shrine", "temple", "hidden sanctum"}) {  // P5-08 11.2
            rows.emplace_back(label);
        }
    }
    return rows;
}

std::vector<std::string> legend_labels(const godot::Array& legend) {
    std::vector<std::string> out;
    for (int64_t i = 0; i < legend.size(); ++i) {
        out.push_back(to_utf8(godot::Dictionary(legend[i])["label"]));
    }
    return out;
}

// The index in get_markers() of the entry of a type and id; -1 when absent.
int64_t marker_index_of(const godot::Array& markers, const char* type, int64_t id) {
    for (int64_t i = 0; i < markers.size(); ++i) {
        const godot::Dictionary m = markers[i];
        if (godot::String(m["type"]) == type && int64_t(m["id"]) == id) {
            return i;
        }
    }
    return -1;
}

// The settlement entry of the model's current set, or null.
const worldgen::inspect::SettlementMarker* model_settlement(const ViewerModel& model, int64_t id) {
    for (const auto& s : model.history_state().markers().settlements) {
        if (s.settlement == id) {
            return &s;
        }
    }
    return nullptr;
}

bool same_marker_lists(const worldgen::inspect::YearMarkers& a, const worldgen::inspect::YearMarkers& b) {
    return a.sites == b.sites && a.settlements == b.settlements && a.places == b.places && a.passes == b.passes;
}

// P5-06 25.1 (e): an old ruin (status 3, OLD_RUIN_YEARS or more) is faded.
godot::String type_word(const worldgen::inspect::SettlementMarker& s) {
    const bool faded = s.status == 4 || (s.status == 3 && s.old_ruin);
    return faded ? "faded" : s.status == 3 ? "ruin" : "standing";
}

// The model's YearMarkers in the order of 6.1 as (type, id, x, y, glyph,
// fill, capital, small), for comparison with get_markers().
std::vector<std::string> expected_marker_rows(const ViewerModel& model) {
    const auto& m = model.history_state().markers();
    std::vector<std::string> rows;
    const auto rgb = [](worldgen::inspect::Rgb c) {
        return (int64_t{c.r} << 16) | (int64_t{c.g} << 8) | int64_t{c.b};
    };
    const auto row = [&rows](const std::string& type, int64_t id, int64_t x, int64_t y, const std::string& glyph,
                             int64_t fill, bool capital, bool small) {
        rows.push_back(type + " " + std::to_string(id) + " " + std::to_string(x) + " " + std::to_string(y) + " " +
                       glyph + " " + std::to_string(fill) + " " + (capital ? "1" : "0") + " " + (small ? "1" : "0"));
    };
    for (const auto& p : m.passes) {
        row("pass", p.pass, p.x, p.y, "pass", -1, false, false);
    }
    for (const auto& p : m.places) {
        row("place", p.events.front(), p.x, p.y, "ring", rgb(worldgen::inspect::event_symbol_colour(p.symbol)), false,
            false);
    }
    const auto colours = model.history_state().colours();
    for (int pass = 3; pass <= 6; ++pass) {
        if (pass == 5) {  // P5-04 11.3: mines between ruins and standing settlements
            for (const auto& site : m.sites) {
                // P5-06 11.4 towers, P5-08 11.1 shrines, temples, sanctums.
                const char* glyph = site.kind == 4   ? "tower"
                                    : site.kind == 5 ? "shrine"
                                    : site.kind == 6 ? "temple"
                                    : site.kind == 7 ? "sanctum"
                                                     : "mine";
                const worldgen::inspect::Rgb fill = site.kind == 4   ? worldgen::inspect::MARKER_TOWER
                                                    : site.kind == 5 ? worldgen::inspect::MARKER_SHRINE
                                                    : site.kind == 6 ? worldgen::inspect::MARKER_TEMPLE
                                                    : site.kind == 7 ? worldgen::inspect::MARKER_SANCTUM
                                                                     : worldgen::inspect::MARKER_MINE;
                row("site", site.site, site.x, site.y, glyph, rgb(fill), false, true);
            }
        }
        for (const auto& s : m.settlements) {
            const godot::String t = type_word(s);
            const int p = t == "faded" ? 3 : t == "ruin" ? 4 : s.capital ? 6 : 5;
            if (p != pass) {
                continue;
            }
            int64_t fill = -1;
            if (p == 3) {
                fill = rgb(worldgen::inspect::MARKER_FADED);
            } else if (p >= 5) {
                fill = s.owner == 0 ? rgb(worldgen::inspect::MARKER_NO_OWNER)
                                    : rgb(colours[static_cast<std::size_t>(s.owner - 1)]);
            }
            const bool small = p <= 4 || (!s.capital && (s.kind == 1 || s.kind == 2));
            row(to_utf8(type_word(s)), s.settlement, s.x, s.y, marker_glyph_name(worldgen::inspect::glyph_of(s)), fill,
                s.capital, small);
        }
    }
    return rows;
}

std::vector<std::string> actual_marker_rows(const godot::Array& markers) {
    std::vector<std::string> rows;
    for (int64_t i = 0; i < markers.size(); ++i) {
        const godot::Dictionary m = markers[i];
        rows.push_back(to_utf8(m["type"]) + " " + std::to_string(int64_t(m["id"])) + " " +
                       std::to_string(int64_t(m["x"])) + " " + std::to_string(int64_t(m["y"])) + " " +
                       to_utf8(m["glyph"]) + " " + std::to_string(int64_t(m["fill"])) + " " +
                       (bool(m["capital"]) ? "1" : "0") + " " + (bool(m["small"]) ? "1" : "0"));
    }
    return rows;
}

// The view image with the model's markers drawn by worldgen at scale 1.
godot::Ref<godot::Image> image_with_markers(const godot::Ref<godot::Image>& view, const ViewerModel& model,
                                            godot::String& problem) {
    if (view.is_null() || view->get_format() != godot::Image::FORMAT_RGB8) {
        problem = "the map view has no RGB8 image";
        return {};
    }
    godot::PackedByteArray data = view->get_data();
    try {
        worldgen::inspect::draw_markers(std::span<std::uint8_t>(data.ptrw(), static_cast<std::size_t>(data.size())),
                                        static_cast<std::uint32_t>(view->get_width()),
                                        static_cast<std::uint32_t>(view->get_height()), 1U,
                                        model.history_state().markers(), model.history_state().colours());
    } catch (const std::exception& e) {
        problem = godot::String("draw_markers: ") + e.what();
        return {};
    }
    return godot::Image::create_from_data(view->get_width(), view->get_height(), false, godot::Image::FORMAT_RGB8,
                                          data);
}

void send_motion(ViewerScreen& screen, godot::Vector2 position) {
    godot::Ref<godot::InputEventMouseMotion> motion;
    motion.instantiate();
    motion->set_position(position);
    motion->set_global_position(position);
    screen.map_view()->_gui_input(motion);
}

// The check's own read of the world: reader, history and marker index.
struct LibraryWorld {
    std::optional<worldgen::db::WorldReader> reader;
    std::optional<worldgen::history::LoadedHistory> history;
    std::optional<worldgen::inspect::MarkerIndex> index;
    std::vector<worldgen::db::RegionRow> regions;
    std::string problem;
};

LibraryWorld open_library_world(const godot::String& path) {
    LibraryWorld w;
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
        auto index = worldgen::inspect::MarkerIndex::load(*w.reader, *w.history);
        if (!index.ok()) {
            w.problem = index.error().message;
            return w;
        }
        w.index.emplace(std::move(index).value());
        auto regions = worldgen::db::read_region_rows(*w.reader);
        if (!regions.ok()) {
            w.problem = regions.error().message;
            return w;
        }
        w.regions = std::move(regions).value();
    } catch (const std::exception& e) {
        w.problem = e.what();
    }
    return w;
}

// M0: a world without history.
void check_markers_without_history(Checker& check, ViewerScreen& screen) {
    godot::Ref<ViewerModel> model = screen.get_model();
    check.expect(!model->get_markers_available() && model->get_markers().is_empty() &&
                     model->get_marker_legend().is_empty() && screen.marker_legend()->get_item_count() == 0,
                 "M0 no markers without history: available false, no entries, no key list");
    check.expect(screen.markers_toggle()->is_disabled() && screen.mark_faded()->is_disabled() &&
                     screen.mark_events()->is_disabled() && screen.mark_passes()->is_disabled() &&
                     screen.mark_sites()->is_disabled() && screen.mark_shrines()->is_disabled() &&
                     screen.mark_temples()->is_disabled(),
                 "M0 every control of the Markers group is disabled");
    const bool enabled = model->get_markers_enabled();
    const bool set = model->set_markers_enabled(!enabled);
    screen.get_viewport()->gui_release_focus();
    send_key(screen, godot::KEY_M, false);
    check.expect(!set && model->get_markers_enabled() == enabled && model->get_markers().is_empty() &&
                     !model->set_marker_option("faded", true) && !model->get_marker_option("faded"),
                 "M0 set_markers_enabled, set_marker_option and key M change nothing");
}

// M1: at open, before any other history check changes the year.
void check_markers_at_open(Checker& check, ViewerScreen& screen) {
    godot::Ref<ViewerModel> model = screen.get_model();
    check.expect(model->get_markers_available() && model->get_markers_enabled() && model->get_markers().size() > 0,
                 "M1 markers available and on at open (" + num(model->get_markers().size()) + " entries)" +
                     (model->get_markers_available() ? godot::String()
                                                     : ": " + model->get_markers_error_code() + ": " +
                                                           model->get_markers_error_message()));
    const bool options = !model->get_marker_option("faded") && model->get_marker_option("events") &&
                         model->get_marker_option("passes") && model->get_marker_option("sites") &&
                         model->get_marker_option("shrines") && model->get_marker_option("temples") &&
                         !model->get_marker_option("other");
    const bool boxes = screen.markers_toggle()->is_pressed() && !screen.mark_faded()->is_pressed() &&
                       screen.mark_events()->is_pressed() && screen.mark_passes()->is_pressed() &&
                       screen.mark_sites()->is_pressed() && !screen.mark_sites()->is_disabled() &&
                       screen.mark_shrines()->is_pressed() && !screen.mark_shrines()->is_disabled() &&
                       screen.mark_temples()->is_pressed() && !screen.mark_temples()->is_disabled() &&
                       !screen.markers_toggle()->is_disabled() && !screen.mark_faded()->is_disabled();
    const bool words = screen.mark_faded()->get_text() == "Old ruins" && screen.mark_sites()->get_text() == "Sites" &&
                       screen.mark_shrines()->get_text() == "Shrines" && screen.mark_temples()->get_text() == "Temples";
    check.expect(options && boxes && words,
                 "M1 options old ruins off, events on, passes on, sites, shrines and temples on, in the model and the "
                 "boxes 'Old ruins', 'Sites', 'Shrines' and 'Temples' (P5-06 11.4, 25.1 (e), P5-08 11.2)");
    const auto labels = legend_labels(model->get_marker_legend());
    check.expect(labels == expected_marker_legend(false, true, true) &&
                     screen.marker_legend()->get_item_count() == static_cast<int64_t>(labels.size()),
                 "M1 the key list has the " + num(static_cast<int64_t>(labels.size())) + " rows of 8.6 without 'old ruin', 'tower' after 'mine', then 'shrine', 'temple', 'hidden sanctum'");
    // P4-05 12.1 F7: each key icon shows its glyph at g = 2, the rings at
    // g = 1: the drawn extent is that of the glyph's non-zero cells times g.
    const godot::Array legend = model->get_marker_legend();
    godot::String wrong;
    for (int64_t i = 0; i < legend.size() && i < screen.marker_legend()->get_item_count(); ++i) {
        const godot::String glyph_name = godot::Dictionary(legend[i])["glyph"];
        const godot::Dictionary glyph = model->get_marker_glyph(glyph_name);
        const int64_t r = glyph.get("radius", int64_t{-1});
        const godot::PackedByteArray cells = glyph.get("cells", godot::PackedByteArray());
        const int64_t side = 2 * r + 1;
        int64_t lo_x = side;
        int64_t hi_x = -1;
        int64_t lo_y = side;
        int64_t hi_y = -1;
        for (int64_t y = 0; y < side && cells.size() == side * side; ++y) {
            for (int64_t x = 0; x < side; ++x) {
                const uint8_t c = cells[y * side + x];
                if (c >= 1 && c <= 3) {
                    lo_x = std::min(lo_x, x);
                    hi_x = std::max(hi_x, x);
                    lo_y = std::min(lo_y, y);
                    hi_y = std::max(hi_y, y);
                }
            }
        }
        const int64_t g = glyph_name == "ring" ? 1 : 2;
        const godot::Vector2i want(static_cast<int32_t>((hi_x - lo_x + 1) * g),
                                   static_cast<int32_t>((hi_y - lo_y + 1) * g));
        const godot::Ref<godot::Texture2D> icon = screen.marker_legend()->get_item_icon(static_cast<int32_t>(i));
        const godot::Ref<godot::Image> image = icon.is_valid() ? icon->get_image() : godot::Ref<godot::Image>();
        const godot::Vector2i got = image.is_valid() ? image->get_used_rect().size : godot::Vector2i(-1, -1);
        if (got != want) {
            wrong += " " + godot::Dictionary(legend[i])["label"].stringify() + " " + godot::Variant(got).stringify() +
                     " (want " + godot::Variant(want).stringify() + ")";
        }
    }
    check.expect(wrong.is_empty() && legend.size() == screen.marker_legend()->get_item_count(),
                 "M1 the key icons are drawn at glyph cell 2, the rings at 1 (P4-05 F7)" +
                     (wrong.is_empty() ? godot::String() : ":" + wrong));
}

std::string id_list(const std::vector<int64_t>& v) {
    std::string out = "[";
    for (std::size_t i = 0; i < v.size(); ++i) {
        out += (i == 0 ? "" : " ") + std::to_string(v[i]);
    }
    return out + "]";
}

// P5-07 11.2, from the check's own read of the events: the depth words, and
// per mine site and per warren settlement the (year, depth) of its
// deepenings and the year its horror was sealed (-1 none).
const char* depth_word(int64_t d) {
    return d == 1 ? "below daylight" : d == 2 ? "to the mountain's roots" : d == 3 ? "to the old deep" : "";
}

struct DeepRecord {
    std::vector<std::pair<int64_t, int64_t>> deepenings;  // (year, depth), event order
    int64_t monster = 0;
    int64_t sealed = -1;
};

struct DeepWorld {
    std::map<int64_t, DeepRecord> mines;    // by site
    std::map<int64_t, DeepRecord> warrens;  // by settlement
    int64_t lost = 0;                       // hold_lost events
};

DeepWorld read_deep(const worldgen::history::LoadedHistory& h) {
    DeepWorld w;
    const auto& cat = h.view.catalogue();
    const auto* deepened = cat.find("delving_deepened");
    const auto* woken = cat.find("deep_woken");
    const auto* sealed = cat.find("deep_sealed");
    const auto* delved = cat.find("warren_delved");
    const auto* wsealed = cat.find("warren_sealed");
    const auto* lost = cat.find("hold_lost");
    if (deepened == nullptr || woken == nullptr || sealed == nullptr || delved == nullptr || wsealed == nullptr) {
        return w;
    }
    const auto participant = [](const worldgen::history::HistoryView::Event& e, const worldgen::history::KindDef& k,
                                const char* role) -> int64_t {
        const auto* r = k.find_role(role);
        for (const auto& p : e.participants) {
            if (r != nullptr && p.role == r->seq) {
                return p.entity_id;
            }
        }
        return 0;
    };
    const auto value = [](const worldgen::history::HistoryView::Event& e, const worldgen::history::KindDef& k,
                          const char* key) -> int64_t {
        const int64_t seq = k.value_seq(key);
        return seq >= 1 && static_cast<std::size_t>(seq) <= e.values.size() ? e.values[static_cast<std::size_t>(seq - 1)]
                                                                            : 0;
    };
    std::map<int64_t, int64_t> sealed_of;  // monster -> year of its sealing
    for (const auto& e : h.view.events()) {
        const int64_t k = e.row.kind;
        if (k == deepened->id) {
            w.mines[participant(e, *deepened, "site")].deepenings.emplace_back(e.row.year, value(e, *deepened, "depth"));
        } else if (k == delved->id) {
            w.warrens[e.row.settlement].deepenings.emplace_back(e.row.year, value(e, *delved, "depth"));
        } else if (k == woken->id) {
            const int64_t site = participant(e, *woken, "site");
            DeepRecord& r = site != 0 ? w.mines[site] : w.warrens[e.row.settlement];
            r.monster = participant(e, *woken, "monster");
        } else if (k == sealed->id || k == wsealed->id) {
            const int64_t m = participant(e, k == sealed->id ? *sealed : *wsealed, "monster");
            if (sealed_of.find(m) == sealed_of.end()) {
                sealed_of[m] = e.row.year;
            }
        } else if (lost != nullptr && k == lost->id) {
            ++w.lost;
        }
    }
    for (auto* records : {&w.mines, &w.warrens}) {
        for (auto& [key, r] : *records) {
            const auto it = sealed_of.find(r.monster);
            r.sealed = r.monster != 0 && it != sealed_of.end() ? it->second : -1;
        }
    }
    return w;
}

// The hover suffix of 11.2 items 1 and 4 at year y.
std::string deep_suffix(const DeepRecord& r, int64_t y, const char* lead) {
    int64_t depth = 0;
    for (const auto& [year, d] : r.deepenings) {
        depth = year <= y && d > depth ? d : depth;
    }
    std::string out = depth >= 1 ? std::string(lead) + depth_word(depth) : std::string();
    if (r.sealed >= 0 && r.sealed <= y) {
        out += ", sealed in " + std::to_string(r.sealed);
    }
    return out;
}

// M10 (P5-04 11.3): mines. The sites of kind 3 of table site, by the
// half-open rule of P3-04 10, against the model's entries at a few years;
// draw order, glyph, fill and offset of the entries; the hover text; the
// box and the key row.
void check_mines(Checker& check, ViewerScreen& screen, const ViewerCheckOptions& options, const LibraryWorld& lib) {
    godot::Ref<ViewerModel> model = screen.get_model();
    const int64_t end = model->get_end_year();
    std::vector<const worldgen::db::SiteRow*> mines;
    for (const auto& row : lib.history->tables.sites) {
        if (row.kind == 3) {
            mines.push_back(&row);
        }
    }
    check.expect(!mines.empty() && (options.expect_mines < 0 || options.expect_mines == static_cast<int64_t>(mines.size())),
                 "M10 the world has " + num(static_cast<int64_t>(mines.size())) +
                     " mines (sites of kind 3), as many as its mine_opened lines (" + num(options.expect_mines) + ")");
    if (mines.empty()) {
        return;
    }
    const auto open_at = [&mines](int64_t y) {
        std::vector<int64_t> ids;
        for (const auto* m : mines) {
            if (m->origin_year <= y && (!m->ended_year || y < *m->ended_year)) {
                ids.push_back(m->id);
            }
        }
        return ids;
    };
    const auto model_ids = [&model]() {
        std::vector<int64_t> ids;
        for (const auto& site : model->history_state().markers().sites) {
            if (site.kind == 3) {  // P5-06 11.4: towers are sites too
                ids.push_back(site.site);
            }
        }
        return ids;
    };
    // The years: E, and around the opening and the end of the first three
    // mines (and of the last, which may be the one still open at E).
    std::vector<int64_t> years{end};
    std::vector<const worldgen::db::SiteRow*> probe(mines.begin(), mines.begin() + std::min<std::ptrdiff_t>(3, static_cast<std::ptrdiff_t>(mines.size())));
    probe.push_back(mines.back());
    bool ended_seen = false;
    for (const auto* m : probe) {
        for (const int64_t y : {m->origin_year - 1, m->origin_year}) {
            years.push_back(y);
        }
        if (m->ended_year) {
            years.push_back(*m->ended_year - 1);
            years.push_back(*m->ended_year);
            ended_seen = true;
        }
    }
    std::sort(years.begin(), years.end());
    years.erase(std::unique(years.begin(), years.end()), years.end());
    std::string problem;
    int64_t best_year = -1;
    std::size_t best = 0;
    for (const int64_t y : years) {
        if (y < 0 || y > end) {
            continue;
        }
        model->set_year(y);
        const auto want = open_at(y);
        const auto got = model_ids();
        const auto lib_sites = lib.index->markers_at(*lib.history, y, worldgen::inspect::MarkerOptions{}).sites;
        if ((got != want || model->history_state().markers().sites != lib_sites) && problem.empty()) {
            problem = "year " + std::to_string(y) + ": model " + id_list(got) + ", table site " + id_list(want);
        }
        if (want.size() > best) {
            best = want.size();
            best_year = y;
        }
    }
    check.expect(problem.empty() && best_year >= 0,
                 "M10 at " + num(static_cast<int64_t>(years.size())) +
                     " years the mine entries are the mines with origin_year <= Y < ended_year" +
                     (ended_seen ? " (an ended mine among them)" : "") +
                     (problem.empty() ? godot::String() : ": " + to_godot(problem)));
    if (best_year < 0) {
        return;
    }
    // Entries at the year with the most mines open.
    model->set_year(best_year);
    const godot::Array markers = model->get_markers();
    int64_t first_site = -1;
    int64_t last_site = -1;
    int64_t first_mine = -1;
    int64_t mine_entries = 0;
    int64_t last_ruin = -1;
    int64_t first_standing = -1;
    std::string wrong;
    for (int64_t i = 0; i < markers.size(); ++i) {
        const godot::Dictionary m = markers[i];
        const godot::String type = m["type"];
        if (type == "site") {
            first_site = first_site < 0 ? i : first_site;
            last_site = i;
            if (int64_t(m.get("site_kind", int64_t{0})) != 3) {
                continue;  // a tower: M11
            }
            first_mine = first_mine < 0 ? i : first_mine;
            ++mine_entries;
            if (godot::String(m["glyph"]) != "mine" || int64_t(m["fill"]) != 0x3C3C3C || int64_t(m["offset_x"]) != 7 ||
                int64_t(m["offset_y"]) != -7 || !bool(m["small"])) {
                wrong = "entry " + std::to_string(i) + " " + to_utf8(godot::Variant(m).stringify());
            }
        } else if (type == "ruin" || type == "faded") {
            last_ruin = i;
        } else if (type == "standing" && first_standing < 0) {
            first_standing = i;
        }
    }
    const int64_t site_entries = static_cast<int64_t>(model->history_state().markers().sites.size());
    const bool order = first_site >= 0 && last_site - first_site + 1 == site_entries &&
                       mine_entries == static_cast<int64_t>(best) && last_ruin < first_site &&
                       (first_standing < 0 || first_standing > last_site);
    check.expect(order && wrong.empty(),
                 "M10 in " + num(best_year) + " the " + num(static_cast<int64_t>(best)) +
                     " mine entries (glyph mine, fill 60,60,60, offset 7 right and 7 up, small) come after the ruins "
                     "and before the standing settlements" +
                     (wrong.empty() ? godot::String() : ": " + to_godot(wrong)));
    // Hover text and the glyph's place: the first mine entry.
    if (first_mine >= 0) {
        const int64_t first_site_entry = first_mine;
        const godot::Dictionary m = markers[first_site_entry];
        const int64_t id = m["id"];
        const worldgen::db::SiteRow* row = nullptr;
        for (const auto* r : mines) {
            row = r->id == id ? r : row;
        }
        const std::string* name = lib.history->view.name(worldgen::history::EntityType::site, id);
        const std::string* civ =
            row != nullptr ? lib.history->view.name(worldgen::history::EntityType::civilization, row->founder_civ) : nullptr;
        const std::string want = (name != nullptr ? *name : std::string("?")) + " (mine of " +
                                 (civ != nullptr ? *civ : std::string("?")) + "), since " +
                                 (row != nullptr ? std::to_string(row->origin_year) : std::string("?")) +
                                 deep_suffix(read_deep(*lib.history).mines[id], model->get_year(), ", delved ");
        const godot::String text = model->describe_marker(first_site_entry);
        check.expect(to_utf8(text) == want, "M10 describe_marker of the mine site " + num(id) + ": " + text +
                                                (to_utf8(text) == want ? godot::String() : ", expected " + to_godot(want)));
        LayerMapView* view = screen.map_view();
        const godot::Vector2i at = view->get_marker_screen_position(first_site_entry);
        const int64_t g = view->glyph_cell_size();
        // The centre of the mine's own cell, as marker_centre computes it.
        const godot::Rect2 shown = view->get_shown_rect();
        const double zoom = view->get_zoom();
        const auto cx = static_cast<int64_t>(std::floor(static_cast<double>(shown.position.x) + (static_cast<double>(int64_t(m["x"])) + 0.5) * zoom));
        const auto cy = static_cast<int64_t>(std::floor(static_cast<double>(shown.position.y) + (static_cast<double>(int64_t(m["y"])) + 0.5) * zoom));
        const bool placed = at.x >= 0 && std::abs(at.x - (cx + 7 * g)) <= 1 && std::abs(at.y - (cy - 7 * g)) <= 1;
        send_motion(screen, godot::Vector2(static_cast<float>(at.x), static_cast<float>(at.y)));
        check.expect(placed && view->get_hovered_marker() == first_site_entry && screen.cell_label()->get_text() == text,
                     "M10 the mine's glyph is centred 7 glyph cells right and 7 up of its cell (" +
                         godot::String(godot::Variant(at)) + ", cell centre " + num(cx) + "," + num(cy) + ", g " + num(g) +
                         ") and the mouse over it shows its text: " + screen.cell_label()->get_text());
        screen.hover_cell(-1, -1);
    }
    // The box and the key rows (P5-06 11.4: the Sites box, mines and towers).
    {
        const int64_t before = markers.size();
        screen.mark_sites()->set_pressed(false);  // through the box
        bool none = !model->get_marker_option("sites");
        const godot::Array off = model->get_markers();
        for (int64_t i = 0; i < off.size(); ++i) {
            none = none && godot::String(godot::Dictionary(off[i])["type"]) != "site";
        }
        const bool fewer = off.size() == before - site_entries &&
                           legend_labels(model->get_marker_legend()) == expected_marker_legend(false, true, true, false);
        screen.mark_sites()->set_pressed(true);
        const bool back = model->get_marker_option("sites") && model->get_markers().size() == before &&
                          legend_labels(model->get_marker_legend()) == expected_marker_legend(false, true, true, true);
        check.expect(none && fewer && back, "M10 the Sites box removes exactly the " + num(site_entries) + " site entries (" +
                                                num(static_cast<int64_t>(best)) +
                                                " mines) and the key rows 'mine' and 'tower', and puts them back");
    }
}

// M11 (P5-06 11.4): towers and libraries. The sites of kind 4 of table site
// against the model's entries at E and around the first tower's opening;
// glyph, fill, offset (a library one glyph cell to the upper left of its
// settlement, a tower in the wild on its own cell), hover text and the
// glyph's place on screen.
void check_towers(Checker& check, ViewerScreen& screen, const LibraryWorld& lib) {
    godot::Ref<ViewerModel> model = screen.get_model();
    const int64_t end = model->get_end_year();
    std::vector<const worldgen::db::SiteRow*> towers;
    for (const auto& row : lib.history->tables.sites) {
        if (row.kind == 4) {
            towers.push_back(&row);
        }
    }
    int64_t libraries = 0;
    for (const auto* t : towers) {
        libraries += t->settlement != 0 ? 1 : 0;
    }
    check.expect(!towers.empty(), "M11 the world has " + num(static_cast<int64_t>(towers.size())) +
                                      " towers (sites of kind 4), " + num(libraries) + " of them libraries");
    if (towers.empty()) {
        return;
    }
    const auto model_towers = [&model]() {
        std::vector<int64_t> ids;
        for (const auto& site : model->history_state().markers().sites) {
            if (site.kind == 4) {
                ids.push_back(site.site);
            }
        }
        return ids;
    };
    std::string problem;
    // P5-04a 21.4: towers can end; every ended tower is checked the year
    // before its end (shown) and in its end year (gone).
    std::vector<int64_t> years = {towers.front()->origin_year - 1, towers.front()->origin_year, end};
    int64_t ended_towers = 0;
    for (const auto* t : towers) {
        if (t->ended_year) {
            ++ended_towers;
            years.push_back(*t->ended_year - 1);
            years.push_back(*t->ended_year);
        }
    }
    for (const int64_t y : years) {
        if (y < 0 || y > end) {
            continue;
        }
        model->set_year(y);
        std::vector<int64_t> want;
        for (const auto* t : towers) {
            if (t->origin_year <= y && (!t->ended_year || y < *t->ended_year)) {
                want.push_back(t->id);
            }
        }
        const auto lib_sites = lib.index->markers_at(*lib.history, y, worldgen::inspect::MarkerOptions{}).sites;
        if ((model_towers() != want || model->history_state().markers().sites != lib_sites) && problem.empty()) {
            problem = "year " + std::to_string(y) + ": model " + id_list(model_towers()) + ", table site " + id_list(want);
        }
    }
    check.expect(problem.empty(), "M11 the tower entries are the towers open at the year (before and at the first "
                                  "tower's year " + num(towers.front()->origin_year) + ", at E, and the year before "
                                  "and the end year of each of the " + num(ended_towers) + " ended towers)" +
                                      (problem.empty() ? godot::String() : ": " + to_godot(problem)));
    // The entries at E.
    model->set_year(end);
    const godot::Array markers = model->get_markers();
    LayerMapView* view = screen.map_view();
    const int64_t g = view->glyph_cell_size();
    const godot::Rect2 shown = view->get_shown_rect();
    const double zoom = view->get_zoom();
    std::string wrong;
    int64_t entries = 0;
    int64_t hovered = -1;
    int64_t hovered_count = 0;
    for (int64_t i = 0; i < markers.size(); ++i) {
        const godot::Dictionary m = markers[i];
        if (godot::String(m["type"]) != "site" || int64_t(m.get("site_kind", int64_t{0})) != 4) {
            continue;
        }
        ++entries;
        const int64_t id = m["id"];
        const worldgen::db::SiteRow* row = nullptr;
        for (const auto* t : towers) {
            row = t->id == id ? t : row;
        }
        if (row == nullptr) {
            wrong = wrong.empty() ? "entry " + std::to_string(i) + ": no tower " + std::to_string(id) : wrong;
            continue;
        }
        const bool library = row->settlement != 0;
        const int64_t ox = library ? -7 : 0;
        const int64_t oy = library ? -7 : 0;
        if (godot::String(m["glyph"]) != "tower" || int64_t(m["fill"]) != 0x783CB4 || int64_t(m["offset_x"]) != ox ||
            int64_t(m["offset_y"]) != oy || !bool(m["small"])) {
            wrong = wrong.empty() ? "entry " + std::to_string(i) + " " + to_utf8(godot::Variant(m).stringify()) : wrong;
        }
        // The hover text from the check's own read: the figure participant
        // of the tower's founding event.
        std::string figure = "?";
        if (const auto* origin = lib.history->view.event(row->origin_event)) {
            for (const auto& p : origin->participants) {
                if (p.entity_type == 6) {
                    for (const auto& f : lib.history->tables.figures) {
                        figure = f.id == p.entity_id ? worldgen::history::figure_display_name(f) : figure;
                    }
                    break;
                }
            }
        }
        const std::string want = row->name + (library ? " (library of " : " (tower of ") + figure + "), since " +
                                 std::to_string(row->origin_year);
        const godot::String text = model->describe_marker(i);
        if (to_utf8(text) != want && wrong.empty()) {
            wrong = "describe_marker of site " + std::to_string(id) + ": '" + to_utf8(text) + "', expected '" + want + "'";
        }
        const godot::Vector2i at = view->get_marker_screen_position(i);
        const auto cx = static_cast<int64_t>(std::floor(static_cast<double>(shown.position.x) + (static_cast<double>(int64_t(m["x"])) + 0.5) * zoom));
        const auto cy = static_cast<int64_t>(std::floor(static_cast<double>(shown.position.y) + (static_cast<double>(int64_t(m["y"])) + 0.5) * zoom));
        if ((at.x < 0 || std::abs(at.x - (cx + ox * g)) > 1 || std::abs(at.y - (cy + oy * g)) > 1) && wrong.empty()) {
            wrong = "site " + std::to_string(id) + " drawn at " + to_utf8(godot::String(godot::Variant(at))) +
                    ", cell centre " + std::to_string(cx) + "," + std::to_string(cy);
        }
        // The mouse over it: towers are drawn above every other entry
        // (P5-04a), so each tower that no other tower covers is hovered.
        if (at.x >= 0) {
            send_motion(screen, godot::Vector2(static_cast<float>(at.x), static_cast<float>(at.y)));
            const int64_t got = view->get_hovered_marker();
            const bool other_tower = got >= 0 && got != i && godot::String(godot::Dictionary(markers[got])["glyph"]) == "tower";
            if (got == i && screen.cell_label()->get_text() == text) {
                hovered = hovered < 0 ? id : hovered;
                ++hovered_count;
            } else if (!other_tower && wrong.empty()) {
                wrong = "the mouse over site " + std::to_string(id) + " hovers entry " + std::to_string(got);
            }
        }
    }
    screen.hover_cell(-1, -1);
    check.expect(wrong.empty() && entries == static_cast<int64_t>(model_towers().size()) && entries > 0 && hovered > 0,
                 "M11 at " + num(end) + " the " + num(entries) +
                     " tower entries: glyph tower, fill 120,60,180, a library 7 glyph cells left and up, a wild "
                     "tower on its cell, hover '<name> (tower|library of <figure>), since <year>'; the mouse over " +
                     num(hovered_count) + " of them (first site " + num(hovered) + ") shows its text, drawn above "
                     "every other entry" + (wrong.empty() ? godot::String() : ": " + to_godot(wrong)));
}

// M12 (P5-04a 21.7 item 4): a library, the site the script names
// (--expect-library <site id>, from the `library` line of [sites]). Absent
// the year before its origin; at its origin year (and at E, or the year
// before its end) an entry drawn one glyph cell to the upper left of its
// city's marker, with the hover text '<site name> (library of <figure
// display name>), since <year>' and a mouse motion over it showing that
// text; gone in its end year when it has ended.
void check_library(Checker& check, ViewerScreen& screen, int64_t site_id) {
    godot::Ref<ViewerModel> model = screen.get_model();
    const LibraryWorld lib = open_library_world(model->get_world_path());
    const worldgen::db::SiteRow* row = nullptr;
    if (lib.problem.empty()) {
        for (const auto& s : lib.history->tables.sites) {
            row = s.id == site_id ? &s : row;
        }
    }
    check.expect(row != nullptr && row->kind == 4 && row->settlement != 0,
                 "M12 site " + num(site_id) + " is a library (kind 4 in a settlement)" +
                     (lib.problem.empty() ? godot::String() : ": " + to_godot(lib.problem)));
    if (row == nullptr || row->kind != 4 || row->settlement == 0) {
        return;
    }
    std::string figure = "?";
    if (const auto* origin = lib.history->view.event(row->origin_event)) {
        for (const auto& p : origin->participants) {
            if (p.entity_type == 6) {
                for (const auto& f : lib.history->tables.figures) {
                    figure = f.id == p.entity_id ? worldgen::history::figure_display_name(f) : figure;
                }
                break;
            }
        }
    }
    const std::string want_text = row->name + " (library of " + figure + "), since " + std::to_string(row->origin_year);
    const int64_t end = model->get_end_year();
    const int64_t last = row->ended_year ? *row->ended_year - 1 : end;
    LayerMapView* view = screen.map_view();
    // The library's entry and its city's entry in the model at year y.
    const auto find = [&model, row](int64_t& library, int64_t& city) {
        library = -1;
        city = -1;
        const godot::Array markers = model->get_markers();
        for (int64_t i = 0; i < markers.size(); ++i) {
            const godot::Dictionary m = markers[i];
            const godot::String type = m["type"];
            if (type == "site" && int64_t(m["id"]) == row->id) {
                library = i;
            } else if (type != "site" && type != "place" && type != "pass" && int64_t(m["id"]) == row->settlement) {
                city = i;
            }
        }
    };
    int64_t library = -1;
    int64_t city = -1;
    if (row->origin_year > 0) {
        model->set_year(row->origin_year - 1);
        find(library, city);
        check.expect(library < 0, "M12 no entry for library " + num(row->id) + " in " + num(row->origin_year - 1) +
                                      ", the year before its origin");
    }
    for (const int64_t y : {row->origin_year, last}) {
        model->set_year(y);
        find(library, city);
        std::string wrong;
        godot::Vector2i at(-1, -1);
        godot::Vector2i city_at(-1, -1);
        if (library < 0 || city < 0) {
            wrong = library < 0 ? "no library entry" : "no entry for its city s" + std::to_string(row->settlement);
        } else {
            const godot::Dictionary m = model->get_markers()[library];
            const godot::Dictionary c = model->get_markers()[city];
            const int64_t g = view->glyph_cell_size();
            at = view->get_marker_screen_position(library);
            city_at = view->get_marker_screen_position(city);
            if (godot::String(m["glyph"]) != "tower" || int64_t(m["offset_x"]) != -7 || int64_t(m["offset_y"]) != -7 ||
                int64_t(m["x"]) != int64_t(c["x"]) || int64_t(m["y"]) != int64_t(c["y"])) {
                wrong = "entry " + to_utf8(godot::Variant(m).stringify()) + ", city " + to_utf8(godot::Variant(c).stringify());
            } else if (at.x < 0 || city_at.x < 0 || at.x != city_at.x - 7 * g || at.y != city_at.y - 7 * g) {
                wrong = "drawn at " + to_utf8(godot::String(godot::Variant(at))) + ", its city at " +
                        to_utf8(godot::String(godot::Variant(city_at))) + ", glyph cell " + std::to_string(g);
            } else if (to_utf8(model->describe_marker(library)) != want_text) {
                wrong = "hover text '" + to_utf8(model->describe_marker(library)) + "'";
            } else {
                send_motion(screen, godot::Vector2(static_cast<float>(at.x), static_cast<float>(at.y)));
                if (view->get_hovered_marker() != library || to_utf8(screen.cell_label()->get_text()) != want_text) {
                    wrong = "the mouse over it shows '" + to_utf8(screen.cell_label()->get_text()) + "'";
                }
                screen.hover_cell(-1, -1);
            }
        }
        check.expect(wrong.empty(), "M12 in " + num(y) + " library " + num(row->id) + " is drawn 7 glyph cells left "
                                    "and up of its city s" + num(row->settlement) + ", hover '" + to_godot(want_text) +
                                    "'" + (wrong.empty() ? godot::String() : ": " + to_godot(wrong)));
    }
    if (row->ended_year) {
        model->set_year(*row->ended_year);
        find(library, city);
        check.expect(library < 0, "M12 library " + num(row->id) + " is gone in its end year " + num(*row->ended_year));
    } else {
        godot::UtilityFunctions::print("UNMAPPED-CHECK: info: M12 library ", row->id, " stands at the end");
    }
    model->set_year(end);
}

// M13 (P5-07 11.2 item 1): a mine's hover gains ", delved <word>" from its
// first deepening and ", sealed in <year>" from its horror's sealing: the
// deepest mine (smallest id) before its first deepening, at each deepening
// and at its last open year; the first sealed mine the year before and the
// year of its sealing. M14 (item 4): the same for the deepest warren of a
// standing settlement, ", warrens delved <word>". With --expect-depths each
// case must exist.
void check_depths(Checker& check, ViewerScreen& screen, const ViewerCheckOptions& options) {
    godot::Ref<ViewerModel> model = screen.get_model();
    const LibraryWorld lib = open_library_world(model->get_world_path());
    if (!lib.problem.empty()) {
        check.expect(false, "M13 the check reads the world: " + to_godot(lib.problem));
        return;
    }
    const DeepWorld deep = read_deep(*lib.history);
    const int64_t end = model->get_end_year();
    // The hover text of entry (type, id) at y, or "(no entry)".
    const auto hover = [&model](bool site, int64_t id, int64_t y) -> std::string {
        model->set_year(y);
        const godot::Array markers = model->get_markers();
        for (int64_t i = 0; i < markers.size(); ++i) {
            const godot::Dictionary m = markers[i];
            const godot::String type = m["type"];
            if ((site ? type == "site" : type == "standing") && int64_t(m["id"]) == id) {
                return to_utf8(model->describe_marker(i));
            }
        }
        return "(no entry)";
    };
    // The text must end in the suffix and hold no other depth or seal part.
    const auto fits = [](const std::string& text, const std::string& suffix, const char* lead) {
        if (text.size() < suffix.size() || text.compare(text.size() - suffix.size(), suffix.size(), suffix) != 0) {
            return false;
        }
        const std::string head = text.substr(0, text.size() - suffix.size());
        return head.find(lead) == std::string::npos && head.find(", sealed in ") == std::string::npos &&
               head != "(no entry)";
    };
    struct Case {
        bool site;
        const char* name;
        const char* lead;
        const std::map<int64_t, DeepRecord>* records;
    };
    const Case cases[] = {{true, "M13 mine", ", delved ", &deep.mines}, {false, "M14 warren", ", warrens delved ", &deep.warrens}};
    for (const Case& c : cases) {
        // Open (or standing) years of the entity, from the check's own tables.
        const auto span = [&](int64_t id) -> std::pair<int64_t, int64_t> {
            if (c.site) {
                for (const auto& s : lib.history->tables.sites) {
                    if (s.id == id) {
                        return {s.origin_year, s.ended_year ? *s.ended_year - 1 : end};
                    }
                }
                return {0, -1};
            }
            const auto state_end = [&](int64_t y) {
                return lib.history->timeline.settlement_state(id, y).status;
            };
            // The last year the settlement stands, searched down from E.
            int64_t last = end;
            while (last >= 0 && (state_end(last) == 0 || state_end(last) >= 3)) {
                --last;
            }
            int64_t first = last;
            while (first > 0 && state_end(first - 1) != 0 && state_end(first - 1) < 3) {
                --first;
            }
            return {first, last};
        };
        int64_t best = 0;
        int64_t best_depth = 0;
        int64_t sealed_id = 0;
        for (const auto& [id, r] : *c.records) {
            if (id == 0) {
                continue;
            }
            int64_t d = 0;
            for (const auto& p : r.deepenings) {
                d = std::max(d, p.second);
            }
            const auto [from, to] = span(id);
            if (d > best_depth && !r.deepenings.empty() && r.deepenings.front().first <= to) {
                best = id;
                best_depth = d;
            }
            if (sealed_id == 0 && r.sealed >= 0 && r.sealed - 1 >= from && r.sealed <= to) {
                sealed_id = id;
            }
        }
        const std::string what = std::string(c.name);
        if (best == 0) {
            check.expect(!options.expect_depths, to_godot(what) + ": none delved in this world" +
                                                     (options.expect_depths ? " (--expect-depths)" : ""));
        } else {
            const DeepRecord& r = c.records->at(best);
            const auto [from, to] = span(best);
            std::vector<int64_t> years;
            if (r.deepenings.front().first - 1 >= from) {
                years.push_back(r.deepenings.front().first - 1);
            }
            for (const auto& p : r.deepenings) {
                if (p.first <= to) {
                    years.push_back(p.first);
                }
            }
            years.push_back(to);
            std::string wrong;
            for (const int64_t y : years) {
                const std::string text = hover(c.site, best, y);
                const std::string suffix = deep_suffix(r, y, c.lead);
                if (!fits(text, suffix, c.lead) && wrong.empty()) {
                    wrong = "in " + std::to_string(y) + " '" + text + "', expected the ending '" + suffix + "'";
                }
            }
            check.expect(wrong.empty(), to_godot(what) + (c.site ? " site " : " of settlement ") + num(best) + " (depth " + num(best_depth) + ") at " +
                                            num(static_cast<int64_t>(years.size())) +
                                            " years: the hover's depth part follows its deepenings" +
                                            (wrong.empty() ? godot::String() : ": " + to_godot(wrong)));
        }
        if (sealed_id == 0) {
            check.expect(!options.expect_depths || !c.site, to_godot(what) + ": none sealed while open the year before" +
                                                               (options.expect_depths && c.site ? " (--expect-depths)" : ""));
        } else {
            const DeepRecord& r = c.records->at(sealed_id);
            std::string wrong;
            for (const int64_t y : {r.sealed - 1, r.sealed}) {
                const std::string text = hover(c.site, sealed_id, y);
                const std::string suffix = deep_suffix(r, y, c.lead);
                if (!fits(text, suffix, c.lead) && wrong.empty()) {
                    wrong = "in " + std::to_string(y) + " '" + text + "', expected the ending '" + suffix + "'";
                }
            }
            check.expect(wrong.empty(), to_godot(what) + (c.site ? " site " : " of settlement ") + num(sealed_id) + ": ', sealed in " + num(r.sealed) +
                                            "' from its sealing year, not the year before" +
                                            (wrong.empty() ? godot::String() : ": " + to_godot(wrong)));
        }
    }
    godot::UtilityFunctions::print("UNMAPPED-CHECK: info: M13 hold_lost events: ", deep.lost);
    // M15 (P5-07 11.1, 11.2 item 4 of the brief): each kind 93 to 99 the
    // world has: its first event is listed by the browser under the group
    // of 11.1 at level full and has a place square.
    {
        const std::pair<const char*, const char*> groups[] = {
            {"delving_deepened", "settlements"}, {"deep_woken", "monsters"},  {"deep_sealed", "monsters"},
            {"hold_lost", "monsters"},           {"ally_aided_hold", "alliances"}, {"warren_delved", "settlements"},
            {"warren_sealed", "monsters"}};
        model->set_browser_scope("all");
        model->set_browser_level("full");
        std::string wrong;
        int64_t kinds = 0;
        for (const auto& [key, group] : groups) {
            const auto* kind = lib.history->view.catalogue().find(key);
            int64_t first = 0;
            for (const auto& e : lib.history->view.events()) {
                if (kind != nullptr && e.row.kind == kind->id) {
                    first = e.row.id;
                    break;
                }
            }
            if (first == 0) {
                continue;
            }
            ++kinds;
            model->set_browser_group(group);
            const godot::PackedInt64Array ids = model->get_event_ids();
            bool listed = false;
            for (int64_t i = 0; i < ids.size(); ++i) {
                listed = listed || ids[i] == first;
            }
            const godot::Vector2i place = model->get_event_detail(first)["place"];
            if ((!listed || place.x < 0) && wrong.empty()) {
                wrong = std::string(key) + " e" + std::to_string(first) + (listed ? "" : " not listed under ") +
                        (listed ? "" : group) + (place.x < 0 ? " has no place" : "");
            }
        }
        model->set_browser_group("all");
        model->set_browser_level("summary");
        check.expect(wrong.empty() && (kinds >= 4 || !options.expect_depths),
                     "M15 the first event of each of the " + num(kinds) +
                         " depth kinds (93 to 99) present is listed under its group and has a place" +
                         (wrong.empty() ? godot::String() : ": " + to_godot(wrong)));
    }
    model->set_year(end);
}

// --- Pantheon and cults (P5-08 11.2): M16 to M18 and B16 ---------------------

// The entity of role `role` in event e (the kind's role by key), 0 none.
int64_t role_entity(const worldgen::history::HistoryView& v, const worldgen::history::HistoryView::Event* e,
                    const char* role) {
    if (e == nullptr) {
        return 0;
    }
    const auto* kind = v.catalogue().find_id(e->row.kind);
    const auto* r = kind != nullptr ? kind->find_role(role) : nullptr;
    for (const auto& p : e->participants) {
        if (r != nullptr && p.role == r->seq) {
            return p.entity_id;
        }
    }
    return 0;
}

// The check's own text of 11.2 item 2 for a shrine, temple or sanctum at Y:
// the god of the shrine's divine_mark (role actor, value profane), the cult
// of the origin event (role faction), names from the check's tables.
std::string expected_faith_text(const worldgen::history::LoadedHistory& h, const worldgen::db::SiteRow& row,
                                int64_t year) {
    const auto& v = h.view;
    const auto* origin = v.event(row.origin_event);
    std::string text = row.name;
    if (row.kind == 5) {
        const int64_t god = role_entity(v, origin, "actor");
        std::string god_name = "?";
        for (const auto& g : h.tables.gods) {
            god_name = g.id == god ? g.name : god_name;
        }
        bool profane = false;
        const auto* kind = origin != nullptr ? v.catalogue().find_id(origin->row.kind) : nullptr;
        const int64_t seq = kind != nullptr ? kind->value_seq("profane") : 0;
        if (seq >= 1 && static_cast<std::size_t>(seq) <= origin->values.size()) {
            profane = origin->values[static_cast<std::size_t>(seq - 1)] != 0;
        }
        text += std::string(profane ? " (cursed by " : " (holy to ") + god_name + ")";
    } else {
        const int64_t cult = role_entity(v, origin, "faction");
        std::string cult_name = "?";
        for (const auto& k : h.tables.factions) {
            cult_name = k.id == cult ? k.name : cult_name;
        }
        text += std::string(row.kind == 6 ? " (temple of " : " (hidden sanctum of ") + cult_name + ")";
    }
    text += ", since " + std::to_string(row.origin_year);
    if (row.kind == 6 && row.ended_year && *row.ended_year <= year) {
        const char* word = row.ending == 8 ? "suppressed" : row.ending == 7 ? "abandoned" : "lost";
        text += std::string(", ") + word + " in " + std::to_string(*row.ended_year);
    }
    return text;
}

// The index of the model's entry of (type, id) at the current year, -1 none.
int64_t marker_entry(const godot::Ref<ViewerModel>& model, const char* type, int64_t id) {
    const godot::Array markers = model->get_markers();
    for (int64_t i = 0; i < markers.size(); ++i) {
        const godot::Dictionary m = markers[i];
        if (godot::String(m["type"]) == type && int64_t(m["id"]) == id) {
            return i;
        }
    }
    return -1;
}

bool standing_at(const worldgen::history::LoadedHistory& h, int64_t settlement, int64_t year) {
    const auto s = h.timeline.settlement_state(settlement, year);
    return s.exists && (s.status == 1 || s.status == 2);
}

// M16: the first shrine (one at a settlement first), temple and sanctum of
// the world: absent the year before their origin, then at their origin year
// and their last open year an entry with glyph, fill and offset, the hover
// text of 11.2 item 2 (model and mouse), and the settlement's hover with the
// shrine or temple. The Shrines and Temples boxes. M17: an ended temple
// (abandoned or suppressed at a standing settlement first). M18: a realm's
// faith in the territory hover.
void check_pantheon_markers(Checker& check, ViewerScreen& screen, const ViewerCheckOptions& options,
                            const LibraryWorld& lib) {
    godot::Ref<ViewerModel> model = screen.get_model();
    const auto& h = *lib.history;
    const int64_t end = model->get_end_year();
    LayerMapView* view = screen.map_view();
    const worldgen::db::SiteRow* picks[3] = {nullptr, nullptr, nullptr};
    for (const auto& s : h.tables.sites) {
        if (s.kind < 5 || s.kind > 7) {
            continue;
        }
        const worldgen::db::SiteRow*& pick = picks[s.kind - 5];
        if (pick == nullptr || (s.kind == 5 && pick->settlement == 0 && s.settlement != 0)) {
            pick = &s;
        }
    }
    const char* glyphs[3] = {"shrine", "temple", "sanctum"};
    const worldgen::inspect::Rgb fills[3] = {worldgen::inspect::MARKER_SHRINE, worldgen::inspect::MARKER_TEMPLE,
                                             worldgen::inspect::MARKER_SANCTUM};
    for (std::size_t k = 0; k < 3; ++k) {
        const worldgen::db::SiteRow* row = picks[k];
        const std::string what = std::string("M16 ") + glyphs[k];
        if (row == nullptr) {
            if (options.expect_pantheon) {
                check.expect(false, to_godot(what) + ": none in this world (--expect-pantheon)");
            } else {
                godot::UtilityFunctions::print("UNMAPPED-CHECK: info: ", to_godot(what), ": none in this world");
            }
            continue;
        }
        const int64_t last = row->ended_year ? *row->ended_year - 1 : end;
        if (row->origin_year > 0) {
            model->set_year(row->origin_year - 1);
            check.expect(marker_entry(model, "site", row->id) < 0,
                         to_godot(what) + " si" + num(row->id) + " has no entry in " + num(row->origin_year - 1));
        }
        const int64_t ox = row->kind == 5 && row->settlement != 0 ? 7 : row->kind == 6 ? -7 : 0;
        const int64_t oy = row->settlement != 0 && row->kind != 7 ? 7 : 0;
        const int64_t fill = (int64_t{fills[k].r} << 16) | (int64_t{fills[k].g} << 8) | int64_t{fills[k].b};
        std::string wrong;
        for (const int64_t y : {row->origin_year, last}) {
            model->set_year(y);
            const std::string want = expected_faith_text(h, *row, y);
            const int64_t i = marker_entry(model, "site", row->id);
            if (i < 0) {
                wrong = "no entry in " + std::to_string(y);
                break;
            }
            const godot::Dictionary m = model->get_markers()[i];
            if (godot::String(m["glyph"]) != glyphs[k] || int64_t(m["fill"]) != fill || int64_t(m["offset_x"]) != ox ||
                int64_t(m["offset_y"]) != oy || int64_t(m["site_kind"]) != row->kind) {
                wrong = "entry " + to_utf8(godot::Variant(m).stringify());
                break;
            }
            if (to_utf8(model->describe_marker(i)) != want) {
                wrong = "in " + std::to_string(y) + " hover '" + to_utf8(model->describe_marker(i)) + "', expected '" +
                        want + "'";
                break;
            }
            const godot::Vector2i at = view->get_marker_screen_position(i);
            if (at.x >= 0) {
                send_motion(screen, godot::Vector2(static_cast<float>(at.x), static_cast<float>(at.y)));
                const bool hovered = view->get_hovered_marker() == i && to_utf8(screen.cell_label()->get_text()) == want;
                screen.hover_cell(-1, -1);
                if (!hovered) {
                    wrong = "in " + std::to_string(y) + " the mouse over it shows '" +
                            to_utf8(screen.cell_label()->get_text()) + "'";
                    break;
                }
            }
            // The settlement's hover names its shrine or temple.
            if (row->settlement != 0 && row->kind != 7 && standing_at(h, row->settlement, y)) {
                const int64_t s = marker_entry(model, "standing", row->settlement);
                const std::string text = s >= 0 ? to_utf8(model->describe_marker(s)) : std::string("(no entry)");
                if (text.find("; " + want) == std::string::npos) {
                    wrong = "in " + std::to_string(y) + " settlement s" + std::to_string(row->settlement) + " hover '" +
                            text + "' lacks '; " + want + "'";
                    break;
                }
            }
        }
        check.expect(wrong.empty(), to_godot(what) + " si" + num(row->id) + " in " + num(row->origin_year) + " and " +
                                        num(last) + ": glyph " + glyphs[k] + ", its fill, offset (" + num(ox) + ", " +
                                        num(oy) + "), hover '" + to_godot(expected_faith_text(h, *row, last)) + "'" +
                                        (wrong.empty() ? godot::String() : ": " + to_godot(wrong)));
    }
    // The boxes, at E.
    model->set_year(end);
    const auto count_kinds = [&model](int64_t from, int64_t to) {
        int64_t n = 0;
        const godot::Array markers = model->get_markers();
        for (int64_t i = 0; i < markers.size(); ++i) {
            const godot::Dictionary m = markers[i];
            const int64_t kind = m.get("site_kind", int64_t{0});
            n += godot::String(m["type"]) == "site" && kind >= from && kind <= to ? 1 : 0;
        }
        return n;
    };
    const auto legend_has_label = [&model](const char* label) {
        const auto labels = legend_labels(model->get_marker_legend());
        return std::find(labels.begin(), labels.end(), std::string(label)) != labels.end();
    };
    {
        const int64_t total = model->get_markers().size();
        const int64_t shrines = count_kinds(5, 5);
        const int64_t temples = count_kinds(6, 7);
        screen.mark_shrines()->set_pressed(false);
        const bool shrines_off = !model->get_marker_option("shrines") && count_kinds(5, 5) == 0 &&
                                 model->get_markers().size() == total - shrines && !legend_has_label("shrine") &&
                                 legend_has_label("temple");
        screen.mark_shrines()->set_pressed(true);
        screen.mark_temples()->set_pressed(false);
        const bool temples_off = !model->get_marker_option("temples") && count_kinds(6, 7) == 0 &&
                                 model->get_markers().size() == total - temples && !legend_has_label("temple") &&
                                 !legend_has_label("hidden sanctum") && legend_has_label("shrine");
        screen.mark_temples()->set_pressed(true);
        screen.mark_sites()->set_pressed(false);
        const bool follow_sites = screen.mark_shrines()->is_disabled() && screen.mark_temples()->is_disabled() &&
                                  count_kinds(5, 7) == 0;
        screen.mark_sites()->set_pressed(true);
        const bool back = model->get_markers().size() == total && !screen.mark_shrines()->is_disabled() &&
                          !screen.mark_temples()->is_disabled();
        check.expect(shrines_off && temples_off && follow_sites && back,
                     "M16 in " + num(end) + " the Shrines box removes the " + num(shrines) +
                         " shrine entries and the key row 'shrine', Temples the " + num(temples) +
                         " temple and sanctum entries and their rows; both follow Sites; all come back");
    }

    // M17: an ended temple.
    const worldgen::db::SiteRow* ended = nullptr;
    for (const auto& s : h.tables.sites) {
        if (s.kind != 6 || !s.ended_year) {
            continue;
        }
        const bool good = (s.ending == 7 || s.ending == 8) && standing_at(h, s.settlement, *s.ended_year);
        if (ended == nullptr || (good && !((ended->ending == 7 || ended->ending == 8) &&
                                           standing_at(h, ended->settlement, *ended->ended_year)))) {
            ended = &s;
        }
    }
    if (ended == nullptr) {
        if (options.expect_pantheon) {
            check.expect(false, "M17 an ended temple: none in this world (--expect-pantheon)");
        } else {
            godot::UtilityFunctions::print("UNMAPPED-CHECK: info: M17 no ended temple in this world");
        }
    } else {
        const int64_t y = *ended->ended_year;
        std::string wrong;
        if (y - 1 >= ended->origin_year) {
            model->set_year(y - 1);
            if (marker_entry(model, "site", ended->id) < 0) {
                wrong = "no entry in " + std::to_string(y - 1);
            }
        }
        model->set_year(y);
        if (wrong.empty() && marker_entry(model, "site", ended->id) >= 0) {
            wrong = "an entry in its end year";
        }
        const std::string want = expected_faith_text(h, *ended, y);
        const bool settlement_shown = standing_at(h, ended->settlement, y);
        if (wrong.empty() && settlement_shown) {
            const int64_t s = marker_entry(model, "standing", ended->settlement);
            const std::string text = s >= 0 ? to_utf8(model->describe_marker(s)) : std::string("(no entry)");
            if (text.find("; " + want) == std::string::npos) {
                wrong = "settlement hover '" + text + "' lacks '; " + want + "'";
            }
        }
        if (wrong.empty() && options.expect_pantheon && !settlement_shown) {
            wrong = "its settlement does not stand in its end year (--expect-pantheon wants one that does)";
        }
        check.expect(wrong.empty(), "M17 temple si" + num(ended->id) + " (ending " + num(ended->ending) + ") has an entry in " +
                                        num(y - 1) + ", none in its end year " + num(y) +
                                        (settlement_shown ? ", and its settlement's hover reads '" + to_godot(want) + "'"
                                                          : godot::String(" (its settlement is gone)")) +
                                        (wrong.empty() ? godot::String() : ": " + to_godot(wrong)));
    }

    // M18: a realm's faith in the territory hover, at E - 1.
    const int64_t y = end > 0 ? end - 1 : 0;
    model->set_year(y);
    int64_t civ = 0;
    int64_t cult = 0;
    for (const auto& c : h.tables.civilizations) {
        if (civ == 0 && h.timeline.faith_at(c.id, y) != 0) {
            civ = c.id;
            cult = h.timeline.faith_at(c.id, y);
        }
    }
    int64_t region = 0;
    for (const auto& r : lib.regions) {
        const auto holding = model->history_state().holding(r.id);
        if (region == 0 && holding && holding->civ == civ && civ != 0) {
            region = r.id;
        }
    }
    if (civ == 0 || region == 0) {
        check.expect(!options.expect_pantheon, "M18 a realm with a faith and a region at " + num(y) +
                                                   (options.expect_pantheon ? ": none (--expect-pantheon)" : ": none"));
    } else {
        std::string name = "?";
        for (const auto& k : h.tables.factions) {
            name = k.id == cult ? k.name : name;
        }
        const std::string text = model->history_state().holding_text(region);
        const std::string suffix = ", faith: " + name;
        const bool ends = text.size() >= suffix.size() &&
                          text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
        check.expect(ends, "M18 region " + num(region) + " of c" + num(civ) + " in " + num(y) + ": the hover ends '" +
                               to_godot(suffix) + "': '" + to_godot(text) + "'");
    }
    model->set_year(end);
}

// B16: the group gods, the Gods tab, a cult and a god selected with their
// blocks and event lists, the realm's faith line and a site link.
void check_pantheon_browser(Checker& check, ViewerScreen& screen, const ViewerCheckOptions& options,
                            const LibraryWorld& lib) {
    godot::Ref<ViewerModel> model = screen.get_model();
    if (!model->get_browser_available()) {
        check.expect(false, "B16 the browser is available");
        return;
    }
    const auto& h = *lib.history;
    const auto& v = h.view;
    const auto ids_of = [&model]() {
        std::vector<int64_t> out;
        const godot::PackedInt64Array ids = model->get_event_ids();
        for (int64_t i = 0; i < ids.size(); ++i) {
            out.push_back(ids[i]);
        }
        return out;
    };
    model->clear_selection();
    model->set_browser_level("full");
    model->set_browser_years(-1, -1);
    model->set_browser_this_year(false);
    // The group gods: kinds 100 to 108 by key (spec 8.1, 11.1).
    {
        std::set<int64_t> kinds;
        for (const char* key : {"divine_bless", "divine_curse", "divine_champion", "divine_monster", "divine_mark",
                                "cult_founded", "cult_spread", "cult_adopted", "cult_suppressed"}) {
            if (const auto* k = v.catalogue().find(key)) {
                kinds.insert(k->id);
            }
        }
        std::vector<int64_t> want;
        for (const auto& e : v.events()) {
            if (kinds.count(e.row.kind) != 0) {
                want.push_back(e.row.id);
            }
        }
        const bool set = model->set_browser_group("gods");
        bool listed = false;
        for (int32_t i = 0; i < screen.event_group()->get_item_count(); ++i) {
            listed = listed || screen.event_group()->get_item_text(i) == "gods";
        }
        const std::vector<int64_t> got = ids_of();
        check.expect(set && listed && kinds.size() == 9 && got == want && !want.empty() &&
                         godot::String(model->get_selection()["group"]) == "gods",
                     "B16 the group 'gods' is offered and lists the " + num(static_cast<int64_t>(want.size())) +
                         " events of kinds 100 to 108 at full (got " + num(static_cast<int64_t>(got.size())) + ")");
        model->set_browser_group("all");
    }
    // The Gods tab: every god, then every cult.
    const godot::Array gods = model->get_god_list();
    check.expect(gods.size() == static_cast<int64_t>(h.tables.gods.size() + h.tables.factions.size()) &&
                     tree_row_count(screen.god_list()) == gods.size(),
                 "B16 the Gods tab lists the " + num(static_cast<int64_t>(h.tables.gods.size())) + " gods and " +
                     num(static_cast<int64_t>(h.tables.factions.size())) + " cults");
    if (h.tables.factions.empty()) {
        check.expect(!options.expect_pantheon, godot::String("B16 a cult to select: none") +
                                                   (options.expect_pantheon ? " (--expect-pantheon)" : ""));
        model->clear_selection();
        model->set_browser_level("summary");
        return;
    }
    // A cult: the first with a temple seat.
    const worldgen::db::FactionRow& cult = h.tables.factions.front();
    const auto events_naming = [&v](int64_t type, int64_t id) {
        std::vector<int64_t> out;
        for (const auto& e : v.events()) {
            for (const auto& p : e.participants) {
                if (p.entity_type == type && p.entity_id == id) {
                    out.push_back(e.row.id);
                    break;
                }
            }
        }
        return out;
    };
    // A block's links: every target followable, and the targets.
    const auto block_targets = [&model](const godot::Array& block, bool& followable) {
        std::set<std::string> targets;
        followable = true;
        for (int64_t i = 0; i < block.size(); ++i) {
            const godot::Dictionary line = block[i];
            const godot::Array links = line["links"];
            for (int64_t j = 0; j < links.size(); ++j) {
                const godot::String target = godot::Dictionary(links[j])["target"];
                targets.insert(to_utf8(target));
                const bool plain = target.begins_with("ra") || target.begins_with("m");
                followable = followable && (plain || model->can_follow_link(target));
            }
        }
        return targets;
    };
    {
        const godot::String target = "fa" + num(cult.id);
        const bool followed = model->follow_link(target);
        const godot::Dictionary sel = model->get_selection();
        const godot::Array block = model->get_cult_block(cult.id);
        bool followable = false;
        const auto targets = block_targets(block, followable);
        const std::string detail = to_utf8(screen.browser_detail()->get_parsed_text());
        const std::string first = block.is_empty() ? std::string() : to_utf8(godot::Dictionary(block[0])["text"]);
        const std::vector<int64_t> want = events_naming(7, cult.id);
        const bool ok = followed && int64_t(sel["entity_type"]) == 7 && int64_t(sel["entity_id"]) == cult.id &&
                        godot::String(sel["scope"]) == "entity" && !block.is_empty() &&
                        targets.count("g" + std::to_string(cult.god)) != 0 && followable && !first.empty() &&
                        detail.find(first) != std::string::npos && screen.entity_tabs()->get_current_tab() == 2 &&
                        ids_of() == want && !want.empty();
        check.expect(ok, "B16 selecting cult fa" + num(cult.id) + " (" + to_godot(cult.name) +
                             ") shows its [cults] line with a link to g" + num(cult.god) + ", brings the Gods tab "
                             "forward and lists the " + num(static_cast<int64_t>(want.size())) + " events naming it (got " +
                             num(model->get_event_ids().size()) + ")");
        // The god of the cult, through the block's link.
        const bool god_followed = model->follow_link("g" + num(cult.god));
        const godot::Dictionary gsel = model->get_selection();
        const godot::Array gblock = model->get_god_block(cult.god);
        bool gfollow = false;
        block_targets(gblock, gfollow);
        const std::vector<int64_t> gwant = events_naming(2, cult.god);
        check.expect(god_followed && int64_t(gsel["entity_type"]) == 2 && int64_t(gsel["entity_id"]) == cult.god &&
                         !gblock.is_empty() && gfollow && ids_of() == gwant,
                     "B16 its link g" + num(cult.god) + " selects the god: its [pantheon] line, links followable, the " +
                         num(static_cast<int64_t>(gwant.size())) + " events naming it");
    }
    // The realm's faith: civilization_block lines 'gods:' and 'faith:'.
    {
        int64_t civ = 0;
        int64_t faith = 0;
        const int64_t end = model->get_end_year();
        for (const auto& c : h.tables.civilizations) {
            for (int64_t y = 0; y <= end && civ == 0; y += 50) {
                if (h.timeline.faith_at(c.id, y) != 0) {
                    civ = c.id;
                    faith = h.timeline.faith_at(c.id, y);
                }
            }
        }
        if (civ == 0) {
            check.expect(!options.expect_pantheon, "B16 a realm with a faith: none");
        } else {
            model->select_civilization(civ);
            const godot::Array block = model->get_civilization_block(civ);
            std::string wrong = "no line 'faith:' with a link to fa" + std::to_string(faith);
            bool gods_line = false;
            for (int64_t i = 0; i < block.size(); ++i) {
                const godot::Dictionary line = block[i];
                const godot::String text = godot::String(line["text"]).strip_edges();
                gods_line = gods_line || text.begins_with("gods:");
                if (!text.begins_with("faith:")) {
                    continue;
                }
                const godot::Array links = line["links"];
                for (int64_t j = 0; j < links.size(); ++j) {
                    const godot::Dictionary link = links[j];
                    if (godot::String(link["target"]) == "fa" + num(faith)) {
                        const bool followed = model->follow_link(link["target"]);
                        const godot::Dictionary sel = model->get_selection();
                        wrong = followed && int64_t(sel["entity_type"]) == 7 && int64_t(sel["entity_id"]) == faith
                                    ? std::string()
                                    : "the link did not select the cult";
                        break;
                    }
                }
                break;
            }
            check.expect(wrong.empty(), "B16 realm c" + num(civ) + "'s block has a 'faith:' line whose link fa" +
                                            num(faith) + " selects the cult" + (gods_line ? " (and a 'gods:' line)" : "") +
                                            (wrong.empty() ? godot::String() : ": " + to_godot(wrong)));
        }
    }
    // A site link: the cult's seat selects its settlement.
    {
        const worldgen::db::SiteRow* seat = nullptr;
        for (const auto& s : h.tables.sites) {
            seat = s.id == cult.seat ? &s : seat;
        }
        const bool followed = model->follow_link("si" + num(cult.seat));
        const godot::Dictionary sel = model->get_selection();
        check.expect(seat != nullptr && followed && int64_t(sel["place_type"]) == 4 &&
                         int64_t(sel["place_id"]) == seat->settlement && godot::String(sel["scope"]) == "place",
                     "B16 the link si" + num(cult.seat) + " (the cult's temple) selects its settlement as the place");
    }
    model->clear_selection();
    model->set_browser_level("summary");
}

void check_pantheon(Checker& check, ViewerScreen& screen, const ViewerCheckOptions& options) {
    godot::Ref<ViewerModel> model = screen.get_model();
    const LibraryWorld lib = open_library_world(model->get_world_path());
    if (!lib.problem.empty()) {
        check.expect(false, "M16 the check reads the world: " + to_godot(lib.problem));
        return;
    }
    const int64_t year = model->get_year();
    check_pantheon_markers(check, screen, options, lib);
    check_pantheon_browser(check, screen, options, lib);
    model->set_year(year);
}

void check_markers(Checker& check, ViewerScreen& screen, const ViewerCheckOptions& options,
                   const HistoryReport& report) {
    godot::Ref<ViewerModel> model = screen.get_model();
    if (!model->get_markers_available()) {
        check.expect(false, "M markers not available");
        return;
    }
    const int64_t end = model->get_end_year();
    const int64_t territory = model->get_territory_view();
    const godot::Callable counter = callable_mp_static(&count_markers_changed);
    model->connect("markers_changed", counter);
    LibraryWorld lib = open_library_world(model->get_world_path());
    check.expect(lib.problem.empty(), "M the check's own read of the world and its marker index" +
                                          (lib.problem.empty() ? godot::String() : ": " + to_godot(lib.problem)));
    if (!lib.problem.empty()) {
        model->disconnect("markers_changed", counter);
        return;
    }
    std::map<int64_t, const ReportSettlement*> by_id;
    for (const auto& s : report.settlements) {
        by_id[s.id] = &s;
    }
    int64_t r0 = -1;
    for (const auto& s : report.settlements) {
        if (s.ruined >= 0 && (r0 < 0 || s.ruined < r0)) {
            r0 = s.ruined;
        }
    }
    // P5-06 25.1 (e): D0 = R1 + OLD_RUIN_YEARS, R1 the smallest ruined year
    // of [present] with R1 + OLD_RUIN_YEARS <= E; no ruin fades.
    constexpr int64_t old_years = worldgen::inspect::OLD_RUIN_YEARS;
    int64_t r1 = -1;
    int64_t r1_settlement = 0;
    for (const auto& s : report.settlements) {
        if (s.ruined >= 0 && s.ruined + old_years <= end && (r1 < 0 || s.ruined < r1)) {
            r1 = s.ruined;
            r1_settlement = s.id;
        }
    }
    const int64_t d0 = r1 >= 0 ? r1 + old_years : -1;
    const auto is_old_at = [old_years](const ReportSettlement& s, int64_t year) {
        return s.status == "ruined" && s.ruined >= 0 && year - s.ruined >= old_years;
    };
    int64_t faded_lines = 0;
    for (const auto& e : report.events) {
        faded_lines += e.kind == "ruin_faded" ? 1 : 0;
    }
    check.expect(r0 >= 0 && d0 >= 0 && faded_lines == 0 && !report.settlements.empty(),
                 "M the report has settlements (" + num(static_cast<int64_t>(report.settlements.size())) +
                     "), a ruin R0 = " + num(r0) + ", an old ruin s" + num(r1_settlement) + " ruined in R1 = " + num(r1) +
                     " (D0 = " + num(d0) + ") and no ruin_faded line (" + num(faded_lines) + ")");

    // M2: at E with faded on, the entries against [present].
    model->set_year(end);
    model->set_marker_option("faded", true);
    {
        std::vector<std::string> got;
        std::vector<std::string> want;
        std::vector<std::string> got_off;
        std::vector<std::string> want_off;
        auto entries = model->history_state().markers().settlements;
        std::sort(entries.begin(), entries.end(),
                  [](const auto& a, const auto& b) { return a.settlement < b.settlement; });
        for (const auto& s : entries) {
            got.push_back(std::to_string(s.settlement) + " " +
                          std::string(worldgen::history::settlement_kind_word(s.kind)) + " " +
                          std::string(worldgen::history::settlement_status_word(s.status)) + " " +
                          std::to_string(s.population) + " " + std::to_string(s.owner) + " " + std::to_string(s.x) +
                          " " + std::to_string(s.y));
        }
        for (const auto& s : report.settlements) {
            want.push_back(std::to_string(s.id) + " " + s.kind + " " + s.status + " " + std::to_string(s.population) +
                           " " + std::to_string(s.owner) + " " + std::to_string(s.x) + " " + std::to_string(s.y));
            if (s.status != "faded" && !is_old_at(s, end)) {
                want_off.push_back(want.back());
            }
        }
        std::string first_diff;
        for (std::size_t i = 0; i < std::max(got.size(), want.size()) && first_diff.empty(); ++i) {
            if (i >= got.size() || i >= want.size() || got[i] != want[i]) {
                first_diff = ": entry " + std::to_string(i) + " '" + (i < got.size() ? got[i] : "none") +
                             "', [present] '" + (i < want.size() ? want[i] : "none") + "'";
            }
        }
        check.expect(first_diff.empty(), "M2 at " + num(end) + " with faded ruins: " +
                                             num(static_cast<int64_t>(got.size())) +
                                             " settlement entries equal the [present] lines" + to_godot(first_diff));
        std::set<int64_t> capitals;
        for (const auto& s : entries) {
            if (s.capital) {
                capitals.insert(s.settlement);
            }
        }
        std::set<int64_t> want_capitals;
        for (const auto& c : report.civs) {
            const auto it = report.capitals.find(c.id);
            if (report.alive(c, end) && it != report.capitals.end()) {
                want_capitals.insert(it->second);
            }
        }
        check.expect(!capitals.empty() && capitals == want_capitals,
                     "M2 the " + num(static_cast<int64_t>(capitals.size())) +
                         " capital entries are the capitals of the civilizations alive at the present (" +
                         num(static_cast<int64_t>(want_capitals.size())) + ")");
        model->set_marker_option("faded", false);
        for (const auto& s : model->history_state().markers().settlements) {
            const auto it = by_id.find(s.settlement);
            if (it != by_id.end()) {
                got_off.push_back(std::to_string(s.settlement));
            }
        }
        std::vector<std::string> want_off_ids;
        for (const auto& s : report.settlements) {
            if (s.status != "faded" && !is_old_at(s, end)) {
                want_off_ids.push_back(std::to_string(s.id));
            }
        }
        check.expect(got_off == want_off_ids && model->history_state().markers().settlements.size() == want_off.size(),
                     "M2 without old ruins: the " + num(static_cast<int64_t>(want_off.size())) +
                         " lines less the old ruins at E (" +
                         num(static_cast<int64_t>(report.settlements.size() - want_off.size())) + ")");
    }

    // M3: ruins and faded ruins.
    {
        std::string problem;
        int64_t ruins = 0;
        for (const auto& s : report.settlements) {
            if (s.ruined < 1) {
                continue;
            }
            ++ruins;
            model->set_year(s.ruined - 1);
            const auto* before = model_settlement(*model.ptr(), s.id);
            const bool standing = before != nullptr && (before->status == 1 || before->status == 2);
            model->set_year(s.ruined);
            const auto* after = model_settlement(*model.ptr(), s.id);
            if (!standing || after == nullptr || after->status != 3 || after->ruined_year != s.ruined) {
                problem = "s" + std::to_string(s.id) + " ruined " + std::to_string(s.ruined);
                break;
            }
        }
        check.expect(problem.empty() && ruins > 0, "M3 " + num(ruins) +
                                                       " ruins: standing the year before, a ruin in their year" +
                                                       (problem.empty() ? godot::String() : ": " + to_godot(problem)));
        // P5-06 25.1 (e): the settlement of R1 is a ruin at D0 - 1, absent
        // at D0 with the option off and an old ruin (type faded) with it on.
        problem.clear();
        if (r1_settlement != 0) {
            const godot::String sid = num(r1_settlement);
            model->set_year(d0 - 1);
            const auto* before = model_settlement(*model.ptr(), r1_settlement);
            const bool ruin = before != nullptr && type_word(*before) == "ruin" &&
                              marker_index_of(model->get_markers(), "ruin", r1_settlement) >= 0;
            model->set_year(d0);
            const bool absent = model_settlement(*model.ptr(), r1_settlement) == nullptr;
            model->set_marker_option("faded", true);
            const auto* shown = model_settlement(*model.ptr(), r1_settlement);
            const int64_t index = marker_index_of(model->get_markers(), "faded", r1_settlement);
            const bool old = shown != nullptr && shown->status == 3 && shown->old_ruin && index >= 0 &&
                             godot::String(godot::Dictionary(model->get_markers()[index])["glyph"]) == "faded" &&
                             model->describe_marker(index).contains(", old ruin of a ");
            model->set_marker_option("faded", false);
            if (!ruin || !absent || !old) {
                problem = std::string("s") + std::to_string(r1_settlement) + ": " + (ruin ? "" : "not a ruin at D0 - 1 ") +
                          (absent ? "" : "shown at D0 without the option ") + (old ? "" : "not an old ruin at D0 with it");
            }
        }
        check.expect(problem.empty() && r1_settlement != 0,
                     "M3 s" + num(r1_settlement) + " ruined in " + num(r1) + ": a ruin in " + num(d0 - 1) + ", in " +
                         num(d0) + " absent, or an old ruin ('old ruin of a') with the option" +
                         (problem.empty() ? godot::String() : ": " + to_godot(problem)));
    }

    // M4: event places.
    {
        std::map<int64_t, std::pair<int64_t, int64_t>> seeds;
        for (const auto& r : lib.regions) {
            seeds[r.id] = {r.seed_x, r.seed_y};
        }
        std::vector<int64_t> years = options.marker_years;
        std::set<int64_t> placed_years;
        for (const auto& e : report.events) {
            if ((e.settlement != 0 || e.region != 0) && placed_years.size() < 20) {
                placed_years.insert(e.start);
            }
        }
        years.insert(years.end(), placed_years.begin(), placed_years.end());
        std::string problem;
        int64_t places = 0;
        for (const int64_t y : years) {
            model->set_year(y);
            std::set<int64_t> got;
            for (const auto& p : model->history_state().markers().places) {
                ++places;
                for (const int64_t e : p.events) {
                    got.insert(e);
                    const ReportEvent* ev = nullptr;
                    for (const auto& r : report.events) {
                        if (r.id == e) {
                            ev = &r;
                        }
                    }
                    std::pair<int64_t, int64_t> cell{-1, -1};
                    if (ev != nullptr && ev->settlement != 0 && by_id.count(ev->settlement) != 0) {
                        cell = {by_id[ev->settlement]->x, by_id[ev->settlement]->y};
                    } else if (ev != nullptr && ev->settlement == 0 && seeds.count(ev->region) != 0) {
                        cell = seeds[ev->region];
                    }
                    if (cell != std::make_pair(p.x, p.y) && problem.empty()) {
                        problem = "year " + std::to_string(y) + ": e" + std::to_string(e) + " at " +
                                  std::to_string(p.x) + " " + std::to_string(p.y) + ", expected " +
                                  std::to_string(cell.first) + " " + std::to_string(cell.second);
                    }
                }
            }
            std::set<int64_t> want;
            for (const auto& e : report.events) {
                if (e.start <= y && y <= e.end && (e.settlement != 0 || e.region != 0)) {
                    want.insert(e.id);
                }
            }
            if (got != want && problem.empty()) {
                problem = "year " + std::to_string(y) + ": " + std::to_string(got.size()) + " events in places, " +
                          std::to_string(want.size()) + " placed events in the chronicle";
            }
        }
        check.expect(problem.empty() && places > 0,
                     "M4 " + num(static_cast<int64_t>(years.size())) + " years, " + num(places) +
                         " places: their events are the placed chronicle events of the year, at the settlement's or "
                         "the region seed's cell" +
                         (problem.empty() ? godot::String() : ": " + to_godot(problem)));
        model->set_marker_option("events", false);
        const bool none = model->history_state().markers().places.empty();
        model->set_marker_option("events", true);
        check.expect(none, "M4 with event places off there are no place entries");
    }

    // M5: passes.
    {
        const auto& passes = model->history_state().markers().passes;
        const auto rows = lib.index->passes();
        bool same = passes.size() == rows.size() && options.expect_passes == static_cast<int64_t>(passes.size());
        for (std::size_t i = 0; same && i < passes.size(); ++i) {
            same = passes[i].pass == rows[i].id && passes[i].x == rows[i].col_x && passes[i].y == rows[i].col_y;
        }
        check.expect(same && !passes.empty(), "M5 " + num(static_cast<int64_t>(passes.size())) +
                                                  " pass entries at the col cells, report: " +
                                                  num(options.expect_passes));
        model->set_marker_option("passes", false);
        const bool none = model->history_state().markers().passes.empty();
        model->set_marker_option("passes", true);
        check.expect(none, "M5 with passes off there are no pass entries");
    }

    // M6: pixels against worldgen-cli render --markers.
    {
        screen.choose_view(territory);
        const auto compare_at = [&](int64_t year, const godot::String& file, const godot::String& what) {
            screen.year_slider()->set_value(static_cast<double>(year));
            godot::String problem;
            const godot::Ref<godot::Image> drawn = image_with_markers(screen.map_view()->get_image(), *model.ptr(),
                                                                      problem);
            if (problem.is_empty()) {
                problem = compare_images(drawn, options.reference_dir.path_join(file));
            }
            const bool order = actual_marker_rows(model->get_markers()) == expected_marker_rows(*model.ptr());
            check.expect(model->get_year() == year && problem.is_empty() && order,
                         "M6 " + what + " in " + num(year) + " with the model's markers equals " + file +
                             (order ? godot::String() : godot::String(", get_markers() not in the order of 6.1")) +
                             (problem.is_empty() ? godot::String() : ": " + problem));
        };
        for (const int64_t year : options.marker_years) {
            compare_at(year, "markers-" + num(year) + ".png", "territory");
        }
        model->set_marker_option("faded", true);
        for (const int64_t year : {end, d0}) {
            compare_at(year, "markers-faded-" + num(year) + ".png", "territory with faded ruins");
        }
        model->set_marker_option("faded", false);
        const godot::Ref<WorldFile> world = model->get_world();
        const int64_t elevation = world->find_layer("elevation");
        screen.choose_view(elevation);
        compare_at(r0, "elevation-markers-" + num(r0) + ".png", "elevation");
        screen.choose_view(territory);
    }

    // M7: hover texts.
    {
        model->set_year(end);
        const godot::Array markers = model->get_markers();
        const auto& sets = model->history_state().markers().settlements;
        int64_t standing = -1;
        int64_t capital = -1;
        int64_t ruin = -1;
        for (const auto& s : sets) {
            // The first standing entry that is not a capital (the first
            // standing one may be a capital, which the next case covers).
            if ((s.status == 1 || s.status == 2) && !s.capital && standing < 0) {
                standing = s.settlement;
            }
            if (s.capital && capital < 0) {
                capital = s.settlement;
            }
            if (s.status == 3 && !s.old_ruin && ruin < 0) {
                ruin = s.settlement;
            }
        }
        const auto civ_name = [&report](int64_t id) {
            const ReportCiv* c = report.civ(id);
            return c != nullptr ? c->name : std::string("?");
        };
        const auto sentence = [&report](int64_t id) {
            for (const auto& e : report.events) {
                if (e.id == id) {
                    return e.sentence;
                }
            }
            return std::string("?");
        };
        // P5-08 11.2: the settlement's shrines and temples, from the check's
        // own read (expected_faith_text).
        const auto faith_suffix = [&lib, &model](int64_t id) {
            std::string out;
            if (lib.history) {
                for (const auto& row : lib.history->tables.sites) {
                    if (row.settlement == id && (row.kind == 5 || row.kind == 6) && row.origin_year <= model->get_year()) {
                        out += "; " + expected_faith_text(*lib.history, row, model->get_year());
                    }
                }
            }
            return out;
        };
        const auto expected_text = [&](int64_t id) -> std::string {
            const auto it = by_id.find(id);
            const auto* m = model_settlement(*model.ptr(), id);
            if (it == by_id.end() || m == nullptr) {
                return "?";
            }
            const ReportSettlement& r = *it->second;
            if (r.status == "ruined") {
                return r.name + ", ruin of a " + r.kind + ", ruined in " + std::to_string(r.ruined) + ": " +
                       (r.ruin_event != 0 ? sentence(r.ruin_event) : std::string("cause not recorded"));
            }
            const bool is_capital = m->capital;
            return r.name + ", " + r.kind + (is_capital ? " (capital)" : "") + " of " + civ_name(r.owner) + ", " +
                   r.status + ", population " + std::to_string(r.population) + " recorded in " +
                   std::to_string(m->since) + ", founded in " + std::to_string(r.founded) + faith_suffix(id);
        };
        const std::array<std::pair<const char*, int64_t>, 3> cases{
            {{"standing", standing}, {"standing", capital}, {"ruin", ruin}}};
        for (const auto& [type, id] : cases) {
            const int64_t index = marker_index_of(markers, type, id);
            const std::string want = expected_text(id);
            const godot::String text = model->describe_marker(index);
            check.expect(index >= 0 && to_utf8(text) == want,
                         "M7 describe_marker of s" + num(id) + ": " + text +
                             (to_utf8(text) == want ? godot::String() : ", expected " + to_godot(want)));
            const godot::Vector2i at = screen.map_view()->get_marker_screen_position(index);
            send_motion(screen, godot::Vector2(static_cast<float>(at.x), static_cast<float>(at.y)));
            check.expect(at.x >= 0 && screen.cell_label()->get_text() == text,
                         "M7 the mouse over s" + num(id) + " at " + godot::String(godot::Variant(at)) +
                             " shows it: " + screen.cell_label()->get_text());
        }
        // A ring around a drawn settlement: the first year, from E down,
        // whose places share a cell with a drawn settlement entry.
        int64_t ring_year = -1;
        int64_t ring_settlement = -1;
        for (int64_t y = end; y >= 0 && ring_year < 0; --y) {
            const auto ym = lib.index->markers_at(*lib.history, y, worldgen::inspect::MarkerOptions{});
            for (const auto& p : ym.places) {
                for (const auto& s : ym.settlements) {
                    if (s.x == p.x && s.y == p.y && ring_year < 0) {
                        ring_year = y;
                        ring_settlement = s.settlement;
                    }
                }
            }
        }
        if (ring_year >= 0) {
            model->set_year(ring_year);
            const godot::Array at_year = model->get_markers();
            const auto* s = model_settlement(*model.ptr(), ring_settlement);
            const int64_t index =
                s == nullptr ? -1 : marker_index_of(at_year, to_utf8(type_word(*s)).c_str(), ring_settlement);
            const godot::Vector2i at = screen.map_view()->get_marker_screen_position(index);
            send_motion(screen, godot::Vector2(static_cast<float>(at.x), static_cast<float>(at.y)));
            check.expect(index >= 0 && screen.cell_label()->get_text() == model->describe_marker(index),
                         "M7 the mouse at the centre of a ring in " + num(ring_year) + " shows the settlement s" +
                             num(ring_settlement) + " inside: " + screen.cell_label()->get_text());
        } else {
            check.expect(false, "M7 no ring around a settlement in any year");
        }
        // Away from every marker: the cell description again.
        godot::Vector2 away(-1.0F, -1.0F);
        const godot::Rect2 shown = screen.map_view()->get_shown_rect();
        for (float y = shown.position.y + 2.0F; y < shown.position.y + shown.size.y && away.x < 0.0F; y += 7.0F) {
            for (float x = shown.position.x + 2.0F; x < shown.position.x + shown.size.x && away.x < 0.0F; x += 7.0F) {
                if (screen.map_view()->marker_at(godot::Vector2(x, y)) < 0) {
                    away = godot::Vector2(x, y);
                }
            }
        }
        send_motion(screen, away);
        const godot::Ref<godot::Image> shown_image = screen.map_view()->get_image();
        const float cells_x = shown_image.is_valid() ? static_cast<float>(shown_image->get_width()) : 1.0F;
        const float cells_y = shown_image.is_valid() ? static_cast<float>(shown_image->get_height()) : 1.0F;
        const godot::Vector2i cell(static_cast<int32_t>(std::floor((away.x - shown.position.x) / (shown.size.x / cells_x))),
                                   static_cast<int32_t>(std::floor((away.y - shown.position.y) / (shown.size.y / cells_y))));
        const godot::String label = screen.cell_label()->get_text();
        check.expect(away.x >= 0.0F && screen.map_view()->get_hovered_marker() == -1 && label.begins_with("x ") &&
                         label == model->describe_cell(cell.x, cell.y),
                     "M7 the mouse away from every marker shows the cell again: " + label);
        screen.hover_cell(-1, -1);
    }

    // P4-05 12.1 F1 and F6: the glyph cell steps, the hit margin of small
    // entries and passes, and the text of a ruin without a ruin event.
    {
        LayerMapView* view = screen.map_view();
        const bool steps = LayerMapView::glyph_cell_size_at(0.25) == 1 && LayerMapView::glyph_cell_size_at(0.49) == 1 &&
                           LayerMapView::glyph_cell_size_at(0.5) == 2 && LayerMapView::glyph_cell_size_at(4.0) == 2 &&
                           LayerMapView::glyph_cell_size_at(9.99) == 2 && LayerMapView::glyph_cell_size_at(10.0) == 3;
        const double zoom = view->get_zoom();
        check.expect(steps && (zoom < 0.5 || zoom >= 10.0 || view->glyph_cell_size() == 2),
                     "F1 glyph cell 1 below zoom 0.5, 2 from 0.5 to 10, 3 above; at the fitted zoom " +
                         godot::String::num(zoom, 3) + " it is " + num(view->glyph_cell_size()));
        model->set_year(end);
        const godot::Array markers = model->get_markers();
        const int64_t g = view->glyph_cell_size();
        const int64_t half = g / 2;
        const auto radius_of = [&](int64_t i) -> int64_t {
            const godot::Dictionary glyph = model->get_marker_glyph(godot::Dictionary(markers[i])["glyph"]);
            return glyph.get("radius", int64_t{0});
        };
        // An entry far from every other drawn one, so that only it can be hit
        // near it.
        const auto isolated = [&](int64_t i) {
            const godot::Vector2i c = view->get_marker_screen_position(i);
            if (c.x < 0) {
                return false;
            }
            for (int64_t j = 0; j < markers.size(); ++j) {
                const godot::Vector2i d = view->get_marker_screen_position(j);
                if (j == i || d.x < 0) {
                    continue;
                }
                const int64_t reach = (radius_of(i) + radius_of(j) + 3) * g;
                if (std::max(std::abs(d.x - c.x), std::abs(d.y - c.y)) <= reach) {
                    return false;
                }
            }
            return true;
        };
        // The screen position of glyph cell (dx, 0) of entry i.
        const auto at_cell = [&](int64_t i, int64_t dx) {
            const godot::Vector2i c = view->get_marker_screen_position(i);
            return godot::Vector2(static_cast<float>(c.x + dx * g - half) + 0.5F, static_cast<float>(c.y) + 0.5F);
        };
        // The cells of the column at the glyph's right edge, rows -1..1.
        const auto edge_set = [&](int64_t i) {
            const godot::Dictionary glyph = model->get_marker_glyph(godot::Dictionary(markers[i])["glyph"]);
            const int64_t r = glyph.get("radius", int64_t{0});
            const godot::PackedByteArray cells = glyph.get("cells", godot::PackedByteArray());
            const int64_t side = 2 * r + 1;
            bool any = false;
            for (int64_t dy = -1; dy <= 1; ++dy) {
                const int64_t row = r + dy;
                any = any || (row >= 0 && row < side && cells.size() == side * side && cells[row * side + side - 1] != 0);
            }
            return any;
        };
        int64_t small = -1;
        int64_t large = -1;
        int64_t pass = -1;
        for (int64_t i = 0; i < markers.size(); ++i) {
            const godot::Dictionary m = markers[i];
            const bool is_pass = godot::String(m["glyph"]) == "pass";
            if (!isolated(i) || !edge_set(i)) {
                continue;
            }
            if (is_pass && pass < 0) {
                pass = i;
            } else if (bool(m["small"]) && small < 0) {
                small = i;
            } else if (!is_pass && !bool(m["small"]) && large < 0) {
                large = i;
            }
        }
        for (const auto& [what, i] : std::array<std::pair<const char*, int64_t>, 2>{{{"small", small}, {"pass", pass}}}) {
            if (i < 0) {
                check.expect(std::string(what) == "pass", godot::String("F6 no isolated ") + what + " entry at E");
                continue;
            }
            const int64_t r = radius_of(i);
            const bool near = view->marker_at(at_cell(i, r + 1)) == i;
            const bool far = view->marker_at(at_cell(i, r + 2)) != i;
            check.expect(near && far, godot::String("F6 the ") + what + " entry " + num(i) +
                                          " is hit one glyph cell beyond its glyph and not two (" +
                                          (near ? "near hit" : "near missed") + ", " + (far ? "far missed" : "far hit") +
                                          ")");
        }
        if (large >= 0) {
            check.expect(view->marker_at(at_cell(large, radius_of(large) + 1)) != large,
                         "F6 a large entry (" + godot::String(godot::Dictionary(markers[large])["glyph"]) +
                             ") has no hit margin");
        } else {
            check.expect(false, "F6 no isolated large entry at E");
        }
        int64_t unrecorded = 0;
        godot::String wrong;
        for (int64_t i = 0; i < markers.size(); ++i) {
            const godot::Dictionary m = markers[i];
            const godot::String type = m["type"];
            if (type != "ruin" && type != "faded") {
                continue;
            }
            const auto* s = model_settlement(*model.ptr(), m["id"]);
            const bool none = s != nullptr && s->ruin_event == 0;
            unrecorded += none ? 1 : 0;
            const godot::String text = model->describe_marker(i);
            if (s == nullptr || text.ends_with(": cause not recorded") != none) {
                wrong += " " + text;
            }
        }
        check.expect(wrong.is_empty(), "F6 every ruin at E without a ruin event (" + num(unrecorded) +
                                           ") reads ': cause not recorded', and no other" + wrong);
    }

    // M8: toggles.
    {
        model->set_year(end);
        const godot::Array before = model->get_markers();
        g_markers_changed = 0;
        model->set_markers_enabled(false);
        const bool off = model->get_markers().is_empty() && g_markers_changed == 1 &&
                         !screen.markers_toggle()->is_pressed() && screen.mark_faded()->is_disabled() &&
                         model->get_marker_legend().is_empty();
        g_markers_changed = 0;
        model->set_markers_enabled(false);
        const bool same_off = g_markers_changed == 0;
        screen.get_viewport()->gui_release_focus();
        send_key(screen, godot::KEY_M, false);
        const bool on = model->get_markers_enabled() && g_markers_changed == 1 &&
                        actual_marker_rows(model->get_markers()) == actual_marker_rows(before) &&
                        screen.markers_toggle()->is_pressed();
        check.expect(off && same_off && on, "M8 markers off empties the list (one markers_changed, none again), key M "
                                            "turns them back on (one more)");
        g_markers_changed = 0;
        screen.mark_faded()->set_pressed(true);  // through the box
        const godot::Array with_faded = model->get_markers();
        int64_t faded_entries = 0;
        for (int64_t i = 0; i < with_faded.size(); ++i) {
            faded_entries += godot::String(godot::Dictionary(with_faded[i])["type"]) == "faded" ? 1 : 0;
        }
        int64_t old_at_end = 0;
        for (const auto& s : report.settlements) {
            old_at_end += is_old_at(s, end) || s.status == "faded" ? 1 : 0;
        }
        const bool adds = model->get_marker_option("faded") && g_markers_changed == 1 && faded_entries > 0 &&
                          faded_entries == old_at_end &&
                          with_faded.size() == before.size() + faded_entries &&
                          legend_labels(model->get_marker_legend()) == expected_marker_legend(true, true, true);
        g_markers_changed = 0;
        model->set_marker_option("faded", true);
        const bool again = g_markers_changed == 0;
        screen.mark_faded()->set_pressed(false);
        const bool back = g_markers_changed == 1 && !model->get_marker_option("faded");
        model->set_year(0);  // no faded ruin exists in year 0
        g_markers_changed = 0;
        model->set_marker_option("faded", true);
        const bool nothing = g_markers_changed == 0;
        model->set_marker_option("faded", false);
        check.expect(adds && again && back && nothing && g_markers_changed == 0,
                     "M8 the Old ruins box adds exactly the " + num(faded_entries) + " old ruins of E (" +
                         num(old_at_end) + " in [present]; one markers_changed); a change that alters nothing emits none");
    }

    // M9: rebuilds over 0 .. E.
    {
        model->set_year(0);
        std::string problem;
        int64_t emitted = 0;
        int64_t expected = 0;
        worldgen::inspect::YearMarkers previous =
            lib.index->markers_at(*lib.history, 0, worldgen::inspect::MarkerOptions{});
        for (int64_t y = 1; y <= end; ++y) {
            g_markers_changed = 0;
            model->set_year(y);
            const worldgen::inspect::YearMarkers now =
                lib.index->markers_at(*lib.history, y, worldgen::inspect::MarkerOptions{});
            const bool differs = !same_marker_lists(now, previous);
            emitted += g_markers_changed;
            expected += differs ? 1 : 0;
            if ((g_markers_changed == 1) != differs && problem.empty()) {
                problem = "year " + std::to_string(y) + ": " + std::to_string(g_markers_changed) +
                          " markers_changed, the library's set " + (differs ? "differs" : "is the same");
            }
            previous = now;
        }
        check.expect(problem.empty(), "M9 stepping 0 to " + num(end) + ": " + num(emitted) +
                                          " markers_changed, in exactly the " + num(expected) +
                                          " years whose marker set differs from the year before" +
                                          (problem.empty() ? godot::String() : ": " + to_godot(problem)));
    }
    check_mines(check, screen, options, lib);
    check_towers(check, screen, lib);
    model->set_year(end);
    model->disconnect("markers_changed", counter);
}

}  // namespace

int run_viewer_check(ViewerScreen& screen, const ViewerCheckOptions& options) {
    Checker check;
    godot::UtilityFunctions::print("UNMAPPED-CHECK: viewer headless check, Godot ",
                                   godot::Engine::get_singleton()->get_version_info()["string"]);

    check.expect(options.wired, "viewer scene has all its parts (%OpenButton ... %LegendList, %CellLabel)");
    if (!options.wired) {
        godot::UtilityFunctions::print("UNMAPPED-CHECK: FAIL");
        return 1;
    }
    godot::Ref<ViewerModel> model = screen.get_model();
    check.expect(model->has_world(), "world opened: " + model->get_world_path() +
                                         (model->has_world() ? godot::String()
                                                             : " (" + model->get_error_code() + ": " +
                                                                   model->get_error_message() + ")"));
    if (!model->has_world()) {
        godot::UtilityFunctions::print("UNMAPPED-CHECK: FAIL");
        return 1;
    }

    const int64_t layer_count = model->get_layer_count();
    const int64_t composite_count = model->get_composite_count();
    const int64_t count = model->get_view_count();
    const godot::Ref<WorldFile> world = model->get_world();
    check.expect(layer_count == world->get_layer_count(),
                 "model lists every layer of the file: " + godot::String(", ").join(world->get_layer_names()));
    // P4-03 5.1: the territory view comes in addition to worldgen's composites.
    const int64_t territory_views = model->get_territory_view() >= 0 ? 1 : 0;
    check.expect(composite_count == world->get_composite_names().size() + territory_views &&
                     count == layer_count + composite_count,
                 "model lists every composite: " + godot::String(territory_views > 0 ? "territory, " : "") +
                     godot::String(", ").join(world->get_composite_names()));
    if (options.expect_layers >= 0) {
        check.expect(layer_count == options.expect_layers,
                     godot::String("layer count {0}, expected {1}")
                         .format(godot::Array::make(layer_count, options.expect_layers)));
    }
    if (options.expect_composites >= 0) {
        check.expect(composite_count == options.expect_composites,
                     godot::String("composite count {0}, expected {1}")
                         .format(godot::Array::make(composite_count, options.expect_composites)));
    }
    // The list: every view once, under group headings, composites first.
    {
        godot::ItemList* list = screen.layer_list();
        int64_t views = 0;
        int64_t headings = 0;
        bool each_view_once = true;
        for (int64_t item = 0; item < list->get_item_count(); ++item) {
            const int64_t view = screen.view_of_item(item);
            if (view < 0) {
                ++headings;
                each_view_once = each_view_once && !list->is_item_selectable(static_cast<int32_t>(item));
            } else {
                ++views;
                each_view_once = each_view_once && screen.item_of_view(view) == item;
            }
        }
        check.expect(views == count && each_view_once && headings >= 1,
                     godot::String("layer list shows {0} views under {1} group headings")
                         .format(godot::Array::make(views, headings)));
        if (composite_count > 0) {
            const godot::String first = list->get_item_count() > 0 ? list->get_item_text(0) : godot::String();
            check.expect(first == "Composites" && screen.view_of_item(1) == layer_count,
                         godot::String("the list starts with the composites (heading '{0}')")
                             .format(godot::Array::make(first)));
        }
        // The screen shows the first entry of the list after opening.
        check.expect(model->get_selected_view() == screen.view_of_item(1),
                     godot::String("the first list entry is shown on opening: {0}")
                         .format(godot::Array::make(model->get_view_info(model->get_selected_view())["name"])));
    }
    if (world->find_layer("biome") >= 0) {
        check.expect(world->get_biome_table_problem().is_empty() && world->get_biome_entries().size() > 0,
                     godot::String("table biome read: {0} rows{1}")
                         .format(godot::Array::make(world->get_biome_entries().size(),
                                                    world->get_biome_table_problem().is_empty()
                                                        ? godot::String()
                                                        : " (" + world->get_biome_table_problem() + ")")));
    }
    const int64_t meta_count = model->get_meta_entries().size();
    check.expect(meta_count > 0 && tree_row_count(screen.meta_tree()) == meta_count,
                 godot::String("meta panel shows {0} of {1} meta rows")
                     .format(godot::Array::make(tree_row_count(screen.meta_tree()), meta_count)));

    // The library world (P5-04a 21.7 item 4) has no reference images.
    const bool library_only = options.expect_history == "library";
    for (int64_t i = 0; i < (library_only ? 0 : count); ++i) {
        const bool is_layer = i < layer_count;
        const godot::String name = model->get_view_info(i)["name"];
        screen.choose_view(i);
        const bool selected = model->get_selected_view() == i;
        const godot::String kind = is_layer ? "layer" : "composite";
        check.expect(selected, kind + godot::String(" ") + name + godot::String(" selected through the layer list"));
        if (!selected) {
            continue;
        }
        // The image is drawn inside the map view, fitted (not off-screen).
        const godot::Rect2 shown = screen.map_view()->get_shown_rect();
        const godot::Vector2 view = screen.map_view()->get_size();
        const bool inside = shown.size.x > 0.0F && shown.size.y > 0.0F &&
                            godot::Rect2(godot::Vector2(), view).grow(0.5F).encloses(shown) &&
                            (shown.size.x >= view.x - 1.0F || shown.size.y >= view.y - 1.0F);
        check.expect(inside, godot::String("{0} {1} drawn fitted inside the map view: {2} in {3}")
                                 .format(godot::Array::make(kind, name, shown, view)));
        const godot::Dictionary info = model->get_view_info(i);  // palette known once decoded
        const godot::String png =
            options.reference_dir.path_join((is_layer ? name : "composite." + name) + godot::String(".png"));
        const godot::String difference = compare_images(screen.map_view()->get_image(), png);
        check.expect(difference.is_empty(),
                     godot::String("{0} {1} ({2} x {3} {4}, palette {5}): map view pixels equal worldgen-cli render{6}")
                         .format(godot::Array::make(kind, name, info["width"], info["height"], info["element_type"],
                                                    info["palette"],
                                                    difference.is_empty() ? godot::String() : ": " + difference)));
        const godot::Array legend = model->get_legend();
        check.expect(legend.size() > 0 && screen.legend_list()->get_item_count() == legend.size() &&
                         !screen.legend_title()->get_text().is_empty(),
                     godot::String("{0} {1}: legend '{2}' with {3} rows")
                         .format(godot::Array::make(kind, name, screen.legend_title()->get_text(), legend.size())));
        const bool biome_legend = info["palette"] == godot::Variant("biome") || name == "biomes" ||
                                  name == "regions" || name == "territory";
        if (biome_legend) {
            const godot::String missing = missing_biome(legend, world->get_biome_entries(), !is_layer);
            check.expect(missing.is_empty(),
                         godot::String("{0} {1}: legend lists every biome of table biome by name and colour{2}")
                             .format(godot::Array::make(kind, name,
                                                        missing.is_empty() ? godot::String()
                                                                           : godot::String(", not ") + missing)));
        }
        if (!is_layer && name == "regions") {
            check.expect(legend_has(legend, "region border", 0), "composite regions: legend has the black border");
        }
        if (info["palette"] == godot::Variant("biome")) {
            // Both sides magenta would also compare equal: the colour of a
            // biome code missing from the table (P1-00 10.2).
            const int64_t unknown = count_rgb(screen.map_view()->get_image(), 255, 0, 255);
            check.expect(unknown == 0, godot::String("layer {0}: {1} cells in the unknown-biome colour (255, 0, 255)")
                                           .format(godot::Array::make(name, unknown)));
        }
    }

    // Cell inspection: the text under the mouse for a cell of a region.
    godot::String hover_text;
    if (count > 0) {
        const int64_t region_index = world->find_layer("region");
        const int64_t biome_index = world->find_layer("biome");
        int64_t cx = 0;
        int64_t cy = 0;
        int64_t region_id = 0;
        if (region_index >= 0) {
            const godot::Ref<WorldLayer> region = model->get_layer(region_index);
            // Decoded by the composite regions above, or now through the model.
            const godot::Ref<WorldLayer> layer =
                region.is_valid() ? region : world->read_layer("region");
            static const worldgen::db::LayerGrid empty_grid;
            const auto& grid = layer.is_valid() ? layer->layer().grid : empty_grid;
            // The first cell with a region from the middle row on.
            for (std::uint32_t y = grid.height / 2; y < grid.height && region_id == 0; ++y) {
                for (std::uint32_t x = 0; x < grid.width && region_id == 0; ++x) {
                    std::int64_t v = 0;
                    if (worldgen::inspect::cell_value(grid, x, y, v) && v > 0) {
                        cx = x;
                        cy = y;
                        region_id = v;
                    }
                }
            }
        }
        screen.choose_view(screen.view_of_item(1));
        const godot::String view = model->get_view_info(model->get_selected_view())["name"];
        screen.hover_cell(cx, cy);
        const godot::String text = screen.cell_label()->get_text();
        hover_text = text;
        bool ok = text.begins_with(godot::String("x {0}, y {1}").format(godot::Array::make(cx, cy)));
        if (model->get_selected_view() >= layer_count) {
            // A composite shows the elevation in metres.
            const godot::Ref<WorldLayer> elevation = model->get_layer(world->find_layer("elevation"));
            ok = ok && elevation.is_valid() &&
                 text.contains(godot::String("elevation ") + to_godot(worldgen::inspect::format_layer_value(
                                                  "elevation", elevation->get_cell_text(cx, cy).to_int())));
        }
        if (biome_index >= 0) {
            const godot::Ref<WorldLayer> biome = world->read_layer("biome");
            const int64_t code = biome.is_valid() ? biome->get_cell_text(cx, cy).to_int() : -1;
            const worldgen::inspect::BiomeRecord* record = world->find_biome(code);
            ok = ok && record != nullptr && text.contains(godot::String("biome ") + to_godot(record->name));
        }
        if (region_id > 0) {
            // P4-01 r3 16.5: with history the name in force at the year shown,
            // from the check's own read of the file; else the geographic name.
            godot::String region_name = world->get_region_name(region_id);
            if (model->has_history()) {
                try {
                    auto reader = worldgen::db::WorldReader::open(to_utf8(model->get_world_path()));
                    if (reader.ok()) {
                        auto loaded = worldgen::history::load_history(reader.value());
                        if (loaded.ok()) {
                            region_name = to_godot(
                                std::string(loaded.value().timeline.region_name(region_id, model->get_year())));
                        }
                    }
                } catch (const std::exception&) {
                    region_name = godot::String();  // fails the check below
                }
            }
            ok = ok && !region_name.is_empty() &&
                 text.contains(godot::String("region {0} {1}").format(godot::Array::make(region_id, region_name)));
        }
        check.expect(ok, godot::String("hovering cell ({0}, {1}) of {2} shows: {3}")
                             .format(godot::Array::make(cx, cy, view, text)));
        screen.hover_cell(-1, -1);
        check.expect(screen.cell_label()->get_text() == "Point at the map to inspect a cell.",
                     "leaving the map shows the prompt again");
        // Units of a few catalogue layers, in display form.
        if (region_index >= 0) {
            check.expect(worldgen::inspect::format_layer_value("elevation", 1234) == "123.4 m" &&
                             worldgen::inspect::format_layer_value("temperature", -15) == "-1.5 \u00B0C" &&
                             worldgen::inspect::format_layer_value("water", 2) == "lake" &&
                             worldgen::inspect::format_layer_value("flow_dir", 8) == "none (ocean)" &&
                             worldgen::inspect::format_layer_value("coast_dist", -48) == "48 m offshore",
                         "display units of elevation, temperature, water, flow_dir, coast_dist");
        }
    }

    // History (P4-03 10.2).
    if (options.expect_history == "none") {
        check_without_history(check, screen, hover_text);
        check_markers_without_history(check, screen);
        check_browser_without_history(screen, [&check](bool ok, const godot::String& what) { check.expect(ok, what); });
        check_playback_without_history(screen, [&check](bool ok, const godot::String& what) { check.expect(ok, what); });
    } else if (options.expect_history == "loaded") {
        check_playback_at_open(screen, [&check](bool ok, const godot::String& what) { check.expect(ok, what); });
        check_markers_at_open(check, screen);
        check_with_history(check, screen, options);
        const HistoryReport report = read_history_report(options.history_report, model->get_world_path());
        if (report.problem.empty()) {
            check_markers(check, screen, options, report);
        }
        // P4-05 8.2: B1 to B12.
        const int64_t browser_start = usec_now();
        check_browser(screen, options, [&check](bool ok, const godot::String& what) { check.expect(ok, what); });
        godot::UtilityFunctions::print("UNMAPPED-CHECK: info: browser checks ",
                                       num((usec_now() - browser_start) / 1000), " ms, browser built at open in ",
                                       num(model->browser_state().load_usec() / 1000), " ms");
        // P4-06 11.2: P2 to P10.
        const int64_t playback_start = usec_now();
        check_playback(screen, options, [&check](bool ok, const godot::String& what) { check.expect(ok, what); });
        godot::UtilityFunctions::print("UNMAPPED-CHECK: info: playback checks ",
                                       num((usec_now() - playback_start) / 1000), " ms");
    } else if (options.expect_history == "library") {
        check.expect(model->has_history(), "the world has a history (library or depths world)");
    } else if (!options.expect_history.is_empty()) {
        check.expect(false, "--expect-history " + options.expect_history + ": expected none, loaded or library");
    }
    if (options.expect_library > 0 && model->has_history()) {
        check_library(check, screen, options.expect_library);
    }
    if ((options.expect_depths || options.expect_history == "loaded") && model->has_history()) {
        check_depths(check, screen, options);  // P5-07 11.2: M13, M14
    }
    if ((options.expect_pantheon || options.expect_history == "loaded") && model->has_history()) {
        check_pantheon(check, screen, options);  // P5-08 11.2: M16 to M18, B16
    }

    // The reader's distinct errors reach Godot as code and message.
    godot::Ref<WorldFile> probe;
    probe.instantiate();
    // (Each open() is a statement of its own: the order in which function
    // arguments are evaluated is unspecified.)
    const auto expect_open_error = [&](const godot::String& path, const char* expected_code, const char* what) {
        const bool opened = probe->open(path);
        const godot::String code = probe->get_error_code();
        const godot::String message = probe->get_error_message();
        check.expect(!opened && code == expected_code && !message.is_empty(),
                     godot::String(what) + ": " + code + ": " + message);
    };
    expect_open_error(options.reference_dir.path_join("no-such-world.db"), "file_unreadable", "missing file");
    if (layer_count > 0 && !library_only) {
        const godot::String layer_name = model->get_view_info(0)["name"];
        expect_open_error(options.reference_dir.path_join(layer_name + godot::String(".png")), "not_sqlite",
                          "PNG file opened as a world");
    }

    godot::UtilityFunctions::print(check.failed() ? "UNMAPPED-CHECK: FAIL" : "UNMAPPED-CHECK: PASS");
    return check.failed() ? 1 : 0;
}

}  // namespace unmapped
