#pragma once

// Stage P3-03 chronology (spec docs/specs/p3-03-chronology.md; P3-00
// sections 2.1, 4.5, 4.6, 5 and 6.1).
//
// Fixes the frame of every history before anything is simulated: which
// races are present, the year each one awakens (C3), the ages those
// awakenings open (C4), one named creator god per race (C5), and an echo of
// the race and culture data (tables race, race_biome, culture, god, age;
// schema 6). Every random decision is a keyed draw (P3-00 H4): one per
// present race after the first, on stream hist.chronology.awakening, and
// the god names of P3-02 on hist.names.god. No Rng, no noise, no layer.
//
// Every function raises FatalError (P0-01 4.1) on a contract violation.

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "worldgen/core/fx.hpp"
#include "worldgen/data/content.hpp"
#include "worldgen/db/writer.hpp"
#include "worldgen/history/data_checks.hpp"
#include "worldgen/history/names.hpp"
#include "worldgen/history/params.hpp"
#include "worldgen/stages/stage.hpp"

namespace worldgen::stages {

// The stage name recorded in meta.stages (P3-00 5).
inline constexpr std::string_view CHRONOLOGY_STAGE = "chronology";

// The fields of definition history.chronology (P3-01 5.1), in declaration
// order.
struct ChronologyTuning {
    Fx awakening_jitter_frac;               // fx(0, 0.05)
    std::int64_t awakening_min_gap_years = 0;  // int(1, 2000)
    std::string age_name_form;              // string(64), named check chronology_age_name_form
    // P5-08 4.2 (generator 0.30.0): a creator's title, {plural} once.
    std::string creator_title = "the Maker of {plural}";

    friend bool operator==(const ChronologyTuning&, const ChronologyTuning&) = default;
};

// The tuning of the content's definition history.chronology. FatalError
// ("chronology: the content has no definition history.chronology") if it
// is absent.
[[nodiscard]] ChronologyTuning chronology_tuning_from(const data::ContentSet& content);

// C1 item 2: each field inside its schema range of P3-01 5.1 ("chronology:
// tuning: <field> outside its schema range", declaration order), then the
// named check ("chronology: tuning: age_name_form must contain {plural} or
// {adjective} and no other placeholder").
void check_chronology_tuning(const ChronologyTuning& tuning);

// C2: the effective least gap G = min(awakening_min_gap_years, span div
// present) and the largest shift jmax = round_int(mul_int(jitter, span)).
// FatalError if span < 1 or present < 1.
struct AwakeningBounds {
    std::int64_t gap = 0;
    std::int64_t jmax = 0;

    friend bool operator==(const AwakeningBounds&, const AwakeningBounds&) = default;
};
[[nodiscard]] AwakeningBounds awakening_bounds(std::int64_t span, std::int64_t present,
                                               const ChronologyTuning& tuning);

// C3: the awakening years of the present races, in race order. present_ids
// are the races' ids (= order), present_shares their awakening_share; the
// first present race awakens at year 0, each later one at
// clamp(base + d, lo, hi) with d = pick(draw(K_aw, 0, id, 0, 0), 2 jmax + 1)
// - jmax, K_aw = stream_key(seed, "hist.chronology.awakening"). FatalError if
// the two spans differ in length, are empty, span < 1, or the least gap G of
// C2 is below 1 ("awakening_years: the least gap is below 1 year").
[[nodiscard]] std::vector<std::int64_t> awakening_years(std::uint64_t seed, std::int64_t span,
                                                        std::span<const std::int64_t> present_ids,
                                                        std::span<const Fx> present_shares,
                                                        const ChronologyTuning& tuning);

// C4 step 1 (history/data_checks.hpp).
using history::age_name_text;

// The rows of the five tables (P3-03 5.2), each in its write order, and the
// name choice behind each god (god id order: the attempt that gave the name,
// and whether the numeral fallback was used).
struct ChronologyResult {
    std::vector<db::RaceRow> races;
    std::vector<db::RaceBiomeRow> race_biomes;
    std::vector<db::CultureRow> cultures;
    std::vector<db::GodRow> gods;
    std::vector<db::AgeRow> ages;
    std::vector<history::NameChoice> god_names;
};

// C1 items 2 to 4 (tuning, settings, data; in this order, each fatal with
// the messages of P3-03 C1), then C2 to C5. region_names are the names of
// table region in id order: they fill the name registry first (P3-03 8.1).
// An empty list is accepted (the regions precondition, C1 item 1, is
// run_chronology's). Pure: no state is kept between calls.
[[nodiscard]] ChronologyResult chronology_stage(const data::ContentSet& content, const history::HistoryParams& params,
                                                std::uint64_t seed, std::span<const std::string> region_names);

// The stage entry point (P1-00 3): C1 item 1 ("chronology: input
// precondition: no region rows" when no earlier stage wrote a region row),
// then chronology_stage on the context's content, history params, seed and
// region names, and the write of C6 (P3-03 5.3).
void run_chronology(StageContext& context);

}  // namespace worldgen::stages
