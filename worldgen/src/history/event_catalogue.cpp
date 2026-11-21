// The event catalogue, the template grammar and the renderer (spec P3-04
// section 8).

#include "worldgen/history/event_catalogue.hpp"

#include <algorithm>
#include <cstddef>
#include <new>
#include <utility>

#include "worldgen/core/error.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/db/schema.hpp"

namespace worldgen::history {

namespace {

[[noreturn]] void fail(const std::string& message) {
    throw FatalError(message);
}

std::string int_text(std::int64_t v) {
    return db::format_i64(v);
}

std::string size_text(std::size_t v) {
    return db::format_u64(static_cast<std::uint64_t>(v));
}

// [a-z][a-z0-9_]*
bool is_key(std::string_view s) noexcept {
    if (s.empty() || s.front() < 'a' || s.front() > 'z') {
        return false;
    }
    for (const char c : s) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) {
            return false;
        }
    }
    return true;
}

RoleDef role(std::int64_t seq, std::string_view name, EntityType type, std::int64_t min_count,
             std::int64_t max_count) {
    return RoleDef{seq, std::string(name), type, min_count, max_count};
}

// The kinds of P3-04 8.2, in id order. Later specs append here.
EventCatalogue build_compiled() {
    EventCatalogue c;
    c.kinds.push_back(KindDef{1, "awakening", false, 1, 0,
                              {role(1, "race", EntityType::race, 1, 1), role(2, "creator", EntityType::god, 1, 1)},
                              {}, 0, {}});
    c.kinds.push_back(KindDef{2, "civ_founded", true, 1, 0,
                              {role(1, "civilization", EntityType::civilization, 1, 1),
                               role(2, "race", EntityType::race, 1, 1)},
                              {}, 0, {}});
    c.kinds.push_back(KindDef{3, "settlement_founded", false, 1, 1,
                              {role(1, "settlement", EntityType::settlement, 1, 1),
                               role(2, "founder", EntityType::civilization, 1, 1)},
                              {"population"}, 0, {}});
    c.kinds.push_back(KindDef{4, "quiet_era", false, 1, 0, {role(1, "civilization", EntityType::civilization, 1, 1)},
                              {"years", "generations"}, 0, {}});
    c.kinds.push_back(KindDef{5, "flare_up", true, 0, 0, {}, {"years"}, 0, {}});
    // P3-05 section 11: kinds 6 to 19, every one with a region and a settlement
    // use of 1 and every role count 1-1.
    const auto settlement_and_civ = [] {
        return std::vector<RoleDef>{role(1, "settlement", EntityType::settlement, 1, 1),
                                    role(2, "civilization", EntityType::civilization, 1, 1)};
    };
    c.kinds.push_back(KindDef{6, "colony_founded", false, 1, 1,
                              {role(1, "settlement", EntityType::settlement, 1, 1),
                               role(2, "founder", EntityType::civilization, 1, 1),
                               role(3, "origin", EntityType::settlement, 1, 1)},
                              {"population"}, 0, {}});
    c.kinds.push_back(KindDef{7, "became_village", true, 1, 1, settlement_and_civ(), {"population"}, 0, {}});
    c.kinds.push_back(KindDef{8, "became_town", true, 1, 1, settlement_and_civ(), {"population"}, 0, {}});
    c.kinds.push_back(KindDef{9, "became_city", true, 1, 1, settlement_and_civ(), {"population"}, 0, {}});
    c.kinds.push_back(KindDef{10, "settlement_dwindled", false, 1, 1, settlement_and_civ(), {"population"}, 0, {}});
    c.kinds.push_back(KindDef{11, "famine", false, 1, 1, settlement_and_civ(), {"dead", "population"}, 0, {}});
    c.kinds.push_back(KindDef{12, "plague", false, 1, 1, settlement_and_civ(), {"dead", "population"}, 0, {}});
    c.kinds.push_back(KindDef{13, "plague_spread", true, 1, 1, settlement_and_civ(), {"dead", "population"}, 0, {}});
    c.kinds.push_back(KindDef{14, "succession_crisis", false, 1, 1,
                              {role(1, "civilization", EntityType::civilization, 1, 1)}, {}, 0, {}});
    c.kinds.push_back(KindDef{15, "civ_seceded", true, 1, 1,
                              {role(1, "civilization", EntityType::civilization, 1, 1),
                               role(2, "from", EntityType::civilization, 1, 1)},
                              {"settlements"}, 0, {}});
    c.kinds.push_back(KindDef{16, "settlement_abandoned", true, 1, 1, settlement_and_civ(), {"survivors"}, 0, {}});
    c.kinds.push_back(KindDef{17, "capital_moved", true, 1, 1,
                              {role(1, "civilization", EntityType::civilization, 1, 1),
                               role(2, "settlement", EntityType::settlement, 1, 1)},
                              {}, 0, {}});
    c.kinds.push_back(KindDef{18, "civ_ended", true, 1, 1,
                              {role(1, "civilization", EntityType::civilization, 1, 1)}, {}, 0, {}});
    c.kinds.push_back(KindDef{19, "ruin_faded", true, 1, 1, {role(1, "settlement", EntityType::settlement, 1, 1)},
                              {}, 0, {}});
    // P3-06 section 11: kinds 20 to 30, every one caused, every role count
    // 1-1 except parties 2-2.
    const auto two = [](std::string_view first, std::string_view second) {
        return std::vector<RoleDef>{role(1, first, EntityType::civilization, 1, 1),
                                    role(2, second, EntityType::civilization, 1, 1)};
    };
    c.kinds.push_back(KindDef{20, "claim", true, 1, 0, two("claimant", "holder"), {}, 0, {}});
    c.kinds.push_back(KindDef{21, "war_declared", true, 1, 0, two("attacker", "defender"), {}, 0, {}});
    c.kinds.push_back(KindDef{22, "battle", true, 1, 0, two("winner", "loser"), {"winner_dead", "loser_dead"}, 0, {}});
    c.kinds.push_back(KindDef{23, "battle_at_pass", true, 1, 0, two("winner", "loser"),
                              {"winner_dead", "loser_dead", "pass"}, 0, {}});
    c.kinds.push_back(KindDef{24, "siege", true, 1, 1, two("attacker", "defender"), {"fell"}, 0, {}});
    c.kinds.push_back(KindDef{25, "sacked", true, 1, 1, two("attacker", "defender"), {"dead", "survivors"}, 0, {}});
    c.kinds.push_back(
        KindDef{26, "settlement_captured", true, 1, 1, two("attacker", "defender"), {"dead", "fled"}, 0, {}});
    c.kinds.push_back(KindDef{27, "peace", true, 0, 0, two("winner", "loser"), {"years"}, 0, {}});
    c.kinds.push_back(KindDef{28, "truce", true, 0, 0, {role(1, "parties", EntityType::civilization, 2, 2)},
                              {"years"}, 0, {}});
    c.kinds.push_back(KindDef{29, "region_ceded", true, 1, 0, two("from", "to"), {}, 0, {}});
    c.kinds.push_back(KindDef{30, "civ_conquered", true, 1, 0, two("civilization", "conqueror"), {}, 0, {}});
    // P3-05 r6 section 11 (generator 0.17.0): kind 31, an abandonment whose
    // survivors reached another settlement.
    c.kinds.push_back(KindDef{31, "settlement_abandoned_to", true, 1, 1,
                              {role(1, "settlement", EntityType::settlement, 1, 1),
                               role(2, "civilization", EntityType::civilization, 1, 1),
                               role(3, "destination", EntityType::settlement, 1, 1)},
                              {"settled", "lost"}, 0, {}});
    // P3-06 r9 20.9 (P3-09, generator 0.18.0): kind 32, the survivors of a
    // fallen last refuge found a hidden settlement; kind 33, a loser with no
    // region to cede pays people.
    c.kinds.push_back(KindDef{32, "refugees_settled", true, 1, 1,
                              {role(1, "settlement", EntityType::settlement, 1, 1),
                               role(2, "founder", EntityType::civilization, 1, 1),
                               role(3, "origin", EntityType::settlement, 1, 1)},
                              {"population"}, 0, {}});
    c.kinds.push_back(KindDef{33, "tribute", true, 0, 0, two("payer", "payee"), {"people"}, 0, {}});
    // P3-06 r10 20.15, 20.17, 20.22 (P3-09 r3): a war won where the loser did
    // not sue for peace, a search that found no one, a last refuge that held.
    c.kinds.push_back(KindDef{34, "war_won", true, 0, 0, two("winner", "loser"), {"years"}, 0, {}});
    c.kinds.push_back(KindDef{35, "search_abandoned", true, 1, 0, two("searcher", "hidden"), {"years"}, 0, {}});
    c.kinds.push_back(KindDef{36, "refuge_held", true, 1, 1, two("attacker", "defender"), {"reason"}, 0, {}});
    return c;
}

