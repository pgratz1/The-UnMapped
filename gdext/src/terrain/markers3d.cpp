#include "terrain/markers3d.hpp"

#include <algorithm>
#include <map>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <initializer_list>
#include <new>
#include <stdexcept>
#include <utility>

#include "terrain/ground.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/db/result.hpp"
#include "worldgen/history/digest.hpp"
#include "worldgen/history/event_catalogue.hpp"
#include "worldgen/history/timeline.hpp"
#include "worldgen/inspect/markers.hpp"
#include "worldgen/inspect/territory.hpp"

namespace unmapped::terrain {

namespace {

using worldgen::history::EntityType;

std::uint64_t now_usec() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

std::uint32_t rgb_word(worldgen::inspect::Rgb c) {
    return (static_cast<std::uint32_t>(c.r) << 16U) | (static_cast<std::uint32_t>(c.g) << 8U) |
           static_cast<std::uint32_t>(c.b);
}

std::string name_of(const worldgen::history::HistoryView& view, EntityType type, std::int64_t id, const char* word) {
    const std::string* name = view.name(type, id);
    return name != nullptr && !name->empty() ? *name : std::string(word) + " " + std::to_string(id);
}

template <class Row>
const Row* find_row(const std::vector<Row>& rows, std::int64_t id) {
    const auto it = std::lower_bound(rows.begin(), rows.end(), id,
                                     [](const Row& row, std::int64_t key) { return row.id < key; });
    return it != rows.end() && it->id == id ? &*it : nullptr;
}

std::uint32_t colour_of(const std::vector<worldgen::inspect::Rgb>& colours, std::int64_t civ, std::uint32_t none) {
    if (civ < 1 || static_cast<std::size_t>(civ) > colours.size()) {
        return none;
    }
    return rgb_word(colours[static_cast<std::size_t>(civ - 1)]);
}

bool is_site(MarkerKind kind) {
    return kind == MarkerKind::lair || kind == MarkerKind::outpost || kind == MarkerKind::mine ||
           kind == MarkerKind::tower;
}

}  // namespace

// --- Text (3.2) -------------------------------------------------------------------------

std::string standing_text(const SettlementTextParts& p) {
    std::string text = p.name + ", " + std::string(worldgen::history::settlement_kind_word(p.kind));
    if (p.capital) {
        text += " (capital)";
    }
    if (!p.owner.empty()) {
        text += " of " + p.owner;
    }
    text += ", " + std::string(worldgen::history::settlement_status_word(p.status)) + ", population " +
            std::to_string(p.population) + " recorded in " + std::to_string(p.since) + ", founded in " +
            std::to_string(p.founded);
    return text;
}

std::string ruin_text(const std::string& name, std::string_view word, std::int64_t kind, std::int64_t ruined_year,
                      const std::string& cause) {
    return name + ", " + std::string(word) + " " + std::string(worldgen::history::settlement_kind_word(kind)) +
           ", ruined in " + std::to_string(ruined_year) + ": " + (cause.empty() ? std::string("cause not recorded") : cause);
}

std::string mine_text(const std::string& name, const std::string& founder, std::int64_t since, std::int64_t depth) {
    std::string text = name + " (mine";
    if (!founder.empty()) {
        text += " of " + founder;
    }
    text += "), since " + std::to_string(since);
    if (depth >= 1) {
        text += ", delved " + std::string(worldgen::inspect::mine_depth_word(depth));
    }
    return text;
}

std::string tower_text(const std::string& name, bool library, const std::string& figure, std::int64_t since) {
    std::string text = name + (library ? " (library" : " (tower");
    if (!figure.empty()) {
        text += " of " + figure;
    }
    return text + "), since " + std::to_string(since);
}

std::string lair_text(const std::string& name, const std::string& monster, const std::string& monster_kind, bool asleep,
                      std::int64_t since) {
    std::string text = name + " (lair";
    if (monster.empty()) {
        text += ", empty";
    } else {
        text += " of " + monster;
        if (!monster_kind.empty()) {
            text += ", " + monster_kind;
        }
        if (asleep) {
            text += ", asleep";
        }
    }
    return text + "), since " + std::to_string(since);
}

std::string outpost_text(const std::string& name, const std::string& founder, std::int64_t since) {
    std::string text = name + " (outpost";
    if (!founder.empty()) {
        text += " of " + founder;
    }
    return text + "), since " + std::to_string(since);
}

// --- Records (3) ------------------------------------------------------------------------

MarkerLoad load_marker_records(const worldgen::db::WorldReader& reader, std::optional<std::int64_t> year, bool pads,
                              int map_n) {
    namespace history = worldgen::history;
    namespace inspect = worldgen::inspect;
    MarkerLoad out;
    out.asked_year = year.value_or(-1);
    const auto fail = [&out](std::string code, std::string message) {
        out.status = MarkerStatus::failed;
        out.code = std::move(code);
        out.message = std::move(message);
        out.records.clear();
        return out;
    };
    try {
        std::uint64_t t0 = now_usec();
        worldgen::db::Result<history::LoadedHistory> loaded = history::load_history(reader);
        out.history_usec = now_usec() - t0;
        if (!loaded.ok()) {
            const worldgen::db::Error& error = loaded.error();
            if (error.code == worldgen::db::ErrorCode::history_missing) {
                out.status = MarkerStatus::none;
                out.code = worldgen::db::error_code_name(error.code);
                out.message = error.message;
                return out;
            }
            return fail(worldgen::db::error_code_name(error.code), error.message);
        }
        t0 = now_usec();
        const history::LoadedHistory& h = loaded.value();
        const history::HistoryView& view = h.view;
        const std::vector<inspect::Rgb> colours = inspect::civilization_colours(h.tables);
        worldgen::db::Result<inspect::MarkerIndex> index = inspect::MarkerIndex::load(reader, h);
        if (!index.ok()) {
            return fail(worldgen::db::error_code_name(index.error().code), index.error().message);
        }
        const std::int64_t end = h.timeline.end_year();
        out.end_year = end;
        std::int64_t shown = end;
        if (year) {
            shown = std::min(*year, end);
            out.year_clamped = *year > end;
        }
        out.year = shown;
        const auto build_year = [&](std::int64_t y, bool texts) {
            std::vector<MarkerRecord> records;
            inspect::MarkerOptions options;
            options.faded = true;
            options.events = false;
            options.passes = false;
            options.sites = true;
            const inspect::YearMarkers markers = index.value().markers_at(h, y, options);
            const auto sentence = [&view, texts](std::int64_t event) -> std::string {
                if (!texts) {
                    return {};
                }
                try {
                    return history::render_event(view, event);
                } catch (const std::exception& e) {
                    return std::string("(") + e.what() + ")";
                }
            };

            // Settlement entries, ascending settlement id.
            for (const inspect::SettlementMarker& s : markers.settlements) {
                MarkerRecord r;
                r.id = s.settlement;
                r.x = static_cast<int>(s.x);
                r.y = static_cast<int>(s.y);
                r.settlement_kind = static_cast<int>(s.kind);
                r.population = s.population;
                r.name = texts ? name_of(view, EntityType::settlement, s.settlement, "settlement") : std::string();
                const std::string kind_word(history::settlement_kind_word(s.kind));
                const inspect::MarkerType type = inspect::settlement_marker_type(s);
                if (type == inspect::MarkerType::standing) {
                    r.kind = MarkerKind::settlement;
                    r.capital = s.capital;
                    r.declining = s.status == 2;
                    r.colour = s.owner == 0 ? MARKER_NO_OWNER : colour_of(colours, s.owner, MARKER_NO_OWNER);
                    r.kind_word = kind_word + (s.capital ? ", capital" : "");
                    SettlementTextParts parts;
                    parts.name = r.name;
                    parts.kind = s.kind;
                    parts.status = s.status;
                    parts.capital = s.capital;
                    parts.owner = s.owner != 0 && texts ? name_of(view, EntityType::civilization, s.owner, "civilization") : "";
                    parts.population = s.population;
                    parts.since = s.since;
                    parts.founded = s.founded;
                    r.text = standing_text(parts);
                } else {
                    const bool faded = type == inspect::MarkerType::faded;
                    r.kind = faded ? MarkerKind::old_ruin : MarkerKind::ruin;
                    r.colour = faded ? OLD_RUIN_STONE : RUIN_STONE;
                    const char* word = !faded ? "ruin of a" : s.status == 4 ? "faded ruin of a" : "old ruin of a";
                    r.kind_word = std::string(word) + " " + kind_word;
                    r.text = ruin_text(r.name, word, s.kind, s.ruined_year, s.ruin_event != 0 ? sentence(s.ruin_event) : "");
                }
                records.push_back(std::move(r));
            }

            // Sites: mines and towers from the markers, lairs and outposts open at
            // Y from the timeline, merged ascending site id.
            std::vector<MarkerRecord> sites;
            for (const inspect::SiteMarker& m : markers.sites) {
                if (m.kind != 3 && m.kind != 4) {
                    continue;
                }
                MarkerRecord r;
                r.id = m.site;
                r.x = static_cast<int>(m.x);
                r.y = static_cast<int>(m.y);
                r.name = texts ? name_of(view, EntityType::site, m.site, "site") : std::string();
                const worldgen::db::SiteRow* row = find_row(h.tables.sites, m.site);
                const std::int64_t since = row != nullptr ? row->origin_year : 0;
                if (m.kind == 3) {
                    r.kind = MarkerKind::mine;
                    r.colour = m.founder == 0 ? MARKER_MINE : colour_of(colours, m.founder, MARKER_MINE);
                    r.kind_word = "mine";
                    const std::string founder =
                        m.founder != 0 && texts ? name_of(view, EntityType::civilization, m.founder, "civilization") : "";
                    if (texts) {
                        r.text = mine_text(r.name, founder, since, h.timeline.mine_depth_at(m.site, y));
                    }
                } else {
                    r.kind = MarkerKind::tower;
                    r.library = m.settlement != 0;
                    r.colour = MARKER_TOWER;
                    r.kind_word = r.library ? "library" : "tower";
                    std::string figure;
                    const history::HistoryView::Event* origin = row != nullptr && texts ? view.event(row->origin_event) : nullptr;
                    if (origin != nullptr) {
                        for (const auto& p : origin->participants) {
                            if (p.entity_type == static_cast<std::int64_t>(EntityType::figure)) {
                                const worldgen::db::FigureRow* f = find_row(h.tables.figures, p.entity_id);
                                figure = f != nullptr ? history::figure_display_name(*f) : "figure " + std::to_string(p.entity_id);
                                break;
                            }
                        }
                    }
                    r.text = tower_text(r.name, r.library, figure, since);
                }
                sites.push_back(std::move(r));
            }
            for (const std::int64_t site : h.timeline.sites_existing(y)) {
                const history::SiteState st = h.timeline.site_at(site, y);
                if (!st.open || (st.kind != 1 && st.kind != 2)) {
                    continue;
                }
                MarkerRecord r;
                r.id = site;
                r.x = static_cast<int>(st.x);
                r.y = static_cast<int>(st.y);
                r.name = texts ? name_of(view, EntityType::site, site, "site") : std::string();
                const worldgen::db::SiteRow* row = find_row(h.tables.sites, site);
                const std::int64_t since = row != nullptr ? row->origin_year : 0;
                if (st.kind == 1) {
                    r.kind = MarkerKind::lair;
                    r.colour = LAIR_ROCK;
                    r.kind_word = "lair";
                    std::string monster;
                    std::string monster_kind;
                    bool asleep = false;
                    if (st.monster != 0 && texts) {
                        const history::MonsterState ms = h.timeline.monster_at(st.monster, y);
                        if (ms.alive) {
                            monster = name_of(view, EntityType::monster, st.monster, "monster");
                            const worldgen::db::MonsterKindRow* kind = view.monster_kind_row(st.monster);
                            monster_kind = kind != nullptr ? kind->name : "";
                            asleep = !ms.awake;
                        }
                    }
                    r.text = lair_text(r.name, monster, monster_kind, asleep, since);
                } else {
                    r.kind = MarkerKind::outpost;
                    r.colour = st.founder == 0 ? MARKER_NO_OWNER : colour_of(colours, st.founder, MARKER_NO_OWNER);
                    r.kind_word = "outpost";
                    const std::string founder =
                        st.founder != 0 && texts ? name_of(view, EntityType::civilization, st.founder, "civilization") : "";
                    r.text = outpost_text(r.name, founder, since);
                }
                sites.push_back(std::move(r));
            }
            std::stable_sort(sites.begin(), sites.end(),
                             [](const MarkerRecord& a, const MarkerRecord& b) { return a.id < b.id; });
            for (MarkerRecord& r : sites) {
                records.push_back(std::move(r));
            }
            return records;
        };
        out.records = build_year(shown, true);
        out.records_usec = now_usec() - t0;
        if (pads) {
            // V3D-06 7 with Paul's ruling Q-V3D-06-4: a pad under every place
            // that ever stood, the union over the years 0 to E of each year's
            // records at their model cells (steps 1 and 2 of V3D-05 4 with
            // that year's records), one per (settlement or site, id, model
            // cell) with the largest r0 it had in any year.
            t0 = now_usec();
            std::map<std::array<std::int64_t, 4>, std::int32_t> places;
            for (std::int64_t year_k = 0; year_k <= end; ++year_k) {
                const std::vector<MarkerRecord> records = year_k == shown ? out.records : build_year(year_k, false);
                const std::vector<ModelCell> cells = marker_cells(records, map_n);
                for (std::size_t i = 0; i < records.size(); ++i) {
                    const std::array<std::int64_t, 4> key = {is_site(records[i].kind) ? 1 : 0, records[i].id,
                                                             cells[i].x, cells[i].y};
                    const std::int32_t r0 = pad_radius(records[i]);
                    auto it = places.find(key);
                    if (it == places.end()) {
                        places.emplace(key, r0);
                    } else {
                        it->second = std::max(it->second, r0);
                    }
                }
            }
            for (const auto& [key, r0] : places) {
                out.pad_places.push_back(PadPlace{static_cast<int>(key[2]), static_cast<int>(key[3]), r0});
            }
            out.pad_years = end + 1;
            out.places_usec = now_usec() - t0;
        }
        out.status = MarkerStatus::loaded;
        return out;
    } catch (const std::bad_alloc&) {
        return fail("out_of_memory", "out of memory while reading the markers");
    } catch (const worldgen::FatalError& e) {
        return fail("fatal_error", e.what());
    } catch (const std::exception& e) {
        return fail("fatal_error", e.what());
    }
}

// --- Placement (4) ----------------------------------------------------------------------

std::vector<ModelCell> marker_cells(std::span<const MarkerRecord> records, int n) {
    const auto clamp_cell = [n](int v) { return std::min(std::max(v, 0), n - 1); };
    std::vector<ModelCell> out;
    out.reserve(records.size());
    for (const MarkerRecord& rec : records) {
        ModelCell c;
        int cx = rec.x;
        int cy = rec.y;
        if (rec.kind == MarkerKind::mine || rec.kind == MarkerKind::tower) {
            worldgen::inspect::SiteMarker site;
            site.kind = rec.kind == MarkerKind::mine ? 3 : 4;
            site.settlement = rec.library ? 1 : 0;
            const std::array<std::int64_t, 2> off = worldgen::inspect::site_glyph_offset(site);
            cx = clamp_cell(cx + static_cast<int>(off[0]));
            cy = clamp_cell(cy + static_cast<int>(off[1]));
        } else {
            cx = clamp_cell(cx);
            cy = clamp_cell(cy);
        }
        if (is_site(rec.kind)) {
            const auto crowded = [&out](int x, int y) {
                for (const ModelCell& e : out) {
                    if (std::max(std::abs(e.x - x), std::abs(e.y - y)) <= MARKER_CLEAR_CELLS) {
                        return true;
                    }
                }
                return false;
            };
            while (c.moves < MARKER_MAX_MOVES && crowded(cx, cy)) {
                cx = clamp_cell(cx + MARKER_MOVE_CELLS);
                cy = clamp_cell(cy + MARKER_MOVE_CELLS);
                ++c.moves;
            }
        }
        c.x = cx;
        c.y = cy;
        out.push_back(c);
    }
    return out;
}

std::vector<Marker3D> place_markers(std::span<const MarkerRecord> records, const Layout& layout,
                                    std::span<const float> heights) {
    const int n = layout.n;
    if (heights.size() != static_cast<std::size_t>(n) * static_cast<std::size_t>(n)) {
        throw std::invalid_argument("place_markers: the height buffer is not N^2 long");
    }
    return place_markers(records, layout, coarse_field(layout, std::vector<float>(heights.begin(), heights.end())));
}

std::vector<Marker3D> place_markers(std::span<const MarkerRecord> records, const Layout& layout, const GroundField& field) {
    const std::vector<ModelCell> cells = marker_cells(records, layout.n);
    std::vector<Marker3D> out;
    out.reserve(records.size());
    for (std::size_t i = 0; i < records.size(); ++i) {
        const MarkerRecord& rec = records[i];
        Marker3D m;
        m.rec = rec;
        m.cell_x = cells[i].x;
        m.cell_y = cells[i].y;
        m.moves = cells[i].moves;
        const Vec2 w = cell_to_world(layout, m.cell_x, m.cell_y);
        m.X = w.x;
        m.Z = w.z;
        m.Y = field_at_cell(field, m.cell_x, m.cell_y);
        const MeshData mesh = build_marker_mesh(m, field.layout, field.heights);
        float top = 0.0f;
        for (std::size_t v = 1; v < mesh.positions.size(); v += 3) {
            top = std::max(top, mesh.positions[v]);
        }
        m.top = m.Y + top;
        m.beacon = rec.kind != MarkerKind::old_ruin;
        switch (rec.kind) {
            case MarkerKind::settlement:
            case MarkerKind::outpost: m.beacon_colour = rec.colour; break;
            case MarkerKind::ruin: m.beacon_colour = RUIN_BEACON; break;
            case MarkerKind::lair: m.beacon_colour = LAIR_BEACON; break;
            case MarkerKind::mine: m.beacon_colour = MINE_BEACON; break;
            case MarkerKind::tower: m.beacon_colour = MARKER_TOWER; break;
            case MarkerKind::old_ruin: m.beacon_colour = 0; break;
        }
        out.push_back(std::move(m));
    }
    return out;
}

// --- Models (5) -------------------------------------------------------------------------

const std::array<HouseSlot, 289>& house_slots() {
    static const std::array<HouseSlot, 289> slots = [] {
        std::array<HouseSlot, 289> s{};
        std::size_t k = 0;
        for (int j = -8; j <= 8; ++j) {
            for (int i = -8; i <= 8; ++i) {
                s[k++] = HouseSlot{i, j};
            }
        }
        std::sort(s.begin(), s.end(), [](const HouseSlot& a, const HouseSlot& b) {
            const int ra = a.i * a.i + a.j * a.j;
            const int rb = b.i * b.i + b.j * b.j;
            if (ra != rb) {
                return ra < rb;
            }
            if (a.j != b.j) {
                return a.j < b.j;
            }
            return a.i < b.i;
        });
        return s;
    }();
    return slots;
}

int house_count(int kind, std::int64_t population) {
    const int k = kind >= 1 && kind <= 5 ? kind : 1;
    const std::int64_t root = isqrt64(std::max<std::int64_t>(population, 0) / 8);
    const std::int64_t count = std::max<std::int64_t>(MIN_HOUSES[static_cast<std::size_t>(k)], root);
    return static_cast<int>(std::min<std::int64_t>(MAX_HOUSES, count));
}

float model_far(MarkerKind kind) {
    return kind == MarkerKind::old_ruin ? OLD_RUIN_FAR : MODEL_FAR;
}

namespace {

struct V3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V3 operator*(V3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }
double dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
V3 normalize(V3 a) {
    const double l = std::sqrt(dot(a, a));
    return l > 0.0 ? a * (1.0 / l) : a;
}

constexpr double PI = 3.14159265358979323846;
constexpr double TAU = 2.0 * PI;

constexpr std::uint32_t rgba(std::uint32_t rgb) { return (rgb << 8) | 0xFFu; }

std::uint32_t dim_roof(std::uint32_t rgb) {
    std::uint32_t out = 0;
    for (int shift = 16; shift >= 0; shift -= 8) {
        const std::uint32_t c = (rgb >> shift) & 0xFFu;
        out |= ((c * 12u) / 16u) << shift;
    }
    return out;
}

struct Builder {
    MeshData mesh;
    const Layout& layout;
    std::span<const float> heights;
    double X = 0.0;
    double Y = 0.0;
    double Z = 0.0;
    std::vector<GroundedPart>* parts = nullptr;

