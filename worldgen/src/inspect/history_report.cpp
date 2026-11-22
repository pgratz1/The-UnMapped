// The history report, format "# worldgen history v1" (spec P3-07 section 6
// with amendments 21 to 23).

#include "worldgen/inspect/history_report.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "worldgen/db/schema.hpp"
#include "worldgen/history/digest.hpp"
#include "worldgen/history/event_catalogue.hpp"

namespace worldgen::inspect {

namespace {

using history::EntityType;
using history::HistoryView;
using Id = std::int64_t;

std::string num(std::int64_t v) {
    return db::format_i64(v);
}

// pl(n, w) of 6.1.
std::string pl(std::int64_t n, std::string_view word) {
    std::string out = num(n) + " " + std::string(word);
    if (n != 1) {
        out += "s";
    }
    return out;
}

std::string span_of(std::int64_t from, std::int64_t to) {
    return num(from) + "-" + num(to);
}

// An event's span: "<year>" or "<year>-<end_year>".
std::string event_span(const db::EventRow& e) {
    return e.end_year == e.year ? num(e.year) : span_of(e.year, e.end_year);
}

constexpr std::array<std::string_view, 9> PREFIX = {"ra", "g", "c", "s", "rg", "f", "fa", "m", "si"};
constexpr std::array<std::string_view, 4> PHASE_KIND = {"opening", "quiet", "flare_up", "final"};
constexpr std::array<std::string_view, 3> CAMP = {"good", "neutral", "evil"};
constexpr std::array<std::string_view, 5> SETTLEMENT_KIND = {"hamlet", "village", "town", "city", "fortress"};
constexpr std::array<std::string_view, 4> STATUS = {"thriving", "declining", "ruined", "faded"};

template <std::size_t N>
std::string_view word(const std::array<std::string_view, N>& words, std::int64_t code, std::int64_t first) {
    const std::int64_t k = code - first;
    return (k >= 0 && static_cast<std::size_t>(k) < N) ? words[static_cast<std::size_t>(k)] : std::string_view("?");
}

bool standing(std::int64_t status) noexcept {
    return status == 1 || status == 2;
}

// The upland rule of P3-00 7, 4 * walk_cells < cells, without the product:
// cells and walk_cells are only bounded below by 5.3 (0 <= walk_cells <=
// cells, cells >= 1), so 4 * walk_cells could overflow on a damaged file.
bool upland(const history::RegionInfo& r) noexcept {
    return r.walk_cells < r.cells / 4 + (r.cells % 4 != 0 ? 1 : 0);
}

// The two reads of the file (read_history_tables, then HistoryView::load)
// are separate snapshots. Everything the digest and the report index in one
// by a value of the other is checked here, so that a file changed between
// the reads is a corrupt-table error and never an out-of-range index.
std::optional<std::string> reads_disagree(const HistoryView& view, const history::HistoryTables& t) {
    if (static_cast<std::int64_t>(view.events().size()) != t.event_rows) {
        return std::string("corrupt table 'event': the file changed while it was read");
    }
    for (const HistoryView::Event& e : view.events()) {
        if (e.row.phase < 1 || static_cast<std::size_t>(e.row.phase) > t.phases.size()) {
            return std::string("corrupt table 'event': the file changed while it was read");
        }
    }
    return std::nullopt;
}

class Report {
public:
    Report(const HistoryView& view, const history::HistoryTables& t, const history::HistoryDigest& d,
           HistoryLevel level)
        : v_(view), t_(t), d_(d), level_(level) {}

    std::string build() {
        line(std::string(HISTORY_REPORT_HEADER_V1));
        line(std::string("level = ") + (level_ == HistoryLevel::full ? "full" : "summary"));
        settings();
        ages();
        phases();
        civilizations();
        chronicle();
        present();
        stats();
        return std::move(out_);
    }

private:
    // One line: the trailing-space rule of 6.1, then LF.
    void line(std::string text) {
        std::size_t keep = text.size();
        while (keep > 0 && text[keep - 1] == ' ') {
            --keep;
        }
        out_.append(text, 0, keep);
        for (std::size_t i = keep; i < text.size(); ++i) {
            out_ += "\\x20";
        }
        out_ += '\n';
    }

    // --- References (6.1) ---------------------------------------------------