// The compile-time rules of 8.2: ids 1, 2, ...; keys distinct and of the
// key grammar; within a kind, role and value keys distinct from each other
// and of the grammar, role seqs 1, 2, ..., counts sane, uses 0 to 2.
void check_compiled(const EventCatalogue& c) {
    for (std::size_t i = 0; i < c.kinds.size(); ++i) {
        const KindDef& k = c.kinds[i];
        if (k.id != static_cast<std::int64_t>(i) + 1 || !is_key(k.key)) {
            fail("compiled_event_catalogue: kind " + int_text(k.id) + " has a bad id or key");
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (c.kinds[j].key == k.key) {
                fail("compiled_event_catalogue: kind key '" + k.key + "' is used twice");
            }
        }
        if (k.region_use < 0 || k.region_use > 2 || k.settlement_use < 0 || k.settlement_use > 2) {
            fail("compiled_event_catalogue: kind '" + k.key + "' has a region or settlement use outside 0 to 2");
        }
        std::vector<std::string_view> names;
        for (std::size_t r = 0; r < k.roles.size(); ++r) {
            const RoleDef& d = k.roles[r];
            if (d.seq != static_cast<std::int64_t>(r) + 1 || !is_key(d.role) || d.min_count < 0 || d.max_count < 0 ||
                (d.max_count != 0 && d.max_count < d.min_count)) {
                fail("compiled_event_catalogue: kind '" + k.key + "' has a bad role '" + d.role + "'");
            }
            names.push_back(d.role);
        }
        for (const std::string& v : k.values) {
            if (!is_key(v)) {
                fail("compiled_event_catalogue: kind '" + k.key + "' has a bad value key '" + v + "'");
            }
            names.push_back(v);
        }
        std::vector<std::string_view> sorted = names;
        std::stable_sort(sorted.begin(), sorted.end());
        if (std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end()) {
            fail("compiled_event_catalogue: kind '" + k.key + "' repeats a role or value key");
        }
    }
}