    void triangle(V3 a, V3 b, V3 c, V3 outward, std::uint32_t colour) {
        V3 n = cross(b - a, c - a);
        if (dot(n, outward) > 0.0) {
            std::swap(b, c);
            n = cross(b - a, c - a);
        }
        const V3 face = normalize(n * -1.0);
        for (const V3& p : {a, b, c}) {
            mesh.positions.push_back(static_cast<float>(p.x));
            mesh.positions.push_back(static_cast<float>(p.y));
            mesh.positions.push_back(static_cast<float>(p.z));
            mesh.normals.push_back(static_cast<float>(face.x));
            mesh.normals.push_back(static_cast<float>(face.y));
            mesh.normals.push_back(static_cast<float>(face.z));
            mesh.colours.push_back(colour);
        }
    }

    void quad(V3 a, V3 b, V3 c, V3 d, V3 outward, std::uint32_t colour) {
        triangle(a, b, c, outward, colour);
        triangle(a, c, d, outward, colour);
    }

    // The four footprint corners (x, z) of a w x d rectangle centred at
    // (cx, cz) and turned by yaw (degrees): local x maps to (cos, sin).
    static std::array<V3, 4> footprint(double cx, double cz, double w, double d, double yaw_deg) {
        const double a = yaw_deg * PI / 180.0;
        const double c = std::cos(a);
        const double s = std::sin(a);
        const double hx[4] = {-w / 2, w / 2, w / 2, -w / 2};
        const double hz[4] = {-d / 2, -d / 2, d / 2, d / 2};
        std::array<V3, 4> out{};
        for (int k = 0; k < 4; ++k) {
            out[static_cast<std::size_t>(k)] = V3{cx + hx[k] * c - hz[k] * s, 0.0, cz + hx[k] * s + hz[k] * c};
        }
        return out;
    }