    [[nodiscard]] std::string ref(Id type, Id id) const {
        const std::string tag = std::string(word(PREFIX, type, 1)) + num(id);
        const std::optional<EntityType> t = history::entity_type_from_code(type);
        const std::string* name = t ? v_.name(*t, id) : nullptr;
        if (name == nullptr) {
            return tag;  // types 6 to 9 have no names
        }
        return escape_history_text(*name) + " (" + tag + ")";
    }
    [[nodiscard]] std::string civ(Id id) const { return ref(3, id); }
    [[nodiscard]] std::string settlement(Id id) const { return ref(4, id); }
    [[nodiscard]] std::string region(Id id) const { return ref(5, id); }

    [[nodiscard]] std::string sentence(Id event) const { return escape_history_text(history::render_event(v_, event)); }

    // " (<note>)" or nothing.
    [[nodiscard]] std::string note(Id event, Id civilization) const {
        const std::string n = history::history_note(v_, event, civilization);
        return n.empty() ? std::string() : " (" + escape_history_text(n) + ")";
    }

    [[nodiscard]] const HistoryView::Event& event(Id id) const { return v_.events()[static_cast<std::size_t>(id - 1)]; }
    [[nodiscard]] const history::KindDef& kind_of(const HistoryView::Event& e) const {
        return *v_.catalogue().find_id(e.row.kind);  // checked by HistoryView::load
    }

    [[nodiscard]] const db::AgeRow* age_of(std::int64_t year) const {
        for (const db::AgeRow& a : t_.ages) {
            if (a.start_year <= year && year <= a.end_year) {
                return &a;
            }
        }
        return nullptr;
    }
    [[nodiscard]] const db::RaceRow* race(Id id) const {
        for (const db::RaceRow& r : t_.races) {
            if (r.id == id) {
                return &r;
            }
        }
        return nullptr;
    }
    [[nodiscard]] const db::GodRow* god(Id id) const {
        for (const db::GodRow& g : t_.gods) {
            if (g.id == id) {
                return &g;
            }
        }
        return nullptr;
    }
    [[nodiscard]] const history::RegionInfo* region_row(Id id) const {
        const auto it = std::lower_bound(t_.regions.begin(), t_.regions.end(), id,
                                         [](const history::RegionInfo& r, Id x) { return r.id < x; });
        return it != t_.regions.end() && it->id == id ? &*it : nullptr;
    }
    [[nodiscard]] std::string biome_key(Id id) const {
        for (const auto& [b, key] : t_.biome_keys) {
            if (b == id) {
                return escape_history_text(key);
            }
        }
        return "?";
    }

    // --- [settings] (6.3) ---------------------------------------------------

    void settings() {
        line("[settings]");
        for (const auto& [key, value] : t_.settings) {
            line(value.empty() ? key + " =" : key + " = " + escape_history_text(value));
        }
    }

    // --- [ages] (6.4) -------------------------------------------------------

    void ages() {
        line("[ages]");
        for (const db::AgeRow& a : t_.ages) {
            std::int64_t founded = 0;
            for (const db::CivilizationRow& c : t_.civilizations) {
                founded += (a.start_year <= c.founded_year && c.founded_year <= a.end_year) ? 1 : 0;
            }
            const db::RaceRow* r = race(a.race);          // a present race (P3-03 9.1)
            const db::GodRow* g = god(r->creator_god);    // a god (read_history_tables)
            line("a" + num(a.id) + " " + escape_history_text(a.name) + ": " + span_of(a.start_year, a.end_year) +
                 ", " + pl(a.end_year - a.start_year + 1, "year") + "; " + escape_history_text(r->plural) + " (ra" +
                 num(r->id) + ") made by " + escape_history_text(g->name) + " (g" + num(g->id) + ", " +
                 std::string(word(CAMP, g->camp, 0)) + "); " + pl(founded, "civilization") + " founded");
        }
    }

    // --- [phases] (6.5) -----------------------------------------------------

    void phases() {
        phase_events_.assign(t_.phases.size() + 1, 0);
        phase_shown_.assign(t_.phases.size() + 1, 0);
        for (const HistoryView::Event& e : v_.events()) {
            const auto p = static_cast<std::size_t>(e.row.phase);  // a phase id (read_history_tables)
            ++phase_events_[p];
            phase_shown_[p] += shown(e.row.id) ? 1 : 0;
        }
        line("[phases]");
        for (const db::PhaseRow& p : t_.phases) {
            std::string s = "p" + num(p.id) + " a" + num(p.age) + " " + std::string(word(PHASE_KIND, p.kind, 1)) + " " +
                            span_of(p.start_year, p.end_year) + ", " + pl(p.end_year - p.start_year + 1, "year");
            if (p.trigger_event != 0) {
                s += "; trigger e" + num(p.trigger_event);
            }
            s += "; " + pl(phase_events_[static_cast<std::size_t>(p.id)], "event");
            line(std::move(s));
        }
    }

