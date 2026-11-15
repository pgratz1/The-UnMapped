// TOML schemas of the shared noise records (P1-01 section 4.1).

#include "worldgen/geo/noise_schema.hpp"

#include <vector>

#include "worldgen/core/error.hpp"

namespace worldgen::geo {

namespace {

data::Field octaves_field() {
    return data::Field::required("octaves", data::Type::integer(1, 10));
}

data::Field gain_field() {
    return data::Field::required("gain", data::Type::fixed("0.0625", "1"));
}

std::int32_t octaves_of(const data::Record& r) {
    const std::int64_t octaves = r.get_int("octaves");
    if (octaves < 1 || octaves > 10) {
        fatal("noise record: octaves outside [1, 10]");
    }
    return static_cast<std::int32_t>(octaves);
}

}  // namespace

data::Schema fbm_map_schema() {
    return data::Schema(std::vector<data::Field>{
        data::Field::required("cycles", data::Type::fixed("0.125", "64")),
        octaves_field(),
        gain_field(),
    });
}

data::Type fbm_map_type() {
    return data::Type::record(fbm_map_schema());
}

data::Schema fbm_m_schema() {
    return data::Schema(std::vector<data::Field>{
        data::Field::required("wavelength_m", data::Type::integer(64, 65536)),
        octaves_field(),
        gain_field(),
    });
}

data::Type fbm_m_type() {
    return data::Type::record(fbm_m_schema());
}

FbmMap fbm_map_from(const data::Value& value) {
    const data::Record& r = value.as_record();
    FbmMap out;
    out.cycles = r.get_fx("cycles");
    out.octaves = octaves_of(r);
    out.gain = r.get_fx("gain");
    return out;
}

FbmM fbm_m_from(const data::Value& value) {
    const data::Record& r = value.as_record();
    FbmM out;
    out.wavelength_m = r.get_int("wavelength_m");
    out.octaves = octaves_of(r);
    out.gain = r.get_fx("gain");
    return out;
}

}  // namespace worldgen::geo