    // g0 - Y: the least ground_low at the corners, relative to the anchor.
    double ground_rel(const std::array<V3, 4>& corners) const {
        double g0 = 1e30;
        for (const V3& p : corners) {
            const float g = ground_low(layout, heights, static_cast<float>(X + p.x), static_cast<float>(Z + p.z));
            g0 = std::min(g0, static_cast<double>(g));
        }
        return g0 - Y;
    }

    void prism(const std::array<V3, 4>& c, double y0, double y1, std::uint32_t colour) {
        V3 centre{0.0, 0.0, 0.0};
        for (const V3& p : c) {
            centre = centre + p * 0.25;
        }
        for (int k = 0; k < 4; ++k) {
            const V3 p = c[static_cast<std::size_t>(k)];
            const V3 q = c[static_cast<std::size_t>((k + 1) % 4)];
            const V3 mid = (p + q) * 0.5 - centre;
            quad(V3{p.x, y0, p.z}, V3{q.x, y0, q.z}, V3{q.x, y1, q.z}, V3{p.x, y1, p.z}, V3{mid.x, 0.0, mid.z}, colour);
        }
        quad(V3{c[0].x, y1, c[0].z}, V3{c[1].x, y1, c[1].z}, V3{c[2].x, y1, c[2].z}, V3{c[3].x, y1, c[3].z},
             V3{0.0, 1.0, 0.0}, colour);
        quad(V3{c[0].x, y0, c[0].z}, V3{c[1].x, y0, c[1].z}, V3{c[2].x, y0, c[2].z}, V3{c[3].x, y0, c[3].z},
             V3{0.0, -1.0, 0.0}, colour);
    }

