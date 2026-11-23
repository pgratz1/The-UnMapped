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
#include "worldgen/inspect/chronology.hpp"
#include "worldgen/inspect/geography.hpp"
#include "worldgen/inspect/history.hpp"
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
        constexpr std::size_t GEO_LAYERS = 17;
        constexpr std::size_t RESOURCES_FIRST = 9;  // the seven layers of P1-05 3 follow biome
        constexpr std::size_t REGION_INDEX = 16;    // the layer of P1-06 3.1
        std::optional<db::LayerGrid> geo_grids[GEO_LAYERS];
        static constexpr std::string_view GEO_NAMES[GEO_LAYERS] = {
            "elevation", "water",     "flow_dir", "flow_accum",  "river",  "lake",         "temperature", "rainfall",
            "biome",     "fertility", "ore",      "timber",      "fresh_water", "forage", "habitability", "walk_area",
            "region"};
        static_assert(REGION_INDEX == RESOURCES_FIRST + RESOURCE_LAYER_NAMES.size());
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

        // Tables climate and biome (schema 4, P1-03 9, P1-04 9): read and
        // checked whenever they exist; a malformed row is a corrupt table.
        std::optional<std::int64_t> wind_toward;
        std::optional<std::int64_t> temp_offset_dc;
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
                temp_offset_dc = climate.temp_offset_dc;
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

        // The biome layer (P1-04 9): u8, the size of elevation, every value
        // with a row of table biome (an absent table counts as empty).
        if (geo_grids[8]) {
            const std::vector<BiomeRecord> none;
            if (std::optional<std::string> why = biome_layer_problem(
                    *geo_grids[8], geo_grids[0] ? &*geo_grids[0] : nullptr, biomes ? *biomes : none)) {
                return db::Error{db::ErrorCode::corrupt_layer, "corrupt layer 'biome': " + *why};
            }
            if (!biomes) {
                biomes.emplace();
            }
        }

        // The resources layers (P1-05 9): each u8 (walk_area u16) and the size
        // of elevation, whenever it exists.
        for (std::size_t k = 0; k < RESOURCE_LAYER_NAMES.size(); ++k) {
            const std::optional<db::LayerGrid>& g = geo_grids[RESOURCES_FIRST + k];
            if (!g) {
                continue;
            }
            if (std::optional<std::string> why =
                    resource_layer_problem(*g, RESOURCE_LAYER_NAMES[k], geo_grids[0] ? &*geo_grids[0] : nullptr)) {
                return db::Error{db::ErrorCode::corrupt_layer,
                                 "corrupt layer '" + std::string(RESOURCE_LAYER_NAMES[k]) + "': " + *why};
            }
        }

        // [geography] (P1-00 10.3), when the world has an elevation layer;
        // appended after the region checks below.
        std::optional<std::string> geography;
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
            geo.temp_offset_dc = temp_offset_dc;
            geo.biome = ptr(8);
            geo.biomes = biomes ? &*biomes : nullptr;
            for (std::size_t k = 0; k < geo.resources.size(); ++k) {
                geo.resources[k] = ptr(RESOURCES_FIRST + k);
            }
            std::string problem;
            std::optional<std::string> section = geography_section(geo, &problem);
            if (!section) {
                return db::Error{db::ErrorCode::corrupt_layer, "cannot report geography: " + problem};
            }
            geography = std::move(*section);
        }

        // The region checks (P1-06 9), after every check of the earlier
        // [geography] groups. The region layer (P1-06 9): u16 and the size of elevation, whenever
        // it exists.
        if (const std::optional<db::LayerGrid>& g = geo_grids[REGION_INDEX]) {
            if (std::optional<std::string> why = region_layer_problem(*g, geo_grids[0] ? &*geo_grids[0] : nullptr)) {
                return db::Error{db::ErrorCode::corrupt_layer, "corrupt layer 'region': " + *why};
            }
        }

        // Tables region and region_border (schema 5, P1-06 9): read and
        // checked whenever they exist, region first (the border rows name
        // its ids).
        std::optional<std::vector<RegionRecord>> regions;
        std::optional<std::vector<RegionBorderRecord>> region_borders;
        for (const db::TableSummary& t : tables.value()) {
            if (t.name != db::REGION_TABLE) {
                continue;
            }
            db::Result<std::vector<std::vector<db::ColumnValue>>> rows = world.table_rows(db::REGION_TABLE);
            if (!rows.ok()) {
                return rows.error();
            }
            RegionTableResult region = read_region_rows(rows.value());
            if (region.problem) {
                return db::Error{db::ErrorCode::corrupt_layer, "corrupt table 'region': " + *region.problem};
            }
            regions = std::move(region.records);
        }
        for (const db::TableSummary& t : tables.value()) {
            if (t.name != db::REGION_BORDER_TABLE) {
                continue;
            }
            db::Result<std::vector<std::vector<db::ColumnValue>>> rows = world.table_rows(db::REGION_BORDER_TABLE);
            if (!rows.ok()) {
                return rows.error();
            }
            const std::vector<RegionRecord> none;
            RegionBorderTableResult border = read_region_border_rows(rows.value(), regions ? *regions : none);
            if (border.problem) {
                return db::Error{db::ErrorCode::corrupt_layer, "corrupt table 'region_border': " + *border.problem};
            }
            region_borders = std::move(border.records);
        }

        // The region keys close [geography] when the layer and both tables
        // exist (P1-06 9).
        if (geography && geo_grids[REGION_INDEX] && regions && region_borders) {
            *geography += region_lines(*regions, *region_borders);
        }
        // Tables race and age (schema 6, P3-03 9.1): checked after every
        // [geography] check whenever either has rows; [chronology] follows
        // [geography] (or [layers]) when age has a row.
        std::vector<std::vector<db::ColumnValue>> race_rows;
        std::vector<std::vector<db::ColumnValue>> age_rows;
        std::uint64_t god_rows = 0;
        for (const db::TableSummary& t : tables.value()) {
            if (t.name == db::RACE_TABLE || t.name == db::AGE_TABLE) {
                db::Result<std::vector<std::vector<db::ColumnValue>>> rows = world.table_rows(t.name);
                if (!rows.ok()) {
                    return rows.error();
                }
                (t.name == db::RACE_TABLE ? race_rows : age_rows) = std::move(rows.value());
            } else if (t.name == db::GOD_TABLE) {
                god_rows = t.row_count;
            }
        }
        std::int64_t span = 0;
        if (const std::optional<std::string> text = world.meta(db::META_PARAM_HISTORY_SPAN)) {
            if (const std::optional<std::uint64_t> v = db::parse_canonical_u64(*text)) {
                span = static_cast<std::int64_t>(*v);  // validated by the reader: 2000 ... 12000
            }
        }
        ChronologySectionResult chronology = chronology_section(race_rows, age_rows, god_rows, span);
        if (chronology.problem) {
            return db::Error{db::ErrorCode::corrupt_layer, *chronology.problem};
        }
        // The history tables (schema 7, P3-04 11.1): checked after the
        // [chronology] checks when phase or event has rows; [history]
        // follows [chronology] when phase has a row.
        std::int64_t end_year = 0;
        if (const std::optional<std::string> text = world.meta(db::META_HISTORY_END_YEAR)) {
            if (const std::optional<std::uint64_t> v = db::parse_canonical_u64(*text)) {
                end_year = static_cast<std::int64_t>(*v);  // validated by the reader: = param.history_span
            }
        }
        db::Result<HistorySectionResult> history = history_section(world, end_year);
        if (!history.ok()) {
            return history.error();
        }
        if (history.value().problem) {
            return db::Error{db::ErrorCode::corrupt_layer, *history.value().problem};
        }
        if (geography) {
            out += *geography;
        }
        out += chronology.section;
        out += history.value().section;
        return out;
    } catch (const std::bad_alloc&) {
        return db::Error{db::ErrorCode::out_of_memory, "out of memory while building the report"};
    }
}

}  // namespace worldgen::inspect
