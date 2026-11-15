// Spec P0-01 section 13.8: golden vectors from the independent reference.
//
// Reads worldgen/tests/golden/core_vectors.txt (format in its header and in
// worldgen/tests/reference/README.md) and checks every vector line against
// the C++ API. For each line the test parses the arguments strictly, makes
// the call, formats the actual result as tokens in the file's canonical
// format (FATAL when worldgen::FatalError is raised, ERROR for an error
// value) and requires the token list to equal the expected one.
//
// An unknown kind, a malformed token, a wrong argument count or a blank line
// fails the test: no vector is skipped silently.

#include <charconv>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "core_test_util.hpp"

#ifndef UNMAPPED_CORE_VECTORS_PATH
#error "UNMAPPED_CORE_VECTORS_PATH must be defined by worldgen/tests/CMakeLists.txt"
#endif

using namespace worldgen;

namespace {

// Number of vector lines in core_vectors.txt. Update it deliberately when
// the reference regenerates the file with a different vector set.
constexpr std::size_t EXPECTED_VECTOR_COUNT = 1458;

struct ParseError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

using Tokens = std::vector<std::string>;

// --- Token parsing (strict) ---------------------------------------------------

bool is_lower_hex_digit(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }
bool is_digit(char c) { return c >= '0' && c <= '9'; }

std::uint64_t parse_hex(std::string_view t) {
    if (t.size() < 3 || t.size() > 18 || t[0] != '0' || t[1] != 'x') {
        throw ParseError("bad 0x token '" + std::string(t) + "'");
    }
    for (std::size_t i = 2; i < t.size(); ++i) {
        if (!is_lower_hex_digit(t[i])) {
            throw ParseError("bad 0x token '" + std::string(t) + "'");
        }
    }
    std::uint64_t v = 0;
    const auto r = std::from_chars(t.data() + 2, t.data() + t.size(), v, 16);
    if (r.ec != std::errc{} || r.ptr != t.data() + t.size()) {
        throw ParseError("bad 0x token '" + std::string(t) + "'");
    }
    return v;
}

std::int64_t parse_dec(std::string_view t) {
    const std::size_t start = (!t.empty() && t[0] == '-') ? 1 : 0;
    if (t.size() == start) {
        throw ParseError("bad decimal token '" + std::string(t) + "'");
    }
    for (std::size_t i = start; i < t.size(); ++i) {
        if (!is_digit(t[i])) {
            throw ParseError("bad decimal token '" + std::string(t) + "'");
        }
    }
    std::int64_t v = 0;
    const auto r = std::from_chars(t.data(), t.data() + t.size(), v, 10);
    if (r.ec != std::errc{} || r.ptr != t.data() + t.size()) {
        throw ParseError("bad decimal token '" + std::string(t) + "'");
    }
    return v;
}

std::uint64_t parse_count(std::string_view t) {
    const std::int64_t v = parse_dec(t);
    if (v < 0) {
        throw ParseError("negative count '" + std::string(t) + "'");
    }
    return static_cast<std::uint64_t>(v);
}

std::vector<std::string_view> list_items(std::string_view t) {
    if (t.size() < 2 || t.front() != '[' || t.back() != ']') {
        throw ParseError("bad list token '" + std::string(t) + "'");
    }
    std::vector<std::string_view> items;
    const std::string_view body = t.substr(1, t.size() - 2);
    if (body.empty()) {
        return items;
    }
    std::size_t pos = 0;
    while (true) {
        const std::size_t comma = body.find(',', pos);
        const std::string_view item = body.substr(pos, comma == std::string_view::npos ? body.npos : comma - pos);
        if (item.empty()) {
            throw ParseError("empty list item in '" + std::string(t) + "'");
        }
        items.push_back(item);
        if (comma == std::string_view::npos) {
            break;
        }
        pos = comma + 1;
    }
    return items;
}

std::vector<std::uint64_t> parse_hex_list(std::string_view t) {
    std::vector<std::uint64_t> out;
    for (const std::string_view item : list_items(t)) {
        out.push_back(parse_hex(item));
    }
    return out;
}

int hex_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    throw ParseError("bad hex digit in string escape");
}