    // A box on the ground (5's ground rule); returns its top (relative y).
    double box(double cx, double cz, double w, double d, double h, double yaw, std::uint32_t rgb) {
        const std::array<V3, 4> c = footprint(cx, cz, w, d, yaw);
        const double g = ground_rel(c);
        const double y0 = g - static_cast<double>(SINK);
        const double y1 = g + h;
        prism(c, y0, y1, rgba(rgb));
        if (parts != nullptr) {
            GroundedPart part;
            for (std::size_t k = 0; k < 4; ++k) {
                part.corners[2 * k] = static_cast<float>(c[k].x);
                part.corners[2 * k + 1] = static_cast<float>(c[k].z);
            }
            part.bottom = static_cast<float>(y0);
            parts->push_back(part);
        }
        return y1;
    }

    // A box from y0 to y1 (relative), not on the ground.
    void box_at(double cx, double cz, double w, double d, double y0, double y1, double yaw, std::uint32_t rgb) {
        prism(footprint(cx, cz, w, d, yaw), y0, y1, rgba(rgb));
    }

    // Two slopes and two gables on a w x d box top at y = base, ridge along
    // the longer side, height rh.
    void gable(double cx, double cz, double w, double d, double rh, double yaw, double base, std::uint32_t rgb) {
        const std::uint32_t colour = rgba(rgb);
        const bool along_x = w >= d;
        const double a = yaw * PI / 180.0;
        const double c = std::cos(a);
        const double s = std::sin(a);
        const auto at = [&](double lx, double ly, double lz) {
            return V3{cx + lx * c - lz * s, ly, cz + lx * s + lz * c};
        };
        const auto dir = [&](double lx, double ly, double lz) { return V3{lx * c - lz * s, ly, lx * s + lz * c}; };
        const double hw = w / 2;
        const double hd = d / 2;
        const double top = base + rh;
        if (along_x) {
            quad(at(-hw, base, -hd), at(hw, base, -hd), at(hw, top, 0), at(-hw, top, 0), dir(0, hd, -rh), colour);
            quad(at(-hw, base, hd), at(hw, base, hd), at(hw, top, 0), at(-hw, top, 0), dir(0, hd, rh), colour);
            triangle(at(-hw, base, -hd), at(-hw, base, hd), at(-hw, top, 0), dir(-1, 0, 0), colour);
            triangle(at(hw, base, -hd), at(hw, base, hd), at(hw, top, 0), dir(1, 0, 0), colour);
        } else {
            quad(at(-hw, base, -hd), at(-hw, base, hd), at(0, top, hd), at(0, top, -hd), dir(-rh, hw, 0), colour);
            quad(at(hw, base, -hd), at(hw, base, hd), at(0, top, hd), at(0, top, -hd), dir(rh, hw, 0), colour);
            triangle(at(-hw, base, -hd), at(hw, base, -hd), at(0, top, -hd), dir(0, 0, -1), colour);
            triangle(at(-hw, base, hd), at(hw, base, hd), at(0, top, hd), dir(0, 0, 1), colour);
        }
    }

