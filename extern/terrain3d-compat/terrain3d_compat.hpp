// Compatibility header for Terrain3D v1.1.0-stable built against the
// project's godot-cpp 10.0.0-stable (API 4.7). Not part of the vendored
// Terrain3D source: it is force-included into every Terrain3D translation
// unit by the block TERRAIN3D CONFIGURATION in extern/CMakeLists.txt
// (-include on GCC and Clang, /FI on MSVC). See extern/README.md, "Terrain3D".
//
// Upstream v1.1.0 is built against a godot-cpp master commit of 2026-03
// (60b5a419). godot-cpp 10.0.0 differs from it in three ways that this
// header bridges, each with the meaning the older godot-cpp had:
//
//  1. The macros Math_PI and Math_TAU are gone; the same double constants are
//     godot::Math::PI and godot::Math::TAU (core/math_defs.hpp).
//  2. is_power_of_2 and closest_power_of_2 were free functions of namespace
//     godot (closest_power_of_2 taking unsigned int); they now exist only in
//     godot::Math, where closest_power_of_2 is overloaded for uint32_t and
//     uint64_t, so a call with an enum argument would be ambiguous. The
//     forwarding functions below restore the old signatures; the algorithm
//     is the same (next or previous power of two, whichever is nearer).
//  3. Vector2i's conversion to String is now explicit. `"text" + Vector2i`
//     used the implicit conversion and String's operator+(const char *,
//     const String &); the operator below does the same explicitly. A
//     copy-initialisation `String s = v;` cannot be bridged by a header and
//     is edited in the vendored source (terrain3d-godot-cpp-10.patch).
//
// Nothing here changes behaviour: every name resolves to the code the older
// godot-cpp ran.

#pragma once

#include <cstdint>

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/core/math_funcs_binary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#ifndef Math_PI
#define Math_PI (godot::Math::PI)
#endif
#ifndef Math_TAU
#define Math_TAU (godot::Math::TAU)
#endif

namespace godot {

template <typename T>
constexpr bool is_power_of_2(const T x) {
	return Math::is_power_of_2(x);
}

inline unsigned int closest_power_of_2(unsigned int x) {
	return static_cast<unsigned int>(Math::closest_power_of_2(static_cast<uint32_t>(x)));
}

inline String operator+(const char *p_a, const Vector2i &p_b) {
	return String(p_a) + String(p_b);
}

} // namespace godot
