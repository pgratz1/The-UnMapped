// The history report, format "# worldgen history v4" (spec P3-07 section 6
// with amendments 21 to 26; P5-02 12, generator 0.23.0).

#include "worldgen/inspect/history_report.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
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
constexpr std::array<std::string_view, 3> CAMP = {"good", "neutral", "evil"};

template <std::size_t N>
std::string_view word(const std::array<std::string_view, N>& words, std::int64_t code, std::int64_t first) {
    const std::int64_t k = code - first;
    return (k >= 0 && static_cast<std::size_t>(k) < N) ? words[static_cast<std::size_t>(k)] : std::string_view("?");
}

// The phase kind word of the report: history::phase_kind_word, "?" for a
// kind it does not know (as the other code tables here).
std::string_view phase_word(std::int64_t kind) noexcept {
    const std::string_view w = history::phase_kind_word(kind);
    return w.empty() ? std::string_view("?") : w;
}

// The settlement words of the report: history::settlement_kind_word and
// settlement_status_word, "?" for a code they do not know.
std::string_view kind_word(std::int64_t kind) noexcept {
    const std::string_view w = history::settlement_kind_word(kind);
    return w.empty() ? std::string_view("?") : w;
}
std::string_view status_word(std::int64_t status) noexcept {
    const std::string_view w = history::settlement_status_word(status);
    return w.empty() ? std::string_view("?") : w;
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

class Report {
public:
    Report(const HistoryView& view, const history::HistoryTables& t, const history::HistoryDigest& d,
           HistoryLevel level)
        : v_(view), t_(t), d_(d), level_(level) {}

    std::string build() {
        line(std::string(HISTORY_REPORT_HEADER_V4));  // P5-02 12.1
        line(std::string("level = ") + (level_ == HistoryLevel::full ? "full" : "summary"));
        // r17 24.4: the chronicle before the civilization blocks; [phases]
        // at full only.
        settings();
        ages();
        count_phases();
        if (level_ == HistoryLevel::full) {
            phases();
        }
        const std::int64_t before = lines_;
        chronicle();
        chronicle_lines_ = lines_ - before;
        civilizations();
        present();
        monsters();  // P5-02 12.4
        sites();     // P5-02 12.5
        stats();
        return std::move(out_);
    }

    // After build(): the lines outside [chronicle] (P5-02 12.9's O at summary).
    [[nodiscard]] std::int64_t outside_lines() const noexcept { return lines_ - chronicle_lines_; }

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
        ++lines_;
    }

    // --- References (6.1) ---------------------------------------------------

    [[nodiscard]] std::string ref(Id type, Id id) const {
        const std::string tag = std::string(word(PREFIX, type, 1)) + num(id);
        const std::optional<EntityType> t = history::entity_type_from_code(type);
        // r21 26.4: a region by its present name, its name at E.
        const std::string* name = !t ? nullptr
                                  : *t == EntityType::region ? v_.region_name_at_year(id, t_.end_year)
                                                             : v_.name(*t, id);
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
            std::int64_t awakening = 0;
            const history::KindDef* civ_founded = v_.catalogue().find("civ_founded");
            for (const db::CivilizationRow& c : t_.civilizations) {
                if (a.start_year <= c.founded_year && c.founded_year <= a.end_year) {
                    ++founded;
                    awakening += (civ_founded != nullptr && event(c.founding_event).row.kind == civ_founded->id) ? 1 : 0;
                }
            }
            const db::RaceRow* r = race(a.race);          // a present race (P3-03 9.1)
            const db::GodRow* g = god(r->creator_god);    // a god (read_history_tables)
            line("a" + num(a.id) + " " + escape_history_text(a.name) + ": " + span_of(a.start_year, a.end_year) +
                 ", " + pl(a.end_year - a.start_year + 1, "year") + "; " + escape_history_text(r->plural) + " (ra" +
                 num(r->id) + ") made by " + escape_history_text(g->name) + " (g" + num(g->id) + ", " +
                 std::string(word(CAMP, g->camp, 0)) + "); " + pl(founded, "civilization") + " founded: " +
                 num(awakening) + " at the awakening, " + num(founded - awakening) + " by secession");
        }
    }

    // --- [phases] (6.5) -----------------------------------------------------

    void count_phases() {
        phase_events_.assign(t_.phases.size() + 1, 0);
        phase_shown_.assign(t_.phases.size() + 1, 0);
        for (const HistoryView::Event& e : v_.events()) {
            const auto p = static_cast<std::size_t>(e.row.phase);  // a phase id (history_reads_disagree)
            ++phase_events_[p];
            phase_shown_[p] += shown(e.row.id) ? 1 : 0;
        }
    }

    // "<start>-<end>, <pl(years)>[; trigger e<t>]; <pl(m, event)>", shared by
    // [phases] (6.5) and the chronicle's phase headings (24.4).
    [[nodiscard]] std::string phase_tail(const db::PhaseRow& p) const {
        std::string s = span_of(p.start_year, p.end_year) + ", " + pl(p.end_year - p.start_year + 1, "year");
        if (p.trigger_event != 0) {
            s += "; trigger e" + num(p.trigger_event);
        }
        return s + "; " + pl(phase_events_[static_cast<std::size_t>(p.id)], "event");
    }

    void phases() {
        line("[phases]");
        for (const db::PhaseRow& p : t_.phases) {
            line("p" + num(p.id) + " a" + num(p.age) + " " + std::string(phase_word(p.kind)) + " " +
                 phase_tail(p));
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
        // r19 25.5: an ended realm's block at summary leaves out home, the
        // peak lines and events, with at most MILESTONES_ENDED milestones.
        const bool short_block = level_ == HistoryLevel::summary && c.ended_year.has_value();
        line("c" + num(c.id) + " " + escape_history_text(c.name));
        const db::RaceRow* r = race(c.race);
        line("  race: " + escape_history_text(r->name) + " (ra" + num(r->id) + ")");
        const history::RegionInfo* home = region_row(c.home_region);
        if (!short_block) {
            line("  home: " + region(c.home_region) + ", " + biome_key(home->biome) +
                 (upland(*home) ? ", upland" : ""));
        }
        const db::AgeRow* age = age_of(c.founded_year);
        line("  founded: " + num(c.founded_year) + " in " + escape_history_text(age->name) + " (a" + num(age->id) +
             "), e" + num(c.founding_event));
        if (d.seceded_from != 0) {
            line("  seceded from: " + civ(d.seceded_from) + ", e" + num(c.founding_event));
        }
        const std::string capital = c.ended_year ? "  last capital: " : "  capital: ";
        line(capital + (c.capital != 0 ? settlement(c.capital) : std::string("none")));
        if (!short_block) {
            line(d.peak_regions.count > 0 ? "  peak regions: " + num(d.peak_regions.count) + ", " +
                                                span_of(d.peak_regions.from_year, d.peak_regions.to_year)
                                          : std::string("  peak regions: 0"));
            line(d.peak_population.population > 0 ? "  peak population: " + num(d.peak_population.population) +
                                                        " in " + num(d.peak_population.year)
                                                  : std::string("  peak population: 0"));
        }
        line("  settlements: " + num(d.settlements_founded) + " founded, " + num(d.settlements_held) +
             " held at the end, " + num(d.settlements_ruined) + " ruined, " + num(d.settlements_faded) +
             " faded");  // r19 25.3 item 3
        line("  wars: " + num(d.wars_declared) + " declared, " + num(d.wars_defended) + " defended; won " +
             num(d.wars_won) + ", lost " + num(d.wars_lost) + ", drawn " + num(d.wars_drawn) + ", open " +
             num(d.wars_open));
        if (level_ == HistoryLevel::full) {
            relations(d);
        } else {
            adversaries(d);
        }
        if (!short_block) {
            line("  events: " + num(d.events));
        }
        if (!c.ended_year) {
            line("  fate: alive in " + num(t_.end_year) + ": " + pl(d.regions_held, "region") + ", " +
                 pl(d.settlements_held, "settlement") + ", population " + num(d.population));
        } else {
            line("  fate: ended in " + num(*c.ended_year) + ", e" + num(c.ending_event) + ": " +
                 sentence(c.ending_event));
        }
        line("  milestones:");
        for (const Id m : short_block ? d.milestones_ended : d.milestones) {
            const HistoryView::Event& e = event(m);
            line("    " + event_span(e.row) + " e" + num(m) + " " + escape_history_text(kind_of(e).key) + ": " +
                 sentence(m) + note(m, c.id));
        }
    }

    // r17 24.5: one line per adversary, summary only; r19 25.4: a line for
    // each adversary with a war row, then one count line for the others.
    void adversaries(const history::CivilizationDigest& d) {
        if (d.adversaries.empty()) {
            line("  adversaries: none");
            return;
        }
        line("  adversaries: " + num(static_cast<std::int64_t>(d.adversaries.size())));
        std::int64_t without = 0;
        std::int64_t enmity = 0;
        for (const history::CivilizationDigest::Adversary& a : d.adversaries) {
            if (a.wars == 0) {
                ++without;
                enmity += a.state == "enmity" ? 1 : 0;
                continue;
            }
            line("    " + ref(a.type, a.id) + ": " + pl(a.wars, "war") + ", won " + num(a.won) + ", lost " +
                 num(a.lost) + ", drawn " + num(a.drawn) + ", open " + num(a.open) + ", " + span_of(a.first, a.last) +
                 "; now " + a.state);
        }
        if (without > 0) {
            line("    without war: " + num(without) + " (enmity " + num(enmity) + ", none " + num(without - enmity) + ")");
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
                line("-- p" + num(p.id) + " " + std::string(phase_word(p.kind)) + ": " + phase_tail(p));
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
                const std::string* present = v_.region_name_at_year(g.id, t_.end_year);  // r21 26.4
                line("  rg" + num(g.id) + " " + escape_history_text(present != nullptr ? *present : g.name) + ": " +
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
        ruins();
        refuges();
        open_wars();
        standing_claims();
        renamed_regions();  // r21 26.2
        if (level_ == HistoryLevel::full) {
            beasts();  // P5-02 12.3
        }
    }

    // P5-02 12.3: the world totals of each species at E (the last row of each
    // (region, beast) summed), species in ascending id.
    void beasts() {
        std::vector<std::int64_t> total(t_.beasts.size(), 0);
        const std::vector<db::BeastPopulationRow>& rows = t_.beast_populations;
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const bool last = i + 1 == rows.size() || rows[i + 1].region != rows[i].region ||
                              rows[i + 1].beast != rows[i].beast;
            const std::int64_t b = rows[i].beast;
            if (last && b >= 1 && static_cast<std::size_t>(b) <= total.size()) {
                std::int64_t& t = total[static_cast<std::size_t>(b - 1)];
                t = t > std::numeric_limits<std::int64_t>::max() - rows[i].count ? std::numeric_limits<std::int64_t>::max()
                                                                                 : t + rows[i].count;
            }
        }
        std::string l = "beasts:";
        for (std::size_t i = 0; i < t_.beasts.size(); ++i) {
            l += (i == 0 ? " " : ", ") + escape_history_text(t_.beasts[i].plural) + " " + num(total[i]);
        }
        line(std::move(l));
    }

    // P5-02 12.4: [monsters].
    void monsters() {
        line("[monsters]");
        std::array<std::int64_t, 5> status{};
        for (const db::MonsterRow& m : t_.monsters) {
            if (m.status >= 1 && m.status <= 4) {
                ++status[static_cast<std::size_t>(m.status)];
            }
        }
        line("monsters: " + num(static_cast<std::int64_t>(t_.monsters.size())) + "; awake " + num(status[1]) +
             ", asleep " + num(status[2]) + ", slain " + num(status[3]) + ", died " + num(status[4]));
        // Raids and their dead per monster (index id - 1).
        std::vector<std::int64_t> raids(t_.monsters.size(), 0);
        std::vector<std::int64_t> killed(t_.monsters.size(), 0);
        const history::KindDef* raid = v_.catalogue().find("monster_raid");
        for (const HistoryView::Event& e : v_.events()) {
            if (raid == nullptr || e.row.kind != raid->id) {
                continue;
            }
            const Id m = role_of(e, "monster");
            const std::int64_t seq = raid->value_seq("dead");
            if (m >= 1 && static_cast<std::size_t>(m) <= raids.size()) {
                ++raids[static_cast<std::size_t>(m - 1)];
                if (seq >= 1 && static_cast<std::size_t>(seq) <= e.values.size()) {
                    std::int64_t& k = killed[static_cast<std::size_t>(m - 1)];
                    const std::int64_t d = std::max<std::int64_t>(0, e.values[static_cast<std::size_t>(seq - 1)]);
                    k = k > std::numeric_limits<std::int64_t>::max() - d ? std::numeric_limits<std::int64_t>::max() : k + d;
                }
            }
        }
        std::vector<const db::MonsterRow*> shown;
        std::int64_t more = 0;
        if (level_ == HistoryLevel::full) {
            for (const db::MonsterRow& m : t_.monsters) {
                shown.push_back(&m);
            }
        } else if (static_cast<std::int64_t>(t_.civilizations.size()) <= history::SUMMARY_LINES_CIVS) {
            // P5-02 12.9 (Paul, 2026-11-23): with more than SUMMARY_LINES_CIVS
            // realms the summary's [monsters] is its count line only.
            for (const db::MonsterRow& m : t_.monsters) {
                if (m.status == 1 || m.status == 2) {
                    shown.push_back(&m);
                }
            }
            std::stable_sort(shown.begin(), shown.end(), [](const db::MonsterRow* a, const db::MonsterRow* b) {
                return a->hoard != b->hoard ? a->hoard > b->hoard : a->id < b->id;
            });
            if (shown.size() > history::SUMMARY_MONSTER_LINES) {
                more = static_cast<std::int64_t>(shown.size() - history::SUMMARY_MONSTER_LINES);
                shown.resize(history::SUMMARY_MONSTER_LINES);
            }
        }
        for (const db::MonsterRow* m : shown) {
            const std::size_t i = static_cast<std::size_t>(m->id - 1);  // ids 1, 2, ... (P5-02 11.4)
            std::string state;
            switch (m->status) {
                case 1: state = "awake"; break;
                case 2: state = "asleep"; break;
                case 3: state = "slain by " + civ(m->slain_by); break;
                default: state = "died"; break;
            }
            const db::SiteRow& lair = t_.sites[static_cast<std::size_t>(m->lair_site - 1)];  // a lair (11.4)
            const std::string kind_name = m->kind >= 1 && static_cast<std::size_t>(m->kind) <= t_.monster_kinds.size()
                                              ? escape_history_text(t_.monster_kinds[static_cast<std::size_t>(m->kind - 1)].name)
                                              : std::string("?");
            std::string l = "  m" + num(m->id) + " " + escape_history_text(m->name) + ": " + kind_name + ", " + state +
                            ", lair " + ref(9, lair.id) + " in " + region(lair.region) + ", since " + num(m->born) +
                            " e" + num(m->born_event) + ", raids " + num(raids[i]) + ", killed " + num(killed[i]) +
                            ", hoard " + num(m->hoard);
            if (m->died) {
                l += ", ended " + num(*m->died) + " e" + num(m->death_event);
            }
            line(std::move(l));
        }
        if (more > 0) {
            line("  and " + num(more) + " more");
        }
    }

    // P5-02 12.5: [sites].
    void sites() {
        line("[sites]");
        std::int64_t open = 0, lairs = 0, outposts = 0;
        for (const db::SiteRow& x : t_.sites) {
            open += x.ended_year ? 0 : 1;
            lairs += x.kind == 1 ? 1 : 0;
            outposts += x.kind == 2 ? 1 : 0;
        }
        line("sites: " + num(static_cast<std::int64_t>(t_.sites.size())) + "; open " + num(open) + "; lairs " +
             num(lairs) + ", outposts " + num(outposts));
        if (level_ != HistoryLevel::full) {
            return;
        }
        static constexpr std::array<std::string_view, 6> ENDING = {"slain", "died", "moved", "lost", "fell", "quiet"};
        for (const db::SiteRow& x : t_.sites) {
            std::string l = "  si" + num(x.id) + " " + escape_history_text(x.name) + ": " +
                            (x.kind == 1 ? "lair" : "outpost") + " in " + region(x.region) + " at " + num(x.x) + " " +
                            num(x.y) + ", since " + num(x.origin_year) + " e" + num(x.origin_event);
            if (x.founder_civ != 0) {
                l += ", of " + civ(x.founder_civ);
            }
            if (x.settlement != 0) {
                l += ", at " + settlement(x.settlement);
            }
            if (x.ended_year) {
                l += "; ended " + num(*x.ended_year) + " e" + num(x.ending_event) + " " +
                     std::string(word(ENDING, x.ending, 1));
            } else {
                l += "; open";
            }
            line(std::move(l));
        }
    }

    void settlement_line(const db::SettlementRow& s) {
        std::string l = "  s" + num(s.id) + " " + escape_history_text(s.name) + ": " +
                        std::string(kind_word(s.kind)) + ", " + std::string(status_word(s.status)) +
                        ", population " + num(s.population) + ", " +
                        (s.owner_civ != 0 ? civ(s.owner_civ) : std::string("no owner")) + ", at " + num(s.x) + " " +
                        num(s.y) + " in " + region(s.region) + ", founded " + num(s.founded_year) + " by " +
                        civ(s.founder_civ);
        if (s.ruined_year) {
            l += ", ruined " + num(*s.ruined_year) + ", e" + num(s.ruin_event);
        }
        if (s.status == 3 || s.status == 4) {
            l += ", last held by " + last_held_by(s.id);  // r17 24.7 (A10)
        }
        line(std::move(l));
    }

    // --- [present]'s lists (24.7) -------------------------------------------

    [[nodiscard]] std::string last_held_by(Id settlement_id) const {
        const Id* c = settlement_row(std::span<const Id>(d_.last_held_by), settlement_id);  // r21 26.7
        return c != nullptr && *c != 0 ? civ(*c) : std::string("no one");
    }

    // The first participant of `role` (by key) of event e, or 0.
    [[nodiscard]] Id role_of(const HistoryView::Event& e, std::string_view role) const {
        const history::RoleDef* r = kind_of(e).find_role(role);
        if (r == nullptr) {
            return 0;
        }
        for (const db::EventParticipantRow& p : e.participants) {  // seq order
            if (p.role == r->seq) {
                return p.entity_id;
            }
        }
        return 0;
    }

    void ruins() {
        std::vector<const db::SettlementRow*> rows;
        for (const db::SettlementRow& s : t_.settlements) {
            if (s.status == 3) {
                rows.push_back(&s);
            }
        }
        line("ruins: " + num(static_cast<std::int64_t>(rows.size())));
        for (const db::SettlementRow* s : rows) {
            std::string l = "  s" + num(s->id) + " " + escape_history_text(s->name) + " in " + region(s->region);
            // P5-02 12.3: the region's name when it was ruined, if it differs.
            if (s->ruined_year) {
                const std::string* then = v_.region_name_at_year(s->region, *s->ruined_year);
                const std::string* now = v_.region_name_at_year(s->region, t_.end_year);
                if (then != nullptr && now != nullptr && *then != *now) {
                    l += ", then called " + escape_history_text(*then);
                }
            }
            l += ":";
            if (s->ruined_year) {  // set while ruined (5.3: with ruin_event)
                const HistoryView::Event& e = event(s->ruin_event);
                l += " ruined " + num(*s->ruined_year) + ", e" + num(s->ruin_event) + " " + ruin_how(e, s->ending) + ";";
            }
            line(l + " last held by " + last_held_by(s->id));
        }
    }

    // r19 25.3 item 1: how a settlement was ruined, by its ruin event's kind.
    // P5-02 12.3: an overrun, and the plague or famine of an abandonment.
    [[nodiscard]] std::string ruin_how(const HistoryView::Event& e, std::int64_t ending) const {
        const std::string& key = kind_of(e).key;
        if (key == "sacked") {
            const Id by = role_of(e, "attacker");
            return by != 0 ? "sacked by " + civ(by) : std::string("sacked");
        }
        if (key == "settlement_overrun") {
            const Id m = role_of(e, "monster");
            return m != 0 ? "overrun by " + ref(8, m) : std::string("overrun");
        }
        const std::string after = ending == 3 ? " after plague" : ending == 4 ? " after famine" : "";
        if (key == "settlement_abandoned") {
            return "abandoned" + after;
        }
        if (key == "settlement_abandoned_to") {
            return "abandoned" + after + ", survivors to " + settlement(role_of(e, "destination"));
        }
        return escape_history_text(key);
    }

    // r21 26.3 (F3): the refuges still hidden at E.
    void refuges() {
        const history::KindDef* refugees = v_.catalogue().find("refugees_settled");
        std::vector<const db::SettlementRow*> rows;
        std::int64_t n = 0;
        for (const db::SettlementRow& s : t_.settlements) {
            if (refugees != nullptr && standing(s.status) && event(s.founding_event).row.kind == refugees->id) {
                ++n;
                if (hidden_at_end(s.id)) {
                    rows.push_back(&s);
                }
            }
        }
        line("hidden refuges: " + num(static_cast<std::int64_t>(rows.size())) + " of " + num(n) +
             " founded by flight");
        for (const db::SettlementRow* s : rows) {
            const Id origin = role_of(event(s->founding_event), "origin");
            line("  s" + num(s->id) + " " + escape_history_text(s->name) + " in " + region(s->region) + ": " +
                 (s->owner_civ != 0 ? civ(s->owner_civ) : std::string("no owner")) + ", population " +
                 num(s->population) + ", founded " + num(s->founded_year) + " by flight from " + settlement(origin) +
                 ", e" + num(s->founding_event));
        }
    }

    // 26.3: the refuge's refuge_hidden (smallest id) has end_year >= E - 1;
    // "not hidden" when it has none (26.7).
    [[nodiscard]] bool hidden_at_end(Id settlement_id) const {
        const std::int64_t* end = settlement_row(std::span<const std::int64_t>(d_.refuge_hidden_end), settlement_id);
        return end != nullptr && *end >= 0 && *end >= t_.end_year - 1;
    }

    // r21 26.2: every row of region_name with the name it replaced.
    void renamed_regions() {
        const std::span<const db::RegionNameRow> rows = v_.region_names();
        line("renamed regions: " + num(static_cast<std::int64_t>(rows.size())));
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const db::RegionNameRow& r = rows[i];
            const std::string* geographic = v_.name(EntityType::region, r.region);
            const std::string& previous =
                (i > 0 && rows[i - 1].region == r.region) ? rows[i - 1].name
                                                          : (geographic != nullptr ? *geographic : r.name);
            const HistoryView::Event& e = event(r.event);
            std::string l = "  rg" + num(r.region) + ": " + escape_history_text(r.name) + " since " + num(r.from_year) +
                            ", e" + num(r.event);
            if (!e.causes.empty()) {
                const Id c = e.causes.front();
                l += " after e" + num(c) + " " + escape_history_text(kind_of(event(c)).key);
            }
            line(l + "; formerly " + escape_history_text(previous));
        }
    }

    void open_wars() {
        std::vector<const db::RelationRow*> rows;
        for (const db::RelationRow& r : t_.relations) {
            if (r.kind == "war" && !r.until) {
                rows.push_back(&r);
            }
        }
        std::stable_sort(rows.begin(), rows.end(), [](const db::RelationRow* a, const db::RelationRow* b) {
            if (a->since != b->since) return a->since < b->since;
            if (a->a_type != b->a_type) return a->a_type < b->a_type;
            if (a->a_id != b->a_id) return a->a_id < b->a_id;
            if (a->b_type != b->b_type) return a->b_type < b->b_type;
            return a->b_id < b->b_id;
        });
        line("open wars: " + num(static_cast<std::int64_t>(rows.size())));
        for (const db::RelationRow* r : rows) {
            line("  e" + num(r->since_event) + " " + ref(r->a_type, r->a_id) + " against " + ref(r->b_type, r->b_id) +
                 " over " + region(event(r->since_event).row.region) + ", since " + num(r->since));
        }
    }

    void standing_claims() {
        line("standing claims: " + num(static_cast<std::int64_t>(d_.standing_claims.size())));
        for (const Id id : d_.standing_claims) {
            const HistoryView::Event& e = event(id);
            std::string l = "  e" + num(id) + " " + num(e.row.year) + " " + civ(role_of(e, "claimant")) + " on " +
                            civ(role_of(e, "holder")) + ": " + region(e.row.region);
            const history::RoleDef* s = kind_of(e).find_role("settlement");
            bool first = true;
            for (const db::EventParticipantRow& p : e.participants) {
                if (s != nullptr && p.role == s->seq) {
                    l += (first ? ", for " : ", ") + settlement(p.entity_id);
                    first = false;
                }
            }
            line(std::move(l));
        }
    }

    // --- [stats] (6.9) ------------------------------------------------------

    // r19 25.7 item 4: at summary only the keys of SUMMARY_STATS.
    void stats() {
        line("[stats]");
        for (const auto& [key, value] : d_.stats) {
            if (level_ == HistoryLevel::summary &&
                std::find(history::SUMMARY_STATS.begin(), history::SUMMARY_STATS.end(), key) ==
                    history::SUMMARY_STATS.end()) {
                continue;
            }
            line(escape_history_text(key) + " = " + num(value));
        }
    }

    const HistoryView& v_;
    const history::HistoryTables& t_;
    const history::HistoryDigest& d_;
    HistoryLevel level_;
    std::string out_;
    std::int64_t lines_ = 0;
    std::int64_t chronicle_lines_ = 0;
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
        if (const std::optional<std::string> why = history::history_reads_disagree(view.value(), tables.value())) {
            return db::Error{db::ErrorCode::corrupt_layer, *why};
        }
        history::HistoryDigest digest = history::digest_history(view.value(), tables.value());
        // P5-02 12.9: O is counted on the summary rendered with the set of
        // SUMMARY_BUDGET (no line outside [chronicle] depends on the set);
        // the set is chosen again with B when B is smaller, for both levels.
        Report summary(view.value(), tables.value(), digest, HistoryLevel::summary);
        std::string text = summary.build();
        const history::HistoryTables& t = tables.value();
        const std::int64_t budget = history::summary_budget(
            static_cast<std::int64_t>(t.civilizations.size()), summary.outside_lines(),
            static_cast<std::int64_t>(t.ages.size()), static_cast<std::int64_t>(t.phases.size()));
        const bool again = budget != digest.summary_budget;
        if (again) {
            history::choose_summary_set(digest, budget);
        }
        if (level == HistoryLevel::full) {
            return Report(view.value(), tables.value(), digest, level).build();
        }
        if (again) {
            text = Report(view.value(), tables.value(), digest, level).build();
        }
        return text;
    } catch (const std::bad_alloc&) {
        return db::Error{db::ErrorCode::out_of_memory, "out of memory while building the history report"};
    }
}

}  // namespace worldgen::inspect