    // A square pyramid of base w at y = base, apex h above, no bottom.
    void pyramid(double cx, double cz, double w, double h, double base, std::uint32_t rgb) {
        const std::uint32_t colour = rgba(rgb);
        const double hw = w / 2;
        const V3 apex{cx, base + h, cz};
        const V3 c[4] = {{cx - hw, base, cz - hw}, {cx + hw, base, cz - hw}, {cx + hw, base, cz + hw}, {cx - hw, base, cz + hw}};
        for (int k = 0; k < 4; ++k) {
            const V3 p = c[k];
            const V3 q = c[(k + 1) % 4];
            const V3 mid = (p + q) * 0.5;
            triangle(p, q, apex, V3{mid.x - cx, hw, mid.z - cz}, colour);
        }
    }

    // V3D-02 6.2's ellipsoid, 8 sides and 4 rings, centred at g0 - Y of its
    // bounding square.
    void ellipsoid(double cx, double cz, double rx, double ry, double rz, std::uint32_t rgb) {
        const std::array<V3, 4> square = footprint(cx, cz, 2 * rx, 2 * rz, 0.0);
        const double g = ground_rel(square);
        const V3 centre{cx, g, cz};
        const std::uint32_t colour = rgba(rgb);
        constexpr int sides = 8;
        constexpr int rings = 4;
        const auto point = [&](int k, int i) {
            const double phi = -TAU / 4.0 + TAU / 2.0 * static_cast<double>(k) / static_cast<double>(rings);
            const double t = TAU * static_cast<double>(i) / static_cast<double>(sides);
            return centre + V3{rx * std::cos(phi) * std::cos(t), ry * std::sin(phi), rz * std::cos(phi) * std::sin(t)};
        };
        for (int k = 0; k < rings; ++k) {
            for (int i = 0; i < sides; ++i) {
                const V3 a = point(k, i);
                const V3 b = point(k, i + 1);
                const V3 d = point(k + 1, i);
                const V3 e = point(k + 1, i + 1);
                if (k == 0) {
                    triangle(a, d, e, ((a + d + e) * (1.0 / 3.0)) - centre, colour);
                } else if (k == rings - 1) {
                    triangle(a, b, d, ((a + b + d) * (1.0 / 3.0)) - centre, colour);
                } else {
                    triangle(a, b, e, ((a + b + e) * (1.0 / 3.0)) - centre, colour);
                    triangle(a, e, d, ((a + e + d) * (1.0 / 3.0)) - centre, colour);
                }
            }
        }
        if (parts != nullptr) {
            GroundedPart part;
            for (std::size_t k = 0; k < 4; ++k) {
                part.corners[2 * k] = static_cast<float>(square[k].x);
                part.corners[2 * k + 1] = static_cast<float>(square[k].z);
            }
            part.bottom = static_cast<float>(g - ry);
            parts->push_back(part);
        }
    }
};

struct House {
    double x = 0.0, z = 0.0, w = 0.0, d = 0.0, h = 0.0, yaw = 0.0;
    std::uint32_t bits = 0;
};

House house_of(std::int64_t id, int k) {
    const HouseSlot slot = house_slots()[static_cast<std::size_t>(k)];
    const std::uint32_t h = hash32(static_cast<std::uint32_t>(id), static_cast<std::uint32_t>(k), HOUSE_KEY);
    House out;
    out.bits = h;
    out.x = 16.0 * slot.i + (static_cast<double>(h & 15u) - 7.5) * 0.125;
    out.z = 16.0 * slot.j + (static_cast<double>((h >> 4) & 15u) - 7.5) * 0.125;
    out.w = 6.0 + 0.5 * static_cast<double>((h >> 8) & 7u);
    out.d = 5.0 + 0.5 * static_cast<double>((h >> 11) & 7u);
    out.h = 3.5 + 0.75 * static_cast<double>((h >> 14) & 3u);
    out.yaw = ((h >> 16) & 1u ? 90.0 : 0.0) + (static_cast<double>((h >> 17) & 15u) - 7.5) * 1.25;
    return out;
}

int extent(int count) {
    int e = 0;
    for (int k = 0; k < count; ++k) {
        const HouseSlot s = house_slots()[static_cast<std::size_t>(k)];
        e = std::max(e, std::max(std::abs(s.i), std::abs(s.j)));
    }
    return e;
}

}  // namespace

int marker_extent(const MarkerRecord& rec) {
    switch (rec.kind) {
        case MarkerKind::settlement: {
            const int kind = rec.settlement_kind >= 1 && rec.settlement_kind <= 5 ? rec.settlement_kind : 1;
            return extent(house_count(kind, rec.population));
        }
        case MarkerKind::ruin:
        case MarkerKind::old_ruin: {
            const int H = house_count(rec.settlement_kind, rec.population);
            return extent(rec.kind == MarkerKind::old_ruin ? (H + 3) / 4 : (H + 1) / 2);
        }
        default:
            return 0;
    }
}

std::int32_t pad_radius(const MarkerRecord& rec) {
    switch (rec.kind) {
        case MarkerKind::settlement:
            return 16 * marker_extent(rec) + ((rec.settlement_kind == 4 || rec.settlement_kind == 5) ? 24 : 16);
        case MarkerKind::ruin:
        case MarkerKind::old_ruin: return 16 * marker_extent(rec) + 16;
        case MarkerKind::lair: return 16;
        case MarkerKind::outpost: return 20;
        case MarkerKind::mine: return 24;
        case MarkerKind::tower: return rec.library ? 20 : 8;
    }
    return 16;
}

namespace {

void city_wall(Builder& b, int e, double height) {
    const double S = 16.0 * e + 12.0;
    const double len = 2.0 * S + 2.0;
    b.box(0.0, -S, len, 2.0, height, 0.0, WALL_STONE);  // north
    b.box(S, 0.0, 2.0, len, height, 0.0, WALL_STONE);   // east
    b.box(-S, 0.0, 2.0, len, height, 0.0, WALL_STONE);  // west
    const double half = S + 1.0 - 5.0;                  // south, a 10 m gate at the centre
    b.box(-(5.0 + half / 2.0), S, half, 2.0, height, 0.0, WALL_STONE);
    b.box(5.0 + half / 2.0, S, half, 2.0, height, 0.0, WALL_STONE);
}

void ruin_ring(Builder& b, int e, std::initializer_list<int> segments, double height, std::uint32_t rgb) {
    const double R = 16.0 * e + 10.0;
    const double len = 2.0 * R * std::sin(22.5 * PI / 180.0) * 0.85;
    for (const int m : segments) {
        const double a = 45.0 * m;
        const double r = a * PI / 180.0;
        b.box(R * std::sin(r), -R * std::cos(r), len, 2.0, height, a, rgb);
    }
}

void build_into(Builder& b, const Marker3D& marker) {
    const MarkerRecord& rec = marker.rec;
    switch (rec.kind) {
        case MarkerKind::settlement: {
            const int kind = rec.settlement_kind >= 1 && rec.settlement_kind <= 5 ? rec.settlement_kind : 1;
            const int H = house_count(kind, rec.population);
            const std::uint32_t roof = rec.declining ? dim_roof(rec.colour) : rec.colour;
            for (int k = 0; k < H; ++k) {
                if (k == 0 && kind == 5) {
                    const double s = rec.capital ? 12.0 : 10.0;
                    const double top = b.box(0.0, 0.0, s, s, rec.capital ? 20.0 : 16.0, 0.0, WALL_STONE);
                    b.pyramid(0.0, 0.0, rec.capital ? 13.0 : 11.0, rec.capital ? 6.0 : 5.0, top, roof);
                    continue;
                }
                if (k == 0 && rec.capital) {
                    const double top = b.box(0.0, 0.0, 12.0, 12.0, 10.0, 0.0, HOUSE_WALL);
                    b.pyramid(0.0, 0.0, 13.0, 6.0, top, roof);
                    continue;
                }
                const House h = house_of(rec.id, k);
                const double top = b.box(h.x, h.z, h.w, h.d, h.h, h.yaw, HOUSE_WALL);
                const bool roofless = rec.declining && ((h.bits >> 21) & 1u) != 0;
                if (!roofless) {
                    b.gable(h.x, h.z, h.w, h.d, 0.45 * std::min(h.w, h.d), h.yaw, top, roof);
                }
            }
            if (kind == 4 || kind == 5) {
                city_wall(b, extent(H), kind == 5 ? 8.0 : 6.0);
            }
            break;
        }
        case MarkerKind::ruin:
        case MarkerKind::old_ruin: {
            const bool old = rec.kind == MarkerKind::old_ruin;
            const int H = house_count(rec.settlement_kind, rec.population);
            const int stubs = old ? (H + 3) / 4 : (H + 1) / 2;
            const std::uint32_t stone = old ? OLD_RUIN_STONE : RUIN_STONE;
            for (int k = 0; k < stubs; ++k) {
                const House h = house_of(rec.id, k);
                double height = 1.0 + 0.5 * static_cast<double>((h.bits >> 14) & 3u);
                if (old) {
                    height *= 0.5;
                }
                b.box(h.x, h.z, h.w, h.d, height, h.yaw, stone);
            }
            if (old) {
                ruin_ring(b, extent(stubs), {0, 3, 5}, 1.0, stone);
            } else {
                ruin_ring(b, extent(stubs), {0, 2, 3, 5, 7}, 2.0, stone);
            }
            break;
        }
        case MarkerKind::lair:
            b.ellipsoid(0.0, 0.0, 10.0, 6.0, 10.0, LAIR_ROCK);
            b.box(0.0, 8.5, 6.0, 2.0, 4.0, 0.0, LAIR_MOUTH);
            break;
        case MarkerKind::outpost: {
            b.box(0.0, -11.0, 23.0, 1.0, 4.0, 0.0, WOOD);
            b.box(0.0, 11.0, 23.0, 1.0, 4.0, 0.0, WOOD);
            b.box(11.0, 0.0, 1.0, 21.0, 4.0, 0.0, WOOD);
            b.box(-11.0, 0.0, 1.0, 21.0, 4.0, 0.0, WOOD);
            const double top = b.box(-9.0, -9.0, 4.0, 4.0, 9.0, 0.0, WOOD);
            b.pyramid(-9.0, -9.0, 5.0, 2.5, top, rec.colour);
            break;
        }
        case MarkerKind::mine: {
            b.box(0.0, 0.0, 8.0, 8.0, 0.4, 0.0, LAIR_MOUTH);
            const double t0 = b.box(-4.0, 0.0, 0.8, 0.8, 10.0, 0.0, WOOD);
            const double t1 = b.box(4.0, 0.0, 0.8, 0.8, 10.0, 0.0, WOOD);
            const double t = std::min(t0, t1);
            b.box_at(0.0, 0.0, 9.6, 0.8, t - 0.8, t, 0.0, rec.colour);
            b.ellipsoid(12.0, 0.0, 7.0, 3.5, 7.0, SPOIL);
            break;
        }
        case MarkerKind::tower: {
            const double top = b.box(0.0, 0.0, 6.0, 6.0, rec.library ? 26.0 : 36.0, 0.0, TOWER_STONE);
            b.pyramid(0.0, 0.0, 7.0, 6.0, top, MARKER_TOWER);
            if (rec.library) {
                const double hall = b.box(9.0, 0.0, 12.0, 8.0, 7.0, 0.0, TOWER_STONE);
                b.gable(9.0, 0.0, 12.0, 8.0, 0.45 * 8.0, 0.0, hall, MARKER_TOWER);
            }
            break;
        }
    }
}

}  // namespace

MeshData build_marker_mesh(const Marker3D& marker, const Layout& layout, std::span<const float> heights) {
    Builder b{MeshData{}, layout, heights, static_cast<double>(marker.X), static_cast<double>(marker.Y),
              static_cast<double>(marker.Z), nullptr};
    build_into(b, marker);
    return std::move(b.mesh);
}

std::vector<GroundedPart> grounded_parts(const Marker3D& marker, const Layout& layout, std::span<const float> heights) {
    std::vector<GroundedPart> parts;
    Builder b{MeshData{}, layout, heights, static_cast<double>(marker.X), static_cast<double>(marker.Y),
              static_cast<double>(marker.Z), &parts};
    build_into(b, marker);
    return parts;
}

// --- Beacons (6) ------------------------------------------------------------------------

BeaconStyle beacon_style(const MarkerRecord& rec) {
    switch (rec.kind) {
        case MarkerKind::settlement: {
            static constexpr float H[5] = {80.0f, 120.0f, 180.0f, 260.0f, 220.0f};
            const int k = rec.settlement_kind >= 1 && rec.settlement_kind <= 5 ? rec.settlement_kind : 1;
            return BeaconStyle{H[k - 1] + (rec.capital ? 100.0f : 0.0f), rec.capital ? 6.0f : 4.0f, 40.0f, 16000.0f,
                               24000.0f};
        }
        case MarkerKind::ruin: return BeaconStyle{60.0f, 2.0f, 24.0f, 5000.0f, 8000.0f};
        case MarkerKind::old_ruin: return BeaconStyle{};
        case MarkerKind::tower: return BeaconStyle{90.0f, 3.0f, 24.0f, 8000.0f, 12000.0f};
        case MarkerKind::lair:
        case MarkerKind::outpost:
        case MarkerKind::mine: return BeaconStyle{70.0f, 3.0f, 24.0f, 8000.0f, 12000.0f};
    }
    return BeaconStyle{};
}

float pixel_angle(float fov_deg, int viewport_h) {
    const double t = std::tan(static_cast<double>(fov_deg) * PI / 360.0);
    return static_cast<float>(2.0 * t / static_cast<double>(std::max(viewport_h, 1)));
}

std::array<float, 12> beacon_transform(const Marker3D& m, float cx, float cy, float cz, float a) {
    const float bx = m.X;
    const float by = m.top + LABEL_LIFT;
    const float bz = m.Z;
    std::array<float, 12> t{};
    t[9] = bx;
    t[10] = by;
    t[11] = bz;
    const BeaconStyle s = beacon_style(m.rec);
    const float dx = cx - bx;
    const float dy = cy - by;
    const float dz = cz - bz;
    const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (!m.beacon || d >= s.far) {
        return t;
    }
    const float taper = d <= s.fade ? 1.0f : (s.far - d) / (s.far - s.fade);
    const float w = std::max(BEACON_MIN_W, d * s.px_w * a) * taper;
    const float h = std::max(s.h, d * s.px_h * a);
    const float yaw = std::atan2(dx, dz);
    const float c = std::cos(yaw);
    const float sn = std::sin(yaw);
    t[0] = w * c;
    t[1] = 0.0f;
    t[2] = -w * sn;
    t[3] = 0.0f;
    t[4] = h;
    t[5] = 0.0f;
    t[6] = sn;
    t[7] = 0.0f;
    t[8] = c;
    return t;
}

// --- Labels (7) -------------------------------------------------------------------------

std::vector<std::size_t> select_labels(std::span<const Marker3D> markers, float cx, float cy, float cz,
                                       std::span<const std::size_t> previous) {
    std::vector<float> d(markers.size());
    std::vector<std::size_t> wide;
    const float wide_range = LABEL_RANGE + LABEL_SLACK;
    for (std::size_t i = 0; i < markers.size(); ++i) {
        const float dx = cx - markers[i].X;
        const float dy = cy - (markers[i].top + LABEL_LIFT);
        const float dz = cz - markers[i].Z;
        d[i] = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (d[i] <= wide_range) {
            wide.push_back(i);
        }
    }
    const auto by_distance = [&d](std::size_t a, std::size_t b) { return d[a] != d[b] ? d[a] < d[b] : a < b; };
    std::sort(wide.begin(), wide.end(), by_distance);
    std::vector<std::size_t> shown;
    for (const std::size_t i : wide) {
        if (shown.size() >= LABEL_MAX || d[i] > LABEL_RANGE) {
            break;
        }
        shown.push_back(i);
    }
    for (const std::size_t i : previous) {
        if (i >= markers.size() || std::find(shown.begin(), shown.end(), i) != shown.end()) {
            continue;
        }
        const auto it = std::find(wide.begin(), wide.end(), i);
        if (it != wide.end() && static_cast<std::size_t>(it - wide.begin()) < LABEL_KEEP) {
            shown.push_back(i);
        }
    }
    std::sort(shown.begin(), shown.end(), by_distance);
    if (shown.size() > LABEL_KEEP) {
        shown.resize(LABEL_KEEP);
    }
    return shown;
}

LabelBox label_box(const MarkerRecord& rec, float sx, float sy, float em_px, float offset_px, bool on_screen) {
    std::size_t chars = 0;
    std::size_t line = 0;
    const std::string text = label_text(rec);
    for (const char ch : text) {
        if (ch == '\n') {
            chars = std::max(chars, line);
            line = 0;
        } else if ((static_cast<unsigned char>(ch) & 0xC0u) != 0x80u) {
            ++line;
        }
    }
    chars = std::max(chars, line);
    LabelBox box;
    box.w = static_cast<float>(chars) * LABEL_CHAR_EM * em_px;
    box.h = 2.0f * LABEL_LINE_EM * em_px;
    box.x = sx;
    box.y = sy - offset_px;
    box.on_screen = on_screen;
    return box;
}

std::vector<bool> declutter_labels(std::span<const LabelBox> boxes) {
    std::vector<bool> drawn(boxes.size(), true);
    for (std::size_t j = 0; j < boxes.size(); ++j) {
        const LabelBox& b = boxes[j];
        if (!b.on_screen) {
            continue;
        }
        for (std::size_t i = 0; i < j; ++i) {
            const LabelBox& a = boxes[i];
            if (!drawn[i] || !a.on_screen) {
                continue;
            }
            const float ox = std::min(a.x + a.w / 2, b.x + b.w / 2) - std::max(a.x - a.w / 2, b.x - b.w / 2);
            const float oy = std::min(a.y + a.h / 2, b.y + b.h / 2) - std::max(a.y - a.h / 2, b.y - b.h / 2);
            if (ox > 0.0f && oy > 0.0f && ox * oy > 0.5f * std::min(a.w * a.h, b.w * b.h)) {
                drawn[j] = false;
                break;
            }
        }
    }
    return drawn;
}

std::string label_text(const MarkerRecord& rec) {
    return rec.name + "\n" + rec.kind_word;
}

std::uint32_t label_modulate(MarkerKind kind) {
    switch (kind) {
        case MarkerKind::ruin: return 0xC8C8C8;
        case MarkerKind::old_ruin: return 0xAAAAAA;
        default: return 0xFFFFFF;
    }
}

// --- HUD, status, jump, options (8) ----------------------------------------------------------

std::optional<std::size_t> nearest_marker(std::span<const Marker3D> markers, float X, float Z) {
    std::optional<std::size_t> best;
    float best_d = 0.0f;
    for (std::size_t i = 0; i < markers.size(); ++i) {
        const float dx = markers[i].X - X;
        const float dz = markers[i].Z - Z;
        const float dd = dx * dx + dz * dz;
        if (!best || dd < best_d) {
            best = i;
            best_d = dd;
        }
    }
    return best;
}

std::string bearing_word(float dx, float dz) {
    static constexpr const char* WORDS[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    const double theta = std::atan2(static_cast<double>(dx), static_cast<double>(-dz)) * 180.0 / PI;
    const auto sector = static_cast<long>(std::floor((theta + 22.5) / 45.0));
    return WORDS[((sector % 8) + 8) % 8];
}

std::string distance_text(float m) {
    if (m < 1000.0f) {
        return std::to_string(static_cast<long>(std::floor(std::max(m, 0.0f)))) + " m";
    }
    return one_decimal(static_cast<double>(m) / 1000.0) + " km";
}

std::string cut_utf8(const std::string& text, std::size_t max) {
    if (text.size() <= max) {
        return text;
    }
    std::size_t end = max >= 3 ? max - 3 : 0;
    while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0u) == 0x80u) {
        --end;
    }
    return text.substr(0, end) + "...";
}

