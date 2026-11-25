#pragma once

// Settlement and place markers (spec P4-04 sections 3 to 6): which markers
// exist at a year Y, where, with which glyph and colour, which settlement is
// a capital, when and by what a ruin fell, the pixels of every glyph, and the
// overlay drawn onto an RGB buffer. Integer arithmetic only; every list in
// the order section 4 states. The marker set and the overlay pixels are part
// of the cross-platform contract (section 13).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "worldgen/db/reader.hpp"
#include "worldgen/db/result.hpp"
#include "worldgen/db/writer.hpp"
#include "worldgen/history/timeline.hpp"
#include "worldgen/inspect/render.hpp"

namespace worldgen::inspect {

enum class MarkerType : std::uint8_t {
    pass = 1,
    place = 2,
    faded = 3,
    ruin = 4,
    standing = 5,
    site = 6 /*P5-04 11.3, P5-06 11.4: mines (kind 3) and towers (kind 4)*/,
    figure = 7 /*reserved*/
};
// The symbol of an event place (3.4); the enumerator order is the priority.
enum class EventSymbol : std::uint8_t { plain = 0, plague = 1, battle = 2, siege = 3, sack = 4 };

struct MarkerOptions {
    bool faded = false;
    bool events = true;
    bool passes = true;
    bool sites = true;  // P5-04 11.3
};

struct SettlementMarker {
    std::int64_t settlement = 0, x = 0, y = 0;
    std::int64_t kind = 0, status = 0, owner = 0, population = 0, since = 0, founded = 0;
    bool capital = false;                           // standing only (3.2)
    std::int64_t ruined_year = -1, ruin_event = 0;  // ruin and faded only (3.3); standing: -1 and 0
    // P5-06 25.1 (e): a status 3 entry whose ruin is OLD_RUIN_YEARS or more
    // before Y, an entry of type faded (an old ruin).
    bool old_ruin = false;
    friend bool operator==(const SettlementMarker&, const SettlementMarker&) = default;
};
struct PlaceMarker {
    std::int64_t x = 0, y = 0;
    EventSymbol symbol = EventSymbol::plain;
    std::vector<std::int64_t> events;  // ascending id, at least one
    friend bool operator==(const PlaceMarker&, const PlaceMarker&) = default;
};
struct PassMarker {
    std::int64_t pass = 0, x = 0, y = 0, kind = 0;
    friend bool operator==(const PassMarker&, const PassMarker&) = default;
};
// P5-04 11.3: a site of kind 3 (a mine) existing at Y, at its cell; P5-06
// 11.4: or of kind 4 (a tower; settlement its library's settlement, 0 for a
// tower in the wild).
struct SiteMarker {
    std::int64_t site = 0, x = 0, y = 0, kind = 0, founder = 0;
    std::int64_t settlement = 0;
    friend bool operator==(const SiteMarker&, const SiteMarker&) = default;
};
inline constexpr std::uint8_t GLYPH_MINE = 19;   // 16 + kind 3
inline constexpr std::uint8_t GLYPH_TOWER = 20;  // 16 + kind 4 (P5-06 11.4)
inline constexpr Rgb MARKER_MINE{60, 60, 60};
inline constexpr Rgb MARKER_TOWER{120, 60, 180};
// The mine glyph is drawn one glyph cell (7 cells) to the upper right of
// the site's cell, so that a town does not hide its own mine; a library's
// one glyph cell to the upper left of its settlement; a tower in the wild
// at its own cell (P5-06 11.4).
inline constexpr std::int64_t SITE_GLYPH_OFFSET = 7;
// P5-06 25.1 (e): a ruin this many years old or more is an old ruin, an
// entry of type faded.
inline constexpr std::int64_t OLD_RUIN_YEARS = 1500;

// The offset (dx, dy; y southward) of a site's glyph centre from its cell:
// a mine (+7, -7), a library (-7, -7), a tower in the wild (0, 0).
[[nodiscard]] std::array<std::int64_t, 2> site_glyph_offset(const SiteMarker& site) noexcept;
// The glyph key of a site entry: GLYPH_MINE for kind 3, GLYPH_TOWER for kind
// 4, base 0 otherwise.
[[nodiscard]] std::uint8_t site_glyph_base(const SiteMarker& site) noexcept;

struct YearMarkers {
    std::int64_t year = 0;
    std::vector<SiteMarker> sites;              // ascending site id (P5-04 11.3)
    std::vector<SettlementMarker> settlements;  // ascending settlement id (3.1)
    std::vector<PlaceMarker> places;            // ascending smallest event id (3.4)
    std::vector<PassMarker> passes;             // ascending pass id (3.5)
    friend bool operator==(const YearMarkers&, const YearMarkers&) = default;
};

// The symbol of an event kind key (3.4), compared as bytes; plain for any
// key the table does not name.
[[nodiscard]] EventSymbol event_symbol(std::string_view kind_key) noexcept;
// "plain", "plague", "battle", "siege", "sack".
[[nodiscard]] std::string_view event_symbol_word(EventSymbol symbol) noexcept;
// The ring colour of 3.4.
[[nodiscard]] Rgb event_symbol_colour(EventSymbol symbol) noexcept;

class MarkerIndex {
public:
    // Reads tables region (seed cells) and pass, and builds the capital runs
    // (3.2) and the ruin years and events (3.3) from the history. Errors,
    // first returned: the reader's; table pass with the report's row checks
    // and message (P1-00 10.3, P0-04 amendment 17: corrupt_layer "corrupt
    // table 'pass': <reason>", map size from meta.param.map_size); region ids
    // not equal, in order, to history.tables.regions: corrupt_layer "corrupt
    // table 'region': the file changed while it was read". std::bad_alloc:
    // out_of_memory "out of memory while building the markers". Never throws
    // for a bad file. Holds no reference into `history` or `world`.
    [[nodiscard]] static db::Result<MarkerIndex> load(const db::WorldReader& world,
                                                      const history::LoadedHistory& history);

