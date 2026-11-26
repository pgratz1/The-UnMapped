// worldgen-cli: argument handling and the commands generate, render, report
// and hash (spec P0-04). The work itself is done by the worldgen library
// (generate_world, render_rgb, build_report, detail_hashes); this file
// parses arguments, calls it and writes the results.
//
// Never calls setlocale and never formats a number through a locale
// (P0-04 4.4): integers are formatted with the project's own functions.

#include "cli.hpp"

#include <stb_image_write.h>

#include <algorithm>
#include <chrono>  // determinism-lint: allow(banned-include) -- --verbose timings go to stderr only, never to a world, report or hash (P0-04 4.5)
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <functional>
#include <limits>
#include <map>
#include <new>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <thread>  // determinism-lint: allow(thread) -- only for std::thread::hardware_concurrency, the --threads default; the CLI starts no thread
#include <utility>
#include <vector>

#include "output.hpp"
#include "platform.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/parallel.hpp"
#include "worldgen/core/sha256.hpp"
#include "worldgen/data/content.hpp"
#include "worldgen/data/data_root.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/data/settings.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/db/schema.hpp"
#include "worldgen/db/utf8_path.hpp"
#include "worldgen/db/writer.hpp"
#include "worldgen/generate.hpp"
#include "worldgen/history/data_checks.hpp"
#include "worldgen/history/params.hpp"
#include "worldgen/history/timeline.hpp"
#include "worldgen/inspect/geography.hpp"
#include "worldgen/inspect/hashes.hpp"
#include "worldgen/inspect/render.hpp"
#include "worldgen/inspect/history_report.hpp"
#include "worldgen/inspect/report.hpp"
#include "worldgen/inspect/markers.hpp"
#include "worldgen/inspect/territory.hpp"
#include "worldgen/stages/stage.hpp"
#include "worldgen/version.hpp"

#if defined(WORLDGEN_CLI_TEST_HOOKS)
#include "worldgen/stages/testing.hpp"
#endif

namespace cli {

namespace {

namespace db = worldgen::db;
namespace inspect = worldgen::inspect;
using db::format_u64;

// --- stdout and stderr (P0-04 4.4, 4.5) --------------------------------------

// True if every byte was written. A short count means the write failed;
// stdio may also only set the stream's error flag, so run() checks ferror
// after the final flush as well (P0-04 4.6: a failed write is exit 4).
bool write_stream(std::FILE* stream, std::string_view text) noexcept {
    if (text.empty()) {
        return true;
    }
    return std::fwrite(text.data(), 1, text.size(), stream) == text.size();
}

// Set once any write to stdout has failed; checked by run(). The CLI is
// single-threaded.
bool stdout_failed = false;

void print_out(std::string_view text) noexcept {
    if (!write_stream(stdout, text)) {
        stdout_failed = true;
    }
}

// stderr failures are ignored: there is nowhere left to report them.
void print_line_err(std::string_view text) noexcept {
    static_cast<void>(write_stream(stderr, text));
    static_cast<void>(write_stream(stderr, "\n"));
}

// --- --verbose (P0-04 4.5): progress and timings on stderr only -------------

// determinism-lint: allow(wall-clock) -- --verbose timings go to stderr only, never to a world, report or hash (P0-04 4.5)
using Clock = std::chrono::steady_clock;

class Progress {
public:
    explicit Progress(bool enabled) : enabled_(enabled), start_(Clock::now()) {}

