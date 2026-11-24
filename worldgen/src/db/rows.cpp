// Typed rows of the region, chronology and history tables (spec P3-04 3.2):
// the inverse of the StageWriter row writers, for history_inputs_from_world,
// read_event_catalogue and HistoryView.
//
// Nothing here throws to the caller: a malformed row is reported as an
// Error, and std::bad_alloc is caught.

#include <cstddef>
#include <new>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "worldgen/db/reader.hpp"
#include "worldgen/db/schema.hpp"

namespace worldgen::db {

namespace {

// Thrown inside a row decoder and turned into an Error by read_rows.
struct RowProblem {
    std::string reason;
};

// Cursor over the columns of one row, in declared order.
class Columns {
public:
    explicit Columns(std::span<const ColumnValue> row) : row_(row) {}

    std::int64_t integer() {
        const ColumnValue& v = next();
        if (v.type != ValueType::integer) {
            throw RowProblem{"column " + format_u64(index_) + " is not an integer"};
        }
        return v.integer;
    }

    std::optional<std::int64_t> nullable_integer() {
        const ColumnValue& v = next();
        if (v.type == ValueType::null) {
            return std::nullopt;
        }
        if (v.type != ValueType::integer) {
            throw RowProblem{"column " + format_u64(index_) + " is not an integer or NULL"};
        }
        return v.integer;
    }

    std::string text() {
        const ColumnValue& v = next();
        if (v.type != ValueType::text) {
            throw RowProblem{"column " + format_u64(index_) + " is not text"};
        }
        return v.bytes;
    }

    // Call after the last column: the row must have no more.
    void end() const {
        if (index_ != row_.size()) {
            throw RowProblem{"has " + format_u64(row_.size()) + " columns, expected " + format_u64(index_)};
        }
    }

private:
    const ColumnValue& next() {
        if (index_ >= row_.size()) {
            throw RowProblem{"has " + format_u64(row_.size()) + " columns, expected more"};
        }
        return row_[index_++];
    }