std::string parse_bytes(std::string_view t) {
    if (t.size() < 2 || t.front() != '"' || t.back() != '"') {
        throw ParseError("bad string token '" + std::string(t) + "'");
    }
    std::string out;
    for (std::size_t i = 1; i + 1 < t.size(); ++i) {
        const auto c = static_cast<unsigned char>(t[i]);
        if (c == '\\') {
            if (i + 4 >= t.size() || t[i + 1] != 'x') {
                throw ParseError("bad escape in '" + std::string(t) + "'");
            }
            const int v = hex_value(t[i + 2]) * 16 + hex_value(t[i + 3]);
            out.push_back(static_cast<char>(static_cast<unsigned char>(v)));
            i += 3;
        } else if (c >= 0x21 && c <= 0x7e && c != '"') {
            out.push_back(static_cast<char>(c));
        } else {
            throw ParseError("unescaped byte in '" + std::string(t) + "'");
        }
    }
    return out;
}

// --- Token formatting (canonical, as the reference writes it) ---------------

std::string hx(std::uint64_t v) {
    std::array<char, 24> buf{};
    const auto r = std::to_chars(buf.data(), buf.data() + buf.size(), v, 16);
    return "0x" + std::string(buf.data(), r.ptr);
}

std::string dc(std::int64_t v) {
    std::array<char, 24> buf{};
    const auto r = std::to_chars(buf.data(), buf.data() + buf.size(), v, 10);
    return std::string(buf.data(), r.ptr);
}

std::string dcu(std::uint64_t v) {
    std::array<char, 24> buf{};
    const auto r = std::to_chars(buf.data(), buf.data() + buf.size(), v, 10);
    return std::string(buf.data(), r.ptr);
}

std::string bl(bool b) { return b ? "true" : "false"; }

std::string str_token(std::string_view bytes) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string out = "\"";
    for (const char ch : bytes) {
        const auto c = static_cast<unsigned char>(ch);
        if (c >= 0x21 && c <= 0x7e && c != '"' && c != '\\') {
            out.push_back(ch);
        } else {
            out += "\\x";
            out.push_back(digits[c >> 4]);
            out.push_back(digits[c & 0x0F]);
        }
    }
    out.push_back('"');
    return out;
}

std::string join(const Tokens& t) {
    std::string s;
    for (std::size_t i = 0; i < t.size(); ++i) {
        if (i > 0) {
            s += ' ';
        }
        s += t[i];
    }
    return s;
}

// --- Vector evaluation ---------------------------------------------------------

struct Vector {
    std::string kind;
    Tokens args;
    Tokens expected;
};

void need_args(const Vector& v, std::size_t n) {
    if (v.args.size() != n) {
        throw ParseError("kind " + v.kind + " takes " + std::to_string(n) + " arguments, line has " +
                         std::to_string(v.args.size()));
    }
}

std::array<std::uint64_t, 4> state_args(const Vector& v) {
    return {parse_hex(v.args[0]), parse_hex(v.args[1]), parse_hex(v.args[2]), parse_hex(v.args[3])};
}

Rng rng_from(const std::array<std::uint64_t, 4>& s) { return Rng::from_state(s[0], s[1], s[2], s[3]); }

// Number of next() calls between the start state s and the generator's state,
// found by stepping a twin; "?" if not reached within a bound.
std::string draws_token(const Rng& used, const std::array<std::uint64_t, 4>& s) {
    Rng twin = rng_from(s);
    for (std::uint64_t k = 0; k <= 100000; ++k) {
        if (twin.state() == used.state()) {
            return dcu(k);
        }
        worldgen_test::advance(twin, 1);
    }
    return "?";
}

std::uint64_t tag_by_name(const std::string& name) {
    if (name == "TAG_STREAM") return TAG_STREAM;
    if (name == "TAG_SUB") return TAG_SUB;
    if (name == "TAG_CELL2") return TAG_CELL2;
    if (name == "TAG_CELL3") return TAG_CELL3;
    if (name == "TAG_PHRASE") return TAG_PHRASE;
    throw ParseError("unknown tag name '" + name + "'");
}