    void note(std::string_view message) const {
        if (!enabled_) {
            return;
        }
        const auto elapsed = Clock::now() - start_;
        // determinism-lint: allow(wall-clock) -- --verbose timings go to stderr only, never to a world, report or hash (P0-04 4.5)
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        std::string line = "worldgen-cli: [";
        line += format_u64(static_cast<std::uint64_t>(ms < 0 ? 0 : ms));
        line += " ms] ";
        line += message;
        print_line_err(line);
    }

private:
    bool enabled_;
    Clock::time_point start_;
};

// --- usage texts (P0-04 4.1, 5 to 8) -----------------------------------------

constexpr std::string_view TOP_USAGE =
    "usage: worldgen-cli <command> [--option value | --flag]...\n"
    "       worldgen-cli --help | --version\n"
    "\n"
    "commands:\n"
    "  generate   generate a world file\n"
    "  render     export a layer of a world file as a PNG image\n"
    "  report     print a text summary of a world file\n"
    "  hash       print the SHA-256 of a file, and logical hashes of a world\n"
    "\n"
    "Options take their value as the next argument (--seed 42). Every command\n"
    "accepts --help and --verbose. 'worldgen-cli <command> --help' lists a\n"
    "command's options.\n";

constexpr std::string_view GENERATE_USAGE =
    "usage: worldgen-cli generate (--seed N | --seed-phrase TEXT) [--out PATH]\n"
    "                             [--map-size 512|1024|2048] [--settings FILE] [--data DIR]\n"
    "                             [--until STAGE|none | --test-pattern] [--threads N]\n"
    "                             [--history-span N] [--history-detail D] [--civs-per-race C]\n"
    "                             [--races LIST] [--event-intensity I] [--monster-pressure M]\n"
    "                             [--divine-activity A] [--force] [--verbose]\n"
    "\n"
    "  --seed N            the 64-bit world seed: decimal, or 0x and 1 to 16 hex digits\n"
    "  --seed-phrase TEXT  a text phrase from which the seed is derived; letters are\n"
    "                      lower-cased, anything but a-z and 0-9 counts as a space\n"
    "  --out PATH          output world file (default <build root>/out/world-<seed>.db)\n"
    "  --map-size N        map side in cells: 512, 1024 or 2048 (default from settings, else 1024)\n"
    "  --settings FILE     world settings file; flags override it key by key\n"
    "  --data DIR          data root (default UNMAPPED_DATA_DIR, else the source tree's game/data)\n"
    "  --until STAGE       run the stages up to and including STAGE (elevation ...\n"
    "                      regions, chronology, history); 'none' runs no stage\n"
    "                      (default: every implemented stage)\n"
    "  --test-pattern      run only the P0 test-pattern stage, no geography\n"
    "  --threads N         threads for parallel steps, 1 to 256 (default: hardware threads);\n"
    "                      never changes the output\n"
    "  --history-span N    years of history, 2000 to 12000 (default 6000)\n"
    "  --history-detail D  sparse, normal or dense (default normal)\n"
    "  --civs-per-race C   civilizations per race: auto or 1 to 4 (default auto)\n"
    "  --races LIST        races present: all, or race ids separated by commas without\n"
    "                      spaces, e.g. elf,orc (default all)\n"
    "  --event-intensity I calm, normal or turbulent (default normal)\n"
    "  --monster-pressure M\n"
    "                      low, normal or high (default normal)\n"
    "  --divine-activity A distant, normal or meddling (default normal)\n"
    "  --divine-script FILE\n"
    "                      not supported yet: divine scripts need phase P5\n"
    "  --force             replace an existing output file\n"
    "  --verbose           print progress and timings to stderr\n"
    "\n"
    "The history settings are recorded in every world. The chronology stage uses\n"
    "the span and the races; the history stage uses the span, the detail, the\n"
    "civilizations per race and the event intensity. The monster pressure and\n"
    "the divine activity take effect with later history subsystems (phase P5).\n"
    "--test-pattern records them and writes no history.\n"
    "\n"
    "Prints the absolute path of the written file.\n";

constexpr std::string_view RENDER_USAGE =
    "usage: worldgen-cli render --db PATH (--layer NAME [--palette P] | --composite NAME) [--year Y] [--markers [--faded]]\n"
    "                           [--out PATH] [--scale N] [--force] [--verbose]\n"
    "\n"
    "  --db PATH           world file\n"
    "  --layer NAME        layer to render\n"
    "  --composite NAME    map: shaded relief from elevation, with water and river when present;\n"
    "                      biomes: shaded biome colours from layer and table biome, with water\n"
    "                      and river when present;\n"
    "                      regions: the biomes composite with the borders of layer region in black;\n"
    "                      territory: the regions composite with each region held in the year in the\n"
    "                      colour of its holder (needs the history)\n"
    "  --year Y            with --composite territory or --markers: the year, 0 to the history's end year\n"
    "                      (default the end year, the present)\n"
    "  --markers           draw the settlements, ruins, event places and passes of the year over the\n"
    "                      image (needs the history)\n"
    "  --faded             with --markers: also old ruins (1500 years or more) and faded ruins\n"
    "  --out PATH          output PNG (default <build root>/out/<db file stem>.<layer>.png,\n"
    "                      <db file stem>.<composite>.png for --composite, or\n"
    "                      <db file stem>.territory-<year>.png for territory, with -<year>.markers.png for --markers)\n"
    "  --palette P         auto (the layer's registered palette, default), gray, hypsometric,\n"
    "                      water, river, log2, direction, ids, signed, temperature, rainfall,\n"
    "                      biome or score; --layer only\n"
    "  --scale N           each cell becomes an N x N block, 1 to 16 (default 1)\n"
    "  --force             replace an existing output file\n"
    "  --verbose           print progress and timings to stderr\n"
    "\n"
    "Prints the absolute path of the written image.\n";

constexpr std::string_view REPORT_USAGE =
    "usage: worldgen-cli report --db PATH [--history [--level summary|full]] [--out PATH] [--force] [--verbose]\n"
    "\n"
    "  --db PATH           world file\n"
    "  --history           print the history report (format '# worldgen history v9') instead of the world report\n"
    "  --level LEVEL       with --history: summary (default) or full\n"
    "  --out PATH          write the report to this file instead of stdout\n"
    "  --force             replace an existing output file\n"
    "  --verbose           print progress and timings to stderr\n";

constexpr std::string_view HASH_USAGE =
    "usage: worldgen-cli hash --db PATH [--detail] [--expect HEX] [--verbose]\n"
    "\n"
    "  --db PATH           file to hash (need not be a world unless --detail is given)\n"
    "  --detail            also print logical hashes of the schema, tables and layers\n"
    "  --expect HEX        compare the file hash with 64 hex digits; exit 5 on a mismatch\n"
    "  --verbose           print progress and timings to stderr\n";

// --- option parsing (P0-04 4.1) ----------------------------------------------

struct OptionSpec {
    std::string_view name;  // without "--"
    bool takes_value;
};

struct CommandSpec {
    std::string_view name;
    std::string_view usage;
    std::vector<OptionSpec> options;
};

class Options {
public:
    [[nodiscard]] bool flag(std::string_view name) const { return flags_.find(name) != flags_.end(); }
    [[nodiscard]] const std::string* value(std::string_view name) const {
        const auto it = values_.find(name);
        return it == values_.end() ? nullptr : &it->second;
    }
    [[nodiscard]] bool has(std::string_view name) const { return flag(name) || value(name) != nullptr; }

    void set_flag(std::string name) { flags_.insert(std::move(name)); }
    void set_value(std::string name, std::string v) { values_.emplace(std::move(name), std::move(v)); }

private:
    std::set<std::string, std::less<>> flags_;
    std::map<std::string, std::string, std::less<>> values_;
};

[[noreturn]] void usage_error(const std::string& message) {
    throw Failure(Exit::usage, message);
}

// Every command accepts --help and --verbose (P0-04 4.1, 4.5).
const OptionSpec* find_option(const CommandSpec& command, std::string_view name) {
    static constexpr OptionSpec help{"help", false};
    static constexpr OptionSpec verbose{"verbose", false};
    if (name == help.name) return &help;
    if (name == verbose.name) return &verbose;
    for (const OptionSpec& o : command.options) {
        if (o.name == name) return &o;
    }
    return nullptr;
}

// Parses args[1..]: "--name value" or "--flag"; anything else is a usage
// error (P0-04 4.1).
Options parse_options(const CommandSpec& command, const std::vector<std::string>& args) {
    Options options;
    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg.size() < 3 || arg.compare(0, 2, "--") != 0) {
            if (!arg.empty() && arg[0] == '-') {
                usage_error("unknown option '" + arg + "' for " + std::string(command.name) +
                            " (there are no short options)");
            }
            usage_error("unexpected argument '" + arg + "' (" + std::string(command.name) +
                        " takes no positional arguments)");
        }
        const std::string name = arg.substr(2);
        if (name.find('=') != std::string::npos) {
            usage_error("unknown option '" + arg + "' (write the value as the next argument: --name value)");
        }
        const OptionSpec* spec = find_option(command, name);
        if (spec == nullptr) {
            usage_error("unknown option '" + arg + "' for " + std::string(command.name));
        }
        if (options.has(name)) {
            usage_error("option '" + arg + "' given more than once");
        }
        if (spec->takes_value) {
            if (i + 1 >= args.size()) {
                usage_error("option '" + arg + "' needs a value");
            }
            ++i;
            options.set_value(name, args[i]);  // the next argument, even if it starts with '-'
        } else {
            options.set_flag(name);
        }
    }
    return options;
}

const std::string& required(const Options& options, std::string_view command, std::string_view name) {
    const std::string* v = options.value(name);
    if (v == nullptr) {
        usage_error(std::string(command) + " needs --" + std::string(name));
    }
    return *v;
}

// A seed (P0-04 4.2): the grammar of P0-03 section 6, parse_seed_text.
std::uint64_t parse_seed_option(const std::string& text) {
    const std::optional<std::uint64_t> seed = worldgen::data::parse_seed_text(text);
    if (!seed) {
        usage_error("invalid --seed '" + text +
                    "'; expected a decimal integer 0 to 18446744073709551615 without leading zeros, "
                    "or 0x and 1 to 16 hex digits");
    }
    return *seed;
}

