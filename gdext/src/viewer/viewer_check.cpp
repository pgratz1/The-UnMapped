#include "viewer/viewer_check.hpp"

#include <algorithm>
#include <cstdint>
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
#include <godot_cpp/classes/spin_box.hpp>
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

#include "viewer/viewer_screen.hpp"
#include "world_file.hpp"
#include "worldgen/history/timeline.hpp"
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
struct HistoryReport {
    std::string problem;  // empty when read
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

HistoryReport read_history_report(const godot::String& path) {
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
            } else if (section == "civilizations") {
                if (std::regex_match(line, m, civ_re)) {
                    report.civs.push_back(ReportCiv{to_i64(m[1]), 0, -1, -1, m[2]});
                } else if (!report.civs.empty() && std::regex_match(line, m, race_re)) {
                    report.civs.back().race = to_i64(m[1]);
                } else if (!report.civs.empty() && std::regex_search(line, m, founded_re)) {
                    report.civs.back().founded = to_i64(m[1]);
                } else if (!report.civs.empty() && std::regex_search(line, m, ended_re)) {
                    report.civs.back().ended = to_i64(m[1]);
                }
            } else if (section == "present" && std::regex_search(line, m, present_re)) {
                report.present.emplace_back(to_i64(m[1]), to_i64(m[2]));
            } else if (section == "stats" && std::regex_match(line, m, end_re)) {
                report.end_year = to_i64(m[1]);
            }
        }
    } catch (const std::exception& e) {
        report.problem = std::string("cannot read the history report: ") + e.what();
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
    const HistoryReport report = read_history_report(options.history_report);
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
                return "   |   held by " + (c != nullptr ? to_godot(c->name) : godot::String("?")) + " since " +
                       num(since);
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
            screen.hover_cell(held_x, held_y);
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

    for (int64_t i = 0; i < count; ++i) {
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
    } else if (options.expect_history == "loaded") {
        check_with_history(check, screen, options);
    } else if (!options.expect_history.is_empty()) {
        check.expect(false, "--expect-history " + options.expect_history + ": expected none or loaded");
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
    if (layer_count > 0) {
        const godot::String layer_name = model->get_view_info(0)["name"];
        expect_open_error(options.reference_dir.path_join(layer_name + godot::String(".png")), "not_sqlite",
                          "PNG file opened as a world");
    }

    godot::UtilityFunctions::print(check.failed() ? "UNMAPPED-CHECK: FAIL" : "UNMAPPED-CHECK: PASS");
    return check.failed() ? 1 : 0;
}

}  // namespace unmapped