std::string markers_hud_line(MarkersHud state, std::span<const Marker3D> markers, std::int64_t year, float cam_x,
                             float cam_z) {
    switch (state) {
        case MarkersHud::off: return "markers off (M)";
        case MarkersHud::none: return "markers: none, no history in this world";
        case MarkersHud::failed: return "markers: none, the history could not be read";
        case MarkersHud::disabled: return "markers off (--no-markers)";
        case MarkersHud::on: break;
    }
    std::string text = "markers " + std::to_string(markers.size()) + " at year " + std::to_string(year);
    const std::optional<std::size_t> near = nearest_marker(markers, cam_x, cam_z);
    if (near) {
        const Marker3D& m = markers[*near];
        const float dx = m.X - cam_x;
        const float dz = m.Z - cam_z;
        const float d = std::sqrt(dx * dx + dz * dz);
        if (d <= HUD_TEXT_RADIUS) {
            text += "; nearest " + distance_text(d) + " " + bearing_word(dx, dz) + ": " + m.rec.text;
        } else {
            text += "; nearest " + m.rec.name + " (" + m.rec.kind_word + "), " + distance_text(d) + " " +
                    bearing_word(dx, dz);
        }
    }
    return cut_utf8(text, HUD_LINE_MAX);
}

std::vector<std::size_t> jump_order(std::span<const Marker3D> markers) {
    std::vector<std::size_t> order;
    for (std::size_t i = 0; i < markers.size(); ++i) {
        if (markers[i].rec.kind == MarkerKind::settlement) {
            order.push_back(i);
        }
    }
    std::sort(order.begin(), order.end(), [&markers](std::size_t a, std::size_t b) {
        const MarkerRecord& ra = markers[a].rec;
        const MarkerRecord& rb = markers[b].rec;
        if (ra.population != rb.population) {
            return ra.population > rb.population;
        }
        return ra.id < rb.id;
    });
    return order;
}

