// P5-11 9 (schema 15): the reader checks of the artifact tables, one
// function for both history readers (HistoryView::load and the digest's
// read_history_tables), as monster_tables_problem and
// pantheon_tables_problem.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "worldgen/db/schema.hpp"
#include "worldgen/history/event_catalogue.hpp"

namespace worldgen::history {

namespace {

std::string text(std::int64_t v) {
    return db::format_i64(v);
}

// The holder type of each state (6.2): 1 kept by a realm (3), 2 borne by a
// figure (6), 3 hoarded by a monster (8), 4 lost (4, 5 or 9), 5 buried with
// a figure (6), 6 enshrined at a site (9).
bool holder_type_fits(std::int64_t state, std::int64_t type) noexcept {
    switch (state) {
        case 1: return type == 3;
        case 2: return type == 6;
        case 3: return type == 8;
        case 4: return type == 4 || type == 5 || type == 9;
        case 5: return type == 6;
        case 6: return type == 9;
        default: return false;
    }
}

// Lookups over the stored log, by event id (ids 1, 2, ... checked before).
class Log {
public:
    explicit Log(const ArtifactTables& t) : t_(t), parts_(t.events.size()), vals_(t.events.size()),
                                            causes_(t.events.size()) {
        const auto spans = [](auto rows, std::vector<std::pair<std::size_t, std::size_t>>& out) {
            for (std::size_t i = 0; i < rows.size(); ++i) {
                const std::int64_t e = rows[i].event;
                if (e >= 1 && static_cast<std::uint64_t>(e) <= out.size()) {
                    auto& r = out[static_cast<std::size_t>(e - 1)];
                    if (r.first == r.second) {
                        r = {i, i};
                    }
                    r.second = i + 1;
                }
            }
        };
        spans(t.participants, parts_);
        spans(t.values, vals_);
        spans(t.causes, causes_);
    }