    [[nodiscard]] bool shown(Id event) const {
        return level_ == HistoryLevel::full || d_.summary[static_cast<std::size_t>(event - 1)] != 0;
    }

    // --- [civilizations] (6.6) ----------------------------------------------

    void civilizations() {
        line("[civilizations]");
        if (t_.civilizations.empty()) {
            line("(none)");
            return;
        }
        for (std::size_t k = 0; k < t_.civilizations.size(); ++k) {
            civilization(t_.civilizations[k], d_.civilizations[k]);
        }
    }

    void civilization(const db::CivilizationRow& c, const history::CivilizationDigest& d) {
        line("c" + num(c.id) + " " + escape_history_text(c.name));
        const db::RaceRow* r = race(c.race);
        line("  race: " + escape_history_text(r->name) + " (ra" + num(r->id) + ")");
        const history::RegionInfo* home = region_row(c.home_region);
        line("  home: " + region(c.home_region) + ", " + biome_key(home->biome) +
             (upland(*home) ? ", upland" : ""));
        const db::AgeRow* age = age_of(c.founded_year);
        line("  founded: " + num(c.founded_year) + " in " + escape_history_text(age->name) + " (a" + num(age->id) +
             "), e" + num(c.founding_event));
        line(c.capital != 0 ? "  capital: " + settlement(c.capital) : std::string("  capital: none"));
        line(d.peak_regions.count > 0 ? "  peak regions: " + num(d.peak_regions.count) + ", " +
                                            span_of(d.peak_regions.from_year, d.peak_regions.to_year)
                                      : std::string("  peak regions: 0"));
        line(d.peak_population.population > 0
                 ? "  peak population: " + num(d.peak_population.population) + " in " + num(d.peak_population.year)
                 : std::string("  peak population: 0"));
        line("  settlements: " + num(d.settlements_founded) + " founded, " + num(d.settlements_held) +
             " held at the end, " + num(d.settlements_ruined) + " ruined");
        relations(d);
        line("  events: " + num(d.events));
        if (!c.ended_year) {
            line("  fate: alive in " + num(t_.end_year) + ": " + pl(d.regions_held, "region") + ", " +
                 pl(d.settlements_held, "settlement") + ", population " + num(d.population));
        } else {
            line("  fate: ended in " + num(*c.ended_year) + ", e" + num(c.ending_event) + ": " +
                 sentence(c.ending_event));
        }
        line("  milestones:");
        for (const Id m : d.milestones) {
            const HistoryView::Event& e = event(m);
            line("    " + event_span(e.row) + " e" + num(m) + " " + escape_history_text(kind_of(e).key) + ": " +
                 sentence(m) + note(m, c.id));
        }
    }

    void relations(const history::CivilizationDigest& d) {
        if (d.relations.empty()) {
            line("  relations: none");
            return;
        }
        std::vector<std::pair<std::string, std::int64_t>> kinds;  // ascending bytes
        for (const std::size_t i : d.relations) {
            const std::string& k = t_.relations[i].kind;
            const auto it = std::lower_bound(kinds.begin(), kinds.end(), k,
                                             [](const std::pair<std::string, std::int64_t>& a, const std::string& b) {
                                                 return a.first < b;
                                             });
            if (it != kinds.end() && it->first == k) {
                ++it->second;
            } else {
                kinds.insert(it, {k, 1});
            }
        }
        std::string s = "  relations: " + num(static_cast<std::int64_t>(d.relations.size())) + " (";
        for (std::size_t k = 0; k < kinds.size(); ++k) {
            s += (k > 0 ? ", " : "") + escape_history_text(kinds[k].first) + " " + num(kinds[k].second);
        }
        line(s + ")");
        for (const std::size_t i : d.relations) {
            const db::RelationRow& r = t_.relations[i];
            std::string l = "    " + escape_history_text(r.kind) + ": " + ref(r.a_type, r.a_id) + ", " +
                            ref(r.b_type, r.b_id) + ", ";
            if (r.until) {
                // r6: a relation that began and ended within one year.
                const std::int64_t last = *r.until == r.since ? r.since : *r.until - 1;
                l += span_of(r.since, last) + ", e" + num(r.since_event) + " to e" + num(r.until_event);
            } else {
                l += "since " + num(r.since) + ", e" + num(r.since_event);
            }
            line(std::move(l));
        }
    }

