// The text report, format v1 (spec P0-04 section 7).

#include "worldgen/inspect/report.hpp"

#include <cstdint>
#include <new>
#include <vector>

#include "worldgen/db/schema.hpp"
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
        out += REPORT_HEADER_V1;
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
        }
        return out;
    } catch (const std::bad_alloc&) {
        return db::Error{db::ErrorCode::out_of_memory, "out of memory while building the report"};
    }
}

}  // namespace worldgen::inspect
