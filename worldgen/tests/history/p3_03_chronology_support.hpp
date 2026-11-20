#pragma once

// Test-owned helpers of the P3-03 chronology tests: the shipped and fixture
// data roots, edited data roots for the data checks of section 6, an
// independent C2/C3 (own_schedule) and C4 step 1, the P0-04 8 logical hash
// of the five tables computed from rows, the [chronology] report keys of
// 9.1 computed from rows, the golden file reader of
// worldgen/tests/golden/p3-03-chronology.txt, and generated worlds cached
// per process. Library API only through p3_03_chronology_api.hpp,
// p3_02_names_api.hpp and API older than P3-03 (loader, reader, inspect,
// stream_key, sha256).

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../db_test_support.hpp"
#include "../p3_01_history_data.hpp"
#include "p3_02_names_api.hpp"
#include "p3_02_names_support.hpp"
#include "p3_03_chronology_api.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/sha256.hpp"
#include "worldgen/data/content.hpp"
#include "worldgen/data/load_error.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/generate.hpp"
#include "worldgen/inspect/hashes.hpp"
#include "worldgen/stages/testing.hpp"

#ifndef UNMAPPED_TEST_SOURCE_ROOT
#error "UNMAPPED_TEST_SOURCE_ROOT must name the repository root"
#endif
#ifndef UNMAPPED_P3_03_GOLDEN_PATH
#error "UNMAPPED_P3_03_GOLDEN_PATH must name worldgen/tests/golden/p3-03-chronology.txt"
#endif