// --- Template grammar (8.4) -----------------------------------------------------

enum class TokenType : std::uint8_t { literal, role, value, choice, year, end_year, region, settlement };

enum class Form : std::uint8_t { name, plural, adjective, all };

struct Token {
    TokenType type = TokenType::literal;
    std::string text;           // literal text, or the role / value key
    Form form = Form::name;     // role
    std::string word_one;       // choice: the word for 1
    std::string word_other;     // choice: the word otherwise
    std::string word_zero;      // three-word choice (r10): the word for 0
    bool three = false;         // three-word choice: w0 | w1 | w2, '#' is the value
};

struct Parsed {
    std::vector<Token> tokens;
    std::optional<std::string> problem;
};

bool is_word(std::string_view w) noexcept {
    return w.find_first_of("{}|?") == std::string_view::npos;
}

// Classifies a placeholder body at byte `at`; a problem or a token.
std::optional<std::string> classify(const KindDef& kind, std::string_view body, std::size_t at, Token& out) {
    const std::string where = " at byte " + size_text(at);
    const std::string unknown = "unknown placeholder '{" + std::string(body) + "}'" + where;
    if (!body.empty() && body.front() == '@') {
        if (body == "@year") {
            out.type = TokenType::year;
        } else if (body == "@end_year") {
            out.type = TokenType::end_year;
        } else if (body == "@region") {
            if (kind.region_use != 1) {
                return "{@region} needs a kind with a region" + where;
            }
            out.type = TokenType::region;
        } else if (body == "@settlement") {
            if (kind.settlement_use != 1) {
                return "{@settlement} needs a kind with a settlement" + where;
            }
            out.type = TokenType::settlement;
        } else {
            return unknown;
        }
        return std::nullopt;
    }
    if (const std::size_t q = body.find('?'); q != std::string_view::npos) {
        const std::string_view name = body.substr(0, q);
        const std::string_view rest = body.substr(q + 1);
        // Two words w1|w2, or (r10) three words w0|w1|w2; more bars are unknown.
        std::vector<std::string_view> words;
        std::size_t from = 0;
        while (true) {
            const std::size_t bar = rest.find('|', from);
            words.push_back(rest.substr(from, bar == std::string_view::npos ? std::string_view::npos : bar - from));
            if (bar == std::string_view::npos) {
                break;
            }
            from = bar + 1;
        }
        if (words.size() < 2U || words.size() > 3U || kind.value_seq(name) == 0) {
            return unknown;
        }
        for (const std::string_view w : words) {
            if (!is_word(w)) {
                return unknown;
            }
        }
        out.type = TokenType::choice;
        out.text = std::string(name);
        if (words.size() == 3U) {
            out.three = true;
            out.word_zero = std::string(words[0]);
            out.word_one = std::string(words[1]);
            out.word_other = std::string(words[2]);
        } else {
            out.word_one = std::string(words[0]);
            out.word_other = std::string(words[1]);
        }
        return std::nullopt;
    }
    std::string_view name = body;
    std::optional<std::string_view> form;
    if (const std::size_t colon = body.find(':'); colon != std::string_view::npos) {
        name = body.substr(0, colon);
        form = body.substr(colon + 1);
    }
    const RoleDef* r = kind.find_role(name);
    if (r == nullptr) {
        if (!form && kind.value_seq(name) != 0) {
            out.type = TokenType::value;
            out.text = std::string(name);
            return std::nullopt;
        }
        return unknown;
    }
    out.type = TokenType::role;
    out.text = std::string(name);
    out.form = Form::name;
    if (form) {
        if (*form == "name") {
            out.form = Form::name;
        } else if (*form == "plural") {
            out.form = Form::plural;
        } else if (*form == "adjective") {
            out.form = Form::adjective;
        } else if (*form == "all") {
            out.form = Form::all;
        } else {
            return unknown;
        }
    }
    if ((out.form == Form::plural || out.form == Form::adjective) && r->type != EntityType::race) {
        return "form '" + std::string(*form) + "' needs a race role" + where;
    }
    if (r->min_count == 0 && out.form != Form::all) {
        return "role '" + r->role + "' may have no participant" + where;
    }
    return std::nullopt;
}