FlyState jump_view(const Marker3D& m, const Layout& layout, std::span<const float> heights) {
    FlyState s;
    s.x = m.X;
    s.z = m.Z + JUMP_DIST;
    s.heading_deg = 0.0f;
    if (m.Z + JUMP_DIST > 8.0f * static_cast<float>(layout.n) - 16.0f) {
        s.z = m.Z - JUMP_DIST;
        s.heading_deg = 180.0f;
    }
    s.y = std::max(m.top + JUMP_ABOVE, ground_clamp(layout, heights, s.x, s.z) + 100.0f);
    const double pitch = -std::atan2(static_cast<double>(s.y - m.top), static_cast<double>(JUMP_DIST)) * 180.0 / PI;
    s.pitch_deg = static_cast<float>(std::clamp(pitch, -89.0, 89.0));
    return s;
}

namespace {

std::optional<std::int64_t> parse_digits(std::string_view text) {
    if (text.empty() || text.size() > 18) {
        return std::nullopt;
    }
    std::int64_t v = 0;
    for (const char ch : text) {
        if (ch < '0' || ch > '9') {
            return std::nullopt;
        }
        v = v * 10 + (ch - '0');
    }
    return v;
}

char fold(char c) {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

bool folded_equal(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t k = 0; k < a.size(); ++k) {
        if (fold(a[k]) != fold(b[k])) {
            return false;
        }
    }
    return true;
}

}  // namespace