namespace chrono_test {

namespace fs = std::filesystem;
namespace wd = worldgen::data;
using chrono_api::AgeRow;
using chrono_api::CultureRow;
using chrono_api::GodRow;
using chrono_api::Params;
using chrono_api::RaceBiomeRow;
using chrono_api::RaceRow;
using chrono_api::Rows;
using chrono_api::Tuning;

inline constexpr std::uint64_t M64 = ~std::uint64_t{0};
inline constexpr std::int64_t ONE_RAW = std::int64_t{1} << 32;

// The empty world of 4.3 at schema 6 (5.1): 18 pages of 4096 bytes. Spec
// 14.5: this is the spec writer's measurement with Python's SQLite 3.46.1,
// "to be confirmed with the vendored 3.54.1 before use". The test writer
// measured the same 18 pages (sqlite_schema text 2928 bytes) by adding the
// five statements of 5.1 and the eight meta rows to a 0.12.0 empty world
// and running VACUUM INTO with the vendored 3.54.1 amalgamation (page size
// 4096, the defines of extern/CMakeLists.txt); the engineer's confirmation
// on the real writer is still due. Every test reads the figure from here.
inline constexpr std::int64_t EMPTY_WORLD_PAGES = 34;  // 18 at schema 6; 34 from schema 7 (P3-04 9.6)
inline constexpr std::int64_t EMPTY_WORLD_BYTES = EMPTY_WORLD_PAGES * 4096;

inline const std::vector<std::string>& six() {
    static const std::vector<std::string> v = {"elf", "goblin", "dwarf", "faerie", "human", "orc"};
    return v;
}

inline fs::path source_root() { return fs::path(UNMAPPED_TEST_SOURCE_ROOT); }

inline std::string read_text(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    REQUIRE_MESSAGE(in.good(), "cannot open " << p.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

inline std::string errors_text(const std::vector<wd::LoadError>& errors) {
    std::string s;
    for (const wd::LoadError& e : errors) {
        s += e.to_string() + "\n";
    }
    return s;
}

// --- Data roots ---------------------------------------------------------------------------------

inline wd::ContentSet load_root(const fs::path& root) {
    auto r = wd::load_content(wd::builtin_kinds(), root);
    REQUIRE_MESSAGE(r.ok(), "data root " << root.string() << " does not load:\n" << errors_text(r.errors));
    return std::move(*r.value);
}

inline const wd::ContentSet& shipped() {
    static const wd::ContentSet c = load_root(source_root() / "game" / "data");
    return c;
}
inline const wd::ContentSet& fixture_geo() {
    static const wd::ContentSet c = load_root(source_root() / "worldgen" / "tests" / "data" / "geo");
    return c;
}
inline const wd::ContentSet& fixture_geo_small() {
    static const wd::ContentSet c = load_root(source_root() / "worldgen" / "tests" / "data" / "geo-small");
    return c;
}

// game/data as an in-memory root (every kind directory, files sorted).
inline wd::MemoryDataRoot shipped_memory_root() {
    wd::MemoryDataRoot root;
    const fs::path base = source_root() / "game" / "data";
    std::vector<fs::path> dirs;
    for (const auto& e : fs::directory_iterator(base)) {
        if (e.is_directory()) {
            dirs.push_back(e.path());
        }
    }
    std::sort(dirs.begin(), dirs.end());
    for (const fs::path& d : dirs) {
        wd::MemoryDirectory md{d.filename().string(), {}};
        std::vector<fs::path> files;
        for (const auto& e : fs::directory_iterator(d)) {
            if (e.is_regular_file()) {
                files.push_back(e.path());
            }
        }
        std::sort(files.begin(), files.end());
        for (const fs::path& f : files) {
            md.entries.push_back(wd::MemoryEntry{f.filename().string(), read_text(f), false});
        }
        root.directories.push_back(std::move(md));
    }
    return root;
}

// One text edit of a shipped file: in <directory>/<file>, the first `from`
// becomes `to` (REQUIREs that `from` occurs).
struct Edit {
    std::string directory;
    std::string file;
    std::string from;
    std::string to;
};

inline wd::MemoryDataRoot edited_root(const std::vector<Edit>& edits) {
    wd::MemoryDataRoot root = shipped_memory_root();
    for (const Edit& e : edits) {
        bool done = false;
        for (wd::MemoryDirectory& d : root.directories) {
            if (d.name != e.directory) {
                continue;
            }
            for (wd::MemoryEntry& f : d.entries) {
                if (f.name == e.file) {
                    const std::size_t at = f.bytes.find(e.from);
                    REQUIRE_MESSAGE(at != std::string::npos, e.directory << "/" << e.file << " has no '" << e.from << "'");
                    f.bytes.replace(at, e.from.size(), e.to);
                    done = true;
                }
            }
        }
        REQUIRE_MESSAGE(done, "no file " << e.directory << "/" << e.file);
    }
    return root;
}

inline const char* const D0_MESSAGE = "no race is defined; kind 'race' needs at least one definition";

// The edited data must load (the loader cannot see the cross-kind
// conditions of section 6).
inline wd::ContentSet edited_content(const std::vector<Edit>& edits) {
    auto r = wd::load_content_from_memory(wd::builtin_kinds(), edited_root(edits));
    REQUIRE_MESSAGE(r.ok(), "edited data does not load:\n" << errors_text(r.errors));
    return std::move(*r.value);
}

// Writes an edited copy of game/data to `dir` (for the CLI tests' --data).
inline void write_root(const wd::MemoryDataRoot& root, const fs::path& dir) {
    for (const wd::MemoryDirectory& d : root.directories) {
        fs::create_directories(dir / d.name);
        for (const wd::MemoryEntry& f : d.entries) {
            std::ofstream out(dir / d.name / f.name, std::ios::binary | std::ios::trunc);
            out << f.bytes;
        }
    }
}

// The five data edits of 14.1 item 6, each making exactly one condition of
// section 6 fail, with the messages it must produce (sorted by bytes).
struct DataCase {
    std::string label;
    std::vector<Edit> edits;
    std::vector<std::string> messages;
};

inline std::string long_form() { return std::string(42, 'X') + "{plural}"; }  // 49 bytes for 7-letter plurals

inline const std::vector<DataCase>& data_cases() {
    static const std::vector<DataCase> v = [] {
        std::vector<DataCase> out;
        out.push_back({"D1 lake",
                       {{"races", "elf.toml", "  { biome = \"alpine\", weight = -600 },",
                         "  { biome = \"alpine\", weight = -600 },\n  { biome = \"lake\", weight = 10 },"}},
                       {"race 'elf' prefers water biome 'lake'"}});
        out.push_back({"D2 blighted_waste",
                       {{"races", "goblin.toml", "  { biome = \"alpine\", weight = -400 },",
                         "  { biome = \"alpine\", weight = -400 },\n  { biome = \"blighted_waste\", weight = 10 },"}},
                       {"race 'goblin' prefers biome 'blighted_waste', which no geography stage assigns"}});
        out.push_back({"D3 culture of another race",
                       {{"races", "dwarf.toml", "culture = \"dwarven\"", "culture = \"elven\""}},
                       {"race 'dwarf' has culture 'elven' of race 'elf'"}});
        const std::string x42(42, 'X');
        out.push_back({"D4 49 bytes",
                       {{"history", "chronology.toml", "age_name_form = \"The Age of {plural}\"",
                         "age_name_form = \"" + long_form() + "\""}},
                       {"race 'dwarf' has the age name '" + x42 + "Dwarves', which is longer than 48 bytes",
                        "race 'faerie' has the age name '" + x42 + "Faeries', which is longer than 48 bytes",
                        "race 'goblin' has the age name '" + x42 + "Goblins', which is longer than 48 bytes"}});
        std::vector<std::string> d5;
        // race id order of the sorted messages: dwarf, elf, faerie, goblin, human, orc
        const std::vector<std::pair<std::string, std::string>> ids = {
            {"dwarf", "Dwarves"}, {"elf", "Elves"}, {"faerie", "Faeries"},
            {"goblin", "Goblins"}, {"human", "Humans"}, {"orc", "Orcs"}};
        for (const auto& [id, pl] : ids) {
            d5.push_back("race '" + id + "' has the age name 'Age " + pl +
                         " 2', which is not ASCII letters with single spaces, ' and -");
        }
        out.push_back({"D5 digit",
                       {{"history", "chronology.toml", "age_name_form = \"The Age of {plural}\"",
                         "age_name_form = \"Age {plural} 2\""}},
                       d5});
        return out;
    }();
    return v;
}

// --- Shipped data as the spec states it (independent of the loader) ---------------------------

// Biome codes of P1-04 (biome.id).
inline std::int64_t biome_code(std::string_view id) {
    static const std::map<std::string, std::int64_t, std::less<>> codes = {
        {"ocean", 1}, {"lake", 2}, {"temperate_forest", 3}, {"grassland", 4}, {"marsh", 5}, {"highland_moor", 6},
        {"alpine", 7}, {"blighted_waste", 8}, {"heath", 9}, {"steppe", 10}, {"wet_woodland", 11}, {"fen", 12}};
    const auto it = codes.find(id);
    REQUIRE(it != codes.end());
    return it->second;
}

inline std::int64_t camp_code(std::string_view camp) {
    if (camp == "good") return 0;
    if (camp == "neutral") return 1;
    REQUIRE(camp == "evil");
    return 2;
}

// Culture numbers: canonical id order (5.2).
inline std::int64_t culture_number(std::string_view id) {
    return static_cast<std::int64_t>(names_test::culture_number(id));
}

// The transcribed P3-01 race of a race id.
inline const history_data::Race& race_listing(std::string_view id) {
    for (const history_data::Race& r : history_data::races()) {
        if (r.id == id) {
            return r;
        }
    }
    FAIL("no race " << id);
    return history_data::races().front();
}

// The listed races in race order.
inline std::vector<const history_data::Race*> races_in_order() {
    std::vector<const history_data::Race*> out;
    for (const std::string& id : history_data::race_order()) {
        out.push_back(&race_listing(id));
    }
    return out;
}

inline Tuning shipped_tuning() {
    return Tuning{history_data::CHRONOLOGY_JITTER_RAW, history_data::CHRONOLOGY_MIN_GAP_YEARS,
                  std::string(history_data::CHRONOLOGY_AGE_NAME_FORM)};
}

// --- Own C2, C3, C4 step 1 -------------------------------------------------------------------

inline std::int64_t own_rha(std::int64_t n, std::int64_t d) {
    // round half away from zero of n / d, d > 0; |n| < 2^62 here
    const std::int64_t an = n < 0 ? -n : n;
    std::int64_t q = an / d;
    if (2 * (an % d) >= d) {
        ++q;
    }
    return n < 0 ? -q : q;
}

struct OwnStep {
    std::int64_t id = 0;
    std::int64_t base = 0;
    std::optional<std::uint64_t> u;
    std::int64_t pick = 0;
    std::int64_t d = 0;
    std::int64_t year = 0;
    bool clamped = false;
};

inline std::int64_t own_gap(std::int64_t span, std::int64_t present, std::int64_t gap) {
    return std::min(gap, span / present);
}
inline std::int64_t own_jmax(std::int64_t span, std::int64_t jitter_raw) { return own_rha(jitter_raw * span, ONE_RAW); }

inline std::vector<OwnStep> own_schedule(std::uint64_t seed, std::int64_t span, const std::vector<std::int64_t>& ids,
                                         const std::vector<std::int64_t>& shares, std::int64_t jitter_raw,
                                         std::int64_t gap_tuning) {
    REQUIRE(ids.size() == shares.size());
    REQUIRE(!ids.empty());
    const auto p = static_cast<std::int64_t>(ids.size());
    const std::int64_t g = own_gap(span, p, gap_tuning);
    const std::int64_t jmax = own_jmax(span, jitter_raw);
    const std::uint64_t key = worldgen::stream_key(seed, "hist.chronology.awakening");
    std::vector<OwnStep> out;
    for (std::int64_t i = 1; i <= p; ++i) {
        OwnStep s;
        s.id = ids[static_cast<std::size_t>(i - 1)];
        s.base = own_rha(shares[static_cast<std::size_t>(i - 1)] * span, ONE_RAW);
        if (i == 1) {
            s.year = 0;
        } else {
            const std::uint64_t u = names_api::draw(key, 0, static_cast<std::uint64_t>(s.id), 0, 0);
            s.u = u;
            s.pick = static_cast<std::int64_t>(names_api::pick(u, static_cast<std::uint64_t>(2 * jmax + 1)));
            s.d = s.pick - jmax;
            const std::int64_t lo = out.back().year + g;
            const std::int64_t hi = span - g * (p - i + 1);
            REQUIRE(lo <= hi);
            s.year = std::min(std::max(s.base + s.d, lo), hi);
            s.clamped = s.year != s.base + s.d;
        }
        out.push_back(s);
    }
    return out;
}

inline std::vector<std::int64_t> years_of(const std::vector<OwnStep>& s) {
    std::vector<std::int64_t> y;
    for (const OwnStep& x : s) {
        y.push_back(x.year);
    }
    return y;
}

// Ids and share raws of the listed races named in `ids` (race order).
inline std::pair<std::vector<std::int64_t>, std::vector<std::int64_t>> present_of(const std::vector<std::string>& ids) {
    std::pair<std::vector<std::int64_t>, std::vector<std::int64_t>> out;
    for (const history_data::Race* r : races_in_order()) {
        if (std::find(ids.begin(), ids.end(), std::string(r->id)) != ids.end()) {
            out.first.push_back(r->order);
            out.second.push_back(r->awakening_share_raw);
        }
    }
    return out;
}

inline std::vector<OwnStep> own_shipped_schedule(std::uint64_t seed, std::int64_t span,
                                                 const std::vector<std::string>& ids) {
    const auto [o, s] = present_of(ids);
    return own_schedule(seed, span, o, s, history_data::CHRONOLOGY_JITTER_RAW, history_data::CHRONOLOGY_MIN_GAP_YEARS);
}

inline std::string own_age_text(std::string_view form, std::string_view plural, std::string_view adjective) {
    std::string out;
    std::size_t i = 0;
    while (i < form.size()) {
        if (form.substr(i, 8) == "{plural}") {
            out += plural;
            i += 8;
        } else if (form.substr(i, 11) == "{adjective}") {
            out += adjective;
            i += 11;
        } else {
            out += form[i];
            ++i;
        }
    }
    return out;
}

// --- Subsets ----------------------------------------------------------------------------------

// The 63 non-empty subsets of the six races, each in race order.
inline std::vector<std::vector<std::string>> all_subsets() {
    std::vector<std::vector<std::string>> out;
    for (unsigned m = 1; m < 64; ++m) {
        std::vector<std::string> s;
        for (unsigned i = 0; i < 6; ++i) {
            if ((m >> i) & 1U) {
                s.push_back(six()[i]);
            }
        }
        out.push_back(s);
    }
    return out;
}

inline std::string joined(const std::vector<std::string>& ids, char sep = ',') {
    std::string s;
    for (const std::string& i : ids) {
        if (!s.empty()) {
            s += sep;
        }
        s += i;
    }
    return s;
}

inline std::vector<std::string> races_of_text(std::string_view text) {
    if (text == "all") {
        return six();
    }
    std::vector<std::string> wanted;
    std::string cur;
    for (const char c : text) {
        if (c == ',') {
            wanted.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    wanted.push_back(cur);
    std::vector<std::string> out;
    for (const std::string& id : six()) {
        if (std::find(wanted.begin(), wanted.end(), id) != wanted.end()) {
            out.push_back(id);
        }
    }
    return out;
}

inline Params params_with(std::int64_t span, const std::vector<std::string>& races) {
    Params p;
    p.span = span;
    p.races = races;
    return p;
}

// --- P0-04 8 logical hashes of the five tables, from rows ---------------------------------------

class TableHasher {
public:
    explicit TableHasher(std::uint64_t columns) { u64(columns); }
    void null() { bytes_.push_back(0x00); }
    void integer(std::int64_t v) {
        bytes_.push_back(0x01);
        u64(static_cast<std::uint64_t>(v));
    }
    void text(std::string_view s) {
        bytes_.push_back(0x03);
        u64(s.size());
        bytes_.insert(bytes_.end(), s.begin(), s.end());
    }
    [[nodiscard]] std::string hex() const { return worldgen::sha256_hex(std::span<const std::uint8_t>(bytes_)); }

private:
    void u64(std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            bytes_.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
        }
    }
    std::vector<std::uint8_t> bytes_;
};

inline std::map<std::string, std::string> table_hashes(const Rows& r) {
    std::map<std::string, std::string> out;
    {
        TableHasher h(12);
        for (const RaceRow& x : r.races) {
            h.integer(x.id);
            h.text(x.key);
            h.text(x.name);
            h.text(x.plural);
            h.text(x.adjective);
            h.integer(x.camp);
            h.integer(x.lifespan_years);
            h.integer(x.generation_years);
            h.integer(x.culture);
            h.integer(x.present);
            if (x.awakening_year.has_value()) {
                h.integer(*x.awakening_year);
            } else {
                h.null();
            }
            h.integer(x.creator_god);
        }
        out["race"] = h.hex();
    }
    {
        TableHasher h(3);
        for (const RaceBiomeRow& x : r.race_biomes) {
            h.integer(x.race);
            h.integer(x.biome);
            h.integer(x.weight);
        }
        out["race_biome"] = h.hex();
    }
    {
        TableHasher h(4);
        for (const CultureRow& x : r.cultures) {
            h.integer(x.id);
            h.text(x.key);
            h.text(x.name);
            h.integer(x.race);
        }
        out["culture"] = h.hex();
    }
    {
        TableHasher h(3);
        for (const GodRow& x : r.gods) {
            h.integer(x.id);
            h.text(x.name);
            h.integer(x.camp);
        }
        out["god"] = h.hex();
    }
    {
        TableHasher h(5);
        for (const AgeRow& x : r.ages) {
            h.integer(x.id);
            h.integer(x.race);
            h.integer(x.start_year);
            h.integer(x.end_year);
            h.text(x.name);
        }
        out["age"] = h.hex();
    }
    return out;
}

// The [chronology] lines of 9.1 after the header, from rows.
inline std::vector<std::string> report_lines(const Rows& r) {
    std::vector<std::string> out;
    if (r.ages.empty()) {
        return out;
    }
    std::int64_t present = 0;
    std::string years;
    for (const RaceRow& x : r.races) {
        if (x.present == 1) {
            ++present;
            years += (years.empty() ? "" : " ") + std::to_string(x.awakening_year.value_or(-1));
        }
    }
    std::string lens;
    std::int64_t lo = 0;
    for (std::size_t i = 0; i < r.ages.size(); ++i) {
        const std::int64_t n = r.ages[i].end_year - r.ages[i].start_year + 1;
        lens += (i == 0 ? "" : " ") + std::to_string(n);
        lo = i == 0 ? n : std::min(lo, n);
    }
    out.push_back("races_present = " + std::to_string(present));
    out.push_back("awakening_years = " + years);
    out.push_back("age_years = " + lens);
    out.push_back("age_years_min = " + std::to_string(lo));
    out.push_back("gods = " + std::to_string(r.gods.size()));
    return out;
}

// --- Golden file ------------------------------------------------------------------------------

struct GoldenCase {
    std::string name;
    std::string kind;  // "seam" or "world"
    std::uint64_t seed = 0;
    std::int64_t map_size = 0;  // world cases
    std::int64_t span = 0;
    std::string races;    // "all" or ids
    std::string regions;  // seam: "none" or "collision"
    std::map<std::string, std::string> items;  // "years", "trace", "ages", "gods", "table age", "meta <key>", ...
    std::vector<std::string> report;
};

struct GoldenSched {
    std::uint64_t seed = 0;
    std::int64_t span = 0;
    std::string races;
    std::vector<std::int64_t> years;
};

struct GoldenTuned {
    std::string name;
    std::uint64_t seed = 0;
    std::int64_t span = 0, gap = 0, jitter_raw = 0;
    std::vector<std::int64_t> ids, shares;
    std::int64_t g = 0, jmax = 0;
    std::vector<std::int64_t> years;
};

struct Golden {
    std::vector<GoldenCase> cases;
    std::vector<GoldenSched> sched;
    std::vector<GoldenTuned> tuned;
    [[nodiscard]] const GoldenCase& find(std::string_view name) const {
        for (const GoldenCase& c : cases) {
            if (c.name == name) {
                return c;
            }
        }
        FAIL("no golden case " << name);
        return cases.front();
    }
};

inline std::vector<std::string> split_ws(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream in(s);
    std::string t;
    while (in >> t) {
        out.push_back(t);
    }
    return out;
}

inline std::vector<std::int64_t> ints_of(const std::string& s, char sep) {
    std::vector<std::int64_t> out;
    std::string cur;
    for (const char c : s + std::string(1, sep)) {
        if (c == sep) {
            if (!cur.empty()) {
                out.push_back(std::stoll(cur));
            }
            cur.clear();
        } else {
            cur += c;
        }
    }
    return out;
}

inline const Golden& golden() {
    static const Golden g = [] {
        Golden out;
        std::istringstream in(read_text(fs::path(UNMAPPED_P3_03_GOLDEN_PATH)));
        std::string line;
        std::map<std::string, std::size_t> index;
        while (std::getline(in, line)) {
            REQUIRE_MESSAGE((line.empty() || line.back() != '\r'), "golden file is not LF");
            if (line.empty() || line[0] == '#') {
                continue;
            }
            const std::vector<std::string> t = split_ws(line);
            REQUIRE(t.size() >= 2);
            if (t[0] == "case") {
                GoldenCase c;
                c.name = t[1];
                c.kind = t[2];
                c.seed = std::stoull(t[3]);
                if (c.kind == "seam") {
                    REQUIRE(t.size() == 7);
                    c.span = std::stoll(t[4]);
                    c.races = t[5];
                    c.regions = t[6];
                } else {
                    REQUIRE(t.size() == 7);
                    c.map_size = std::stoll(t[4]);
                    c.span = std::stoll(t[5]);
                    c.races = t[6];
                }
                index[c.name] = out.cases.size();
                out.cases.push_back(c);
            } else if (t[0] == "sched") {
                GoldenSched s;
                s.seed = std::stoull(t[1]);
                s.span = std::stoll(t[2]);
                s.races = t[3];
                for (std::size_t i = 4; i < t.size(); ++i) {
                    s.years.push_back(std::stoll(t[i]));
                }
                out.sched.push_back(s);
            } else if (t[0] == "tuned") {
                REQUIRE(t.size() == 11);
                GoldenTuned x;
                x.name = t[1];
                x.seed = std::stoull(t[2]);
                x.span = std::stoll(t[3]);
                x.gap = std::stoll(t[4]);
                x.jitter_raw = std::stoll(t[5]);
                x.ids = ints_of(t[6], ',');
                x.shares = ints_of(t[7], ',');
                x.g = std::stoll(t[8]);
                x.jmax = std::stoll(t[9]);
                x.years = ints_of(t[10], ',');
                out.tuned.push_back(x);
            } else {
                const auto it = index.find(t[0]);
                REQUIRE_MESSAGE(it != index.end(), "golden line of an unknown case: " << line);
                GoldenCase& c = out.cases[it->second];
                const std::string rest = line.substr(t[0].size() + 1);
                const std::size_t sp = rest.find(' ');
                const std::string item = rest.substr(0, sp);
                const std::string value = sp == std::string::npos ? std::string() : rest.substr(sp + 1);
                if (item == "table") {
                    const std::size_t sp2 = value.find(' ');
                    c.items["table " + value.substr(0, sp2)] = value.substr(sp2 + 1);
                } else if (item == "meta") {
                    const std::size_t eq = value.find('=');
                    c.items["meta " + value.substr(0, eq)] = value.substr(eq + 1);
                } else if (item == "report") {
                    c.report.push_back(value);
                } else {
                    c.items[item] = value;
                }
            }
        }
        return out;
    }();
    return g;
}

inline const std::string& item(const GoldenCase& c, const std::string& key) {
    const auto it = c.items.find(key);
    REQUIRE_MESSAGE(it != c.items.end(), "golden case " << c.name << " has no item " << key);
    return it->second;
}

// The "ages" item of rows: id:race:start:end:name|...
inline std::string ages_text(const std::vector<AgeRow>& ages) {
    std::string s;
    for (const AgeRow& a : ages) {
        s += (s.empty() ? "" : "|") + std::to_string(a.id) + ":" + std::to_string(a.race) + ":" +
             std::to_string(a.start_year) + ":" + std::to_string(a.end_year) + ":" + a.name;
    }
    return s;
}

inline std::string hex16(std::uint64_t v) {
    static const char* d = "0123456789abcdef";
    std::string s(16, '0');
    for (int i = 15; i >= 0; --i) {
        s[static_cast<std::size_t>(i)] = d[v & 0xF];
        v >>= 4;
    }
    return s;
}

inline std::string trace_text(const std::vector<OwnStep>& s) {
    std::string out;
    for (const OwnStep& x : s) {
        out += out.empty() ? "" : "|";
        if (!x.u.has_value()) {
            out += std::to_string(x.id) + ":" + std::to_string(x.base) + ":-:-:-:" + std::to_string(x.year);
        } else {
            out += std::to_string(x.id) + ":" + std::to_string(x.base) + ":" + hex16(*x.u) + ":" +
                   std::to_string(x.pick) + ":" + std::to_string(x.d) + ":" + std::to_string(x.year);
        }
    }
    return out;
}

// The registry of 8.1 replayed with the P3-02 library: region names, the age
// names of `rows`, then each god named in id order by choose_name (r3: with the
// shipped name filter and the culture's god_patterns); returns
// "id:name:attempt|..." ("F" for the numeral fallback). An independent
// reading of C5 against the stage's god rows.
inline std::string replay_gods(const Rows& rows, std::uint64_t seed, const std::vector<std::string>& regions,
                               const wd::ContentSet& content) {
    names_api::Registry reg;
    for (const std::string& n : regions) {
        reg.add(n);
    }
    for (const AgeRow& a : rows.ages) {
        reg.add(a.name);
    }
    std::string out;
    for (const history_data::Race* r : races_in_order()) {
        const wd::Definition* culture = content.find(wd::CULTURE_KIND, r->culture);
        REQUIRE(culture != nullptr);
        const names_api::Naming n = names_api::naming_from_record(culture->record.get_record("naming"));
        // P3-03 r3 (0.17.0): gods are drawn with the world's name filter (P3-02 r5 11.3); every
        // data root the tests use holds game/data's name_filters/ (identical copies).
        const names_api::Choice ch =
            names_api::choose_name(n, r->culture, names_api::Kind::god, seed,
                                   static_cast<std::uint64_t>(culture_number(r->culture)),
                                   static_cast<std::uint64_t>(r->order), reg, names_test::shipped_filter());
        out += (out.empty() ? "" : "|") + std::to_string(r->order) + ":" + ch.name + ":" +
               (ch.fallback ? std::string("F") : std::to_string(ch.attempt));
    }
    return out;
}

inline std::vector<std::string> collision_regions() { return {"Fielie", "The Age of Elves", "Krok", "Krok II"}; }

// --- Worlds -----------------------------------------------------------------------------------

inline worldgen::WorldParams world_params(std::uint64_t seed, std::int64_t map_size, const Params& h,
                                          const wd::ContentSet& content, std::optional<std::string> until = {},
                                          std::uint32_t threads = 1) {
    worldgen::WorldParams p;
    p.seed = seed;
    p.map_size = map_size;
    p.content_hash = wd::content_hash(content);
    p.until = std::move(until);
    p.threads = threads;
    chrono_api::set_history(p, h);
    return p;
}

// A per-process directory for cached worlds, removed at exit.
inline const dbtest::TempDir& world_dir() {
    static const dbtest::TempDir d("chronology-worlds");
    return d;
}

struct WorldKey {
    std::uint64_t seed;
    std::string phrase;
    std::int64_t map_size;
    std::int64_t span;
    std::string races;
    std::string until;  // "" = every stage
    std::uint32_t threads;
    friend auto operator<=>(const WorldKey&, const WorldKey&) = default;
};

// generate_world with the shipped data, cached per process; returns the path.
inline std::string world(const WorldKey& k) {
    static std::map<WorldKey, std::string> cache;
    const auto it = cache.find(k);
    if (it != cache.end()) {
        return it->second;
    }
    const std::string path = world_dir().file("w" + std::to_string(cache.size()) + ".db");
    worldgen::WorldParams p = world_params(k.seed, k.map_size, params_with(k.span, races_of_text(k.races)), shipped(),
                                           k.until.empty() ? std::nullopt : std::optional<std::string>(k.until),
                                           k.threads);
    p.seed_phrase = k.phrase;
    const worldgen::db::Status s = worldgen::generate_world(p, shipped(), path, worldgen::db::Overwrite::refuse);
    REQUIRE_MESSAGE(s.ok(), "generate_world failed: " << s.error().message);
    cache[k] = path;
    return path;
}

// The world of a golden world case.
inline std::string world_of(const GoldenCase& c, std::uint32_t threads = 1) {
    const bool phrase = c.name == "geo-phrase-512";
    return world(WorldKey{c.seed, phrase ? "the unmapped" : "", c.map_size, c.span, c.races, "", threads});
}

inline worldgen::db::WorldReader open_world(const std::string& path) {
    auto r = worldgen::db::WorldReader::open(path);
    REQUIRE_MESSAGE(r.ok(), "cannot open " << path << ": " << (r.ok() ? "" : r.error().message));
    return std::move(r).value();
}

inline std::vector<std::vector<worldgen::db::ColumnValue>> rows_of(const worldgen::db::WorldReader& r,
                                                                  std::string_view table) {
    auto rows = r.table_rows(table);
    REQUIRE_MESSAGE(rows.ok(), "cannot read table " << table);
    return std::move(rows).value();
}

inline std::int64_t int_of(const worldgen::db::ColumnValue& v) {
    REQUIRE(v.type == worldgen::db::ValueType::integer);
    return v.integer;
}
inline std::string text_of(const worldgen::db::ColumnValue& v) {
    REQUIRE(v.type == worldgen::db::ValueType::text);
    return v.bytes;
}

// The five tables of a stored world as rows (also checks column types).
inline Rows rows_of_world(const worldgen::db::WorldReader& r) {
    Rows out;
    for (const auto& x : rows_of(r, "race")) {
        REQUIRE(x.size() == 12U);
        RaceRow row{int_of(x[0]), text_of(x[1]), text_of(x[2]), text_of(x[3]), text_of(x[4]), int_of(x[5]),
                    int_of(x[6]), int_of(x[7]), int_of(x[8]), int_of(x[9]), std::nullopt, int_of(x[11])};
        if (x[10].type != worldgen::db::ValueType::null) {
            row.awakening_year = int_of(x[10]);
        }
        out.races.push_back(row);
    }
    for (const auto& x : rows_of(r, "race_biome")) {
        REQUIRE(x.size() == 3U);
        out.race_biomes.push_back(RaceBiomeRow{int_of(x[0]), int_of(x[1]), int_of(x[2])});
    }
    for (const auto& x : rows_of(r, "culture")) {
        REQUIRE(x.size() == 4U);
        out.cultures.push_back(CultureRow{int_of(x[0]), text_of(x[1]), text_of(x[2]), int_of(x[3])});
    }
    for (const auto& x : rows_of(r, "god")) {
        REQUIRE(x.size() == 3U);
        out.gods.push_back(GodRow{int_of(x[0]), text_of(x[1]), int_of(x[2])});
    }
    for (const auto& x : rows_of(r, "age")) {
        REQUIRE(x.size() == 5U);
        out.ages.push_back(AgeRow{int_of(x[0]), int_of(x[1]), int_of(x[2]), int_of(x[3]), text_of(x[4])});
    }
    return out;
}

inline std::vector<std::string> region_names_of(const worldgen::db::WorldReader& r) {
    std::vector<std::string> out;
    for (const auto& x : rows_of(r, "region")) {
        out.push_back(text_of(x[1]));
    }
    return out;
}

// hash --detail's table hashes of a world, by table name.
inline std::map<std::string, std::string> detail_tables(const worldgen::db::WorldReader& r) {
    auto d = worldgen::inspect::detail_hashes(r);
    REQUIRE_MESSAGE(d.ok(), "detail_hashes failed");
    std::map<std::string, std::string> out;
    for (const auto& t : d.value().tables) {
        out[t.name] = worldgen::to_hex(t.digest);
    }
    return out;
}

inline std::string meta_or(const worldgen::db::WorldReader& r, std::string_view key) {
    return r.meta(key).value_or("<absent>");
}

inline const std::vector<std::string>& eight_keys() {
    static const std::vector<std::string> v = {
        "param.civs_per_race",  "param.divine_activity", "param.divine_script",    "param.event_intensity",
        "param.history_detail", "param.history_span",    "param.monster_pressure", "param.races"};
    return v;
}

inline const std::vector<std::string>& five_tables() {
    static const std::vector<std::string> v = {"age", "culture", "god", "race", "race_biome"};
    return v;
}

// A stage writing hand-made region rows (ids 1.., the given names), for the
// pipeline tests of the stage entry.
inline std::vector<std::string>& synthetic_region_names() {
    static std::vector<std::string> names;
    return names;
}
inline void synthetic_regions_stage(worldgen::stages::StageContext& ctx) {
    std::int64_t id = 0;
    for (const std::string& n : synthetic_region_names()) {
        worldgen::db::RegionRow row{};
        row.id = ++id;
        row.name = n;
        row.biome = 3;
        row.cells = 1;
        ctx.write_region(row);
    }
}

// generate_world_with_stages: [regions (synthetic, `names`; none when
// empty), chronology]. Returns the FatalError text, or "" on success.
inline std::string run_pipeline(const std::string& path, const wd::ContentSet& content, std::uint64_t seed,
                                const Params& h, const std::vector<std::string>& names, std::uint32_t threads = 1) {
    synthetic_region_names() = names;
    std::vector<worldgen::stages::StageInfo> stages;
    if (!names.empty()) {
        stages.push_back(worldgen::stages::StageInfo{"regions", &synthetic_regions_stage});
    }
    stages.push_back(chrono_api::stage_info());
    try {
        const worldgen::db::Status s = worldgen::stages::testing::generate_world_with_stages(
            world_params(seed, 1024, h, content, std::nullopt, threads), content, stages, path);
        if (!s.ok()) {
            return "status: " + s.error().message;
        }
    } catch (const worldgen::FatalError& e) {
        return e.what();
    }
    return {};
}

}  // namespace chrono_test