    // --- [chronicle] (6.7, 21.3, 21.4) --------------------------------------

    void chronicle() {
        const auto events = static_cast<std::int64_t>(v_.events().size());
        line("[chronicle]");
        line("threshold = " + num(level_ == HistoryLevel::full ? 0 : d_.summary_threshold));
        line("events = " + num(events));
        line("shown = " + num(level_ == HistoryLevel::full ? events : d_.summary_events));
        std::size_t next = 0;  // the next event to print, index id - 1
        const std::span<const HistoryView::Event> all = v_.events();
        for (const db::AgeRow& a : t_.ages) {
            line("== a" + num(a.id) + " " + escape_history_text(a.name) + ": " + span_of(a.start_year, a.end_year));
            for (const db::PhaseRow& p : t_.phases) {
                if (p.age != a.id) {
                    continue;
                }
                line("-- p" + num(p.id) + " " + std::string(word(PHASE_KIND, p.kind, 1)) + ": " +
                     span_of(p.start_year, p.end_year));
                // The events of a phase are consecutive ids (5.3).
                while (next < all.size() && all[next].row.phase == p.id) {
                    if (shown(all[next].row.id)) {
                        event_line(all[next]);
                    }
                    ++next;
                }
                const std::int64_t hidden = phase_events_[static_cast<std::size_t>(p.id)] -
                                            phase_shown_[static_cast<std::size_t>(p.id)];
                if (level_ == HistoryLevel::summary && hidden > 0) {
                    line("(" + pl(hidden, "more event") + " not shown)");
                }
            }
        }
    }

    void event_line(const HistoryView::Event& e) {
        const history::KindDef& k = kind_of(e);
        std::string refs;
        const auto add = [&refs](const std::string& item) {
            refs += refs.empty() ? "" : " ";
            refs += item;
        };
        if (e.row.region != 0) {
            add("@rg" + num(e.row.region));
        }
        if (e.row.settlement != 0) {
            add("@s" + num(e.row.settlement));
        }
        for (const db::EventParticipantRow& p : e.participants) {
            const history::RoleDef& role = k.roles[static_cast<std::size_t>(p.role - 1)];
            add(escape_history_text(role.role) + "=" + std::string(word(PREFIX, p.entity_type, 1)) + num(p.entity_id));
        }
        for (std::size_t i = 0; i < e.values.size(); ++i) {
            add(escape_history_text(k.values[i]) + "=" + num(e.values[i]));
        }
        std::string s = event_span(e.row) + " e" + num(e.row.id) + " " + escape_history_text(k.key) + "/" +
                        num(e.row.salience) + ": " + sentence(e.row.id) + note(e.row.id, 0) + " {" + refs + "}";
        if (!e.causes.empty()) {
            s += " [causes ";
            for (std::size_t i = 0; i < e.causes.size(); ++i) {
                s += (i > 0 ? ", e" : "e") + num(e.causes[i]);
            }
            s += "]";
        }
        line(std::move(s));
    }

    // --- [present] (6.8) ----------------------------------------------------