// A non-negative integer (P0-04 4.2): "0" or [1-9][0-9]*, no sign.
std::uint64_t parse_integer_option(std::string_view option, const std::string& text) {
    const std::optional<std::uint64_t> v = db::parse_canonical_u64(text);
    if (!v) {
        usage_error("invalid --" + std::string(option) + " '" + text +
                    "'; expected a decimal integer without sign or leading zeros");
    }
    return *v;
}

std::string phrase_error_text(worldgen::PhraseError error) {
    switch (error) {
        case worldgen::PhraseError::empty:
            return "it has no letter a-z or digit 0-9; a phrase keeps only letters and digits, "
                   "other characters become spaces";
        case worldgen::PhraseError::too_long:
            return "it is longer than 256 bytes after scrubbing to letters a-z, digits 0-9 and single spaces";
        case worldgen::PhraseError::none:
            break;
    }
    return std::string();  // not reached: called only for an error value
}

std::vector<std::string> load_error_lines(const std::vector<worldgen::data::LoadError>& errors) {
    std::vector<std::string> lines;
    lines.reserve(errors.size());
    for (const worldgen::data::LoadError& e : errors) {
        lines.push_back(e.to_string());
    }
    return lines;
}

db::WorldReader open_world(const std::string& path, const Progress& progress) {
    progress.note("opening '" + path + "'");
    db::Result<db::WorldReader> world = db::WorldReader::open(path);
    if (!world.ok()) {
        fail_with(world.error());
    }
    return std::move(world).value();
}

// --- generate (P0-04 section 5) ----------------------------------------------

const CommandSpec GENERATE{
    "generate",
    GENERATE_USAGE,
    {
        {"seed", true},
        {"seed-phrase", true},
        {"out", true},
        {"map-size", true},
        {"settings", true},
        {"data", true},
        {"test-pattern", false},
        {"until", true},
        {"threads", true},
        {"history-span", true},
        {"history-detail", true},
        {"civs-per-race", true},
        {"races", true},
        {"event-intensity", true},
        {"monster-pressure", true},
        {"divine-activity", true},
        {"divine-script", true},
        {"force", false},
#if defined(WORLDGEN_CLI_TEST_HOOKS)
        // Test-hook build only (P0-04 12.2): a fatal error inside the
        // test_pattern stage.
        {"test-fatal-in-stage", false},
#endif
    },
};

// --until STAGE|none (P1-00 10.1, P0-04 amendment 16 (a)): an unknown or
// not-yet-implemented stage name is a usage error, as is combining it with
// --test-pattern.
std::optional<std::string> parse_until_option(const Options& options) {
    const std::string* text = options.value("until");
    if (text == nullptr) {
        return std::nullopt;
    }
    if (options.flag("test-pattern")) {
        usage_error("--until and --test-pattern are exclusive; --test-pattern runs no geography stage");
    }
    switch (worldgen::stages::check_stage_limit(*text)) {
        case worldgen::stages::StageLimitError::none:
            return *text;
        case worldgen::stages::StageLimitError::not_implemented:
            usage_error("stage '" + *text + "' given to --until is not implemented yet");
        case worldgen::stages::StageLimitError::unknown:
            break;
    }
    std::string names;
    for (const std::string_view name : worldgen::stages::STAGE_NAMES) {
        names += names.empty() ? "" : ", ";
        names += name;
    }
    usage_error("unknown stage '" + *text + "' given to --until; expected none or a stage name (" + names + ")");
}

// --threads N (P1-00 10.1, rule D10): decimal 1 to 256 (P0-04 4.2). Default:
// the hardware thread count, or 1 if unknown, at most 256. The count only
// schedules work; it never reaches the world.
std::uint32_t parse_threads_option(const Options& options) {
    if (const std::string* text = options.value("threads")) {
        const std::uint64_t v = parse_integer_option("threads", *text);
        if (v < 1U || v > worldgen::MAX_THREADS) {
            usage_error("invalid --threads '" + *text + "'; expected 1 to 256");
        }
        return static_cast<std::uint32_t>(v);
    }
    // determinism-lint: allow(hardware-concurrency) -- only the --threads default, capped at 256; the count schedules blocks and never reaches a world, report or hash (P1-00 D10)
    const unsigned int hardware = std::thread::hardware_concurrency();  // the --threads default only
    if (hardware == 0U) {
        return 1U;
    }
    return hardware > worldgen::MAX_THREADS ? worldgen::MAX_THREADS : static_cast<std::uint32_t>(hardware);
}

// The data root (P0-03 7.6 and amendment 15, P1-00 6.3): --data, else
// UNMAPPED_DATA_DIR if set and non-empty, else the configure-time game/data/.
worldgen::data::DataRoot data_root_option(const Options& options) {
    std::optional<std::string_view> flag;
    if (const std::string* dir = options.value("data")) {
        flag = *dir;  // non-empty: checked with the other flag values in run_generate
    }
    std::optional<std::string> env;
    if (!flag) {
        platform::EnvironmentValue v = platform::environment_utf8(worldgen::data::DATA_DIR_ENV.data());
        if (v.invalid) {
            usage_error(std::string(worldgen::data::DATA_DIR_ENV) + " is not valid UTF-16; fix it or pass --data");
        }
        env = std::move(v.value);
    }
    return worldgen::data::resolve_data_root(flag, env ? std::optional<std::string_view>(*env) : std::nullopt);
}

// --- the history flags (P3-01 6.1, 6.2) ---------------------------------------

// The history settings given as flags; each empty when not given.
struct HistoryFlags {
    std::optional<std::int64_t> history_span;
    std::optional<worldgen::data::HistoryDetail> history_detail;
    bool civs_given = false;
    std::optional<std::int64_t> civs_per_race;  // with civs_given: empty for auto
    bool races_given = false;
    std::optional<std::vector<std::string>> races;  // with races_given: empty for all
    std::optional<worldgen::data::EventIntensity> event_intensity;
    std::optional<worldgen::data::MonsterPressure> monster_pressure;
    std::optional<worldgen::data::DivineActivity> divine_activity;
};

// One of the three-valued flags: parse is the enum's parse_* function.
template <typename E>
std::optional<E> parse_enum_option(const Options& options, std::string_view name, std::string_view expected,
                                   std::optional<E> (*parse)(std::string_view) noexcept) {
    const std::string* text = options.value(name);
    if (text == nullptr) {
        return std::nullopt;
    }
    const std::optional<E> v = parse(*text);
    if (!v) {
        usage_error("invalid --" + std::string(name) + " '" + *text + "'; expected " + std::string(expected));
    }
    return v;
}

