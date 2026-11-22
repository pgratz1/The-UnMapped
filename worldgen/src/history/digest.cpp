// The note of a war declaration or a flare-up (spec P3-07 r8 21.4): text
// built from the stored log only, names written as render_event writes them.

#include "worldgen/history/digest.hpp"

#include <array>
#include <string_view>
#include <vector>

#include "worldgen/db/schema.hpp"

namespace worldgen::history {

namespace {

std::string int_text(std::int64_t v) {
    return db::format_i64(v);
}

// render_event's rule (P3-04 8.5 r10): a leading "The " is written "the "
// in the middle of a sentence.
void append_name(std::string& out, const std::string& name, bool sentence_start) {
    constexpr std::string_view THE = "The ";
    if (!sentence_start && name.size() >= THE.size() && name.compare(0, THE.size(), THE) == 0) {
        out += "the ";
        out.append(name, THE.size(), std::string::npos);
    } else {
        out += name;
    }
}

// The entity of the first participant of `role` (a role key of the event's
// kind), or 0.
std::int64_t participant(const HistoryView::Event& e, const KindDef& kind, std::string_view role) {
    const RoleDef* r = kind.find_role(role);
    if (r == nullptr) {
        return 0;
    }
    for (const db::EventParticipantRow& p : e.participants) {
        if (p.role == r->seq) {
            return p.entity_id;
        }
    }
    return 0;
}

bool is_kind(const HistoryView::Event& e, const KindDef* kind) noexcept {
    return kind != nullptr && e.row.kind == kind->id;
}

// The claim kinds of P3-07 r14 23: kind 20 claim and kind 39
// claim_settlements, found by key in the stored catalogue.
struct ClaimKinds {
    const KindDef* claim = nullptr;
    const KindDef* settlements = nullptr;
    [[nodiscard]] bool holds(const HistoryView::Event& e) const noexcept {
        return is_kind(e, claim) || is_kind(e, settlements);
    }
};

// The name of an entity in mid-sentence, appended; false when it has none.
bool append_entity(std::string& out, const HistoryView& view, EntityType type, std::int64_t id) {
    const std::string* n = view.name(type, id);
    if (n == nullptr) {
        return false;
    }
    append_name(out, *n, false);
    return true;
}

std::string war_note(const HistoryView& view, const HistoryView::Event& e, const KindDef& war,
                     const ClaimKinds& claims) {
    // P3-07 r14 23 item 3: the cause of kind claim or claim_settlements with
    // the smallest id.
    const HistoryView::Event* c = nullptr;
    for (const std::int64_t id : e.causes) {
        const HistoryView::Event* k = view.event(id);
        if (k != nullptr && claims.holds(*k) && (c == nullptr || k->row.id < c->row.id)) {
            c = k;
        }
    }
    if (c == nullptr) {
        return {};
    }
    const bool same_year = c->row.year >= e.row.year;
    std::string out;
    if (c->row.region == e.row.region) {
        out = same_year ? "Claimed the same year." : "Claimed since " + int_text(c->row.year) + ".";
    } else {
        // P3-07 r14 23 item 3 (P3-06 r19 23.4): the war's goal is not the
        // claim's region.
        out = same_year ? "The claim, made the same year, was to " : "The claim, since " + int_text(c->row.year) + ", was to ";
        if (!append_entity(out, view, EntityType::region, c->row.region)) {
            return {};
        }
        out += ".";
    }
    const std::int64_t a = participant(e, war, "attacker");
    const std::int64_t b = participant(e, war, "defender");
    std::int64_t n = 1;
    for (const HistoryView::Event& d : view.events()) {
        if (d.row.id >= e.row.id) {
            break;
        }
        if (d.row.kind != war.id) {
            continue;
        }
        const std::int64_t da = participant(d, war, "attacker");
        const std::int64_t db_ = participant(d, war, "defender");
        n += ((da == a && db_ == b) || (da == b && db_ == a)) ? 1 : 0;
    }
    if (n >= 2) {
        out += " The " + history_ordinal(n) + " war between them.";
    }
    return out;
}

// P3-07 r14 23 item 4: a flare-up caused by a claim_settlements.
std::string flare_settlements_note(const HistoryView& view, const HistoryView::Event& c, const KindDef& kind) {
    const RoleDef* role = kind.find_role("settlement");
    std::vector<std::int64_t> settlements;
    for (const db::EventParticipantRow& p : c.participants) {
        if (role != nullptr && p.role == role->seq) {
            settlements.push_back(p.entity_id);
        }
    }
    std::string out = "The claim of ";
    if (settlements.empty() ||
        !append_entity(out, view, EntityType::civilization, participant(c, kind, "claimant"))) {
        return {};
    }
    out += " to ";
    for (std::size_t i = 0; i < settlements.size(); ++i) {
        if (i > 0) {
            out += i + 1 == settlements.size() ? " and " : ", ";
        }
        if (!append_entity(out, view, EntityType::settlement, settlements[i])) {
            return {};
        }
    }
    out += " of ";
    if (!append_entity(out, view, EntityType::civilization, participant(c, kind, "holder"))) {
        return {};
    }
    out += " in ";
    if (!append_entity(out, view, EntityType::region, c.row.region)) {
        return {};
    }
    out += " came to a head.";
    return out;
}

std::string flare_note(const HistoryView& view, const HistoryView::Event& e, const ClaimKinds& claims) {
    if (e.causes.empty()) {
        return {};
    }
    const HistoryView::Event* c = view.event(e.causes.front());
    if (c == nullptr) {
        return {};
    }
    if (is_kind(*c, claims.settlements)) {
        return flare_settlements_note(view, *c, *claims.settlements);
    }
    if (!is_kind(*c, claims.claim)) {
        return {};
    }
    const KindDef* claim = claims.claim;
    const std::string* a = view.name(EntityType::civilization, participant(*c, *claim, "claimant"));
    const std::string* b = view.name(EntityType::civilization, participant(*c, *claim, "holder"));
    const std::string* r = view.name(EntityType::region, c->row.region);
    if (a == nullptr || b == nullptr || r == nullptr) {
        return {};
    }
    // P3-07 r12 22.3: no pronoun, every name mid-sentence.
    std::string out = "The claim of ";
    append_name(out, *a, false);
    out += " to ";
    append_name(out, *r, false);
    out += ", held by ";
    append_name(out, *b, false);
    out += ", came to a head.";
    return out;
}

}  // namespace

std::string history_ordinal(std::int64_t n) {
    static constexpr std::array<std::string_view, 9> WORDS = {"second", "third",   "fourth", "fifth", "sixth",
                                                              "seventh", "eighth", "ninth",  "tenth"};
    if (n >= 2 && n <= 10) {
        return std::string(WORDS[static_cast<std::size_t>(n - 2)]);
    }
    const std::int64_t h = n % 100;
    std::string_view suffix = "th";
    if (h < 11 || h > 13) {
        switch (n % 10) {
            case 1: suffix = "st"; break;
            case 2: suffix = "nd"; break;
            case 3: suffix = "rd"; break;
            default: break;
        }
    }
    return int_text(n) + std::string(suffix);
}

std::string history_note(const HistoryView& view, std::int64_t event_id) {
    const HistoryView::Event* e = view.event(event_id);
    if (e == nullptr) {
        return {};
    }
    const EventCatalogue& catalogue = view.catalogue();
    const ClaimKinds claims{catalogue.find("claim"), catalogue.find("claim_settlements")};
    const KindDef* war = catalogue.find("war_declared");
    if (war != nullptr && e->row.kind == war->id) {
        return war_note(view, *e, *war, claims);
    }
    const KindDef* flare = catalogue.find("flare_up");
    if (flare != nullptr && e->row.kind == flare->id) {
        return flare_note(view, *e, claims);
    }
    return {};
}

}  // namespace worldgen::history