// Evaluates one vector and returns the actual result tokens. Arguments are
// parsed before any library call, so a parse problem is never mistaken for
// a FatalError.
Tokens evaluate(const Vector& v) {
    const std::string& k = v.kind;

    // Fx unary, raw -> raw.
    if (k == "fx.from_int") {
        need_args(v, 1);
        const std::int64_t n = parse_dec(v.args[0]);
        return {dc(Fx::from_int(n).raw())};
    }
    if (k == "fx.from_raw") {
        need_args(v, 1);
        const std::int64_t r = parse_dec(v.args[0]);
        return {dc(Fx::from_raw(r).raw())};
    }
    if (k == "fx.from_ratio") {
        need_args(v, 2);
        const std::int64_t a = parse_dec(v.args[0]);
        const std::int64_t b = parse_dec(v.args[1]);
        return {dc(Fx::from_ratio(a, b).raw())};
    }
    if (k == "fx.floor_int" || k == "fx.ceil_int" || k == "fx.round_int" || k == "fx.trunc_int" ||
        k == "fx.neg" || k == "fx.abs" || k == "fx.fade" || k == "fx.sqrt") {
        need_args(v, 1);
        const std::int64_t r = parse_dec(v.args[0]);
        const Fx a = Fx::from_raw(r);
        if (k == "fx.floor_int") return {dc(a.floor_int())};
        if (k == "fx.ceil_int") return {dc(a.ceil_int())};
        if (k == "fx.round_int") return {dc(a.round_int())};
        if (k == "fx.trunc_int") return {dc(a.trunc_int())};
        if (k == "fx.neg") return {dc((-a).raw())};
        if (k == "fx.abs") return {dc(abs(a).raw())};
        if (k == "fx.fade") return {dc(fade(a).raw())};
        return {dc(sqrt(a).raw())};
    }
    if (k == "fx.add" || k == "fx.sub" || k == "fx.sat_add" || k == "fx.sat_sub" || k == "fx.mul" ||
        k == "fx.div" || k == "fx.hypot") {
        need_args(v, 2);
        const std::int64_t ar = parse_dec(v.args[0]);
        const std::int64_t br = parse_dec(v.args[1]);
        const Fx a = Fx::from_raw(ar);
        const Fx b = Fx::from_raw(br);
        if (k == "fx.add") return {dc((a + b).raw())};
        if (k == "fx.sub") return {dc((a - b).raw())};
        if (k == "fx.sat_add") return {dc(sat_add(a, b).raw())};
        if (k == "fx.sat_sub") return {dc(sat_sub(a, b).raw())};
        if (k == "fx.mul") {
            const Fx m = mul(a, b);
            if (!(a * b == m)) return {"operator* differs from mul"};
            return {dc(m.raw())};
        }
        if (k == "fx.div") {
            const Fx d = div(a, b);
            if (!(a / b == d)) return {"operator/ differs from div"};
            return {dc(d.raw())};
        }
        return {dc(hypot(a, b).raw())};
    }
    if (k == "fx.mul_int" || k == "fx.div_int") {
        need_args(v, 2);
        const std::int64_t ar = parse_dec(v.args[0]);
        const std::int64_t n = parse_dec(v.args[1]);
        const Fx a = Fx::from_raw(ar);
        return {dc((k == "fx.mul_int" ? mul_int(a, n) : div_int(a, n)).raw())};
    }
    if (k == "fx.clamp" || k == "fx.lerp") {
        need_args(v, 3);
        const std::int64_t r0 = parse_dec(v.args[0]);
        const std::int64_t r1 = parse_dec(v.args[1]);
        const std::int64_t r2 = parse_dec(v.args[2]);
        const Fx a = Fx::from_raw(r0);
        const Fx b = Fx::from_raw(r1);
        const Fx c = Fx::from_raw(r2);
        return {dc((k == "fx.clamp" ? clamp(a, b, c) : lerp(a, b, c)).raw())};
    }
    if (k == "fx.parse") {
        need_args(v, 1);
        const std::string text = parse_bytes(v.args[0]);
        const DecimalParse p = parse_decimal(text);
        if (!p.ok()) return {"ERROR"};
        return {dc(p.value.raw())};
    }
    if (k == "fx.canonical") {
        need_args(v, 1);
        const std::string text = parse_bytes(v.args[0]);
        const std::optional<std::string> c = canonical_decimal(text);
        if (!c) return {"ERROR"};
        return {str_token(*c)};
    }
    if (k == "fx.to_decimal") {
        need_args(v, 2);
        const std::int64_t r = parse_dec(v.args[0]);
        const std::int64_t d = parse_dec(v.args[1]);
        if (d < -1000 || d > 1000) throw ParseError("digit count out of test range");
        const Fx a = Fx::from_raw(r);
        const std::string member = a.to_decimal(static_cast<int>(d));
        const std::string free_fn = to_decimal(a, static_cast<int>(d));
        if (member != free_fn) return {"member and free to_decimal differ"};
        return {str_token(free_fn)};
    }

    // Plain integer helpers.
    if (k == "int.mul_div") {
        need_args(v, 3);
        const std::int64_t a = parse_dec(v.args[0]);
        const std::int64_t b = parse_dec(v.args[1]);
        const std::int64_t c = parse_dec(v.args[2]);
        return {dc(mul_div(a, b, c))};
    }
    if (k == "int.checked_add" || k == "int.checked_sub" || k == "int.checked_mul" || k == "int.floor_div" ||
        k == "int.floor_mod") {
        need_args(v, 2);
        const std::int64_t a = parse_dec(v.args[0]);
        const std::int64_t b = parse_dec(v.args[1]);
        if (k == "int.checked_add") return {dc(checked_add(a, b))};
        if (k == "int.checked_sub") return {dc(checked_sub(a, b))};
        if (k == "int.checked_mul") return {dc(checked_mul(a, b))};
        if (k == "int.floor_div") return {dc(floor_div(a, b))};
        return {dc(floor_mod(a, b))};
    }

    // Wide arithmetic and integer square roots.
    if (k == "wide.mul") {
        need_args(v, 2);
        const std::uint64_t a = parse_hex(v.args[0]);
        const std::uint64_t b = parse_hex(v.args[1]);
        const U128 p = mul_u64_wide(a, b);
        return {hx(p.hi), hx(p.lo)};
    }
    if (k == "wide.divmod") {
        need_args(v, 3);
        const std::uint64_t hi = parse_hex(v.args[0]);
        const std::uint64_t lo = parse_hex(v.args[1]);
        const std::uint64_t d = parse_hex(v.args[2]);
        const U128DivMod r = divmod_u128_u64(U128{hi, lo}, d);
        return {hx(r.quotient.hi), hx(r.quotient.lo), hx(r.remainder)};
    }
    if (k == "isqrt.u64") {
        need_args(v, 1);
        const std::uint64_t n = parse_hex(v.args[0]);
        return {hx(isqrt_u64(n))};
    }
    if (k == "isqrt.u128") {
        need_args(v, 2);
        const std::uint64_t hi = parse_hex(v.args[0]);
        const std::uint64_t lo = parse_hex(v.args[1]);
        return {hx(isqrt_u128(U128{hi, lo}))};
    }

    // SplitMix64 and xoshiro256**.
    if (k == "splitmix64") {
        need_args(v, 2);
        std::uint64_t x = parse_hex(v.args[0]);
        const std::uint64_t n = parse_count(v.args[1]);
        Tokens out;
        for (std::uint64_t i = 0; i < n; ++i) {
            out.push_back(hx(splitmix64_next(x)));
        }
        return out;
    }
    if (k == "rng.seed") {
        need_args(v, 1);
        const std::uint64_t key = parse_hex(v.args[0]);
        const Rng rng(key);
        const auto s = rng.state();
        return {hx(s[0]), hx(s[1]), hx(s[2]), hx(s[3])};
    }
    if (k == "rng.next" || k == "rng.state_after") {
        need_args(v, 5);
        const auto s = state_args(v);
        const std::uint64_t n = parse_count(v.args[4]);
        Rng rng = rng_from(s);
        Tokens out;
        for (std::uint64_t i = 0; i < n; ++i) {
            const std::uint64_t x = rng.next();
            if (k == "rng.next") {
                out.push_back(hx(x));
            }
        }
        if (k == "rng.state_after") {
            const auto t = rng.state();
            return {hx(t[0]), hx(t[1]), hx(t[2]), hx(t[3])};
        }
        return out;
    }
    if (k == "rng.stream") {
        need_args(v, 3);
        const std::uint64_t seed = parse_hex(v.args[0]);
        const std::string name = parse_bytes(v.args[1]);
        const std::uint64_t n = parse_count(v.args[2]);
        Rng rng(stream_key(seed, name));
        Tokens out;
        for (std::uint64_t i = 0; i < n; ++i) {
            out.push_back(hx(rng.next()));
        }
        return out;
    }
    if (k == "rng.next_u32" || k == "rng.uniform_fx01") {
        need_args(v, 4);
        const auto s = state_args(v);
        Rng rng = rng_from(s);
        const std::int64_t r = k == "rng.next_u32" ? static_cast<std::int64_t>(rng.next_u32()) : rng.uniform_fx01().raw();
        return {dc(r), draws_token(rng, s)};
    }
    if (k == "rng.uniform_u64") {
        need_args(v, 5);
        const auto s = state_args(v);
        const std::uint64_t bound = parse_hex(v.args[4]);
        Rng rng = rng_from(s);
        const std::uint64_t r = rng.uniform_u64(bound);
        return {hx(r), draws_token(rng, s)};
    }
    if (k == "rng.uniform_i64" || k == "rng.uniform_fx") {
        need_args(v, 6);
        const auto s = state_args(v);
        const std::int64_t lo = parse_dec(v.args[4]);
        const std::int64_t hi = parse_dec(v.args[5]);
        Rng rng = rng_from(s);
        std::int64_t r = 0;
        if (k == "rng.uniform_i64") {
            r = rng.uniform_i64(lo, hi);
        } else {
            const Fx flo = Fx::from_raw(lo);
            const Fx fhi = Fx::from_raw(hi);
            r = rng.uniform_fx(flo, fhi).raw();
        }
        return {dc(r), draws_token(rng, s)};
    }
    if (k == "rng.chance") {
        need_args(v, 5);
        const auto s = state_args(v);
        const std::int64_t p = parse_dec(v.args[4]);
        Rng rng = rng_from(s);
        const Fx fp = Fx::from_raw(p);
        const bool r = rng.chance(fp);
        return {bl(r), draws_token(rng, s)};
    }
    if (k == "rng.chance_ratio") {
        need_args(v, 6);
        const auto s = state_args(v);
        const std::uint64_t num = parse_hex(v.args[4]);
        const std::uint64_t den = parse_hex(v.args[5]);
        Rng rng = rng_from(s);
        const bool r = rng.chance_ratio(num, den);
        return {bl(r), draws_token(rng, s)};
    }
    if (k == "rng.weighted_index") {
        need_args(v, 5);
        const auto s = state_args(v);
        const std::vector<std::uint64_t> w = parse_hex_list(v.args[4]);
        Rng rng = rng_from(s);
        const std::size_t r = rng.weighted_index(w);
        return {dcu(static_cast<std::uint64_t>(r)), draws_token(rng, s)};
    }
    if (k == "rng.shuffle") {
        need_args(v, 5);
        const auto s = state_args(v);
        const std::uint64_t n = parse_count(v.args[4]);
        if (n > 1000000) throw ParseError("shuffle size out of test range");
        std::vector<std::uint64_t> a(static_cast<std::size_t>(n));
        for (std::size_t i = 0; i < a.size(); ++i) {
            a[i] = static_cast<std::uint64_t>(i);
        }
        Rng rng = rng_from(s);
        rng.shuffle(a);
        std::string list = "[";
        for (std::size_t i = 0; i < a.size(); ++i) {
            if (i > 0) list += ',';
            list += dcu(a[i]);
        }
        list += ']';
        return {list, draws_token(rng, s)};
    }

    // Hashing and streams.
    if (k == "hash.tag") {
        need_args(v, 1);
        return {hx(tag_by_name(v.args[0]))};
    }
    if (k == "hash.mix64") {
        need_args(v, 1);
        const std::uint64_t z = parse_hex(v.args[0]);
        return {hx(mix64(z))};
    }
    if (k == "hash.absorb") {
        need_args(v, 2);
        const std::uint64_t h = parse_hex(v.args[0]);
        const std::uint64_t w = parse_hex(v.args[1]);
        return {hx(absorb(h, w))};
    }
    if (k == "hash.words") {
        need_args(v, 2);
        const std::uint64_t tag = parse_hex(v.args[0]);
        const std::vector<std::uint64_t> w = parse_hex_list(v.args[1]);
        return {hx(hash_words(tag, std::span<const std::uint64_t>(w)))};
    }
    if (k == "hash.pack") {
        need_args(v, 1);
        const std::string bytes = parse_bytes(v.args[0]);
        const std::vector<std::uint64_t> words = encode_bytes(std::string_view(bytes));
        std::string list = "[";
        for (std::size_t i = 0; i < words.size(); ++i) {
            if (i > 0) list += ',';
            list += hx(words[i]);
        }
        list += ']';
        return {list};
    }
    if (k == "hash.stream_key") {
        need_args(v, 2);
        const std::uint64_t seed = parse_hex(v.args[0]);
        const std::string name = parse_bytes(v.args[1]);
        return {hx(stream_key(seed, name))};
    }
    if (k == "hash.substream") {
        need_args(v, 2);
        const std::uint64_t key = parse_hex(v.args[0]);
        const std::uint64_t index = parse_hex(v.args[1]);
        return {hx(substream(key, index))};
    }
    if (k == "hash.at") {
        need_args(v, 3);
        const std::uint64_t key = parse_hex(v.args[0]);
        const std::int64_t x = parse_dec(v.args[1]);
        const std::int64_t y = parse_dec(v.args[2]);
        return {hx(hash_at(key, x, y))};
    }
    if (k == "hash.at3") {
        need_args(v, 4);
        const std::uint64_t key = parse_hex(v.args[0]);
        const std::int64_t x = parse_dec(v.args[1]);
        const std::int64_t y = parse_dec(v.args[2]);
        const std::int64_t z = parse_dec(v.args[3]);
        return {hx(hash_at3(key, x, y, z))};
    }
    if (k == "hash.phrase_seed") {
        need_args(v, 1);
        const std::string bytes = parse_bytes(v.args[0]);
        const std::uint64_t a = phrase_seed(std::string_view(bytes));
        std::vector<std::uint8_t> raw_bytes;
        for (const char c : bytes) {
            raw_bytes.push_back(static_cast<std::uint8_t>(static_cast<unsigned char>(c)));
        }
        const std::uint64_t b = phrase_seed(std::span<const std::uint8_t>(raw_bytes));
        if (a != b) return {"phrase_seed overloads differ"};
        return {hx(a)};
    }
    if (k == "hash.phrase_text") {
        need_args(v, 1);
        const std::string bytes = parse_bytes(v.args[0]);
        const PhraseSeed p = seed_from_phrase(bytes);
        if (!p.ok()) return {"ERROR"};
        if (p.seed != phrase_seed(p.phrase)) return {"seed is not phrase_seed(phrase)"};
        return {hx(p.seed)};
    }

    // Noise.
    if (k == "noise.noise2") {
        need_args(v, 3);
        const std::uint64_t key = parse_hex(v.args[0]);
        const std::int64_t x = parse_dec(v.args[1]);
        const std::int64_t y = parse_dec(v.args[2]);
        return {dc(noise2(key, Fx::from_raw(x), Fx::from_raw(y)).raw())};
    }
    if (k == "noise.fbm2") {
        need_args(v, 7);
        const std::uint64_t key = parse_hex(v.args[0]);
        const std::int64_t x = parse_dec(v.args[1]);
        const std::int64_t y = parse_dec(v.args[2]);
        const std::int64_t oct = parse_dec(v.args[3]);
        const std::int64_t base = parse_dec(v.args[4]);
        const std::int64_t lac = parse_dec(v.args[5]);
        const std::int64_t gain = parse_dec(v.args[6]);
        if (oct < -1000 || oct > 1000) throw ParseError("octave count out of test range");
        return {dc(fbm2(key, Fx::from_raw(x), Fx::from_raw(y), static_cast<std::int32_t>(oct), Fx::from_raw(base),
                        Fx::from_raw(lac), Fx::from_raw(gain))
                       .raw())};
    }

    // SHA-256.
    if (k == "sha256") {
        need_args(v, 1);
        const std::string bytes = parse_bytes(v.args[0]);
        return {sha256_hex(bytes)};
    }
    if (k == "sha256.repeat") {
        need_args(v, 2);
        const std::string bytes = parse_bytes(v.args[0]);
        const std::uint64_t count = parse_count(v.args[1]);
        if (bytes.size() * count > 100000000) throw ParseError("sha256.repeat message too large for the test");
        std::string msg;
        msg.reserve(static_cast<std::size_t>(bytes.size() * count));
        for (std::uint64_t i = 0; i < count; ++i) {
            msg += bytes;
        }
        return {sha256_hex(msg)};
    }

    throw ParseError("unknown kind '" + k + "'");
}