// Parses the history flags (P3-01 6.2): every form error is a usage error
// here; unknown race ids are checked once the data is loaded.
HistoryFlags parse_history_flags(const Options& options) {
    namespace data = worldgen::data;
    HistoryFlags flags;
    if (const std::string* text = options.value("history-span")) {
        const std::optional<std::uint64_t> v = db::parse_canonical_u64(*text);
        if (!v || *v > static_cast<std::uint64_t>(data::MAX_HISTORY_SPAN) ||
            !data::is_valid_history_span(static_cast<std::int64_t>(*v))) {
            usage_error("invalid --history-span '" + *text + "'; expected an integer from 2000 to 12000");
        }
        flags.history_span = static_cast<std::int64_t>(*v);
    }
    flags.history_detail =
        parse_enum_option(options, "history-detail", "sparse, normal or dense", &data::parse_history_detail);
    if (const std::string* text = options.value("civs-per-race")) {
        flags.civs_given = true;
        if (*text != "auto") {
            const std::optional<std::uint64_t> v = db::parse_canonical_u64(*text);
            if (!v || *v < static_cast<std::uint64_t>(data::MIN_CIVS_PER_RACE) ||
                *v > static_cast<std::uint64_t>(data::MAX_CIVS_PER_RACE)) {
                usage_error("invalid --civs-per-race '" + *text + "'; expected auto or an integer from 1 to 4");
            }
            flags.civs_per_race = static_cast<std::int64_t>(*v);
        }
    }
    if (const std::string* text = options.value("races")) {
        data::RacesText parsed = data::parse_races_text(*text);
        if (!parsed.ok) {
            usage_error("invalid --races '" + *text + "'; expected all or a comma-separated list of race ids");
        }
        flags.races_given = true;
        flags.races = std::move(parsed.races);
    }
    flags.event_intensity = parse_enum_option(options, "event-intensity", "calm, normal or turbulent",
                                              &data::parse_event_intensity);
    flags.monster_pressure =
        parse_enum_option(options, "monster-pressure", "low, normal or high", &data::parse_monster_pressure);
    flags.divine_activity =
        parse_enum_option(options, "divine-activity", "distant, normal or meddling", &data::parse_divine_activity);
    if (options.value("divine-script") != nullptr) {
        usage_error("--divine-script is not supported yet: divine scripts need phase P5");
    }
    return flags;
}

// Overrides the file's history keys with the flags, key by key (P3-01 6.1).
void apply_history_flags(const HistoryFlags& flags, worldgen::data::Settings& settings) {
    if (flags.history_span) settings.history_span = *flags.history_span;
    if (flags.history_detail) settings.history_detail = *flags.history_detail;
    if (flags.civs_given) settings.civs_per_race = flags.civs_per_race;
    if (flags.races_given) {
        settings.races = flags.races;
        settings.races_origins.clear();
    }
    if (flags.event_intensity) settings.event_intensity = *flags.event_intensity;
    if (flags.monster_pressure) settings.monster_pressure = *flags.monster_pressure;
    if (flags.divine_activity) settings.divine_activity = *flags.divine_activity;
}

// The race ids of --races that name no race definition: a usage error
// naming the first, in the order given (P3-01 6.2).
void check_flag_races(const HistoryFlags& flags, const worldgen::data::ContentSet& content) {
    if (!flags.races_given || !flags.races) {
        return;
    }
    const std::vector<std::string> known = worldgen::data::race_ids_in_order(content);
    for (const std::string& id : *flags.races) {
        if (std::find(known.begin(), known.end(), id) != known.end()) {
            continue;
        }
        std::string list;
        for (const std::string& k : known) {
            list += list.empty() ? "" : ", ";
            list += k;
        }
        usage_error("unknown race '" + id + "' in --races; expected all or ids from: " + list);
    }
}

// The history settings as one line, for --verbose.
std::string history_settings_text(const worldgen::data::Settings& s) {
    namespace data = worldgen::data;
    std::string races;
    for (const std::string& id : s.races.value_or(std::vector<std::string>{})) {
        races += races.empty() ? "" : ",";
        races += id;
    }
    return "history settings: span " + db::format_i64(s.history_span) + ", detail " +
           std::string(data::history_detail_name(s.history_detail)) + ", civs per race " +
           (s.civs_per_race ? db::format_i64(*s.civs_per_race) : std::string("auto")) + ", races " + races +
           ", event intensity " + std::string(data::event_intensity_name(s.event_intensity)) +
           ", monster pressure " + std::string(data::monster_pressure_name(s.monster_pressure)) +
           ", divine activity " + std::string(data::divine_activity_name(s.divine_activity));
}