    // The markers of state.year (section 3). `state` must be
    // history.timeline's year_state of that year and `history` the one load
    // was given (FatalError "markers: ..." otherwise, where detectable:
    // settlement count or year out of range). Reuses out's buffers.
    void markers_at(const history::LoadedHistory& history, const history::YearState& state,
                    const MarkerOptions& options, YearMarkers& out) const;
    // FatalError as Timeline for a bad year.
    [[nodiscard]] YearMarkers markers_at(const history::LoadedHistory& history, std::int64_t year,
                                         const MarkerOptions& options) const;

    // 3.2 and 3.3 directly (FatalError "markers: no civilization <c>" / "no
    // settlement <s>", or a bad year).
    [[nodiscard]] std::int64_t capital(const history::LoadedHistory& history, std::int64_t civ,
                                       std::int64_t year) const;  // 0: none
    [[nodiscard]] std::int64_t ruined_year(std::int64_t settlement) const;  // -1: never ruined
    [[nodiscard]] std::int64_t ruin_event(std::int64_t settlement) const;   // 0: none
    [[nodiscard]] std::span<const db::PassRow> passes() const noexcept { return passes_; }  // ascending id

    // P4-05 4.6, the place of 3.4: the event's settlement cell, else its
    // region's seed cell, else none. markers_at uses it for its places.
    // `history` the one load was given; FatalError "markers: no event <e>"
    // for an id not in the log.
    [[nodiscard]] std::optional<std::array<std::int64_t, 2>> event_place(const history::LoadedHistory& history,
                                                                         std::int64_t event) const;

private:
    struct CapitalEntry {
        std::int64_t year = 0, event = 0, settlement = 0;
    };
    struct EventPlace {
        bool placed = false;
        std::int64_t x = 0, y = 0;
        EventSymbol symbol = EventSymbol::plain;
    };