Parsed parse_template(const KindDef& kind, std::string_view t) {
    Parsed p;
    std::string literal;
    const auto flush = [&] {
        if (!literal.empty()) {
            Token token;
            token.text = std::move(literal);
            p.tokens.push_back(std::move(token));
            literal.clear();
        }
    };
    std::size_t i = 0;
    while (i < t.size()) {
        const auto b = static_cast<unsigned char>(t[i]);
        if (b >= 0x80U) {
            p.problem = "non-ASCII byte at byte " + size_text(i);
            return p;
        }
        if (t[i] == '}') {
            if (i + 1 < t.size() && t[i + 1] == '}') {
                literal += '}';
                i += 2;
                continue;
            }
            p.problem = "unmatched } at byte " + size_text(i);
            return p;
        }
        if (t[i] != '{') {
            literal += t[i];
            ++i;
            continue;
        }
        if (i + 1 < t.size() && t[i + 1] == '{') {
            literal += '{';
            i += 2;
            continue;
        }
        // A placeholder: its body runs to the next '}'; a '{' before it, or
        // no '}', leaves it unclosed.
        const std::size_t close = t.find_first_of("{}", i + 1);
        if (close == std::string_view::npos || t[close] == '{') {
            p.problem = "unclosed placeholder at byte " + size_text(i);
            return p;
        }
        const std::string_view body = t.substr(i + 1, close - i - 1);
        for (std::size_t k = 0; k < body.size(); ++k) {
            if (static_cast<unsigned char>(body[k]) >= 0x80U) {
                p.problem = "non-ASCII byte at byte " + size_text(i + 1 + k);
                return p;
            }
        }
        Token token;
        if (std::optional<std::string> why = classify(kind, body, i, token)) {
            p.problem = std::move(why);
            return p;
        }
        flush();
        p.tokens.push_back(std::move(token));
        i = close + 1;
    }
    flush();
    if (t.empty()) {
        p.problem = "the template is empty";
    }
    return p;
}

// --- history.report (4.2) ---------------------------------------------------------

const data::Definition* report_definition(const data::ContentSet& content) {
    return content.find(data::HISTORY_KIND, data::HISTORY_REPORT_ID);
}

}  // namespace

std::string_view entity_type_key(EntityType type) noexcept {
    switch (type) {
        case EntityType::race: return "race";
        case EntityType::god: return "god";
        case EntityType::civilization: return "civilization";
        case EntityType::settlement: return "settlement";
        case EntityType::region: return "region";
        case EntityType::figure: return "figure";
        case EntityType::faction: return "faction";
        case EntityType::monster: return "monster";
        case EntityType::site: return "site";
    }
    return "";
}

std::optional<EntityType> entity_type_from_code(std::int64_t code) noexcept {
    if (code < 1 || code > 9) {
        return std::nullopt;
    }
    return static_cast<EntityType>(code);
}

const RoleDef* KindDef::find_role(std::string_view name) const noexcept {
    for (const RoleDef& r : roles) {
        if (r.role == name) {
            return &r;
        }
    }
    return nullptr;
}

std::int64_t KindDef::value_seq(std::string_view key_name) const noexcept {
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (values[i] == key_name) {
            return static_cast<std::int64_t>(i) + 1;
        }
    }
    return 0;
}

const KindDef* EventCatalogue::find(std::string_view key) const noexcept {
    for (const KindDef& k : kinds) {
        if (k.key == key) {
            return &k;
        }
    }
    return nullptr;
}

const KindDef* EventCatalogue::find_id(std::int64_t id) const noexcept {
    if (id < 1 || static_cast<std::uint64_t>(id) > kinds.size()) {
        return nullptr;
    }
    const KindDef& k = kinds[static_cast<std::size_t>(id - 1)];
    return k.id == id ? &k : nullptr;
}

const EventCatalogue& compiled_event_catalogue() {
    static const EventCatalogue catalogue = [] {
        EventCatalogue c = build_compiled();
        check_compiled(c);
        return c;
    }();
    return catalogue;
}

std::optional<std::string> template_problem(const KindDef& kind, std::string_view text) {
    return parse_template(kind, text).problem;
}

std::vector<std::string> report_events_problems(const data::Record& report) {
    const EventCatalogue& compiled = compiled_event_catalogue();
    std::vector<std::string> out;
    const std::vector<data::Value>& events = report.get_array("events");
    std::vector<std::string> keys;
    for (std::size_t i = 0; i < events.size(); ++i) {
        const data::Record& e = events[i].as_record();
        const std::string& key = e.get_string("kind");
        keys.push_back(key);
        const std::string at = "events[" + size_text(i) + "]: ";
        const KindDef* kind = compiled.find(key);
        if (kind == nullptr) {
            out.push_back(at + "kind '" + key + "' is not an event kind");
            continue;
        }
        const auto first = std::find(keys.begin(), keys.end() - 1, key);
        if (first != keys.end() - 1) {
            out.push_back(at + "kind '" + key + "' is already listed at events[" +
                          size_text(static_cast<std::size_t>(first - keys.begin())) + "]");
            continue;
        }
        if (std::optional<std::string> why = template_problem(*kind, e.get_string("template"))) {
            out.push_back(at + "template: " + *why);
        }
    }
    for (const KindDef& k : compiled.kinds) {
        if (std::find(keys.begin(), keys.end(), k.key) == keys.end()) {
            out.push_back("event kind '" + k.key + "' has no entry");
        }
    }
    return out;
}