// Splits a line on single spaces. Empty tokens (double or edge spaces) are errors.
Tokens split_tokens(std::string_view line) {
    Tokens out;
    std::size_t pos = 0;
    while (true) {
        const std::size_t sp = line.find(' ', pos);
        const std::string_view tok = line.substr(pos, sp == std::string_view::npos ? line.npos : sp - pos);
        if (tok.empty()) {
            throw ParseError("empty token (stray space)");
        }
        out.emplace_back(tok);
        if (sp == std::string_view::npos) {
            break;
        }
        pos = sp + 1;
    }
    return out;
}

Vector parse_line(std::string_view line) {
    for (const char c : line) {
        const auto u = static_cast<unsigned char>(c);
        if (u < 0x20 || u > 0x7e) {
            throw ParseError("non-printable byte in line");
        }
    }
    const Tokens t = split_tokens(line);
    Vector v;
    std::size_t eq = 0;
    std::size_t eq_count = 0;
    for (std::size_t i = 0; i < t.size(); ++i) {
        if (t[i] == "=") {
            eq = i;
            ++eq_count;
        }
    }
    if (eq_count != 1 || eq == 0 || eq + 1 >= t.size()) {
        throw ParseError("line must have one '=' between a kind with arguments and at least one result");
    }
    v.kind = t[0];
    v.args.assign(t.begin() + 1, t.begin() + static_cast<std::ptrdiff_t>(eq));
    v.expected.assign(t.begin() + static_cast<std::ptrdiff_t>(eq + 1), t.end());
    return v;
}

}  // namespace

