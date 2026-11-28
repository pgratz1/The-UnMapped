// The browser of civilizations and events (spec P4-05 section 4).

#include "worldgen/inspect/history_browser.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "worldgen/core/error.hpp"
#include "worldgen/db/schema.hpp"

namespace worldgen::inspect {

namespace {

using history::EntityType;
using history::HistoryView;
using history::LoadedHistory;

std::string num(std::int64_t v) {
    return db::format_i64(v);
}

[[noreturn]] void browser_fatal(const std::string& what) {
    throw FatalError("browser: " + what);
}

struct GroupRow {
    std::string_view key;
    EventGroup group;
};

// 4.4, by key; P5-02's kinds 44 to 55 in group monsters (section 9).
constexpr std::array<GroupRow, 116> GROUPS = {{
    {"awakening", EventGroup::realms},
    {"civ_founded", EventGroup::realms},
    {"civ_seceded", EventGroup::realms},
    {"civ_ended", EventGroup::realms},
    {"civ_conquered", EventGroup::realms},
    {"capital_moved", EventGroup::realms},
    {"succession_crisis", EventGroup::realms},
    {"quiet_era", EventGroup::realms},
    {"quiet_stretch", EventGroup::realms},
    {"region_renamed", EventGroup::realms},
    {"settlement_founded", EventGroup::settlements},
    {"colony_founded", EventGroup::settlements},
    {"became_village", EventGroup::settlements},
    {"became_town", EventGroup::settlements},
    {"became_city", EventGroup::settlements},
    {"settlement_dwindled", EventGroup::settlements},
    {"settlement_abandoned", EventGroup::settlements},
    {"settlement_abandoned_to", EventGroup::settlements},
    {"ruin_faded", EventGroup::settlements},
    {"refugees_settled", EventGroup::settlements},
    {"refuge_hidden", EventGroup::settlements},
    {"flare_up", EventGroup::war},
    {"claim", EventGroup::war},
    {"claim_settlements", EventGroup::war},
    {"claim_dropped", EventGroup::war},
    {"war_declared", EventGroup::war},
    {"battle", EventGroup::war},
    {"battle_at_pass", EventGroup::war},
    {"siege", EventGroup::war},
    {"sacked", EventGroup::war},
    {"settlement_captured", EventGroup::war},
    {"peace", EventGroup::war},
    {"truce", EventGroup::war},
    {"war_won", EventGroup::war},
    {"region_ceded", EventGroup::war},
    {"settlements_ceded", EventGroup::war},
    {"tribute", EventGroup::war},
    {"search_abandoned", EventGroup::war},
    {"refuge_held", EventGroup::war},
    {"famine", EventGroup::calamity},
    {"plague", EventGroup::calamity},
    {"plague_spread", EventGroup::calamity},
    {"plague_outbreak", EventGroup::calamity},
    {"monster_appeared", EventGroup::monsters},
    {"undead_rose", EventGroup::monsters},
    {"monster_woke", EventGroup::monsters},
    {"monster_slept", EventGroup::monsters},
    {"monster_raid", EventGroup::monsters},
    {"settlement_overrun", EventGroup::monsters},
    {"monster_slain", EventGroup::monsters},
    {"monster_driven_off", EventGroup::monsters},
    {"monster_died", EventGroup::monsters},
    {"beasts_bold", EventGroup::monsters},
    {"outpost_built", EventGroup::monsters},
    {"outpost_abandoned", EventGroup::monsters},
    // P5-03 (lead's ruling): the flight for fear with the monsters, kinds 57
    // to 62 in the new group peoples (code 6).
    {"settlement_fled", EventGroup::monsters},
    {"peoples_met", EventGroup::peoples},
    {"disposition_turned", EventGroup::peoples},
    {"refugees_arrived", EventGroup::peoples},
    {"settlement_mixed", EventGroup::peoples},
    {"standing_changed", EventGroup::peoples},
    {"people_expelled", EventGroup::peoples},
    // P5-04 11.2: kinds 63 to 70, 72 to 76 and 78 in the new group alliances
    // (code 7); the allied battle with the war, the mine with the settlements.
    {"alliance_formed", EventGroup::alliances},
    {"alliance_renewed", EventGroup::alliances},
    {"alliance_lapsed", EventGroup::alliances},
    {"alliance_broken", EventGroup::alliances},
    {"alliance_betrayed", EventGroup::alliances},
    {"ally_joined_war", EventGroup::alliances},
    {"ally_refused", EventGroup::alliances},
    {"ally_withdrew", EventGroup::alliances},
    {"allied_battle", EventGroup::war},
    {"trade_opened", EventGroup::alliances},
    {"trade_broken", EventGroup::alliances},
    {"spoils_shared", EventGroup::alliances},
    {"spoils_withheld", EventGroup::alliances},
    {"alliance_spurned", EventGroup::alliances},
    {"mine_opened", EventGroup::settlements},
    {"arms_sold", EventGroup::alliances},
    // P5-06 11.3: kinds 79 to 84, 86, 87 and 89 to 91 in the new group
    // figures (code 8); the battle led with the war, the hero's slaying with
    // the monsters, the blight with the calamities.
    {"came_to_rule", EventGroup::figures},
    {"figure_died", EventGroup::figures},
    {"succession_disputed", EventGroup::figures},
    {"exiled", EventGroup::figures},
    {"reigned_at_end", EventGroup::figures},
    {"general_appointed", EventGroup::figures},
    {"battle_led", EventGroup::war},
    {"fell_in_battle", EventGroup::figures},
    {"hero_rose", EventGroup::figures},
    {"monster_slain_by_hero", EventGroup::monsters},
    {"hero_slain", EventGroup::figures},
    {"tower_founded", EventGroup::figures},
    {"book_written", EventGroup::figures},
    {"blight", EventGroup::calamity},
    // P5-07 11.1: the deepening with the mines, the waking, the seal and the
    // lost hold with the monsters.
    {"delving_deepened", EventGroup::settlements},
    {"deep_woken", EventGroup::monsters},
    {"deep_sealed", EventGroup::monsters},
    {"hold_lost", EventGroup::monsters},
    // r2: the aid with the alliances, the warren's delving with the
    // settlements, its seal with the monsters.
    {"ally_aided_hold", EventGroup::alliances},
    {"warren_delved", EventGroup::settlements},
    {"warren_sealed", EventGroup::monsters},
    // P5-08 11.1: kinds 100 to 108 in the new group gods (code 9); the
    // tower's take-up and lapse with the figures.
    {"divine_bless", EventGroup::gods},
    {"divine_curse", EventGroup::gods},
    {"divine_champion", EventGroup::gods},
    {"divine_monster", EventGroup::gods},
    {"divine_mark", EventGroup::gods},
    {"cult_founded", EventGroup::gods},
    {"cult_spread", EventGroup::gods},
    {"cult_adopted", EventGroup::gods},
    {"cult_suppressed", EventGroup::gods},
    {"tower_taken_up", EventGroup::figures},
    {"tower_abandoned", EventGroup::figures},
    // P5-11 11.1: kinds 111 to 116 in the new group artifacts (code 10).
    {"artifact_made", EventGroup::artifacts},
    {"artifact_taken", EventGroup::artifacts},
    {"artifact_lost", EventGroup::artifacts},
    {"artifact_hoarded", EventGroup::artifacts},
    {"artifact_recovered", EventGroup::artifacts},
    {"artifact_buried", EventGroup::artifacts},
}};

std::string span_text(std::int64_t year, std::int64_t end_year) {
    return end_year == year ? num(year) : num(year) + "-" + num(end_year);
}

bool contains(std::span<const std::int64_t> sorted, std::int64_t id) {
    return std::binary_search(sorted.begin(), sorted.end(), id);
}

char fold(char c) noexcept {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

// `query` already folded.
bool matches(std::string_view name, std::string_view query) {
    if (query.size() > name.size()) {
        return false;
    }
    for (std::size_t i = 0; i + query.size() <= name.size(); ++i) {
        bool all = true;
        for (std::size_t k = 0; k < query.size(); ++k) {
            if (fold(name[i + k]) != query[k]) {
                all = false;
                break;
            }
        }
        if (all) {
            return true;
        }
    }
    return false;
}

}  // namespace

EventGroup event_group(std::string_view kind_key) noexcept {
    for (const GroupRow& row : GROUPS) {
        if (row.key == kind_key) {
            return row.group;
        }
    }
    return EventGroup::other;
}

std::string_view event_group_word(EventGroup group) noexcept {
    switch (group) {
        case EventGroup::realms: return "realms";
        case EventGroup::settlements: return "settlements";
        case EventGroup::war: return "war";
        case EventGroup::calamity: return "calamity";
        case EventGroup::other: return "other";
        case EventGroup::monsters: return "monsters";
        case EventGroup::peoples: return "peoples";
        case EventGroup::alliances: return "alliances";
        case EventGroup::figures: return "figures";
        case EventGroup::gods: return "gods";
        case EventGroup::artifacts: return "artifacts";
    }
    return "other";
}

db::Result<BrowserIndex> BrowserIndex::build(const LoadedHistory& history) {
    try {
        BrowserIndex index;
        const HistoryView& view = history.view;
        const history::HistoryTables& tables = history.tables;
        index.digest_ = history_report_digest(view, tables);
        // Per stored kind (index id - 1): its group.
        const std::vector<history::KindDef>& kinds = view.catalogue().kinds;
        std::vector<EventGroup> kind_group;
        kind_group.reserve(kinds.size());
        for (const history::KindDef& k : kinds) {
            kind_group.push_back(event_group(k.key));
        }
        const std::span<const HistoryView::Event> events = view.events();
        if (index.digest_.summary.size() != events.size()) {
            browser_fatal("the digest has " + num(static_cast<std::int64_t>(index.digest_.summary.size())) +
                          " events, the log " + num(static_cast<std::int64_t>(events.size())));
        }
        index.events_.reserve(events.size());
        for (std::size_t i = 0; i < events.size(); ++i) {
            const db::EventRow& r = events[i].row;
            if (r.id != static_cast<std::int64_t>(i) + 1 || r.kind < 1 ||
                static_cast<std::size_t>(r.kind) > kind_group.size()) {
                browser_fatal("event " + num(r.id) + " out of order or of no kind");  // checked by load_history
            }
            EventEntry e;
            e.id = r.id;
            e.year = r.year;
            e.end_year = r.end_year;
            e.kind = r.kind;
            e.salience = r.salience;
            e.region = r.region;
            e.settlement = r.settlement;
            e.group = kind_group[static_cast<std::size_t>(r.kind - 1)];
            e.summary = index.digest_.summary[i] != 0;
            index.events_.push_back(e);
        }
        if (index.digest_.civilizations.size() != tables.civilizations.size()) {
            browser_fatal("the digest and the tables disagree on the civilizations");
        }
        index.civilizations_.reserve(tables.civilizations.size());
        for (std::size_t k = 0; k < tables.civilizations.size(); ++k) {
            const db::CivilizationRow& c = tables.civilizations[k];
            CivilizationEntry entry;
            entry.id = c.id;
            entry.race = c.race;
            entry.founded_year = c.founded_year;
            entry.ended_year = c.ended_year ? *c.ended_year : -1;
            entry.ending_event = c.ended_year ? c.ending_event : 0;
            entry.events = index.digest_.civilizations[k].events;
            entry.name = c.name;
            for (const db::RaceRow& r : tables.races) {
                if (r.id == c.race) {
                    entry.race_name = r.name;
                    break;
                }
            }
            index.civilizations_.push_back(std::move(entry));
        }
        // P5-06 11.3: the figures, ascending id.
        index.figures_.reserve(tables.figures.size());
        for (const db::FigureRow& f : tables.figures) {
            FigureEntry entry;
            entry.id = f.id;
            entry.role = f.role;
            entry.civ = f.civ;
            entry.born = f.born;
            entry.died = f.died ? *f.died : -1;
            entry.name = f.name;
            entry.display_name = history::figure_display_name(f);
            index.figures_.push_back(std::move(entry));
        }
        return index;
    } catch (const std::bad_alloc&) {
        return db::Error{db::ErrorCode::out_of_memory, "out of memory while building the browser"};
    }
}

void BrowserIndex::check_history(const LoadedHistory& history) const {
    if (history.view.events().size() != events_.size() ||
        history.tables.civilizations.size() != civilizations_.size() ||
        history.tables.figures.size() != figures_.size()) {
        browser_fatal("the history is not the one the index was built from");
    }
}

const EventEntry& BrowserIndex::entry(std::int64_t event) const {
    if (event < 1 || static_cast<std::uint64_t>(event) > events_.size()) {
        browser_fatal("no event " + num(event));
    }
    return events_[static_cast<std::size_t>(event - 1)];
}

void BrowserIndex::select(const LoadedHistory& history, const EventFilter& filter,
                          std::vector<std::int64_t>& out) const {
    check_history(history);
    out.clear();
    const history::Timeline& tl = history.timeline;
    // The entity lists of the non-zero fields, in field order.
    std::array<std::span<const std::int64_t>, 4> lists{};
    std::size_t n = 0;
    if (filter.civilization != 0) {
        if (filter.civilization < 1 || static_cast<std::uint64_t>(filter.civilization) > tl.civilization_count()) {
            browser_fatal("no civilization " + num(filter.civilization));
        }
        lists[n++] = tl.events_of_civilization(filter.civilization);
    }
    if (filter.settlement != 0) {
        if (filter.settlement < 1 || static_cast<std::uint64_t>(filter.settlement) > tl.settlement_count()) {
            browser_fatal("no settlement " + num(filter.settlement));
        }
        lists[n++] = tl.events_of_settlement(filter.settlement);
    }
    if (filter.region != 0) {
        if (!contains(tl.region_ids(), filter.region)) {
            browser_fatal("no region " + num(filter.region));
        }
        lists[n++] = tl.events_of_region(filter.region);
    }
    if (filter.figure != 0) {  // P5-06 11.3
        if (filter.figure < 1 || static_cast<std::uint64_t>(filter.figure) > tl.figure_count()) {
            browser_fatal("no figure " + num(filter.figure));
        }
        lists[n++] = tl.events_of_figure(filter.figure);
    }
    const auto keep = [&](const EventEntry& e) {
        for (std::size_t k = 1; k < n; ++k) {
            if (!contains(lists[k], e.id)) {
                return false;
            }
        }
        if (filter.to_year >= 0 && e.year > filter.to_year) {
            return false;
        }
        if (filter.from_year >= 0 && e.end_year < filter.from_year) {
            return false;
        }
        if ((filter.groups & (std::uint32_t{1} << static_cast<std::uint32_t>(e.group))) == 0U) {
            return false;
        }
        return !filter.summary_only || e.summary;
    };
    if (n == 0) {
        for (const EventEntry& e : events_) {
            if (keep(e)) {
                out.push_back(e.id);
            }
        }
        return;
    }
    for (const std::int64_t id : lists[0]) {
        if (keep(entry(id))) {
            out.push_back(id);
        }
    }
}

EventDetail BrowserIndex::event_detail(const LoadedHistory& history, std::int64_t event) const {
    check_history(history);
    const EventEntry& en = entry(event);
    const HistoryView& view = history.view;
    const HistoryView::Event& e = view.events()[static_cast<std::size_t>(event - 1)];
    const history::KindDef* kind = view.catalogue().find_id(e.row.kind);
    if (kind == nullptr) {
        browser_fatal("event " + num(event) + ": no kind " + num(e.row.kind));  // checked by load_history
    }
    EventDetail d;
    d.entry = en;
    d.kind_key = kind->key;
    d.sentence = history::render_event_linked(view, event);
    d.note = history::history_note(view, event, 0);
    for (const db::EventParticipantRow& p : e.participants) {
        Participant part;
        if (p.role >= 1 && static_cast<std::size_t>(p.role) <= kind->roles.size()) {
            part.role = kind->roles[static_cast<std::size_t>(p.role - 1)].role;
        }
        part.type = p.entity_type;
        part.id = p.entity_id;
        const std::optional<EntityType> t = history::entity_type_from_code(p.entity_type);
        const std::string* name = !t ? nullptr
                                  : *t == EntityType::region ? view.region_name_at_event(p.entity_id, event)
                                                             : view.name(*t, p.entity_id);
        if (name != nullptr) {
            part.name = *name;
        }
        d.participants.push_back(std::move(part));
    }
    for (std::size_t i = 0; i < e.values.size() && i < kind->values.size(); ++i) {
        d.values.emplace_back(kind->values[i], e.values[i]);
    }
    d.causes = e.causes;
    const std::span<const std::int64_t> effects = history.timeline.effects(event);
    d.effects.assign(effects.begin(), effects.end());
    return d;
}

std::vector<history::LinkedText> BrowserIndex::block(const LoadedHistory& history, std::int64_t civ,
                                                     HistoryLevel level) const {
    check_history(history);
    if (level != HistoryLevel::summary && level != HistoryLevel::full) {
        browser_fatal("bad level");
    }
    const auto it = std::lower_bound(civilizations_.begin(), civilizations_.end(), civ,
                                     [](const CivilizationEntry& c, std::int64_t x) { return c.id < x; });
    if (it == civilizations_.end() || it->id != civ) {
        browser_fatal("no civilization " + num(civ));
    }
    std::vector<history::LinkedText> out;
    for (const BlockLine& line : civilization_block(history.view, history.tables, digest_, civ, level)) {
        out.push_back(block_line_display(history.view, line));
    }
    return out;
}

std::vector<history::LinkedText> BrowserIndex::figure_block(const LoadedHistory& history, std::int64_t figure,
                                                            HistoryLevel level) const {
    check_history(history);
    if (level != HistoryLevel::summary && level != HistoryLevel::full) {
        browser_fatal("bad level");
    }
    if (figure < 1 || static_cast<std::uint64_t>(figure) > figures_.size()) {
        browser_fatal("no figure " + num(figure));
    }
    std::vector<history::LinkedText> out;
    for (const BlockLine& line : inspect::figure_block(history.view, history.tables, digest_, figure, level)) {
        out.push_back(block_line_display(history.view, line));
    }
    return out;
}

std::vector<history::LinkedText> BrowserIndex::god_block(const LoadedHistory& history, std::int64_t god) const {
    check_history(history);
    bool known = false;
    for (const db::GodRow& g : history.view.god_rows()) {
        known = known || g.id == god;
    }
    if (!known) {
        browser_fatal("no god " + num(god));
    }
    std::vector<history::LinkedText> out;
    for (const BlockLine& line : inspect::god_block(history.view, history.tables, god)) {
        out.push_back(block_line_display(history.view, line));
    }
    return out;
}

std::vector<history::LinkedText> BrowserIndex::cult_block(const LoadedHistory& history, std::int64_t faction) const {
    check_history(history);
    if (faction < 1 || static_cast<std::uint64_t>(faction) > history.tables.factions.size()) {
        browser_fatal("no cult " + num(faction));
    }
    std::vector<history::LinkedText> out;
    for (const BlockLine& line : inspect::cult_block(history.view, history.tables, faction)) {
        out.push_back(block_line_display(history.view, line));
    }
    return out;
}

std::vector<history::LinkedText> BrowserIndex::artifact_block(const LoadedHistory& history,
                                                              std::int64_t artifact) const {
    check_history(history);
    if (artifact < 1 || static_cast<std::uint64_t>(artifact) > history.tables.artifacts.size()) {
        browser_fatal("no artifact " + num(artifact));
    }
    std::vector<history::LinkedText> out;
    for (const BlockLine& line : inspect::artifact_block(history.view, history.tables, artifact)) {
        out.push_back(block_line_display(history.view, line));
    }
    return out;
}

std::string BrowserIndex::row_text(const LoadedHistory& history, std::int64_t event) const {
    check_history(history);
    const EventEntry& e = entry(event);
    std::string out = span_text(e.year, e.end_year) + "  " + history::render_event(history.view, event);
    const std::string note = history::history_note(history.view, event, 0);
    if (!note.empty()) {
        out += " (" + note + ")";
    }
    return out;
}

std::vector<NameMatch> BrowserIndex::search(const LoadedHistory& history, std::string_view query,
                                            std::size_t limit) const {
    check_history(history);
    std::vector<NameMatch> out;
    if (query.empty() || limit == 0) {
        return out;
    }
    std::string q(query);
    for (char& c : q) {
        c = fold(c);
    }
    const HistoryView& view = history.view;
    const auto add = [&](EntityType type, std::int64_t id, const std::string& name) {
        out.push_back(NameMatch{static_cast<std::int64_t>(type), id, name});
        return out.size() >= limit;
    };
    for (const auto& [id, name] : view.civilizations()) {
        if (matches(name, q) && add(EntityType::civilization, id, name)) {
            return out;
        }
    }
    for (const auto& [id, name] : view.settlements()) {
        if (matches(name, q) && add(EntityType::settlement, id, name)) {
            return out;
        }
    }
    const std::int64_t end_year = history.timeline.end_year();
    const std::span<const db::RegionNameRow> renamed = view.region_names();  // primary-key order
    for (const auto& [id, geographic] : view.regions()) {
        const std::string* found = nullptr;
        const std::string* present = view.region_name_at_year(id, end_year);
        if (present != nullptr && matches(*present, q)) {
            found = present;
        } else if (matches(geographic, q)) {
            found = &geographic;
        } else {
            const std::int64_t region = id;
            auto it = std::lower_bound(renamed.begin(), renamed.end(), region,
                                       [](const db::RegionNameRow& r, std::int64_t x) { return r.region < x; });
            for (; it != renamed.end() && it->region == region; ++it) {
                if (matches(it->name, q)) {
                    found = &it->name;
                    break;
                }
            }
        }
        if (found != nullptr && add(EntityType::region, id, *found)) {
            return out;
        }
    }
    // P5-06 11.3: figures by display name, after the regions.
    for (const FigureEntry& f : figures_) {
        if (matches(f.display_name, q) && add(EntityType::figure, f.id, f.display_name)) {
            return out;
        }
    }
    // P5-11 11.1: artifacts by name, after the figures.
    for (const db::ArtifactRow& a : view.artifact_rows()) {
        if (matches(a.name, q) && add(EntityType::artifact, a.id, a.name)) {
            return out;
        }
    }
    return out;
}

}  // namespace worldgen::inspect
