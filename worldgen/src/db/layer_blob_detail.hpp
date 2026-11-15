#pragma once

// Private part of the layer blob codec (spec P0-02 section 7).

#include <cstdint>
#include <span>

#include "worldgen/db/layer_blob.hpp"

namespace worldgen::db::detail {

// parse_blob_header for a blob of which only the first bytes are at hand:
// header holds at least the 24 header bytes, blob_size is the length of the
// whole blob (used for the payload length check). For the layer list, which
// reads only the header (P0-02 9).
[[nodiscard]] BlobHeaderParse parse_blob_header_fields(std::span<const std::uint8_t> header,
                                                       std::uint64_t blob_size) noexcept;

}  // namespace worldgen::db::detail