TEST_CASE("P0-01 13.8: golden vectors from the independent reference") {
    std::ifstream in(UNMAPPED_CORE_VECTORS_PATH, std::ios::binary);
    REQUIRE_MESSAGE(in.good(), "cannot open " << UNMAPPED_CORE_VECTORS_PATH);
    std::stringstream buffer;
    buffer << in.rdbuf();
    const std::string content = buffer.str();
    REQUIRE_MESSAGE(!content.empty(), "empty vector file");
    REQUIRE_MESSAGE(content.back() == '\n', "vector file must end with LF");

    std::map<std::string, std::size_t> per_kind;
    std::size_t total = 0;
    std::size_t failures = 0;
    std::size_t line_no = 0;
    std::size_t pos = 0;
    while (pos < content.size()) {
        const std::size_t nl = content.find('\n', pos);
        const std::string_view line(content.data() + pos, nl - pos);
        pos = nl + 1;
        ++line_no;
        if (!line.empty() && line.front() == '#') {
            continue;
        }
        std::string failure;
        try {
            if (line.empty()) {
                throw ParseError("blank line");
            }
            const Vector v = parse_line(line);
            Tokens actual;
            try {
                actual = evaluate(v);
            } catch (const FatalError&) {
                actual = {"FATAL"};
            }
            if (actual != v.expected) {
                failure = "expected '" + join(v.expected) + "', got '" + join(actual) + "'";
            }
            ++per_kind[v.kind];
            ++total;
        } catch (const ParseError& e) {
            failure = std::string("unparsable vector: ") + e.what();
        } catch (const std::exception& e) {
            failure = std::string("unexpected exception: ") + e.what();
        }
        if (!failure.empty()) {
            ++failures;
        }
        CHECK_MESSAGE(failure.empty(), "core_vectors.txt line " << line_no << ": " << std::string(line) << "\n    "
                                                                 << failure);
    }

    std::ostringstream report;
    report << "golden vectors checked: " << total << " (" << failures << " failing)\n";
    for (const auto& [kind, count] : per_kind) {
        report << "  " << kind << ": " << count << "\n";
    }
    MESSAGE(report.str());
    CHECK(failures == 0);
    CHECK(total == EXPECTED_VECTOR_COUNT);
}