EventCatalogue event_catalogue(const data::ContentSet& content) {
    const data::Definition* report = report_definition(content);
    if (report == nullptr) {
        fail("history: report: the content has no definition history.report");
    }
    if (const std::vector<std::string> problems = report_events_problems(report->record); !problems.empty()) {
        fail("history: report: " + problems.front());
    }
    EventCatalogue c = compiled_event_catalogue();
    const std::vector<data::Value>& events = report->record.get_array("events");
    for (KindDef& k : c.kinds) {
        for (const data::Value& v : events) {
            const data::Record& e = v.as_record();
            if (e.get_string("kind") == k.key) {
                k.salience = e.get_int("salience");
                k.templ = e.get_string("template");
                break;
            }
        }
    }
    return c;
}

CatalogueRows catalogue_rows(const EventCatalogue& catalogue) {
    CatalogueRows rows;
    for (const KindDef& k : catalogue.kinds) {
        rows.kinds.push_back(db::EventKindRow{k.id, k.key, k.caused ? 1 : 0, k.region_use, k.settlement_use,
                                              k.salience, k.templ});
        for (const RoleDef& r : k.roles) {
            rows.roles.push_back(db::EventKindRoleRow{k.id, r.seq, r.role, static_cast<std::int64_t>(r.type),
                                                      r.min_count, r.max_count});
        }
        for (std::size_t i = 0; i < k.values.size(); ++i) {
            rows.values.push_back(db::EventKindValueRow{k.id, static_cast<std::int64_t>(i) + 1, k.values[i]});
        }
    }
    return rows;
}

namespace {

template <class T>
T take(db::Result<T> result, std::string_view prefix) {
    if (!result.ok()) {
        fail(std::string(prefix) + result.error().message);
    }
    return std::move(result).value();
}

}  // namespace

EventCatalogue read_event_catalogue(const db::WorldReader& world) {
    const std::string prefix = "read_event_catalogue: ";
    const std::vector<db::EventKindRow> kinds = take(db::read_event_kind_rows(world), prefix);
    const std::vector<db::EventKindRoleRow> roles = take(db::read_event_kind_role_rows(world), prefix);
    const std::vector<db::EventKindValueRow> values = take(db::read_event_kind_value_rows(world), prefix);
    EventCatalogue c;
    for (const db::EventKindRow& k : kinds) {
        if (k.id != static_cast<std::int64_t>(c.kinds.size()) + 1) {
            fail(prefix + "event_kind ids are not 1, 2, ... at id " + int_text(k.id));
        }
        c.kinds.push_back(KindDef{k.id, k.key, k.caused != 0, k.region_use, k.settlement_use, {}, {}, k.salience,
                                  k.template_text});
    }
    for (const db::EventKindRoleRow& r : roles) {
        KindDef* k = r.kind >= 1 && static_cast<std::uint64_t>(r.kind) <= c.kinds.size()
                         ? &c.kinds[static_cast<std::size_t>(r.kind - 1)]
                         : nullptr;
        const std::optional<EntityType> type = entity_type_from_code(r.entity_type);
        if (k == nullptr || r.seq != static_cast<std::int64_t>(k->roles.size()) + 1 || !type) {
            fail(prefix + "event_kind_role row (" + int_text(r.kind) + ", " + int_text(r.seq) + ") is not valid");
        }
        k->roles.push_back(RoleDef{r.seq, r.role, *type, r.min_count, r.max_count});
    }
    for (const db::EventKindValueRow& v : values) {
        KindDef* k = v.kind >= 1 && static_cast<std::uint64_t>(v.kind) <= c.kinds.size()
                         ? &c.kinds[static_cast<std::size_t>(v.kind - 1)]
                         : nullptr;
        if (k == nullptr || v.seq != static_cast<std::int64_t>(k->values.size()) + 1) {
            fail(prefix + "event_kind_value row (" + int_text(v.kind) + ", " + int_text(v.seq) + ") is not valid");
        }
        k->values.push_back(v.key);
    }
    return c;
}

// --- HistoryView ----------------------------------------------------------------------

namespace {

template <class T>
const T* find_by_id(const std::vector<std::pair<std::int64_t, T>>& items, std::int64_t id) noexcept {
    const auto it = std::lower_bound(items.begin(), items.end(), id,
                                     [](const std::pair<std::int64_t, T>& e, std::int64_t v) { return e.first < v; });
    return it != items.end() && it->first == id ? &it->second : nullptr;
}

}  // namespace

namespace {

// A problem found by HistoryView::load: the whole message.
struct ViewProblem {
    std::string message;
};

[[noreturn]] void view_fail(const std::string& message) {
    throw ViewProblem{message};
}

}  // namespace