Exit run_generate(const Options& options) {
    const Progress progress(options.flag("verbose"));

    // Flag values first: every usage error before any file is read.
    if (options.has("seed") && options.has("seed-phrase")) {
        usage_error("--seed and --seed-phrase are exclusive; give one");
    }
    std::optional<std::uint64_t> flag_seed;
    std::optional<std::string> flag_phrase;
    if (const std::string* text = options.value("seed")) {
        flag_seed = parse_seed_option(*text);
    }
    if (const std::string* text = options.value("seed-phrase")) {
        worldgen::PhraseSeed p = worldgen::seed_from_phrase(*text);
        if (!p.ok()) {
            usage_error("invalid --seed-phrase: " + phrase_error_text(p.error));
        }
        flag_seed = p.seed;
        flag_phrase = std::move(p.phrase);
    }
    std::optional<std::int64_t> flag_map_size;
    if (const std::string* text = options.value("map-size")) {
        const std::uint64_t v = parse_integer_option("map-size", *text);
        if (!worldgen::data::is_valid_map_size(v > 4096U ? 0 : static_cast<std::int64_t>(v))) {
            usage_error("invalid --map-size '" + *text + "'; expected 512, 1024 or 2048");
        }
        flag_map_size = static_cast<std::int64_t>(v);
    }
    std::optional<std::string> until = parse_until_option(options);
    const std::uint32_t threads = parse_threads_option(options);
    const HistoryFlags history_flags = parse_history_flags(options);
    if (const std::string* dir = options.value("data"); dir != nullptr && dir->empty()) {
        usage_error("--data needs a directory; an empty value names no data root");
    }
#if defined(WORLDGEN_CLI_TEST_HOOKS)
    if (options.flag("test-fatal-in-stage") && !options.flag("test-pattern")) {
        usage_error("--test-fatal-in-stage needs --test-pattern");
    }
#endif

    // Step 1: settings and data (P0-03).
    worldgen::data::Settings settings;
    if (const std::string* path = options.value("settings")) {
        progress.note("loading settings '" + *path + "'");
        worldgen::data::LoadResult<worldgen::data::Settings> loaded =
            worldgen::data::load_settings_file(db::path_from_utf8(*path));
        if (!loaded.ok()) {
            throw Failure(Exit::input, "invalid settings file '" + *path + "'", load_error_lines(loaded.errors));
        }
        settings = std::move(*loaded.value);
    }
    const worldgen::data::KindRegistry registry = worldgen::data::builtin_kinds();
    worldgen::data::LoadResult<worldgen::data::ContentSet> content;
    const worldgen::data::DataRoot data_root = data_root_option(options);
    if (data_root.source != worldgen::data::DataRootSource::none) {
        // With no kind registered the root is not opened (P0-03 amendment 9).
        progress.note(registry.kinds().empty() ? "data root '" + data_root.path + "' (not read: no kind is registered)"
                                               : "loading data '" + data_root.path + "'");
        content = worldgen::data::load_content(registry, db::path_from_utf8(data_root.path));
        if (!content.ok()) {
            throw Failure(Exit::input, "invalid data in '" + data_root.path + "'", load_error_lines(content.errors));
        }
    } else {
        content = worldgen::data::load_content_without_data(registry);
        if (!content.ok()) {
            throw Failure(Exit::input, "the data needs a data root; pass --data", load_error_lines(content.errors));
        }
    }
    // The data checks across kinds (P3-03 6): an input error whatever the
    // plan, --until none and --test-pattern included.
    if (const std::vector<worldgen::data::LoadError> data_errors = worldgen::history::check_history_data(*content.value);
        !data_errors.empty()) {
        throw Failure(Exit::input, "invalid data in '" + data_root.path + "'", load_error_lines(data_errors));
    }

    // The history settings (P3-01 6): race ids are checked against the
    // loaded data, the flags' first (a usage error), then the file's.
    check_flag_races(history_flags, *content.value);
    if (const std::string* path = options.value("settings")) {
        const std::vector<worldgen::data::LoadError> race_errors =
            worldgen::data::check_settings_races(settings, *content.value);
        if (!race_errors.empty()) {
            throw Failure(Exit::input, "invalid settings file '" + *path + "'", load_error_lines(race_errors));
        }
    }
    apply_history_flags(history_flags, settings);
    worldgen::data::resolve_races(settings, *content.value);
    progress.note(history_settings_text(settings));

    // Seed and parameters: a seed flag replaces both seed keys of the file
    // (P0-03 6); --map-size overrides world.map_size.
    worldgen::WorldParams params;
    if (flag_seed) {
        params.seed = *flag_seed;
        params.seed_phrase = flag_phrase.value_or(std::string());
    } else if (settings.seed) {
        params.seed = *settings.seed;
        params.seed_phrase = settings.seed_phrase.value_or(std::string());
    } else {
        usage_error("no seed given; pass --seed or --seed-phrase");
    }
    params.map_size = flag_map_size.value_or(settings.map_size);
    params.content_hash = worldgen::data::content_hash(*content.value);
    params.test_pattern = options.flag("test-pattern");
    params.until = std::move(until);
    params.history = worldgen::history::history_params_from(settings);  // P3-03 4.1 step 1
    params.threads = threads;

    // Output path (P0-04 4.3).
    std::string out;
    if (const std::string* path = options.value("out")) {
        out = *path;
    } else {
        out = db::path_to_utf8(db::path_from_utf8(default_output_dir()) /
                               db::path_from_utf8("world-" + format_u64(params.seed) + ".db"));
    }
    const db::Overwrite overwrite = options.flag("force") ? db::Overwrite::replace : db::Overwrite::refuse;
    check_output_location(out, overwrite == db::Overwrite::replace);  // fail before generating (P0-04 4.4)

    std::string plan;
    for (const std::string_view stage : worldgen::planned_stages(params)) {
        plan += plan.empty() ? "" : ", ";
        plan += stage;
    }
    progress.note("generating seed " + format_u64(params.seed) + ", map size " +
                  db::format_i64(params.map_size) + ", stages: " + (plan.empty() ? "(none)" : plan));

    db::Status status = db::Status(db::Ok{});
#if defined(WORLDGEN_CLI_TEST_HOOKS)
    if (options.flag("test-fatal-in-stage")) {
        status = worldgen::stages::testing::generate_world_with_fault(
            params, out, overwrite, worldgen::stages::testing::StageFault::fatal_inside_test_pattern);
    } else {
        status = worldgen::generate_world(params, *content.value, out, overwrite);
    }
#else
    status = worldgen::generate_world(params, *content.value, out, overwrite);
#endif
    if (!status.ok()) {
        fail_with(status.error());
    }
    progress.note("wrote '" + out + "'");
    print_out(absolute_utf8(out) + "\n");
    return Exit::success;
}

// --- render (P0-04 section 6) ------------------------------------------------

const CommandSpec RENDER{
    "render",
    RENDER_USAGE,
    {
        {"db", true},
        {"layer", true},
        {"composite", true},
        {"out", true},
        {"palette", true},
        {"scale", true},
        {"year", true},
        {"force", false},
        {"markers", false},
        {"faded", false},
    },
};

// stb_image_write output callback: appends to a byte vector.
void append_png_bytes(void* context, void* data, int size) {
    auto* out = static_cast<std::vector<std::uint8_t>*>(context);
    if (size > 0) {
        const auto* bytes = static_cast<const std::uint8_t*>(data);
        out->insert(out->end(), bytes, bytes + size);
    }
}

std::string layer_name_list(const db::WorldReader& world) {
    db::Result<std::vector<db::LayerInfo>> layers = world.layers();
    if (!layers.ok()) {
        return "(the layer list cannot be read: " + layers.error().message + ")";
    }
    if (layers.value().empty()) {
        return "the world has no layers";
    }
    std::string names = "available layers: ";
    bool first = true;
    for (const db::LayerInfo& info : layers.value()) {
        names += first ? "" : ", ";
        names += info.name;
        first = false;
    }
    return names;
}

// Most raw RGB bytes (3 x width x height) handed to the PNG writer
// (P0-04 amendment 7). stb_image_write sizes its buffers with int; its
// filtered copy adds one byte per row, still far below 2^31.
constexpr std::uint64_t MAX_PNG_RAW_BYTES = std::uint64_t{1} << 30U;

// The names --palette accepts, for its usage message: "auto, gray, ...".
std::string palette_name_list() {
    std::string names = "auto";
    for (const inspect::Palette p : inspect::ALL_PALETTES) {
        names += ", ";
        names += inspect::palette_name(p);
    }
    return names;
}

// A layer for render: its grid, or exit 3 listing the layer names when it
// is missing (P0-04 6).
db::LayerGrid read_render_layer(const db::WorldReader& world, const std::string& name, const Progress& progress) {
    progress.note("reading layer '" + name + "'");
    db::Result<db::Layer> layer = world.read_layer(name);
    if (!layer.ok()) {
        if (layer.code() == db::ErrorCode::layer_not_found) {
            throw Failure(Exit::input, layer.error().message + "; " + layer_name_list(world));
        }
        fail_with(layer.error());
    }
    return std::move(layer.value().grid);
}