    std::span<const ColumnValue> row_;
    std::size_t index_ = 0;
};

template <class Row, class Decode>
Result<std::vector<Row>> read_rows(const WorldReader& world, std::string_view table, const Decode& decode) {
    try {
        std::vector<Row> rows;
        std::optional<std::string> problem;
        const Status status = world.visit_table_rows(table, [&](std::span<const ColumnValue> row) {
            if (problem) {
                return;
            }
            try {
                Columns c(row);
                Row r = decode(c);
                c.end();
                rows.push_back(std::move(r));
            } catch (const RowProblem& p) {
                problem = "table '" + std::string(table) + "': row " + format_u64(rows.size() + 1U) + ": " + p.reason;
            }
        });
        if (!status.ok()) {
            return status.error();
        }
        if (problem) {
            return Error{ErrorCode::corrupt_layer, std::move(*problem)};
        }
        return rows;
    } catch (const std::bad_alloc&) {
        return Error{ErrorCode::out_of_memory, "out of memory while reading table '" + std::string(table) + "'"};
    }
}

}  // namespace

Result<std::vector<BiomeRow>> read_biome_rows(const WorldReader& world) {
    return read_rows<BiomeRow>(world, BIOME_TABLE, [](Columns& c) {
        BiomeRow r;
        r.id = c.integer();
        r.key = c.text();
        r.name = c.text();
        r.colour = c.integer();
        r.water = c.integer();
        return r;
    });
}

Result<std::vector<RegionRow>> read_region_rows(const WorldReader& world) {
    return read_rows<RegionRow>(world, REGION_TABLE, [](Columns& c) {
        RegionRow r;
        r.id = c.integer();
        r.name = c.text();
        r.biome = c.integer();
        r.x_min = c.integer();
        r.y_min = c.integer();
        r.x_max = c.integer();
        r.y_max = c.integer();
        r.seed_x = c.integer();
        r.seed_y = c.integer();
        r.landmass = c.integer();
        r.cells = c.integer();
        r.lake_cells = c.integer();
        r.river_cells = c.integer();
        r.river_class_max = c.integer();
        r.coast_cells = c.integer();
        r.walk_cells = c.integer();
        r.walk_area = c.integer();
        r.elev_mean_dm = c.integer();
        r.elev_max_dm = c.integer();
        r.fertility_mean = c.integer();
        r.forage_mean = c.integer();
        r.timber_mean = c.integer();
        r.ore_mean = c.integer();
        r.fresh_water_mean = c.integer();
        r.habitability_mean = c.integer();
        r.habitability_p90 = c.integer();
        r.ore_rich_cells = c.integer();
        r.ore_reach_cells = c.integer();
        return r;
    });
}

Result<std::vector<RegionBorderRow>> read_region_border_rows(const WorldReader& world) {
    return read_rows<RegionBorderRow>(world, REGION_BORDER_TABLE, [](Columns& c) {
        RegionBorderRow r;
        r.a = c.integer();
        r.b = c.integer();
        r.length_m = c.integer();
        r.low_dm = c.integer();
        r.low_x = c.integer();
        r.low_y = c.integer();
        r.walk_pairs = c.integer();
        r.river_pairs = c.integer();
        r.river_class_max = c.integer();
        r.pass_id = c.integer();
        r.passes = c.integer();
        return r;
    });
}

Result<std::vector<RegionBiomeRow>> read_region_biome_rows(const WorldReader& world) {
    return read_rows<RegionBiomeRow>(world, REGION_BIOME_TABLE, [](Columns& c) {
        RegionBiomeRow r;
        r.region = c.integer();
        r.biome = c.integer();
        r.cells = c.integer();
        return r;
    });
}

Result<std::vector<RaceRow>> read_race_rows(const WorldReader& world) {
    return read_rows<RaceRow>(world, RACE_TABLE, [](Columns& c) {
        RaceRow r;
        r.id = c.integer();
        r.key = c.text();
        r.name = c.text();
        r.plural = c.text();
        r.adjective = c.text();
        r.camp = c.integer();
        r.lifespan_years = c.integer();
        r.generation_years = c.integer();
        r.culture = c.integer();
        r.present = c.integer();
        r.awakening_year = c.nullable_integer();
        r.creator_god = c.integer();
        return r;
    });
}

Result<std::vector<RaceBiomeRow>> read_race_biome_rows(const WorldReader& world) {
    return read_rows<RaceBiomeRow>(world, RACE_BIOME_TABLE, [](Columns& c) {
        RaceBiomeRow r;
        r.race = c.integer();
        r.biome = c.integer();
        r.weight = c.integer();
        return r;
    });
}

Result<std::vector<CultureRow>> read_culture_rows(const WorldReader& world) {
    return read_rows<CultureRow>(world, CULTURE_TABLE, [](Columns& c) {
        CultureRow r;
        r.id = c.integer();
        r.key = c.text();
        r.name = c.text();
        r.race = c.integer();
        return r;
    });
}

Result<std::vector<GodRow>> read_god_rows(const WorldReader& world) {
    return read_rows<GodRow>(world, GOD_TABLE, [](Columns& c) {
        GodRow r;
        r.id = c.integer();
        r.name = c.text();
        r.camp = c.integer();
        return r;
    });
}

Result<std::vector<AgeRow>> read_age_rows(const WorldReader& world) {
    return read_rows<AgeRow>(world, AGE_TABLE, [](Columns& c) {
        AgeRow r;
        r.id = c.integer();
        r.race = c.integer();
        r.start_year = c.integer();
        r.end_year = c.integer();
        r.name = c.text();
        return r;
    });
}

Result<std::vector<EventKindRow>> read_event_kind_rows(const WorldReader& world) {
    return read_rows<EventKindRow>(world, EVENT_KIND_TABLE, [](Columns& c) {
        EventKindRow r;
        r.id = c.integer();
        r.key = c.text();
        r.caused = c.integer();
        r.region_use = c.integer();
        r.settlement_use = c.integer();
        r.salience = c.integer();
        r.template_text = c.text();
        return r;
    });
}

Result<std::vector<EventKindRoleRow>> read_event_kind_role_rows(const WorldReader& world) {
    return read_rows<EventKindRoleRow>(world, EVENT_KIND_ROLE_TABLE, [](Columns& c) {
        EventKindRoleRow r;
        r.kind = c.integer();
        r.seq = c.integer();
        r.role = c.text();
        r.entity_type = c.integer();
        r.min_count = c.integer();
        r.max_count = c.integer();
        return r;
    });
}

Result<std::vector<EventKindValueRow>> read_event_kind_value_rows(const WorldReader& world) {
    return read_rows<EventKindValueRow>(world, EVENT_KIND_VALUE_TABLE, [](Columns& c) {
        EventKindValueRow r;
        r.kind = c.integer();
        r.seq = c.integer();
        r.key = c.text();
        return r;
    });
}

Result<std::vector<PhaseRow>> read_phase_rows(const WorldReader& world) {
    return read_rows<PhaseRow>(world, PHASE_TABLE, [](Columns& c) {
        PhaseRow r;
        r.id = c.integer();
        r.age = c.integer();
        r.kind = c.integer();
        r.start_year = c.integer();
        r.end_year = c.integer();
        r.trigger_event = c.integer();
        return r;
    });
}

Result<std::vector<EventRow>> read_event_rows(const WorldReader& world) {
    return read_rows<EventRow>(world, EVENT_TABLE, [](Columns& c) {
        EventRow r;
        r.id = c.integer();
        r.year = c.integer();
        r.end_year = c.integer();
        r.kind = c.integer();
        r.phase = c.integer();
        r.region = c.integer();
        r.settlement = c.integer();
        r.salience = c.integer();
        return r;
    });
}

Result<std::vector<EventParticipantRow>> read_event_participant_rows(const WorldReader& world) {
    return read_rows<EventParticipantRow>(world, EVENT_PARTICIPANT_TABLE, [](Columns& c) {
        EventParticipantRow r;
        r.event = c.integer();
        r.seq = c.integer();
        r.role = c.integer();
        r.entity_type = c.integer();
        r.entity_id = c.integer();
        return r;
    });
}

Result<std::vector<EventCauseRow>> read_event_cause_rows(const WorldReader& world) {
    return read_rows<EventCauseRow>(world, EVENT_CAUSE_TABLE, [](Columns& c) {
        EventCauseRow r;
        r.event = c.integer();
        r.cause = c.integer();
        return r;
    });
}

Result<std::vector<EventValueRow>> read_event_value_rows(const WorldReader& world) {
    return read_rows<EventValueRow>(world, EVENT_VALUE_TABLE, [](Columns& c) {
        EventValueRow r;
        r.event = c.integer();
        r.seq = c.integer();
        r.value = c.integer();
        return r;
    });
}

Result<std::vector<CivilizationRow>> read_civilization_rows(const WorldReader& world) {
    return read_rows<CivilizationRow>(world, CIVILIZATION_TABLE, [](Columns& c) {
        CivilizationRow r;
        r.id = c.integer();
        r.name = c.text();
        r.race = c.integer();
        r.culture = c.integer();
        r.home_region = c.integer();
        r.capital = c.integer();
        r.founded_year = c.integer();
        r.founding_event = c.integer();
        r.ended_year = c.nullable_integer();
        r.ending_event = c.integer();
        return r;
    });
}

Result<std::vector<SettlementRow>> read_settlement_rows(const WorldReader& world) {
    return read_rows<SettlementRow>(world, SETTLEMENT_TABLE, [](Columns& c) {
        SettlementRow r;
        r.id = c.integer();
        r.name = c.text();
        r.x = c.integer();
        r.y = c.integer();
        r.region = c.integer();
        r.founder_civ = c.integer();
        r.founded_year = c.integer();
        r.founding_event = c.integer();
        r.owner_civ = c.integer();
        r.kind = c.integer();
        r.status = c.integer();
        r.population = c.integer();
        r.ruined_year = c.nullable_integer();
        r.ruin_event = c.integer();
        r.ending = c.integer();  // schema 9
        return r;
    });
}

Result<std::vector<SettlementCensusRow>> read_settlement_census_rows(const WorldReader& world) {
    return read_rows<SettlementCensusRow>(world, SETTLEMENT_CENSUS_TABLE, [](Columns& c) {
        SettlementCensusRow r;
        r.settlement = c.integer();
        r.year = c.integer();
        r.owner_civ = c.integer();
        r.kind = c.integer();
        r.status = c.integer();
        r.population = c.integer();
        return r;
    });
}

Result<std::vector<RegionControlRow>> read_region_control_rows(const WorldReader& world) {
    return read_rows<RegionControlRow>(world, REGION_CONTROL_TABLE, [](Columns& c) {
        RegionControlRow r;
        r.region = c.integer();
        r.from_year = c.integer();
        r.to_year = c.nullable_integer();
        r.civ = c.integer();
        r.event = c.integer();
        return r;
    });
}

Result<std::vector<RelationRow>> read_relation_rows(const WorldReader& world) {
    return read_rows<RelationRow>(world, RELATION_TABLE, [](Columns& c) {
        RelationRow r;
        r.a_type = c.integer();
        r.a_id = c.integer();
        r.b_type = c.integer();
        r.b_id = c.integer();
        r.kind = c.text();
        r.since = c.integer();
        r.until = c.nullable_integer();
        r.since_event = c.integer();
        r.until_event = c.integer();
        return r;
    });
}

Result<std::vector<RegionNameRow>> read_region_name_rows(const WorldReader& world) {
    return read_rows<RegionNameRow>(world, REGION_NAME_TABLE, [](Columns& c) {
        RegionNameRow r;
        r.region = c.integer();
        r.from_year = c.integer();
        r.name = c.text();
        r.culture = c.integer();
        r.event = c.integer();
        return r;
    });
}

// Schema 9 (P5-02 11.1).
Result<std::vector<BeastRow>> read_beast_rows(const WorldReader& world) {
    return read_rows<BeastRow>(world, BEAST_TABLE, [](Columns& c) {
        BeastRow r;
        r.id = c.integer();
        r.key = c.text();
        r.name = c.text();
        r.plural = c.text();
        r.menace = c.integer();
        r.food = c.integer();
        r.bold_phrases = c.text();
        return r;
    });
}

Result<std::vector<MonsterKindRow>> read_monster_kind_rows(const WorldReader& world) {
    return read_rows<MonsterKindRow>(world, MONSTER_KIND_TABLE, [](Columns& c) {
        MonsterKindRow r;
        r.id = c.integer();
        r.key = c.text();
        r.name = c.text();
        r.noun = c.text();
        r.grouped = c.integer();
        r.wake = c.integer();
        r.might = c.integer();
        r.raid_phrases = c.text();
        return r;
    });
}

Result<std::vector<BeastPopulationRow>> read_beast_population_rows(const WorldReader& world) {
    return read_rows<BeastPopulationRow>(world, BEAST_POPULATION_TABLE, [](Columns& c) {
        BeastPopulationRow r;
        r.region = c.integer();
        r.beast = c.integer();
        r.year = c.integer();
        r.count = c.integer();
        return r;
    });
}

Result<std::vector<MonsterRow>> read_monster_rows(const WorldReader& world) {
    return read_rows<MonsterRow>(world, MONSTER_TABLE, [](Columns& c) {
        MonsterRow r;
        r.id = c.integer();
        r.name = c.text();
        r.kind = c.integer();
        r.lair_site = c.integer();
        r.born = c.integer();
        r.born_event = c.integer();
        r.died = c.nullable_integer();
        r.death_event = c.integer();
        r.slain_by = c.integer();
        r.status = c.integer();
        r.hoard = c.integer();
        return r;
    });
}

Result<std::vector<SiteRow>> read_site_rows(const WorldReader& world) {
    return read_rows<SiteRow>(world, SITE_TABLE, [](Columns& c) {
        SiteRow r;
        r.id = c.integer();
        r.name = c.text();
        r.kind = c.integer();
        r.region = c.integer();
        r.x = c.integer();
        r.y = c.integer();
        r.settlement = c.integer();
        r.founder_civ = c.integer();
        r.monster = c.integer();
        r.origin_year = c.integer();
        r.origin_event = c.integer();
        r.ended_year = c.nullable_integer();
        r.ending_event = c.integer();
        r.ending = c.integer();
        return r;
    });
}

// Schema 10 (P5-03 16.1).
Result<std::vector<RaceRelationRow>> read_race_relation_rows(const WorldReader& world) {
    return read_rows<RaceRelationRow>(world, RACE_RELATION_TABLE, [](Columns& c) {
        RaceRelationRow r;
        r.race_a = c.integer();
        r.race_b = c.integer();
        r.year = c.integer();
        r.disposition = c.integer();
        r.band = c.integer();
        return r;
    });
}

Result<std::vector<SettlementRaceRow>> read_settlement_race_rows(const WorldReader& world) {
    return read_rows<SettlementRaceRow>(world, SETTLEMENT_RACE_TABLE, [](Columns& c) {
        SettlementRaceRow r;
        r.settlement = c.integer();
        r.race = c.integer();
        r.year = c.integer();
        r.share = c.integer();
        r.standing = c.integer();
        return r;
    });
}

}  // namespace worldgen::db