db::Result<HistoryView> HistoryView::load(const db::WorldReader& world) {
    std::optional<db::Error> reader_error;
    try {
        const auto read = [&reader_error](auto result, std::string_view table) {
            if (!result.ok()) {
                reader_error = result.error();
                (void)table;
                throw ViewProblem{reader_error->message};
            }
            return std::move(result).value();
        };
        HistoryView v;
        // The stored catalogue, checked as read_event_catalogue checks it.
        const std::vector<db::EventKindRow> kind_rows = read(db::read_event_kind_rows(world), db::EVENT_KIND_TABLE);
        const std::vector<db::EventKindRoleRow> role_rows =
            read(db::read_event_kind_role_rows(world), db::EVENT_KIND_ROLE_TABLE);
        const std::vector<db::EventKindValueRow> value_rows =
            read(db::read_event_kind_value_rows(world), db::EVENT_KIND_VALUE_TABLE);
        for (const db::EventKindRow& k : kind_rows) {
            if (k.id != static_cast<std::int64_t>(v.catalogue_.kinds.size()) + 1) {
                view_fail("corrupt table 'event_kind': row " + int_text(k.id) + ": id not consecutive");
            }
            v.catalogue_.kinds.push_back(KindDef{k.id, k.key, k.caused != 0, k.region_use, k.settlement_use, {}, {},
                                                 k.salience, k.template_text});
        }
        for (const db::EventKindRoleRow& r : role_rows) {
            KindDef* k = r.kind >= 1 && static_cast<std::uint64_t>(r.kind) <= v.catalogue_.kinds.size()
                             ? &v.catalogue_.kinds[static_cast<std::size_t>(r.kind - 1)]
                             : nullptr;
            const std::optional<EntityType> type = entity_type_from_code(r.entity_type);
            if (k == nullptr || r.seq != static_cast<std::int64_t>(k->roles.size()) + 1 || !type) {
                view_fail("corrupt table 'event_kind_role': row (" + int_text(r.kind) + ", " + int_text(r.seq) +
                          "): not a role of a stored kind");
            }
            k->roles.push_back(RoleDef{r.seq, r.role, *type, r.min_count, r.max_count});
        }
        for (const db::EventKindValueRow& r : value_rows) {
            KindDef* k = r.kind >= 1 && static_cast<std::uint64_t>(r.kind) <= v.catalogue_.kinds.size()
                             ? &v.catalogue_.kinds[static_cast<std::size_t>(r.kind - 1)]
                             : nullptr;
            if (k == nullptr || r.seq != static_cast<std::int64_t>(k->values.size()) + 1) {
                view_fail("corrupt table 'event_kind_value': row (" + int_text(r.kind) + ", " + int_text(r.seq) +
                          "): not a value key of a stored kind");
            }
            k->values.push_back(r.key);
        }
        for (const KindDef& k : v.catalogue_.kinds) {
            if (std::optional<std::string> why = template_problem(k, k.templ)) {
                view_fail("corrupt table 'event_kind': row " + int_text(k.id) + ": template: " + *why);
            }
        }
        for (const db::RaceRow& r : read(db::read_race_rows(world), db::RACE_TABLE)) {
            v.races_.emplace_back(r.id, RaceNames{r.name, r.plural, r.adjective});
        }
        for (const db::GodRow& r : read(db::read_god_rows(world), db::GOD_TABLE)) {
            v.gods_.emplace_back(r.id, r.name);
        }
        for (const db::CivilizationRow& r : read(db::read_civilization_rows(world), db::CIVILIZATION_TABLE)) {
            v.civilizations_.emplace_back(r.id, r.name);
        }
        for (const db::SettlementRow& r : read(db::read_settlement_rows(world), db::SETTLEMENT_TABLE)) {
            v.settlements_.emplace_back(r.id, r.name);
        }
        for (const db::RegionRow& r : read(db::read_region_rows(world), db::REGION_TABLE)) {
            v.regions_.emplace_back(r.id, r.name);
        }
        for (const db::EventRow& r : read(db::read_event_rows(world), db::EVENT_TABLE)) {
            const std::string at = "corrupt table 'event': row " + int_text(r.id) + ": ";
            if (r.id != static_cast<std::int64_t>(v.events_.size()) + 1) {
                view_fail(at + "id not consecutive");
            }
            const KindDef* kind = v.catalogue_.find_id(r.kind);
            if (kind == nullptr) {
                view_fail(at + "kind " + int_text(r.kind) + " is not an event kind");
            }
            if ((r.region != 0 || kind->region_use == 1) && find_by_id(v.regions_, r.region) == nullptr) {
                view_fail(at + "region " + int_text(r.region) + " does not exist");
            }
            if ((r.settlement != 0 || kind->settlement_use == 1) && find_by_id(v.settlements_, r.settlement) == nullptr) {
                view_fail(at + "settlement " + int_text(r.settlement) + " does not exist");
            }
            v.events_.push_back(Event{r, {}, {}, {}});
        }
        const auto event_of = [&v](std::int64_t id, std::string_view table, const std::string& key) -> Event& {
            if (id < 1 || static_cast<std::uint64_t>(id) > v.events_.size()) {
                view_fail("corrupt table '" + std::string(table) + "': row " + key + ": event " + int_text(id) +
                          " does not exist");
            }
            return v.events_[static_cast<std::size_t>(id - 1)];
        };
        for (const db::EventParticipantRow& r :
             read(db::read_event_participant_rows(world), db::EVENT_PARTICIPANT_TABLE)) {
            const std::string key = "(" + int_text(r.event) + ", " + int_text(r.seq) + ")";
            Event& e = event_of(r.event, db::EVENT_PARTICIPANT_TABLE, key);
            const KindDef& kind = *v.catalogue_.find_id(e.row.kind);
            const std::string at = "corrupt table 'event_participant': row " + key + ": ";
            if (r.seq != static_cast<std::int64_t>(e.participants.size()) + 1) {
                view_fail(at + "seq not consecutive");
            }
            if (r.role < 1 || static_cast<std::uint64_t>(r.role) > kind.roles.size()) {
                view_fail(at + "role " + int_text(r.role) + " is not a role of kind '" + kind.key + "'");
            }
            const std::optional<EntityType> type = entity_type_from_code(r.entity_type);
            if (!type || *type != kind.roles[static_cast<std::size_t>(r.role - 1)].type) {
                view_fail(at + "entity type " + int_text(r.entity_type) + " is not the role's");
            }
            if (v.name(*type, r.entity_id) == nullptr) {
                view_fail(at + std::string(entity_type_key(*type)) + " " + int_text(r.entity_id) + " does not exist");
            }
            e.participants.push_back(r);
        }
        for (const db::EventValueRow& r : read(db::read_event_value_rows(world), db::EVENT_VALUE_TABLE)) {
            const std::string key = "(" + int_text(r.event) + ", " + int_text(r.seq) + ")";
            Event& e = event_of(r.event, db::EVENT_VALUE_TABLE, key);
            if (r.seq != static_cast<std::int64_t>(e.values.size()) + 1 ||
                e.values.size() >= v.catalogue_.find_id(e.row.kind)->values.size()) {
                view_fail("corrupt table 'event_value': row " + key + ": seq is not the next value key of the kind");
            }
            e.values.push_back(r.value);
        }
        for (const db::EventCauseRow& r : read(db::read_event_cause_rows(world), db::EVENT_CAUSE_TABLE)) {
            const std::string key = "(" + int_text(r.event) + ", " + int_text(r.cause) + ")";
            Event& e = event_of(r.event, db::EVENT_CAUSE_TABLE, key);
            if (r.cause < 1 || r.cause >= r.event) {
                view_fail("corrupt table 'event_cause': row " + key + ": cause not below event");
            }
            e.causes.push_back(r.cause);
        }
        for (const Event& e : v.events_) {
            const KindDef& kind = *v.catalogue_.find_id(e.row.kind);
            if (e.values.size() != kind.values.size()) {
                view_fail("corrupt table 'event_value': event " + int_text(e.row.id) + " lacks a value of its kind");
            }
            // Every role a template may name without :all has a participant.
            for (const RoleDef& r : kind.roles) {
                std::int64_t n = 0;
                for (const db::EventParticipantRow& p : e.participants) {
                    n += p.role == r.seq ? 1 : 0;
                }
                if (n < r.min_count || (r.max_count != 0 && n > r.max_count)) {
                    view_fail("corrupt table 'event_participant': event " + int_text(e.row.id) + " has " +
                              int_text(n) + " participants of role '" + r.role + "', expected " +
                              int_text(r.min_count) + " to " +
                              (r.max_count == 0 ? std::string("any") : int_text(r.max_count)));
                }
            }
        }
        return v;
    } catch (const ViewProblem& p) {
        if (reader_error) {
            return *reader_error;
        }
        return db::Error{db::ErrorCode::corrupt_layer, p.message};
    } catch (const std::bad_alloc&) {
        return db::Error{db::ErrorCode::out_of_memory, "out of memory while reading the history tables"};
    }
}