// An optional layer of a composite: its grid, or std::nullopt when absent.
std::optional<db::LayerGrid> read_optional_layer(const db::WorldReader& world, const std::string& name,
                                                 const Progress& progress) {
    db::Result<db::Layer> layer = world.read_layer(name);
    if (!layer.ok()) {
        if (layer.code() == db::ErrorCode::layer_not_found) {
            progress.note("layer '" + name + "' is absent");
            return std::nullopt;
        }
        fail_with(layer.error());
    }
    progress.note("read layer '" + name + "'");
    return std::move(layer.value().grid);
}

// The colours of table `biome` (P1-04 9): empty when the world has no such
// table; exit 3 naming the table when a row is malformed.
std::vector<inspect::BiomeColour> read_biome_colours(const db::WorldReader& world, const Progress& progress) {
    db::Result<std::vector<std::vector<db::ColumnValue>>> rows = world.table_rows(db::BIOME_TABLE);
    if (!rows.ok()) {
        if (rows.code() == db::ErrorCode::table_not_found) {
            progress.note("table 'biome' is absent");
            return {};
        }
        fail_with(rows.error());
    }
    const inspect::BiomeTableResult table = inspect::read_biome_rows(rows.value());
    if (table.problem) {
        fail_with(db::Error{db::ErrorCode::corrupt_layer, "corrupt table 'biome': " + *table.problem});
    }
    std::vector<inspect::BiomeColour> colours;
    for (const inspect::BiomeRecord& r : table.records) {
        colours.push_back(inspect::BiomeColour{r.id, r.colour});
    }
    progress.note("read table 'biome' (" + format_u64(colours.size()) + " rows)");
    return colours;
}

// The --composite name of the territory image (P4-01 5.4, P0-04 amendment
// 23): a CLI name only, not a Composite enumerator.
constexpr std::string_view TERRITORY = "territory";

// The names --composite accepts, for its usage message: "map, biomes,
// regions, territory".
std::string composite_name_list() {
    std::string names;
    for (const inspect::Composite c : inspect::ALL_COMPOSITES) {
        if (!names.empty()) names += ", ";
        names += inspect::composite_name(c);
    }
    names += ", ";
    names += TERRITORY;
    return names;
}

