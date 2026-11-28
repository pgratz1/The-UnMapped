#include "terrain/look.hpp"

#include <algorithm>
#include <cmath>

#include "terrain/ground.hpp"

namespace unmapped::terrain {

std::optional<Look> parse_look_v3d03(std::string_view list) {
    if (list == "all") {
        return Look{false, false, false, false, false};
    }
    Look look;
    std::size_t start = 0;
    while (true) {
        const std::size_t comma = list.find(',', start);
        const std::string_view item = list.substr(start, comma == std::string_view::npos ? std::string_view::npos : comma - start);
        if (item == "rock") {
            look.rock = false;
        } else if (item == "light") {
            look.light = false;
        } else if (item == "snow") {
            look.snow = false;
        } else if (item == "shrubs") {
            look.shrubs = false;
        } else if (item == "sea") {
            look.sea = false;
        } else {
            return std::nullopt;  // empty, unknown, or a trailing comma
        }
        if (comma == std::string_view::npos) {
            return look;
        }
        start = comma + 1;
    }
}

std::string restored_items(const Look& look) {
    std::string out;
    auto add = [&out](bool restored, const char* name) {
        if (restored) {
            out += out.empty() ? "" : ", ";
            out += name;
        }
    };
    add(!look.rock, "rock");
    add(!look.light, "light");
    add(!look.snow, "snow");
    add(!look.shrubs, "shrubs");
    add(!look.sea, "sea");
    return out;
}

std::array<float, 3> sun_direction(float azimuth_deg, float elevation_deg) {
    constexpr double DEG = 3.14159265358979323846 / 180.0;
    const double a = static_cast<double>(azimuth_deg) * DEG;
    const double e = static_cast<double>(elevation_deg) * DEG;
    return {static_cast<float>(-std::cos(e) * std::sin(a)), static_cast<float>(-std::sin(e)),
            static_cast<float>(std::cos(e) * std::cos(a))};
}

PeakView peak_view(const Layout& layout, std::span<const std::int16_t> elevation, const GroundCells& cells) {
    const int n = layout.n;
    const std::size_t total = static_cast<std::size_t>(n) * static_cast<std::size_t>(n);
    PeakView view;
    view.x = n / 2;
    view.y = n / 2;
    view.cam_x = n / 2;
    view.cam_y = n / 2;
    if (elevation.size() != total || cells.type.size() != total || cells.painted.size() != total) {
        return view;
    }
    int best = 0;
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            const std::size_t k = static_cast<std::size_t>(y) * static_cast<std::size_t>(n) + static_cast<std::size_t>(x);
            const std::uint8_t t = cells.type[k];
            if (t < 1 || t > 10 || cells.painted[k] > 0) {
                continue;
            }
            // Row-major order: a strictly larger value wins, so ties go to the
            // smaller row, then the smaller column.
            if (!view.found || elevation[k] > best) {
                best = elevation[k];
                view.x = x;
                view.y = y;
                view.found = true;
            }
        }
    }
    if (view.found) {
        view.cam_x = view.x;
        view.cam_y = std::min(n - 1, view.y + PEAK_VIEW_ROWS);
    }
    return view;
}

}  // namespace unmapped::terrain
