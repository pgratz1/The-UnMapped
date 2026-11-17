// The text report, format v2 (spec P0-04 section 7 and amendment 16c,
// P1-00 section 10.3).

#include "worldgen/inspect/report.hpp"

#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "worldgen/db/schema.hpp"
#include "worldgen/inspect/geography.hpp"
#include "worldgen/inspect/render.hpp"

namespace worldgen::inspect {

std::string escape_report_text(std::string_view text) {
    static constexpr char HEX[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        const auto b = static_cast<std::uint8_t>(c);
        if (c == '\\') {
            out += "\\\\";
        } else if (b <= 0x1FU || b == 0x7FU) {
            out += "\\x";
            out += HEX[b >> 4U];
            out += HEX[b & 0x0FU];
        } else {
            out += c;
        }
    }
    return out;
}

namespace {

std::string format_range(const ValueRange& range) {
    if (range.is_signed) {
        return "min=" + db::format_i64(range.min_signed) + " max=" + db::format_i64(range.max_signed);
    }
    return "min=" + db::format_u64(range.min_unsigned) + " max=" + db::format_u64(range.max_unsigned);
}

// One row of table pass (P1-01 9.1) as report reads it, checked as P1-00
// 10.3 "Malformed pass rows" asks: kind and capped in {0, 1}, capped 0 when
// kind is 0, the col inside the elevation grid (w x h), path non-empty with a
// length divisible by 4 and every u32 little-endian index below w h. Returns
// the reason when the row fails, std::nullopt (and p filled) otherwise.
std::optional<std::string> read_pass_row(const std::vector<db::ColumnValue>& row, std::uint32_t w, std::uint32_t h,
                                         std::uint64_t cells, PassRecord& p) {
    constexpr std::size_t COLUMNS = 9;  // id, range_id, kind, capped, col_x, col_y, saddle_dm, col_dm, path
    const std::string id = (!row.empty() && row[0].type == db::ValueType::integer) ? db::format_i64(row[0].integer)
                                                                                   : std::string("?");
    const std::string at = "row " + id + ": ";
    if (row.size() != COLUMNS) {
        return at + "has " + db::format_u64(row.size()) + " columns, expected 9";
    }
    for (std::size_t k = 0; k + 1U < COLUMNS; ++k) {
        if (row[k].type != db::ValueType::integer) {
            return at + "column " + db::format_u64(k + 1U) + " is not an integer";
        }
    }
    if (row[8].type != db::ValueType::blob) {
        return at + "path is not a blob";
    }
    p.kind = row[2].integer;
    p.capped = row[3].integer;
    p.col_x = row[4].integer;
    p.col_y = row[5].integer;
    if (p.kind != 0 && p.kind != 1) {
        return at + "kind " + db::format_i64(p.kind) + " is not 0 or 1";
    }
    if (p.capped != 0 && p.capped != 1) {
        return at + "capped " + db::format_i64(p.capped) + " is not 0 or 1";
    }
    if (p.kind == 0 && p.capped != 0) {
        return at + "a natural gap (kind 0) has capped 1";
    }
    if (p.col_x < 0 || p.col_y < 0 || p.col_x >= std::int64_t{w} || p.col_y >= std::int64_t{h}) {
        return at + "col (" + db::format_i64(p.col_x) + ", " + db::format_i64(p.col_y) +
               ") is outside the elevation grid";
    }
    const std::string& b = row[8].bytes;
    if (b.empty()) {
        return at + "path is empty";
    }
    if (b.size() % 4U != 0U) {
        return at + "path length " + db::format_u64(b.size()) + " is not a multiple of 4";
    }
    p.path.clear();
    p.path.reserve(b.size() / 4U);
    for (std::size_t i = 0; i < b.size(); i += 4U) {
        // u32 little-endian, independent of the host byte order.
        std::uint32_t c = 0;
        for (std::size_t k = 0; k < 4U; ++k) {
            c |= static_cast<std::uint32_t>(static_cast<std::uint8_t>(b[i + k])) << (8U * k);
        }
        if (std::uint64_t{c} >= cells) {
            return at + "path cell " + db::format_u64(c) + " is outside the elevation grid";
        }
        p.path.push_back(c);
    }
    return std::nullopt;
}

}  // namespace

db::Result<std::string> build_report(const db::WorldReader& world) {
    try {
        std::string out;
        out += REPORT_HEADER_V2;
        out += '\n';

        out += "[meta]\n";
        for (const db::MetaEntry& entry : world.meta_entries()) {
            out += escape_report_text(entry.key);
            out += " =";
            if (!entry.value.empty()) {
                out += ' ';
                out += escape_report_text(entry.value);
            }
            out += '\n';
        }

        out += "[tables]\n";
        db::Result<std::vector<db::TableSummary>> tables = world.tables();
        if (!tables.ok()) {
            return tables.error();
        }
        for (const db::TableSummary& t : tables.value()) {
            out += escape_report_text(t.name);
            out += " = ";
            out += db::format_u64(t.row_count);
            out += '\n';
        }

        out += "[layers]\n";
        db::Result<std::vector<db::LayerInfo>> layers = world.layers();
        if (!layers.ok()) {
            return layers.error();
        }
        if (layers.value().empty()) {
            out += "(none)\n";
        }
        // The grids [geography] reads, kept from this pass so that no layer
        // is decoded twice. Layer names are unique (P0-02 5.6).
        constexpr std::size_t GEO_LAYERS = 9;
        std::optional<db::LayerGrid> geo_grids[GEO_LAYERS];
        static constexpr std::string_view GEO_NAMES[GEO_LAYERS] = {
            "elevation", "water", "flow_dir", "flow_accum", "river", "lake", "temperature", "rainfall", "biome"};
        for (const db::LayerInfo& info : layers.value()) {
            db::Result<db::Layer> layer = world.read_layer(info.name);
            if (!layer.ok()) {
                return layer.error();
            }
            out += escape_report_text(info.name);
            out += ' ';
            out += db::element_type_name(info.type);
            out += ' ';
            out += db::format_u64(info.width);
            out += 'x';
            out += db::format_u64(info.height);
            out += " codec=";
            out += db::format_u64(static_cast<std::uint8_t>(info.codec));
            out += " bytes=";
            out += db::format_u64(info.encoded_size);
            out += ' ';
            out += format_range(value_range(layer.value().grid));
            out += '\n';
            for (std::size_t k = 0; k < GEO_LAYERS; ++k) {
                if (info.name == GEO_NAMES[k]) {
                    geo_grids[k] = std::move(layer.value().grid);
                }
            }
        }

        // The rows of table pass (P1-01 9.1), for the pass keys of [geography].
        std::optional<std::vector<PassRecord>> passes;
        for (const db::TableSummary& t : tables.value()) {
            if (t.name != db::PASS_TABLE || !geo_grids[0]) {
                continue;
            }
            db::Result<std::vector<std::vector<db::ColumnValue>>> rows = world.table_rows(db::PASS_TABLE);
            if (!rows.ok()) {
                return rows.error();
            }
            const std::uint64_t cells = geo_grids[0]->cell_count();
            passes.emplace();
            for (const std::vector<db::ColumnValue>& row : rows.value()) {
                PassRecord p;
                if (std::optional<std::string> why = read_pass_row(row, geo_grids[0]->width, geo_grids[0]->height,
                                                                   cells, p)) {
                    return db::Error{db::ErrorCode::corrupt_layer, "corrupt table 'pass': " + *why};
                }
                passes->push_back(std::move(p));
            }
        }

        // Tables climate and biome (schema 3, P1-03 9, P1-04 9): read and
        // checked whenever they exist; a malformed row is a corrupt table.
        std::optional<std::int64_t> wind_toward;
        std::optional<std::vector<BiomeRecord>> biomes;
        for (const db::TableSummary& t : tables.value()) {
            if (t.name == db::CLIMATE_TABLE) {
                db::Result<std::vector<std::vector<db::ColumnValue>>> rows = world.table_rows(db::CLIMATE_TABLE);
                if (!rows.ok()) {
                    return rows.error();
                }
                const ClimateTableResult climate = read_climate_rows(rows.value());
                if (climate.problem) {
                    return db::Error{db::ErrorCode::corrupt_layer, "corrupt table 'climate': " + *climate.problem};
                }
                wind_toward = climate.wind_toward;
            } else if (t.name == db::BIOME_TABLE) {
                db::Result<std::vector<std::vector<db::ColumnValue>>> rows = world.table_rows(db::BIOME_TABLE);
                if (!rows.ok()) {
                    return rows.error();
                }
                BiomeTableResult biome = read_biome_rows(rows.value());
                if (biome.problem) {
                    return db::Error{db::ErrorCode::corrupt_layer, "corrupt table 'biome': " + *biome.problem};
                }
                biomes = std::move(biome.records);
            }
        }

        // [geography] (P1-00 10.3), when the world has an elevation layer.
        if (geo_grids[0]) {
            const auto ptr = [&geo_grids](std::size_t k) { return geo_grids[k] ? &*geo_grids[k] : nullptr; };
            GeographyLayers geo;
            geo.elevation = ptr(0);
            geo.water = ptr(1);
            geo.flow_dir = ptr(2);
            geo.flow_accum = ptr(3);
            geo.river = ptr(4);
            geo.lake = ptr(5);
            geo.temperature = ptr(6);
            geo.rainfall = ptr(7);
            geo.passes = passes ? &*passes : nullptr;
            geo.wind_toward = wind_toward;
            geo.biome = ptr(8);
            geo.biomes = biomes ? &*biomes : nullptr;
            std::string problem;
            std::optional<std::string> section = geography_section(geo, &problem);
            if (!section) {
                return db::Error{db::ErrorCode::corrupt_layer, "cannot report geography: " + problem};
            }
            out += *section;
        }
        return out;
    } catch (const std::bad_alloc&) {
        return db::Error{db::ErrorCode::out_of_memory, "out of memory while building the report"};
    }
}

}  // namespace worldgen::inspect