std::vector<std::int64_t> HistoryView::race_ids() const {
    std::vector<std::int64_t> out;
    for (const auto& [id, names] : races_) {
        out.push_back(id);
    }
    return out;
}

const HistoryView::Event* HistoryView::event(std::int64_t id) const noexcept {
    if (id < 1 || static_cast<std::uint64_t>(id) > events_.size()) {
        return nullptr;
    }
    return &events_[static_cast<std::size_t>(id - 1)];
}

const std::string* HistoryView::name(EntityType type, std::int64_t id) const noexcept {
    switch (type) {
        case EntityType::race: {
            const RaceNames* r = find_by_id(races_, id);
            return r != nullptr ? &r->name : nullptr;
        }
        case EntityType::god: return find_by_id(gods_, id);
        case EntityType::civilization: return find_by_id(civilizations_, id);
        case EntityType::settlement: return find_by_id(settlements_, id);
        case EntityType::region: return find_by_id(regions_, id);
        case EntityType::figure:
        case EntityType::faction:
        case EntityType::monster:
        case EntityType::site: return nullptr;
    }
    return nullptr;
}

const std::string* HistoryView::race_plural(std::int64_t id) const noexcept {
    const RaceNames* r = find_by_id(races_, id);
    return r != nullptr ? &r->plural : nullptr;
}