    void present() {
        line("[present]");
        line("year: " + num(t_.end_year));
        std::int64_t alive = 0;
        for (const db::CivilizationRow& c : t_.civilizations) {
            alive += c.ended_year ? 0 : 1;
        }
        line("civilizations alive: " + num(alive) + " of " + num(static_cast<std::int64_t>(t_.civilizations.size())));
        for (std::size_t k = 0; k < t_.civilizations.size(); ++k) {
            const db::CivilizationRow& c = t_.civilizations[k];
            const history::CivilizationDigest& d = d_.civilizations[k];
            if (!c.ended_year) {
                line("c" + num(c.id) + " " + escape_history_text(c.name) + ": " + pl(d.regions_held, "region") + ", " +
                     pl(d.settlements_held, "settlement") + ", population " + num(d.population));
            }
        }
        const auto regions = static_cast<std::int64_t>(t_.regions.size());
        std::int64_t held = 0;
        for (const db::RegionControlRow& r : t_.region_control) {
            held += r.to_year ? 0 : 1;
        }
        line("regions: " + num(regions) + "; held " + num(held) + ", free " + num(regions - held));
        if (level_ == HistoryLevel::full) {
            for (const history::RegionInfo& g : t_.regions) {
                const db::RegionControlRow* open = nullptr;
                const auto first = std::lower_bound(t_.region_control.begin(), t_.region_control.end(), g.id,
                                                    [](const db::RegionControlRow& r, Id x) { return r.region < x; });
                for (auto it = first; it != t_.region_control.end() && it->region == g.id; ++it) {
                    if (!it->to_year) {
                        open = &*it;
                    }
                }
                line("  rg" + num(g.id) + " " + escape_history_text(g.name) + ": " +
                     (open != nullptr ? civ(open->civ) + " since " + num(open->from_year) : std::string("free")));
            }
        }
        std::array<std::int64_t, 5> status{};
        std::array<std::int64_t, 5> kind{};
        const db::SettlementRow* largest = nullptr;
        for (const db::SettlementRow& s : t_.settlements) {
            ++status[static_cast<std::size_t>(s.status - 1)];
            ++kind[static_cast<std::size_t>(s.kind - 1)];
            if (standing(s.status) && (largest == nullptr || s.population > largest->population)) {
                largest = &s;
            }
        }
        line("settlements: " + num(static_cast<std::int64_t>(t_.settlements.size())) + "; thriving " + num(status[0]) +
             ", declining " + num(status[1]) + ", ruined " + num(status[2]) + ", faded " + num(status[3]));
        if (level_ == HistoryLevel::full) {
            for (const db::SettlementRow& s : t_.settlements) {
                settlement_line(s);
            }
        }
        line("settlement kinds: hamlet " + num(kind[0]) + ", village " + num(kind[1]) + ", town " + num(kind[2]) +
             ", city " + num(kind[3]) + ", fortress " + num(kind[4]));
        line(largest != nullptr
                 ? "largest settlement: " + settlement(largest->id) + ", population " + num(largest->population)
                 : std::string("largest settlement: none"));
    }

    void settlement_line(const db::SettlementRow& s) {
        std::string l = "  s" + num(s.id) + " " + escape_history_text(s.name) + ": " +
                        std::string(word(SETTLEMENT_KIND, s.kind, 1)) + ", " + std::string(word(STATUS, s.status, 1)) +
                        ", population " + num(s.population) + ", " +
                        (s.owner_civ != 0 ? civ(s.owner_civ) : std::string("no owner")) + ", at " + num(s.x) + " " +
                        num(s.y) + " in " + region(s.region) + ", founded " + num(s.founded_year) + " by " +
                        civ(s.founder_civ);
        if (s.ruined_year) {
            l += ", ruined " + num(*s.ruined_year) + ", e" + num(s.ruin_event);
        }
        line(std::move(l));
    }

    // --- [stats] (6.9) ------------------------------------------------------

    void stats() {
        line("[stats]");
        for (const auto& [key, value] : d_.stats) {
            line(escape_history_text(key) + " = " + num(value));
        }
    }

    const HistoryView& v_;
    const history::HistoryTables& t_;
    const history::HistoryDigest& d_;
    HistoryLevel level_;
    std::string out_;
    std::vector<std::int64_t> phase_events_;  // index phase id
    std::vector<std::int64_t> phase_shown_;
};

}  // namespace

std::optional<HistoryLevel> parse_history_level(std::string_view text) {
    if (text == "summary") {
        return HistoryLevel::summary;
    }
    if (text == "full") {
        return HistoryLevel::full;
    }
    return std::nullopt;
}

std::string escape_history_text(std::string_view text) {
    static constexpr char HEX[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(text.size());
    for (const char ch : text) {
        const auto b = static_cast<unsigned char>(ch);
        if (b == 0x5CU) {
            out += "\\\\";
        } else if (b < 0x20U || b >= 0x7FU) {
            out += "\\x";
            out += HEX[b >> 4U];
            out += HEX[b & 0x0FU];
        } else {
            out += ch;
        }
    }
    return out;
}

db::Result<std::string> build_history_report(const db::WorldReader& world, HistoryLevel level) {
    try {
        db::Result<history::HistoryTables> tables = history::read_history_tables(world);
        if (!tables.ok()) {
            return tables.error();
        }
        db::Result<HistoryView> view = HistoryView::load(world);
        if (!view.ok()) {
            return view.error();
        }
        if (const std::optional<std::string> why = reads_disagree(view.value(), tables.value())) {
            return db::Error{db::ErrorCode::corrupt_layer, *why};
        }
        const history::HistoryDigest digest = history::digest_history(view.value(), tables.value());
        return Report(view.value(), tables.value(), digest, level).build();
    } catch (const std::bad_alloc&) {
        return db::Error{db::ErrorCode::out_of_memory, "out of memory while building the history report"};
    }
}

}  // namespace worldgen::inspect
