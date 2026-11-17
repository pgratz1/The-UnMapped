// The text report, format v2 (spec P0-04 section 7 and amendment 16c,
// P1-00 section 10.3).

#include "worldgen/inspect/report.hpp"

#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
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
        std::optional<db::LayerGrid> geo_grids[6];
        static constexpr std::string_view GEO_NAMES[6] = {"elevation", "water", "flow_dir",
                                                          "flow_accum", "river", "lake"};
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
            for (std::size_t k = 0; k < 6U; ++k) {
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
                const auto bad = [&]() {
                    return db::Error{db::ErrorCode::corrupt_layer,
                                     "cannot report geography: table 'pass' has a row that is not a pass site"};
                };
                if (row.size() != 9U || row[8].type != db::ValueType::blob || row[8].bytes.empty() ||
                    row[8].bytes.size() % 4U != 0U) {
                    return bad();
                }
                for (std::size_t k = 0; k < 8U; ++k) {
                    if (row[k].type != db::ValueType::integer) {
                        return bad();
                    }
                }
                PassRecord p;
                p.kind = row[2].integer;
                p.capped = row[3].integer;
                p.col_x = row[4].integer;
                p.col_y = row[5].integer;
                const std::string& b = row[8].bytes;
                for (std::size_t i = 0; i < b.size(); i += 4U) {
                    std::uint32_t c = 0;
                    for (std::size_t k = 0; k < 4U; ++k) {
                        c |= static_cast<std::uint32_t>(static_cast<std::uint8_t>(b[i + k])) << (8U * k);
                    }
                    if (c >= cells) {
                        return bad();
                    }
                    p.path.push_back(c);
                }
                if (p.col_x < 0 || p.col_y < 0 || p.col_x >= geo_grids[0]->width || p.col_y >= geo_grids[0]->height) {
                    return bad();
                }
                passes->push_back(std::move(p));
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
            geo.passes = passes ? &*passes : nullptr;
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