const std::string* HistoryView::race_adjective(std::int64_t id) const noexcept {
    const RaceNames* r = find_by_id(races_, id);
    return r != nullptr ? &r->adjective : nullptr;
}

// --- render_event (8.5) ------------------------------------------------------------------

std::string render_event(const HistoryView& view, std::int64_t event_id) {
    const std::string at = "render_event: event " + int_text(event_id) + ": ";
    const HistoryView::Event* e = view.event(event_id);
    if (e == nullptr) {
        fail(at + "not in the log");
    }
    const KindDef* kind = view.catalogue().find_id(e->row.kind);
    if (kind == nullptr) {
        fail(at + "kind " + int_text(e->row.kind) + " is not an event kind");
    }
    Parsed parsed = parse_template(*kind, kind->templ);
    if (parsed.problem) {
        fail(at + *parsed.problem);
    }
    const auto name_of = [&](const db::EventParticipantRow& p, Form form) -> const std::string& {
        const auto type = static_cast<EntityType>(p.entity_type);  // checked by HistoryView::load
        const std::string* s = nullptr;
        if (form == Form::plural) {
            s = view.race_plural(p.entity_id);
        } else if (form == Form::adjective) {
            s = view.race_adjective(p.entity_id);
        } else {
            s = view.name(type, p.entity_id);
        }
        if (s == nullptr) {
            fail(at + "no name for " + std::string(entity_type_key(type)) + " " + int_text(p.entity_id));
        }
        return *s;
    };
    const auto value_of = [&](const std::string& key) {
        const std::int64_t seq = kind->value_seq(key);
        return e->values[static_cast<std::size_t>(seq - 1)];  // every value present (HistoryView::load)
    };
    std::string out;
    // r10 (P1-06 r3 14.7): a name's leading "The " is written "the " unless
    // the name is appended at a sentence start (offset 0 or after ". ").
    const auto append_name = [&out](const std::string& name) {
        constexpr std::string_view THE = "The ";
        const bool sentence_start = out.empty() || (out.size() >= 2U && out.compare(out.size() - 2U, 2U, ". ") == 0);
        if (!sentence_start && name.size() >= THE.size() && name.compare(0, THE.size(), THE) == 0) {
            out += "the ";
            out.append(name, THE.size(), std::string::npos);
        } else {
            out += name;
        }
    };
    for (const Token& t : parsed.tokens) {
        switch (t.type) {
            case TokenType::literal: out += t.text; break;
            case TokenType::role: {
                const std::int64_t seq = kind->find_role(t.text)->seq;
                std::vector<const db::EventParticipantRow*> ps;
                for (const db::EventParticipantRow& p : e->participants) {
                    if (p.role == seq) {
                        ps.push_back(&p);
                    }
                }
                if (t.form == Form::all) {
                    for (std::size_t i = 0; i < ps.size(); ++i) {
                        if (i > 0) {
                            out += i + 1 == ps.size() ? " and " : ", ";
                        }
                        append_name(name_of(*ps[i], Form::name));
                    }
                } else {
                    if (ps.empty()) {
                        fail(at + "no participant with role '" + t.text + "'");
                    }
                    if (t.form == Form::name) {
                        append_name(name_of(*ps.front(), t.form));
                    } else {
                        out += name_of(*ps.front(), t.form);  // race words are copied as they are
                    }
                }
                break;
            }
            case TokenType::value: out += int_text(value_of(t.text)); break;
            case TokenType::choice: {
                const std::int64_t v = value_of(t.text);
                if (!t.three) {
                    out += v == 1 ? t.word_one : t.word_other;
                    break;
                }
                // r10: w0 for 0, w1 for 1, w2 otherwise; '#' is the value.
                const std::string& word = v == 0 ? t.word_zero : v == 1 ? t.word_one : t.word_other;
                for (const char c : word) {
                    if (c == '#') {
                        out += int_text(v);
                    } else {
                        out += c;
                    }
                }
                break;
            }
            case TokenType::year: out += int_text(e->row.year); break;
            case TokenType::end_year: out += int_text(e->row.end_year); break;
            case TokenType::region: {
                const std::string* s = view.name(EntityType::region, e->row.region);
                if (s == nullptr) {
                    fail(at + "no region " + int_text(e->row.region));
                }
                append_name(*s);
                break;
            }
            case TokenType::settlement: {
                const std::string* s = view.name(EntityType::settlement, e->row.settlement);
                if (s == nullptr) {
                    fail(at + "no settlement " + int_text(e->row.settlement));
                }
                append_name(*s);
                break;
            }
        }
    }
    return out;
}

}  // namespace worldgen::history