Exit run_render(const Options& options) {
    const Progress progress(options.flag("verbose"));
    const std::string& db_path = required(options, "render", "db");
    // Exactly one of --layer and --composite (P0-04 amendment 16b).
    const std::string* layer_option = options.value("layer");
    const std::string* composite_option = options.value("composite");
    if ((layer_option == nullptr) == (composite_option == nullptr)) {
        usage_error("render needs exactly one of --layer and --composite");
    }
    std::optional<inspect::Composite> composite;
    const bool territory = composite_option != nullptr && *composite_option == TERRITORY;
    if (composite_option != nullptr) {
        // territory reads the layers of the composite regions (P4-01 5.6).
        composite = territory ? std::optional<inspect::Composite>(inspect::Composite::regions)
                              : inspect::composite_from_name(*composite_option);
        if (!composite) {
            usage_error("invalid --composite '" + *composite_option + "'; expected " + composite_name_list());
        }
        if (options.value("palette") != nullptr) {
            usage_error("--palette applies to --layer only");
        }
    }
    std::optional<inspect::Palette> palette;  // empty: auto
    if (const std::string* text = options.value("palette")) {
        if (*text != "auto") {
            palette = inspect::palette_from_name(*text);
            if (!palette) {
                usage_error("invalid --palette '" + *text + "'; expected " + palette_name_list());
            }
        }
    }
    std::uint32_t scale = 1;
    if (const std::string* text = options.value("scale")) {
        const std::uint64_t v = parse_integer_option("scale", *text);
        if (v < inspect::MIN_RENDER_SCALE || v > inspect::MAX_RENDER_SCALE) {
            usage_error("invalid --scale '" + *text + "'; expected 1 to 16");
        }
        scale = static_cast<std::uint32_t>(v);
    }
    // --markers and --faded (P0-04 amendment 24).
    const bool markers = options.flag("markers");
    const bool faded = options.flag("faded");
    if (faded && !markers) {
        usage_error("--faded applies to --markers only");
    }
    // --year (P0-04 amendments 23 and 24): territory or markers; its range is
    // judged once the history is read.
    const std::string* year_option = options.value("year");
    std::optional<std::uint64_t> year_value;
    if (year_option != nullptr) {
        if (!territory && !markers) {
            usage_error("--year applies to --composite territory or --markers only");
        }
        year_value = parse_integer_option("year", *year_option);
    }

    const db::WorldReader world = open_world(db_path, progress);
    // The image name: the layer name, or the composite name (P0-04
    // amendment 16b: <db file stem>.map.png).
    const std::string image_name = territory   ? std::string(TERRITORY)
                                   : composite ? std::string(inspect::composite_name(*composite))
                                               : *layer_option;
    const db::LayerGrid grid = read_render_layer(world, composite ? std::string("elevation") : *layer_option, progress);
    std::optional<db::LayerGrid> water;
    std::optional<db::LayerGrid> river;
    std::optional<db::LayerGrid> flow_dir;
    std::optional<db::LayerGrid> biome;
    std::optional<db::LayerGrid> region;
    if (composite) {
        if (*composite == inspect::Composite::biomes || *composite == inspect::Composite::regions) {
            biome = read_render_layer(world, "biome", progress);  // required (P1-04 9)
        }
        water = read_optional_layer(world, "water", progress);
        river = read_optional_layer(world, "river", progress);
        flow_dir = read_optional_layer(world, "flow_dir", progress);
        std::optional<std::string> problem =
            biome ? inspect::composite_biomes_problem(grid, *biome, water ? &*water : nullptr,
                                                      river ? &*river : nullptr, flow_dir ? &*flow_dir : nullptr)
                  : inspect::composite_map_problem(grid, water ? &*water : nullptr, river ? &*river : nullptr,
                                                   flow_dir ? &*flow_dir : nullptr);
        // The composite regions (P0-04 amendment 18 (e)): the checks of the
        // composite biomes first, then the layer region.
        if (!problem && *composite == inspect::Composite::regions) {
            region = read_render_layer(world, "region", progress);  // required
            problem = inspect::composite_regions_problem(grid, water ? &*water : nullptr, river ? &*river : nullptr,
                                                         flow_dir ? &*flow_dir : nullptr, *biome, *region);
        }
        if (problem) {
            throw Failure(Exit::input, "cannot render the composite " + image_name + ": " + *problem + "; " +
                                           layer_name_list(world));
        }
    } else if (palette && !inspect::palette_suits(*palette, grid)) {
        usage_error("palette '" + std::string(inspect::palette_name(*palette)) + "' does not suit layer '" +
                    *layer_option + "' (" + std::string(db::element_type_name(grid.type())) + "); it needs " +
                    std::string(inspect::palette_requirement(*palette)));
    }

    // territory or markers (P0-04 amendments 23 (b), 24 (d)): the history,
    // the marker index, then the year range.
    std::optional<worldgen::history::LoadedHistory> loaded;
    std::optional<inspect::MarkerIndex> marker_index;
    std::int64_t year = 0;
    if (territory || markers) {
        progress.note("reading the history");
        db::Result<worldgen::history::LoadedHistory> h = worldgen::history::load_history(world);
        if (!h.ok()) {
            fail_with(h.error());
        }
        loaded.emplace(std::move(h).value());
        if (markers) {
            progress.note("building the markers");
            db::Result<inspect::MarkerIndex> m = inspect::MarkerIndex::load(world, *loaded);
            if (!m.ok()) {
                fail_with(m.error());
            }
            marker_index.emplace(std::move(m).value());
        }
        const std::int64_t end = loaded->timeline.end_year();
        if (year_value) {
            if (*year_value > static_cast<std::uint64_t>(end)) {
                usage_error("--year " + *year_option + " is outside the history of world file '" + world.path() +
                            "' (0 to " + db::format_i64(end) + ")");
            }
            year = static_cast<std::int64_t>(*year_value);
        } else {
            year = end;  // the present (P4-00 V6)
        }
    }

    const std::uint64_t width = static_cast<std::uint64_t>(grid.width) * scale;
    const std::uint64_t height = static_cast<std::uint64_t>(grid.height) * scale;
    if (width * height * 3U > MAX_PNG_RAW_BYTES) {
        usage_error("an image of " + format_u64(width) + " x " + format_u64(height) +
                    " pixels is too large to write; use a smaller --scale");
    }

    std::string out;
    if (const std::string* path = options.value("out")) {
        out = *path;
    } else {
        const std::string stem = db::path_to_utf8(db::path_from_utf8(db_path).stem());
        std::string name = (territory || markers) ? image_name + "-" + db::format_i64(year) : image_name;
        if (markers) {
            name += faded ? ".markers-faded" : ".markers";
        }
        out = db::path_to_utf8(db::path_from_utf8(default_output_dir()) /
                               db::path_from_utf8(stem + "." + name + ".png"));
    }
    const bool force = options.flag("force");
    check_output_location(out, force);  // fail before rendering

    std::vector<std::uint8_t> rgb;
    if (composite) {
        progress.note("rendering " + format_u64(width) + " x " + format_u64(height) + " pixels, composite " +
                      image_name);
        if (territory) {
            const std::vector<inspect::BiomeColour> colours = read_biome_colours(world, progress);
            progress.note("year " + db::format_i64(year));
            const inspect::TerritoryBase base =
                inspect::TerritoryBase::build(grid, water ? &*water : nullptr, river ? &*river : nullptr,
                                              flow_dir ? &*flow_dir : nullptr, *biome, *region, colours);
            const std::vector<inspect::Rgb> civ_colours = inspect::civilization_colours(loaded->tables);
            rgb = inspect::render_territory(base, loaded->timeline, civ_colours, year, scale);
        } else if (region) {
            const std::vector<inspect::BiomeColour> colours = read_biome_colours(world, progress);
            rgb = inspect::render_composite_regions(grid, water ? &*water : nullptr, river ? &*river : nullptr,
                                                    flow_dir ? &*flow_dir : nullptr, *biome, *region, scale,
                                                    colours);
        } else if (biome) {
            const std::vector<inspect::BiomeColour> colours = read_biome_colours(world, progress);
            rgb = inspect::render_composite_biomes(grid, *biome, colours, water ? &*water : nullptr,
                                                   river ? &*river : nullptr, flow_dir ? &*flow_dir : nullptr, scale);
        } else {
            rgb = inspect::render_composite_map(grid, water ? &*water : nullptr, river ? &*river : nullptr,
                                                flow_dir ? &*flow_dir : nullptr, scale);
        }
    } else {
        const inspect::Palette chosen = palette.value_or(inspect::auto_palette(*layer_option, grid));
        if (!palette && chosen != inspect::registered_palette(*layer_option)) {
            progress.note("the registered palette does not suit this layer; using gray");
        }
        progress.note("rendering " + format_u64(width) + " x " + format_u64(height) + " pixels, palette " +
                      std::string(inspect::palette_name(chosen)));
        std::vector<inspect::BiomeColour> colours;
        if (chosen == inspect::Palette::biome) {
            colours = read_biome_colours(world, progress);
        }
        rgb = inspect::render_rgb(grid, chosen, scale, colours);
    }
    if (markers) {
        // P4-04 7: the markers of year Y over the rendered image.
        const inspect::MarkerOptions marker_options{faded, true, true};
        const inspect::YearMarkers year_markers = marker_index->markers_at(*loaded, year, marker_options);
        progress.note("drawing " + format_u64(year_markers.settlements.size()) + " settlements, " +
                      format_u64(year_markers.places.size()) + " event places and " +
                      format_u64(year_markers.passes.size()) + " passes of year " + db::format_i64(year));
        const std::vector<inspect::Rgb> civ_colours = inspect::civilization_colours(loaded->tables);
        inspect::draw_markers(rgb, grid.width, grid.height, scale, year_markers, civ_colours);
    }

    std::vector<std::uint8_t> png;
    const int w = static_cast<int>(width);
    const int h = static_cast<int>(height);
    if (stbi_write_png_to_func(append_png_bytes, &png, w, h, 3, rgb.data(), w * 3) == 0) {
        throw Failure(Exit::output, "encoding the PNG image failed");
    }
    progress.note("writing '" + out + "' (" + format_u64(png.size()) + " bytes)");
    write_file_atomically(out, png, force);
    print_out(absolute_utf8(out) + "\n");
    return Exit::success;
}

// --- report (P0-04 section 7) ------------------------------------------------

const CommandSpec REPORT{
    "report",
    REPORT_USAGE,
    {
        {"db", true},
        {"history", false},
        {"level", true},
        {"out", true},
        {"force", false},
    },
};

// --history and --level (P3-07 4): checked after tokenizing, before any
// file is opened. std::nullopt: the world report.
std::optional<inspect::HistoryLevel> parse_history_options(const Options& options) {
    const std::string* level = options.value("level");
    if (!options.flag("history")) {
        if (level != nullptr) {
            usage_error("--level needs --history");
        }
        return std::nullopt;
    }
    if (level == nullptr) {
        return inspect::HistoryLevel::summary;
    }
    const std::optional<inspect::HistoryLevel> parsed = inspect::parse_history_level(*level);
    if (!parsed) {
        usage_error("invalid --level '" + *level + "'; expected summary or full");
    }
    return parsed;
}

