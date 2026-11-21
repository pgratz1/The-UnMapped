// The note of a war declaration or a flare-up (spec P3-07 r8 21.4): text
// built from the stored log only, names written as render_event writes them.

#include "worldgen/history/digest.hpp"

#include <array>
#include <string_view>

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

std::string war_note(const HistoryView& view, const HistoryView::Event& e, const KindDef& war, const KindDef* claim) {
    const HistoryView::Event* c = nullptr;
    for (const std::int64_t id : e.causes) {
        const HistoryView::Event* k = view.event(id);
        if (k != nullptr && is_kind(*k, claim)) {
            c = k;
            break;
        }
    }
    if (c == nullptr) {
        return {};
    }
    std::string out = c->row.year < e.row.year ? "Claimed since " + int_text(c->row.year) + "." : "Claimed the same year.";
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

std::string flare_note(const HistoryView& view, const HistoryView::Event& e, const KindDef* claim) {
    if (e.causes.empty() || claim == nullptr) {
        return {};
    }
    const HistoryView::Event* c = view.event(e.causes.front());
    if (c == nullptr || !is_kind(*c, claim)) {
        return {};
    }
    const std::string* a = view.name(EntityType::civilization, participant(*c, *claim, "claimant"));
    const std::string* b = view.name(EntityType::civilization, participant(*c, *claim, "holder"));
    const std::string* r = view.name(EntityType::region, c->row.region);
    if (a == nullptr || b == nullptr || r == nullptr) {
        return {};
    }
    std::string out;
    append_name(out, *a, true);
    out += " pressed its claim to ";
    append_name(out, *r, false);
    out += " against ";
    append_name(out, *b, false);
    out += ".";
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
    const KindDef* claim = catalogue.find("claim");
    const KindDef* war = catalogue.find("war_declared");
    if (war != nullptr && e->row.kind == war->id) {
        return war_note(view, *e, *war, claim);
    }
    const KindDef* flare = catalogue.find("flare_up");
    if (flare != nullptr && e->row.kind == flare->id) {
        return flare_note(view, *e, claim);
    }
    return {};
}

}  // namespace worldgen::history
