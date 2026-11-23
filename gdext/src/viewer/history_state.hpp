#pragma once

// HistoryState: the history of the open world as the viewer model holds it
// (spec P4-03 3.3). C++ only, not registered with Godot, no Godot types.
//
// Read once when a world opens (load_history, P4-01 5.2), with the
// civilization colours (P4-03 7). Holds the current year Y and what depends
// on it: the year state and the holder vector (from worldgen: the viewer
// never decides who holds what, P4-03 6.2; the image is drawn with the
// timeline form of render_territory, r4 9.2), the year-independent base of the territory image once it is built,
// and the holder vector the image was last drawn with, which alone decides
// whether the image has to be redrawn (P4-00 V6). Nothing else is cached
// across a change of Y.
//
// Every answer comes from worldgen's timeline and the history tables; no
// SQL, no event kind tested by id (P4-00 V3, V7).

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "worldgen/db/reader.hpp"
#include "worldgen/history/timeline.hpp"
#include "worldgen/inspect/geography.hpp"
#include "worldgen/inspect/layer_catalogue.hpp"
#include "worldgen/inspect/territory.hpp"

namespace unmapped {

enum class HistoryStatus { none, loaded, failed };

// One mark under the year slider (P4-03 4.5, 5.4).
struct YearMark {
    std::int64_t year = 0;
    std::string kind;   // "awakening", "phase" or "present"
    std::string label;  // the tooltip text
};

// The layers the territory base is built from (those of the composite
// regions); null water, river and flow_dir are absent layers.
struct TerritoryLayers {
    const worldgen::db::LayerGrid* elevation = nullptr;
    const worldgen::db::LayerGrid* water = nullptr;
    const worldgen::db::LayerGrid* river = nullptr;
    const worldgen::db::LayerGrid* flow_dir = nullptr;
    const worldgen::db::LayerGrid* biome = nullptr;
    const worldgen::db::LayerGrid* region = nullptr;
};

class HistoryState {
public:
    // Reads the history of a world (P4-03 3.1 steps 2 to 4). Never throws:
    // history_missing gives status none; any other error, out of memory
    // and a FatalError of civilization_colours included, gives failed with
    // worldgen's code name and message. Success: loaded, Y = E.
    void load(const worldgen::db::WorldReader& reader);
    void reset();

    [[nodiscard]] HistoryStatus status() const noexcept { return status_; }
    [[nodiscard]] bool loaded() const noexcept { return status_ == HistoryStatus::loaded; }
    [[nodiscard]] const std::string& error_code() const noexcept { return error_code_; }
    [[nodiscard]] const std::string& error_message() const noexcept { return error_message_; }

    // -1 without history.
    [[nodiscard]] std::int64_t end_year() const noexcept;
    [[nodiscard]] std::int64_t year() const noexcept { return loaded() ? year_ : -1; }
    [[nodiscard]] bool valid_year(std::int64_t y) const noexcept;

    // Y = y and the year state and holders recomputed (3.4 steps 1, 2).
    // False and nothing changed without history or for an invalid year.
    bool set_year(std::int64_t y);

    // Valid only while loaded().
    [[nodiscard]] const worldgen::history::LoadedHistory& history() const { return *history_; }
    [[nodiscard]] const worldgen::history::YearState& state() const noexcept { return state_; }
    [[nodiscard]] std::span<const worldgen::inspect::Rgb> colours() const noexcept { return colours_; }

    // Rows by id; null when there is none.
    [[nodiscard]] const worldgen::db::CivilizationRow* civilization(std::int64_t id) const noexcept;
    [[nodiscard]] const worldgen::db::RaceRow* race(std::int64_t id) const noexcept;
    [[nodiscard]] const worldgen::db::AgeRow* age(std::int64_t id) const noexcept;
    [[nodiscard]] const worldgen::db::PhaseRow* phase(std::int64_t id) const noexcept;
    // The colour of a civilization as 0xRRGGBB; -1 for an unknown id.
    [[nodiscard]] std::int64_t colour_of(std::int64_t civilization) const noexcept;

    // The holding of a region at Y; nullopt for a region the timeline does
    // not have or without history.
    [[nodiscard]] std::optional<worldgen::history::RegionHolding> holding(std::int64_t region) const;
    // The text P4-03 6.1 appends to a cell description for region v at Y
    // ("   |   held by <name> since <Y>", "   |   free since <Y>", "   |   free"),
    // empty when nothing is appended.
    [[nodiscard]] std::string holding_text(std::int64_t region) const;
    // The name of region v at Y (Timeline::region_name, P4-01 r3 16.5,
    // P5-12): nullopt without history or for a region the timeline lacks.
    [[nodiscard]] std::optional<std::string> region_name(std::int64_t region) const;

    // Counts of P4-03 3.5 at Y.
    [[nodiscard]] std::int64_t regions_held() const noexcept;
    [[nodiscard]] std::int64_t settlements_standing() const noexcept;
    [[nodiscard]] std::int64_t wars() const noexcept;

    // The step targets of 3.2; nullopt when there is none.
    [[nodiscard]] std::optional<std::int64_t> previous_event_year() const;
    [[nodiscard]] std::optional<std::int64_t> next_event_year() const;
    [[nodiscard]] std::optional<std::int64_t> previous_change_year() const;
    [[nodiscard]] std::optional<std::int64_t> next_change_year() const;

    // P4-03 5.4: ascending year; ties awakening, phase, present.
    [[nodiscard]] std::vector<YearMark> year_marks() const;

    // The territory legend at Y (P4-03 6.4); biomes is the world's table
    // `biome`.
    [[nodiscard]] std::vector<worldgen::inspect::LegendEntry> legend(
        std::span<const worldgen::inspect::BiomeRecord> biomes) const;

    // --- The territory image -------------------------------------------------
    // Builds the year-independent base once (P4-03 3.1 step 5). Returns
    // worldgen's problem text when the layers do not suit the composite
    // regions (nothing built), else nullopt.
    std::optional<std::string> build_base(const TerritoryLayers& layers,
                                          std::span<const worldgen::inspect::BiomeColour> biome_table);
    [[nodiscard]] bool has_base() const noexcept { return base_.has_value(); }
    [[nodiscard]] std::uint32_t base_width() const noexcept { return base_ ? base_->width() : 0U; }
    [[nodiscard]] std::uint32_t base_height() const noexcept { return base_ ? base_->height() : 0U; }
    // Whether the image drawn last is not the one of Y (never drawn, or the
    // holders differ).
    [[nodiscard]] bool needs_draw() const noexcept;
    // render_territory(base, timeline, colours, Y, 1) into out (3 width height
    // bytes); the drawn holder vector becomes Y's. FatalError from worldgen
    // passes through (a size mismatch: a caller's bug).
    void draw(std::span<std::uint8_t> out);
    // The image must be drawn again from scratch (e.g. it was dropped).
    void forget_drawn() noexcept { drawn_valid_ = false; }

private:
    HistoryStatus status_ = HistoryStatus::none;
    std::string error_code_;
    std::string error_message_;
    std::optional<worldgen::history::LoadedHistory> history_;
    std::vector<worldgen::inspect::Rgb> colours_;
    std::optional<worldgen::inspect::TerritoryBase> base_;
    std::int64_t year_ = 0;
    worldgen::history::YearState state_;
    std::vector<std::int64_t> holders_;
    std::vector<std::int64_t> drawn_holders_;
    bool drawn_valid_ = false;
};

}  // namespace unmapped