Exit run_report(const Options& options) {
    const Progress progress(options.flag("verbose"));
    const std::optional<inspect::HistoryLevel> history = parse_history_options(options);
    const std::string& db_path = required(options, "report", "db");
    const std::string* out = options.value("out");
    const bool force = options.flag("force");  // meaningful with --out only

    const db::WorldReader world = open_world(db_path, progress);
    progress.note(history ? "building the history report" : "building the report");
    db::Result<std::string> report = history ? inspect::build_history_report(world, *history)
                                             : inspect::build_report(world);
    if (!report.ok()) {
        fail_with(report.error());
    }
    const std::string& text = report.value();
    if (out != nullptr) {
        progress.note("writing '" + *out + "'");
        const std::span<const std::uint8_t> bytes(reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
        write_file_atomically(*out, bytes, force);
    } else {
        print_out(text);
    }
    return Exit::success;
}

// --- hash (P0-04 section 8) --------------------------------------------------

const CommandSpec HASH{
    "hash",
    HASH_USAGE,
    {
        {"db", true},
        {"detail", false},
        {"expect", true},
    },
};

std::string lower_hex(std::string text) {
    for (char& c : text) {
        if (c >= 'A' && c <= 'F') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return text;
}

Exit run_hash(const Options& options) {
    const Progress progress(options.flag("verbose"));
    const std::string& db_path = required(options, "hash", "db");
    std::optional<std::string> expect;
    if (const std::string* text = options.value("expect")) {
        if (!inspect::is_sha256_hex(*text)) {
            usage_error("invalid --expect '" + *text + "'; expected 64 hex digits");
        }
        expect = lower_hex(*text);
    }

    progress.note("hashing '" + db_path + "'");
    db::Result<worldgen::Sha256Digest> file = inspect::hash_file(db_path);
    if (!file.ok()) {
        fail_with(file.error());
    }
    const std::string file_hex = worldgen::to_hex(file.value());

    std::string text;
    if (options.flag("detail")) {
        const db::WorldReader world = open_world(db_path, progress);
        progress.note("computing logical hashes");
        db::Result<inspect::DetailHashes> detail = inspect::detail_hashes(world);
        if (!detail.ok()) {
            fail_with(detail.error());
        }
        text = inspect::format_detail(file.value(), detail.value());
    } else {
        text = file_hex + "\n";
    }
    print_out(text);

    if (expect && *expect != file_hex) {
        print_line_err("worldgen-cli: error: hash mismatch: expected " + *expect + ", got " + file_hex);
        return Exit::mismatch;
    }
    return Exit::success;
}

// --- dispatch ----------------------------------------------------------------

struct Command {
    const CommandSpec* spec;
    Exit (*run)(const Options&);
};

const Command* find_command(std::string_view name) {
    static const Command commands[] = {
        {&GENERATE, run_generate},
        {&RENDER, run_render},
        {&REPORT, run_report},
        {&HASH, run_hash},
    };
    for (const Command& c : commands) {
        if (c.spec->name == name) {
            return &c;
        }
    }
    return nullptr;
}

std::string version_line() {
    std::string line = "worldgen-cli ";
    line += worldgen::GENERATOR_VERSION;
    line += " schema ";
    line += db::format_i64(db::SCHEMA_VERSION);
    line += " sqlite ";
    line += db::sqlite_library_version();
    line += "\n";
    return line;
}

void print_failure(const Failure& failure) {
    print_error(failure.what());
    for (const std::string& line : failure.details()) {
        print_line_err(line);
    }
}

Exit dispatch(const std::vector<std::string>& args, std::string& command_name) {
    if (args.empty()) {
        usage_error("no command given");
    }
    const std::string& first = args[0];
    if (first == "--help" || first == "--version") {
        if (args.size() > 1) {
            usage_error("'" + first + "' takes no further arguments");
        }
        print_out(first == "--help" ? std::string(TOP_USAGE) : version_line());
        return Exit::success;
    }
    const Command* command = find_command(first);
    if (command == nullptr) {
        if (first.size() >= 2 && first.compare(0, 2, "--") == 0) {
            usage_error("unknown option '" + first + "'; the command comes first");
        }
        usage_error("unknown command '" + first + "'");
    }
    command_name = first;
    const Options options = parse_options(*command->spec, args);
    if (options.flag("help")) {
        print_out(command->spec->usage);
        return Exit::success;
    }
    return command->run(options);
}

}  // namespace

void print_error(const std::string& message) noexcept {
    try {
        print_line_err("worldgen-cli: error: " + message);
    } catch (...) {
        static_cast<void>(write_stream(stderr, "worldgen-cli: error: (message lost: out of memory)\n"));
    }
}

void print_hint(std::string_view command) noexcept {
    try {
        std::string hint = "worldgen-cli: run 'worldgen-cli ";
        if (!command.empty()) {
            hint += command;
            hint += ' ';
        }
        hint += "--help' for usage";
        print_line_err(hint);
    } catch (...) {
        // out of memory: the error line has been printed already
    }
}

int run(const std::vector<std::string>& args) noexcept {
    std::string command_name;
    Exit exit = Exit::internal;
    try {
        try {
            exit = dispatch(args, command_name);
        } catch (const Failure& failure) {
            print_failure(failure);
            if (failure.exit() == Exit::usage) {
                print_hint(command_name);
            }
            exit = failure.exit();
        } catch (const worldgen::FatalError& error) {
            // The project fatal error (P0-01 4.1): a bug, exit 1. The
            // library has already removed any partial output.
            print_error(std::string("internal error: ") + error.what());
            exit = Exit::internal;
        } catch (const std::bad_alloc&) {
            print_error("internal error: out of memory");
            exit = Exit::internal;
        } catch (const std::exception& error) {
            print_error(std::string("internal error: ") + error.what());
            exit = Exit::internal;
        }
    } catch (...) {
        print_error("internal error: unexpected exception");
        exit = Exit::internal;
    }
    // Every write to stdout is checked: a short fwrite, a failed final
    // flush, or the stream's error flag (set by a write-through that failed)
    // turns success, or a hash mismatch whose output was lost, into an
    // output error (P0-04 4.6). An earlier error keeps its own exit code.
    const bool flush_failed = std::fflush(stdout) != 0;
    if (stdout_failed || flush_failed || std::ferror(stdout) != 0) {
        print_error("writing to stdout failed");
        if (exit == Exit::success || exit == Exit::mismatch) {
            exit = Exit::output;
        }
    }
    return static_cast<int>(exit);
}

}  // namespace cli