    void check_history(const history::LoadedHistory& history) const;
    [[nodiscard]] std::size_t settlement_index(std::int64_t settlement) const;
    // The place entry of event `event` (3.4); FatalError for an id not in the log.
    [[nodiscard]] const EventPlace& place_of(std::int64_t event) const;
    // The settlement of the last run entry of civ index c with year <= Y, 0 when none.
    [[nodiscard]] std::int64_t run_settlement(std::size_t c, std::int64_t year) const;
    // capital(c, Y) of 3.2 given the settlement states at Y and whether c is alive.
    [[nodiscard]] std::int64_t capital_from(std::size_t c, std::int64_t year, bool alive,
                                            std::span<const history::SettlementState> states) const;

    std::int64_t end_year_ = 0;
    std::vector<db::PassRow> passes_;
    // Settlements (index id - 1).
    std::vector<std::int64_t> settlement_x_, settlement_y_, settlement_founded_;
    std::vector<std::int64_t> ruined_year_, ruin_event_;
    // Civilizations (index id - 1): the alive rule of the timeline, and the
    // capital runs capitals_[run_offsets_[c]] .. capitals_[run_offsets_[c + 1] - 1].
    std::vector<std::int64_t> civ_founded_;
    std::vector<std::int64_t> civ_ended_;  // -1: not ended
    std::vector<CapitalEntry> capitals_;
    std::vector<std::size_t> run_offsets_;
    // Events (index id - 1): the place cell and symbol (3.4).
    std::vector<EventPlace> event_places_;
};

// Glyphs (section 5) and drawing (section 6).
struct GlyphKey {
    std::uint8_t base = 0;   // 1..5 settlement kind, 8 ruin, 9 faded, 10 pass, 11 place ring; 16..31 reserved for sites
    bool declining = false;  // base 1..5 only
    bool capital = false;    // base 1..5 only
    friend bool operator==(const GlyphKey&, const GlyphKey&) = default;
};
struct Glyph {
    std::int32_t radius = 0;          // the grid is (2 radius + 1) squared
    std::vector<std::uint8_t> cells;  // row-major from (-radius, -radius); 0 none, 1 halo, 2 fill, 3 ink, 4 interior
};
inline constexpr std::uint8_t GLYPH_NONE = 0;
inline constexpr std::uint8_t GLYPH_HALO = 1;
inline constexpr std::uint8_t GLYPH_FILL = 2;
inline constexpr std::uint8_t GLYPH_INK = 3;
inline constexpr std::uint8_t GLYPH_INTERIOR = 4;

inline constexpr Rgb MARKER_INK{0, 0, 0};
inline constexpr Rgb MARKER_HALO{240, 240, 240};
inline constexpr Rgb MARKER_FADED{150, 150, 150};
inline constexpr Rgb MARKER_NO_OWNER{128, 128, 128};

// FatalError "marker_glyph: ..." for a key section 5 does not define. Ruin
// (8) and faded (9) as P4-05 12.1 F3.
[[nodiscard]] Glyph marker_glyph(GlyphKey key);
// The key of a settlement entry: status 1 or 2 base = kind (0 for a kind
// outside 1..5), declining = status 2, capital; status 3 base 8 (an old
// ruin, P5-06 25.1 (e): base 9); status 4 base 9; any other status base 0.
[[nodiscard]] GlyphKey glyph_of(const SettlementMarker& marker) noexcept;
// The type of a settlement entry: standing (status 1 or 2), faded (status
// 4, or an old ruin), else ruin.
[[nodiscard]] MarkerType settlement_marker_type(const SettlementMarker& marker) noexcept;

// Draws the markers onto an RGB buffer laid out as render_rgb (width x
// height cells at scale k), in the order of 6.1. colours:
// civilization_colours (index id - 1). FatalError "draw_markers: ..." when
// rgb.size() != 3 (width k) (height k), k outside 1..16, or an owner without
// a colour.
void draw_markers(std::span<std::uint8_t> rgb, std::uint32_t width, std::uint32_t height, std::uint32_t scale,
                  const YearMarkers& markers, std::span<const Rgb> colours);

}  // namespace worldgen::inspect
