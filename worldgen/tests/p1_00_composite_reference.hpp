#pragma once

// The river drawing of the composite map (P1-00 10.2 step 4, amended
// 2026-11-16), written by the test writer from the spec text: the paint
// list, its order and the class colours. Used by the composite tests as the
// expected value; it calls nothing of the library's composite.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "worldgen/inspect/render.hpp"

namespace comp_ref {

using worldgen::inspect::Rgb;

// The colour of river class k in the composite (not the `river` palette of
// --layer river): 1 (70, 125, 215), 2 (55, 110, 210), 3 (40, 95, 200),
// 4 (30, 80, 190), 5 (25, 65, 175), 6 and above (20, 50, 160).
inline Rgb river_class_colour(std::uint8_t k) {
    switch (k) {
        case 1: return Rgb{70, 125, 215};
        case 2: return Rgb{55, 110, 210};
        case 3: return Rgb{40, 95, 200};
        case 4: return Rgb{30, 80, 190};
        case 5: return Rgb{25, 65, 175};
        default: return Rgb{20, 50, 160};
    }
}

inline constexpr Rgb LAKE{64, 128, 204};
inline constexpr Rgb RIVER_NO_CLASS{52, 100, 200};

struct Entry {
    std::int64_t x, y;
    std::uint8_t k;
};

// The paint list of step 4, in list order.
inline std::vector<Entry> paint_list(std::uint32_t w, std::uint32_t h, const std::vector<std::uint8_t>& water,
                                     const std::vector<std::uint8_t>& river, const std::vector<std::uint8_t>* flow_dir) {
    static constexpr std::int64_t DX[8] = {1, 1, 0, -1, -1, -1, 0, 1};
    std::vector<Entry> list;
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            const std::size_t i = std::size_t{y} * w + x;
            if (water[i] != 1U) {
                continue;
            }
            const std::uint8_t k = river[i];
            list.push_back({x, y, k});
            if (flow_dir != nullptr) {
                const std::uint8_t d = (*flow_dir)[i];
                if (d <= 7U && (d & 1U) != 0U) {
                    list.push_back({std::int64_t{x} + DX[d], y, k});
                }
            }
            if (k >= 4U) {
                list.push_back({std::int64_t{x} + 1, y, k});
                list.push_back({x, std::int64_t{y} + 1, k});
            }
        }
    }
    return list;
}

// Applies steps 3 and 4 to per-cell colours of steps 1 and 2 (base, w x h).
inline std::vector<Rgb> overlay(std::uint32_t w, std::uint32_t h, std::vector<Rgb> base,
                                const std::vector<std::uint8_t>* water, const std::vector<std::uint8_t>* river,
                                const std::vector<std::uint8_t>* flow_dir) {
    if (water == nullptr) {
        return base;
    }
    for (std::size_t i = 0; i < base.size(); ++i) {
        if ((*water)[i] == 2U) {
            base[i] = LAKE;
        }
    }
    if (river == nullptr) {
        for (std::size_t i = 0; i < base.size(); ++i) {
            if ((*water)[i] == 1U) {
                base[i] = RIVER_NO_CLASS;
            }
        }
        return base;
    }
    std::vector<Entry> list = paint_list(w, h, *water, *river, flow_dir);
    std::stable_sort(list.begin(), list.end(), [](const Entry& a, const Entry& b) { return a.k < b.k; });
    for (const Entry& en : list) {
        if (en.x < 0 || en.y < 0 || en.x >= std::int64_t{w} || en.y >= std::int64_t{h}) {
            continue;
        }
        const std::size_t i = static_cast<std::size_t>(en.y) * w + static_cast<std::size_t>(en.x);
        if ((*water)[i] <= 1U) {
            base[i] = river_class_colour(en.k);
        }
    }
    return base;
}

}  // namespace comp_ref
