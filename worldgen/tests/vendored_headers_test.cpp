// Build scaffold check: the header-only vendored libraries compile under the
// project's warning flags on every compiler and do basic work.

#include <doctest/doctest.h>
#include <stb_image_write.h>

#include <cstddef>
#include <cstdint>
#include <toml.hpp>
#include <vector>

TEST_CASE("toml++ parses a document") {
    const toml::table table = toml::parse("[race]\nname = \"dwarf\"\nlifespan = 250\n");
    CHECK(table["race"]["name"].value_or(std::string_view{}) == "dwarf");
    CHECK(table["race"]["lifespan"].value_or(std::int64_t{0}) == 250);
}

namespace {
void append_bytes(void* context, void* data, int size) {
    auto* out = static_cast<std::vector<unsigned char>*>(context);
    const auto* bytes = static_cast<const unsigned char*>(data);
    out->insert(out->end(), bytes, bytes + size);
}
}  // namespace

TEST_CASE("stb_image_write encodes a PNG in memory") {
    const unsigned char pixels[4] = {0, 64, 128, 255};
    std::vector<unsigned char> png;
    REQUIRE(stbi_write_png_to_func(append_bytes, &png, 2, 2, 1, pixels, 2) != 0);
    REQUIRE(png.size() > std::size_t{8});
    CHECK(png[1] == 'P');
    CHECK(png[2] == 'N');
    CHECK(png[3] == 'G');
}