std::optional<std::size_t> find_marker(std::span<const Marker3D> markers, std::span<const std::size_t> order,
                                       std::string_view v) {
    const auto prefixed = [&v](std::string_view prefix) -> std::optional<std::int64_t> {
        if (v.substr(0, prefix.size()) != prefix) {
            return std::nullopt;
        }
        return parse_digits(v.substr(prefix.size()));
    };
    if (const auto id = prefixed("settlement:")) {
        for (std::size_t i = 0; i < markers.size(); ++i) {
            if (!is_site(markers[i].rec.kind) && markers[i].rec.id == *id) {
                return i;
            }
        }
        return std::nullopt;
    }
    if (const auto id = prefixed("site:")) {
        for (std::size_t i = 0; i < markers.size(); ++i) {
            if (is_site(markers[i].rec.kind) && markers[i].rec.id == *id) {
                return i;
            }
        }
        return std::nullopt;
    }
    if (const auto k = prefixed("rank:")) {
        if (*k >= 1 && static_cast<std::size_t>(*k) <= order.size()) {
            return order[static_cast<std::size_t>(*k - 1)];
        }
        return std::nullopt;
    }
    for (std::size_t i = 0; i < markers.size(); ++i) {
        if (markers[i].rec.name == v) {
            return i;
        }
    }
    for (std::size_t i = 0; i < markers.size(); ++i) {
        if (folded_equal(markers[i].rec.name, v)) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<std::int64_t> parse_year(std::string_view text) {
    return parse_digits(text);
}

std::string markers_status_line(const MarkerLoad& load) {
    switch (load.status) {
        case MarkerStatus::none: return "no history in this world: no markers";
        case MarkerStatus::failed: return "markers: the history could not be read: " + load.code + ": " + load.message;
        case MarkerStatus::loaded: break;
    }
    std::size_t s = 0, c = 0, r = 0, o = 0, l = 0, p = 0, m = 0, t = 0;
    for (const MarkerRecord& rec : load.records) {
        switch (rec.kind) {
            case MarkerKind::settlement:
                ++s;
                c += rec.capital ? 1u : 0u;
                break;
            case MarkerKind::ruin: ++r; break;
            case MarkerKind::old_ruin:
                ++r;
                ++o;
                break;
            case MarkerKind::lair: ++l; break;
            case MarkerKind::outpost: ++p; break;
            case MarkerKind::mine: ++m; break;
            case MarkerKind::tower: ++t; break;
        }
    }
    std::string text = "markers: " + std::to_string(s) + " settlements (" + std::to_string(c) + " capitals), " +
                       std::to_string(r) + " ruins (" + std::to_string(o) + " old), " + std::to_string(l) + " lairs, " +
                       std::to_string(p) + " outposts, " + std::to_string(m) + " mines, " + std::to_string(t) +
                       " towers at year " + std::to_string(load.year) + " of " + std::to_string(load.end_year);
    if (load.year_clamped) {
        text += "\n--year " + std::to_string(load.asked_year) + " is after the history's end: markers at " +
                std::to_string(load.end_year);
    }
    return text;
}

std::string year_refusal(std::string_view v) {
    return "--year " + std::string(v) + ": a whole number from 0";
}

std::string at_marker_miss(std::string_view v, std::int64_t year) {
    return "--at-marker " + std::string(v) + ": no marker of that name or id at year " + std::to_string(year) +
           "; starting at the default view";
}

}  // namespace unmapped::terrain