    [[nodiscard]] const db::EventRow* row(std::int64_t e) const noexcept {
        return e >= 1 && static_cast<std::uint64_t>(e) <= t_.events.size() ? &t_.events[static_cast<std::size_t>(e - 1)]
                                                                            : nullptr;
    }
    [[nodiscard]] const KindDef* kind(std::int64_t e) const noexcept {
        const db::EventRow* r = row(e);
        return r != nullptr ? t_.catalogue->find_id(r->kind) : nullptr;
    }
    [[nodiscard]] std::string_view key(std::int64_t e) const noexcept {
        const KindDef* k = kind(e);
        return k != nullptr ? std::string_view(k->key) : std::string_view();
    }
    // The first participant of `role`, 0 none; the number of them.
    [[nodiscard]] std::int64_t role(std::int64_t e, std::string_view name) const noexcept {
        const KindDef* k = kind(e);
        const RoleDef* r = k != nullptr ? k->find_role(name) : nullptr;
        if (r == nullptr) {
            return 0;
        }
        const auto [a, b] = parts_[static_cast<std::size_t>(e - 1)];
        for (std::size_t i = a; i < b; ++i) {
            if (t_.participants[i].role == r->seq) {
                return t_.participants[i].entity_id;
            }
        }
        return 0;
    }
    [[nodiscard]] std::int64_t count(std::int64_t e, std::string_view name) const noexcept {
        const KindDef* k = kind(e);
        const RoleDef* r = k != nullptr ? k->find_role(name) : nullptr;
        if (r == nullptr) {
            return 0;
        }
        std::int64_t n = 0;
        const auto [a, b] = parts_[static_cast<std::size_t>(e - 1)];
        for (std::size_t i = a; i < b; ++i) {
            n += t_.participants[i].role == r->seq ? 1 : 0;
        }
        return n;
    }
    // The value of `key`, -1 when absent.
    [[nodiscard]] std::int64_t value(std::int64_t e, std::string_view name) const noexcept {
        const KindDef* k = kind(e);
        const std::int64_t seq = k != nullptr ? k->value_seq(name) : 0;
        if (seq < 1) {
            return -1;
        }
        const auto [a, b] = vals_[static_cast<std::size_t>(e - 1)];
        for (std::size_t i = a; i < b; ++i) {
            if (t_.values[i].seq == seq) {
                return t_.values[i].value;
            }
        }
        return -1;
    }
    [[nodiscard]] bool caused_by(std::int64_t e, std::int64_t cause) const noexcept {
        if (row(e) == nullptr) {
            return false;
        }
        const auto [a, b] = causes_[static_cast<std::size_t>(e - 1)];
        for (std::size_t i = a; i < b; ++i) {
            if (t_.causes[i].cause == cause) {
                return true;
            }
        }
        return false;
    }

private:
    const ArtifactTables& t_;
    std::vector<std::pair<std::size_t, std::size_t>> parts_, vals_, causes_;
};

bool has(std::span<const std::int64_t> sorted, std::int64_t id) {
    return std::binary_search(sorted.begin(), sorted.end(), id);
}

template <class Row>
bool dense_has(std::span<const Row> rows, std::int64_t id) {
    return id >= 1 && static_cast<std::uint64_t>(id) <= rows.size() && rows[static_cast<std::size_t>(id - 1)].id == id;
}

std::optional<std::string> kind_rows_problem(const ArtifactTables& t) {
    for (std::size_t i = 0; i < t.kinds.size(); ++i) {
        const db::ArtifactKindRow& k = t.kinds[i];
        const std::string at = "corrupt table 'artifact_kind': row " + text(k.id) + ": ";
        const std::int64_t c = k.artifact_class;
        if (k.id != static_cast<std::int64_t>(i) + 1) {
            return at + "id " + text(k.id) + " is not " + text(static_cast<std::int64_t>(i) + 1);
        }
        if (c < 1 || c > 5) return at + "class " + text(c) + " outside 1 to 5";
        if (k.battle_permille < 0 || k.battle_permille > 500) {
            return at + "battle_permille " + text(k.battle_permille) + " outside 0 to 500";
        }
        if (k.battle_permille != 0 && c != 1 && c != 2) {
            return at + "battle_permille " + text(k.battle_permille) + " needs arms or armour";
        }
        if (k.hunt_permille < 0 || k.hunt_permille > 1000) {
            return at + "hunt_permille " + text(k.hunt_permille) + " outside 0 to 1000";
        }
        if (k.hunt_permille != 0 && (c == 3 || c == 4)) {
            return at + "hunt_permille " + text(k.hunt_permille) + " of regalia or a relic";
        }
        if (k.claim_permille < 0 || k.claim_permille > 1000) {
            return at + "claim_permille " + text(k.claim_permille) + " outside 0 to 1000";
        }
    }
    return std::nullopt;
}

std::optional<std::string> artifact_rows_problem(const ArtifactTables& t, const Log& log) {
    for (std::size_t i = 0; i < t.artifacts.size(); ++i) {
        const db::ArtifactRow& a = t.artifacts[i];
        const std::string at = "corrupt table 'artifact': row " + text(a.id) + ": ";
        if (a.id != static_cast<std::int64_t>(i) + 1) {
            return at + "id " + text(a.id) + " is not " + text(static_cast<std::int64_t>(i) + 1);
        }
        if (!dense_has(t.kinds, a.kind)) return at + "kind " + text(a.kind) + " is not a row of artifact_kind";
        if (!has(t.civ_ids, a.civ)) return at + "unknown civilization " + text(a.civ);
        if (a.maker_kind < 1 || a.maker_kind > 4) return at + "maker_kind " + text(a.maker_kind) + " outside 1 to 4";
        if (a.deep < 0 || a.deep > 1) return at + "deep " + text(a.deep) + " outside 0 to 1";
        if (a.deep == 1 && a.maker_kind > 2) return at + "deep 1 for maker_kind " + text(a.maker_kind);
        const db::EventRow* made = log.row(a.made_event);
        if (made == nullptr || log.key(a.made_event) != "artifact_made" || log.role(a.made_event, "artifact") != a.id) {
            return at + "made_event " + text(a.made_event) + " is not an artifact_made naming it";
        }
        if (made->year != a.made_year) {
            return at + "made_event " + text(a.made_event) + " is not of year " + text(a.made_year);
        }
        const bool smith = a.maker_kind == 1 || a.maker_kind == 2;
        const db::FigureRow* maker = dense_has(t.figures, a.maker) ? &t.figures[static_cast<std::size_t>(a.maker - 1)]
                                                                   : nullptr;
        std::int64_t maker_civ = 0;
        if (smith || a.maker_kind == 3) {
            if (maker == nullptr || maker->role != (smith ? 8 : 5) || (smith && maker->start_event != a.made_event)) {
                return at + "maker " + text(a.maker) + " is not its " + (smith ? "smith" : "caster");
            }
            maker_civ = a.maker_kind == 2 ? log.role(a.made_event, "maker_realm") : maker->civ;
            if (a.god != 0) return at + "god " + text(a.god) + " for maker_kind " + text(a.maker_kind);
        } else {
            if (a.maker != 0) return at + "maker " + text(a.maker) + " for a god-given artifact";
            if (!dense_has(t.gods, a.god)) return at + "god " + text(a.god) + " is not a god";
        }
        const bool agree = log.role(a.made_event, "civilization") == a.civ &&
                           log.role(a.made_event, "maker") == a.maker && log.role(a.made_event, "god") == a.god &&
                           a.maker_civ == maker_civ && log.role(a.made_event, "site") == a.maker_site &&
                           log.value(a.made_event, "maker_kind") == a.maker_kind &&
                           log.value(a.made_event, "deep") == a.deep;
        if (!agree) return at + "made_event " + text(a.made_event) + " disagrees with the row";
        if (a.maker_site != 0 && !dense_has(t.sites, a.maker_site)) {
            return at + "maker_site " + text(a.maker_site) + " is not a site";
        }
        // Lead ruling (P5-11 reading 2): a cause of the making, not the first.
        if (!log.caused_by(a.made_event, a.occasion_event)) {
            return at + "occasion_event " + text(a.occasion_event) + " is not a cause of the making";
        }
    }
    return std::nullopt;
}

bool holder_exists(const ArtifactTables& t, std::int64_t type, std::int64_t id) {
    switch (type) {
        case 3: return has(t.civ_ids, id);
        case 4: return dense_has(t.settlements, id);
        case 5: return has(t.region_ids, id);
        case 6: return dense_has(t.figures, id);
        case 8: return dense_has(t.monsters, id);
        case 9: return dense_has(t.sites, id);
        default: return false;
    }
}

std::optional<std::string> one_holding_problem(const ArtifactTables& t, const Log& log, const db::ArtifactHoldingRow& h) {
    const std::string at = "corrupt table 'artifact_holding': holding " + text(h.seq) + " of artifact " +
                           text(h.artifact) + ": ";
    if (h.to_year && h.from_year > *h.to_year) {
        return at + "from_year " + text(h.from_year) + " above to_year " + text(*h.to_year);
    }
    const db::EventRow* e = log.row(h.event);
    if (e == nullptr || e->year != h.from_year) return at + "event " + text(h.event) + " is not of year " + text(h.from_year);
    if (h.state < 1 || h.state > 6) return at + "state " + text(h.state) + " outside 1 to 6";
    if (!holder_type_fits(h.state, h.holder_type)) {
        return at + "holder_type " + text(h.holder_type) + " for state " + text(h.state);
    }
    if (!holder_exists(t, h.holder_type, h.holder)) {
        return at + "holder " + text(h.holder) + " of type " + text(h.holder_type) + " does not exist";
    }
    if (h.state <= 3) {
        if (h.region != 0 || h.settlement != 0 || h.site != 0 || h.x != 0 || h.y != 0) {
            return at + "place columns not 0 for state " + text(h.state);
        }
        return std::nullopt;
    }
    if (!has(t.region_ids, h.region)) return at + "region " + text(h.region) + " is not a region";
    if (h.x < 0 || h.y < 0 || (t.map_size > 0 && (h.x >= t.map_size || h.y >= t.map_size))) {
        return at + "cell (" + text(h.x) + ", " + text(h.y) + ") outside the map";
    }
    if (h.holder_type == 9 && h.site != h.holder) return at + "site " + text(h.site) + " is not the holder";
    if (h.holder_type == 4 && h.settlement != h.holder) {
        return at + "settlement " + text(h.settlement) + " is not the holder";
    }
    return std::nullopt;
}

std::optional<std::string> holding_rows_problem(const ArtifactTables& t, const Log& log) {
    std::size_t i = 0;
    for (const db::ArtifactRow& a : t.artifacts) {
        std::int64_t seq = 0;
        const db::ArtifactHoldingRow* last = nullptr;
        while (i < t.holdings.size() && t.holdings[i].artifact == a.id) {
            const db::ArtifactHoldingRow& h = t.holdings[i];
            const std::string at = "corrupt table 'artifact_holding': holding " + text(h.seq) + " of artifact " +
                                   text(a.id) + ": ";
            if (h.seq != seq + 1) return at + "seq not consecutive";
            if (h.seq == 1 && (h.from_year != a.made_year || h.event != a.made_event)) {
                return at + "the first holding does not open with the making";
            }
            if (last != nullptr && (last->to_year != h.from_year || last->until_event != h.event)) {
                return "corrupt table 'artifact_holding': holding " + text(last->seq) + " of artifact " + text(a.id) +
                       ": does not close where holding " + text(h.seq) + " opens";
            }
            const bool open = i + 1 == t.holdings.size() || t.holdings[i + 1].artifact != a.id;
            if (!open && (!h.to_year || h.until_event == 0)) return at + "closed holding without to_year";
            if (open && (h.to_year || h.until_event != 0)) return at + "the last holding is not open";
            if (std::optional<std::string> why = one_holding_problem(t, log, h)) {
                return why;
            }
            last = &h;
            seq = h.seq;
            ++i;
        }
        if (last == nullptr) {
            return "corrupt table 'artifact_holding': artifact " + text(a.id) + " has no holding";
        }
        if (a.status != last->state) {
            return "corrupt table 'artifact': row " + text(a.id) + ": status " + text(a.status) +
                   " is not the last holding's state " + text(last->state);
        }
    }
    if (i < t.holdings.size()) {
        const db::ArtifactHoldingRow& h = t.holdings[i];
        return "corrupt table 'artifact_holding': holding " + text(h.seq) + " of artifact " + text(h.artifact) +
               ": not an artifact, or out of order";
    }
    return std::nullopt;
}

std::optional<std::string> event_problem(const ArtifactTables& t, const Log& log) {
    for (const db::FigureRow& f : t.figures) {
        if (f.role == 8 && !(log.key(f.start_event) == "artifact_made" && log.role(f.start_event, "maker") == f.id)) {
            return "corrupt table 'figure': row " + text(f.id) + ": start_event " + text(f.start_event) +
                   " is not an artifact_made naming it as maker";
        }
    }
    for (const db::EventValueRow& v : t.values) {
        const KindDef* k = log.kind(v.event);
        if (k == nullptr || k->id < 111 || k->id > 116 || v.seq < 1 ||
            v.seq > static_cast<std::int64_t>(k->values.size())) {
            continue;
        }
        const std::string& name = k->values[static_cast<std::size_t>(v.seq - 1)];
        std::int64_t lo = 0;
        std::int64_t hi = 0;
        if (name == "maker_kind") {
            lo = 1;
            hi = 4;
        } else if (name == "deep") {
            hi = 1;
        } else if (name == "how") {
            lo = 1;
            hi = k->key == "artifact_taken" ? 4 : k->key == "artifact_hoarded" ? 3 : 2;
        } else {
            continue;
        }
        if (v.value < lo || v.value > hi) {
            return "corrupt table 'event_value': row (" + text(v.event) + ", " + text(v.seq) + "): " + name + " " +
                   text(v.value) + " outside " + text(lo) + " to " + text(hi);
        }
    }
    for (const db::EventRow& e : t.events) {
        if (log.key(e.id) != "artifact_made") {
            continue;
        }
        const std::int64_t mk = log.value(e.id, "maker_kind");
        const bool ok = (log.count(e.id, "maker") == 1) == (mk >= 1 && mk <= 3) &&
                        (log.count(e.id, "god") == 1) == (mk == 4) && (log.count(e.id, "maker_realm") == 1) == (mk == 2);
        if (!ok) {
            return "corrupt table 'event': row " + text(e.id) + ": artifact_made roles and maker_kind disagree";
        }
    }
    for (const db::EventParticipantRow& p : t.participants) {
        if (p.entity_type != static_cast<std::int64_t>(EntityType::artifact)) {
            continue;
        }
        if (!dense_has(t.artifacts, p.entity_id) ||
            t.artifacts[static_cast<std::size_t>(p.entity_id - 1)].made_event > p.event) {
            return "corrupt table 'event_participant': row (" + text(p.event) + ", " + text(p.seq) + "): artifact " +
                   text(p.entity_id) + " does not exist";
        }
    }
    return std::nullopt;
}

}  // namespace

std::optional<std::string> artifact_tables_problem(const ArtifactTables& t) {
    if (t.catalogue == nullptr) {
        return std::nullopt;
    }
    const Log log(t);
    if (std::optional<std::string> why = kind_rows_problem(t)) {
        return why;
    }
    if (std::optional<std::string> why = artifact_rows_problem(t, log)) {
        return why;
    }
    if (std::optional<std::string> why = holding_rows_problem(t, log)) {
        return why;
    }
    return event_problem(t, log);
}

}  // namespace worldgen::history
