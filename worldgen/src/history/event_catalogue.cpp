// The event catalogue, the template grammar and the renderer (spec P3-04
// section 8).

#include "worldgen/history/event_catalogue.hpp"

#include <algorithm>
#include <tuple>
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

// P3-04 r12 20.3: a value as rendered in a sentence, with a comma between
// groups of three digits from |v| >= 1000 (after the sign).
std::string grouped_text(std::int64_t v) {
    const std::string digits = int_text(v);
    const std::size_t sign = !digits.empty() && digits.front() == '-' ? 1U : 0U;
    const std::size_t n = digits.size() - sign;
    if (n <= 3U) {
        return digits;
    }
    std::string out = digits.substr(0, sign);
    for (std::size_t i = 0; i < n; ++i) {
        if (i > 0U && (n - i) % 3U == 0U) {
            out += ',';
        }
        out += digits[sign + i];
    }
    return out;
}

// P3-04 r17 21.1: a value in a sentence; one to nine as lower-case
// words, every other value as grouped_text.
std::string sentence_value_text(std::int64_t v) {
    static constexpr std::string_view WORDS[] = {"one", "two", "three", "four", "five",
                                                 "six", "seven", "eight", "nine"};
    if (v >= 1 && v <= 9) {
        return std::string(WORDS[static_cast<std::size_t>(v - 1)]);
    }
    return grouped_text(v);
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
    // P3-04 r12 20.4 (P3-10, generator 0.19.0): kind 37, an outbreak with its
    // total toll (P3-05 22.1; replaces kind 12); kind 38, one quiet line for
    // the world per quiet phase row (20.1; replaces kind 4).
    c.kinds.push_back(
        KindDef{37, "plague_outbreak", false, 1, 1, settlement_and_civ(), {"dead", "population", "total", "reached"}, 0, {}});
    c.kinds.push_back(KindDef{38, "quiet_stretch", false, 0, 0, {role(1, "realm", EntityType::civilization, 1, 0)},
                              {"years", "realms"}, 0, {}});
    // P3-04 r17 21.2 (P3-11, generator 0.20.0): kind 39, a claim on the
    // settlements of a holder that does not hold the region (P3-06 23.2);
    // kind 40, the foothold cession of P3-06 22.5 item 3.2 (23.3).
    c.kinds.push_back(KindDef{39, "claim_settlements", true, 1, 0,
                              {role(1, "claimant", EntityType::civilization, 1, 1),
                               role(2, "holder", EntityType::civilization, 1, 1),
                               role(3, "settlement", EntityType::settlement, 1, 0)},
                              {}, 0, {}});
    c.kinds.push_back(KindDef{40, "settlements_ceded", true, 1, 0,
                              {role(1, "from", EntityType::civilization, 1, 1),
                               role(2, "to", EntityType::civilization, 1, 1),
                               role(3, "settlement", EntityType::settlement, 1, 0)},
                              {}, 0, {}});
    // P3-04 r19 22.2 (P5-01, generator 0.21.0): kind 41, a claim dropped at a
    // declaration with no allowed goal (P3-06 24.3); kind 42, the years a
    // fled refuge stays hidden, as a span (P3-06 24.4).
    c.kinds.push_back(KindDef{41, "claim_dropped", true, 1, 0, two("claimant", "holder"), {}, 0, {}});
    c.kinds.push_back(KindDef{42, "refuge_hidden", true, 1, 1,
                              {role(1, "settlement", EntityType::settlement, 1, 1),
                               role(2, "founder", EntityType::civilization, 1, 1)},
                              {}, 0, {}});
    // P3-04 r21 23.2 (P5-12, generator 0.22.0): kind 43, a region renamed by
    // history, caused by its defining event.
    c.kinds.push_back(KindDef{43, "region_renamed", true, 1, 2, {role(1, "namer", EntityType::civilization, 0, 1)},
                              {}, 0, {}});
    // P5-02 10.1 (generator 0.23.0): kinds 44 to 55, beasts, monsters and
    // sites.
    const auto monster_role = [] { return role(1, "monster", EntityType::monster, 1, 1); };
    c.kinds.push_back(KindDef{44, "monster_appeared", false, 1, 0,
                              {monster_role(), role(2, "lair", EntityType::site, 1, 1)}, {}, 0, {}});
    c.kinds.push_back(KindDef{45, "undead_rose", true, 1, 2,
                              {monster_role(), role(2, "lair", EntityType::site, 1, 1)}, {}, 0, {}});
    c.kinds.push_back(KindDef{46, "monster_woke", true, 1, 0, {monster_role()}, {}, 0, {}});
    c.kinds.push_back(KindDef{47, "monster_slept", true, 1, 0,
                              {monster_role(), role(2, "lair", EntityType::site, 1, 1)}, {}, 0, {}});
    c.kinds.push_back(KindDef{48, "monster_raid", true, 1, 1,
                              {monster_role(), role(2, "civilization", EntityType::civilization, 1, 1)},
                              {"dead", "plunder"}, 0, {}});
    c.kinds.push_back(KindDef{49, "settlement_overrun", true, 1, 1,
                              {monster_role(), role(2, "civilization", EntityType::civilization, 1, 1),
                               role(3, "lair", EntityType::site, 1, 1)},
                              {"survivors"}, 0, {}});
    c.kinds.push_back(KindDef{50, "monster_slain", true, 1, 0,
                              {monster_role(), role(2, "slayer", EntityType::civilization, 1, 1)}, {"hunters"}, 0, {}});
    c.kinds.push_back(KindDef{51, "monster_driven_off", true, 1, 0,
                              {monster_role(), role(2, "civilization", EntityType::civilization, 1, 1)}, {}, 0, {}});
    c.kinds.push_back(KindDef{52, "monster_died", true, 1, 0, {monster_role()}, {}, 0, {}});
    c.kinds.push_back(KindDef{53, "beasts_bold", false, 1, 1,
                              {role(1, "settlement", EntityType::settlement, 1, 1),
                               role(2, "civilization", EntityType::civilization, 1, 1)},
                              {"beast", "dead"}, 0, {}});
    c.kinds.push_back(KindDef{54, "outpost_built", true, 1, 0,
                              {role(1, "civilization", EntityType::civilization, 1, 1),
                               role(2, "enemy", EntityType::civilization, 1, 1), role(3, "site", EntityType::site, 1, 1)},
                              {}, 0, {}});
    c.kinds.push_back(KindDef{55, "outpost_abandoned", false, 1, 0,
                              {role(1, "civilization", EntityType::civilization, 1, 1),
                               role(2, "site", EntityType::site, 1, 1)},
                              {"reason"}, 0, {}});
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

enum class TokenType : std::uint8_t {
    literal,
    role,
    value,
    choice,
    year,
    end_year,
    region,
    settlement,
    causes,
    region_former,  // r21 23.1
    beast,          // P5-02 10.3: {@beast}
};

enum class Form : std::uint8_t { name, plural, adjective, all, kind };  // kind: P5-02 10.3

struct Token {
    TokenType type = TokenType::literal;
    std::string text;           // literal text, or the role / value key
    Form form = Form::name;     // role
    std::string word_one;       // choice: the word for 1
    std::string word_other;     // choice: the word otherwise
    std::string word_zero;      // three-word choice (r10): the word for 0
    bool three = false;         // three-word choice: w0 | w1 | w2, '#' is the value
    // r12 20.2, {@causes:key=cword,...|before|after}: (kind key, cword) in
    // template order; the two words in word_one (before) and word_other (after).
    std::vector<std::pair<std::string, std::string>> cause_words;
};

struct Parsed {
    std::vector<Token> tokens;
    std::optional<std::string> problem;
};

bool is_word(std::string_view w) noexcept {
    return w.find_first_of("{}|?") == std::string_view::npos;
}

// r12 20.2: a cword is one or more bytes other than { } | ? , =.
bool is_cause_word(std::string_view w) noexcept {
    return !w.empty() && w.find_first_of("{}|?,=") == std::string_view::npos;
}

// r12 20.2: the body after "@causes:"; false when it does not match the grammar.
bool parse_causes(std::string_view rest, Token& out) {
    const std::size_t bar = rest.find('|');
    if (bar == std::string_view::npos) {
        return false;
    }
    const std::string_view entries = rest.substr(0, bar);
    const std::string_view words = rest.substr(bar + 1);
    const std::size_t bar2 = words.find('|');
    if (bar2 == std::string_view::npos) {
        return false;
    }
    const std::string_view before = words.substr(0, bar2);
    const std::string_view after = words.substr(bar2 + 1);
    if (after.find('|') != std::string_view::npos || !is_word(before) || !is_word(after)) {
        return false;
    }
    std::vector<std::pair<std::string, std::string>> list;
    std::size_t from = 0;
    while (true) {
        const std::size_t comma = entries.find(',', from);
        const std::string_view entry =
            entries.substr(from, comma == std::string_view::npos ? std::string_view::npos : comma - from);
        const std::size_t eq = entry.find('=');
        if (eq == std::string_view::npos) {
            return false;
        }
        const std::string_view key = entry.substr(0, eq);
        const std::string_view word = entry.substr(eq + 1);
        if (!is_key(key) || !is_cause_word(word)) {
            return false;
        }
        for (const auto& [k, w] : list) {
            if (k == key) {
                return false;  // the same key twice
            }
        }
        list.emplace_back(std::string(key), std::string(word));
        if (comma == std::string_view::npos) {
            break;
        }
        from = comma + 1;
    }
    out.type = TokenType::causes;
    out.cause_words = std::move(list);
    out.word_one = std::string(before);
    out.word_other = std::string(after);
    return true;
}

// Classifies a placeholder body at byte `at`; a problem or a token.
std::optional<std::string> classify(const KindDef& kind, std::string_view body, std::size_t at, Token& out) {
    const std::string where = " at byte " + size_text(at);
    const std::string unknown = "unknown placeholder '{" + std::string(body) + "}'" + where;
    constexpr std::string_view CAUSES = "@causes:";
    if (body.size() >= CAUSES.size() && body.compare(0, CAUSES.size(), CAUSES) == 0) {
        if (!parse_causes(body.substr(CAUSES.size()), out)) {
            return unknown;
        }
        return std::nullopt;
    }
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
        } else if (body == "@region_former") {  // r21 23.1
            if (kind.region_use != 1) {
                return "{@region_former} needs a kind with a region" + where;
            }
            out.type = TokenType::region_former;
        } else if (body == "@settlement") {
            if (kind.settlement_use != 1) {
                return "{@settlement} needs a kind with a settlement" + where;
            }
            out.type = TokenType::settlement;
        } else if (body == "@beast") {  // P5-02 10.3
            if (kind.value_seq("beast") == 0) {
                return "{@beast} needs a kind with a value 'beast'" + where;
            }
            out.type = TokenType::beast;
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
        } else if (*form == "kind") {  // P5-02 10.3
            out.form = Form::kind;
        } else {
            return unknown;
        }
    }
    if ((out.form == Form::plural || out.form == Form::adjective) && r->type != EntityType::race) {
        return "form '" + std::string(*form) + "' needs a race role" + where;
    }
    if (out.form == Form::kind && r->type != EntityType::monster) {
        return "form 'kind' needs a monster role" + where;
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
            v.races_.emplace_back(r.id, RaceNames{r.name, r.plural, r.adjective, r.generation_years});
        }
        for (const db::GodRow& r : read(db::read_god_rows(world), db::GOD_TABLE)) {
            v.gods_.emplace_back(r.id, r.name);
        }
        for (const db::CivilizationRow& r : read(db::read_civilization_rows(world), db::CIVILIZATION_TABLE)) {
            v.civilizations_.emplace_back(r.id, r.name);
            v.civilization_info_.emplace_back(r.id, CivilizationInfo{r.race, r.founded_year});
        }
        const std::vector<db::SettlementRow> settlement_rows = read(db::read_settlement_rows(world), db::SETTLEMENT_TABLE);
        for (const db::SettlementRow& r : settlement_rows) {
            v.settlements_.emplace_back(r.id, r.name);
        }
        for (const db::RegionRow& r : read(db::read_region_rows(world), db::REGION_TABLE)) {
            v.regions_.emplace_back(r.id, r.name);
        }
        // P5-02 10.3 (schema 9): the names of monsters and sites, the nouns
        // of the monster kinds, the plurals of the beasts. Rows in primary-key
        // order (ids are checked by monster_tables_problem below).
        v.beast_rows_ = read(db::read_beast_rows(world), db::BEAST_TABLE);
        v.monster_kind_rows_ = read(db::read_monster_kind_rows(world), db::MONSTER_KIND_TABLE);
        v.monster_rows_ = read(db::read_monster_rows(world), db::MONSTER_TABLE);
        v.site_rows_ = read(db::read_site_rows(world), db::SITE_TABLE);
        for (const db::BeastRow& r : v.beast_rows_) {
            v.beast_plurals_.emplace_back(r.id, r.plural);
        }
        for (const db::MonsterKindRow& r : v.monster_kind_rows_) {
            v.monster_kind_nouns_.emplace_back(r.id, r.noun);
        }
        for (const db::MonsterRow& r : v.monster_rows_) {
            v.monsters_.emplace_back(r.id, MonsterNames{r.name, r.kind});
        }
        for (const db::SiteRow& r : v.site_rows_) {
            v.sites_.emplace_back(r.id, r.name);
        }
        const auto ascending = [](const auto& list) {
            for (std::size_t i = 1; i < list.size(); ++i) {
                if (list[i].first <= list[i - 1].first) {
                    return false;
                }
            }
            return true;
        };
        if (!ascending(v.beast_plurals_) || !ascending(v.monster_kind_nouns_) || !ascending(v.monsters_) ||
            !ascending(v.sites_)) {
            // A file whose ids do not ascend is refused by the 11.4 checks;
            // the lookups above need sorted ids, so they are cleared here and
            // the checks report the row (P5-02 11.4).
            v.beast_plurals_.clear();
            v.monster_kind_nouns_.clear();
            v.monsters_.clear();
            v.sites_.clear();
        }
        std::vector<std::int64_t> culture_ids;
        for (const db::CultureRow& r : read(db::read_culture_rows(world), db::CULTURE_TABLE)) {
            culture_ids.push_back(r.id);
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
            // P5-02 11.4: a monster or site participant is checked with the
            // tables of schema 9 below.
            if (*type != EntityType::monster && *type != EntityType::site && v.name(*type, r.entity_id) == nullptr) {
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
        // r21 23.1 (schema 8): table region_name, checked as P3-07 r21 26.6.
        v.region_names_ = read(db::read_region_name_rows(world), db::REGION_NAME_TABLE);
        {
            std::vector<std::int64_t> region_ids;
            for (const auto& [id, name] : v.regions_) {
                region_ids.push_back(id);
            }
            std::vector<db::EventRow> event_rows;
            for (const Event& e : v.events_) {
                event_rows.push_back(e.row);
            }
            std::int64_t end_year = 0;
            if (const std::optional<std::string> text = world.meta(db::META_HISTORY_END_YEAR)) {
                if (const std::optional<std::uint64_t> y = db::parse_canonical_u64(*text); y && *y <= 1000000000U) {
                    end_year = static_cast<std::int64_t>(*y);
                }
            }
            const KindDef* renamed = v.catalogue_.find("region_renamed");
            if (std::optional<std::string> why = region_name_rows_problem(
                    v.region_names_, region_ids, culture_ids, end_year, event_rows, renamed != nullptr ? renamed->id : 0)) {
                view_fail(*why);
            }
        }
        // P5-02 11.4 (schema 9), after the region_name checks.
        {
            std::vector<db::EventRow> event_rows;
            std::vector<db::EventParticipantRow> participants;
            std::vector<db::EventValueRow> values;
            std::vector<db::EventCauseRow> causes;
            for (const Event& e : v.events_) {
                event_rows.push_back(e.row);
                participants.insert(participants.end(), e.participants.begin(), e.participants.end());
                for (std::size_t i = 0; i < e.values.size(); ++i) {
                    values.push_back(db::EventValueRow{e.row.id, static_cast<std::int64_t>(i) + 1, e.values[i]});
                }
                for (const std::int64_t c : e.causes) {
                    causes.push_back(db::EventCauseRow{e.row.id, c});
                }
            }
            std::vector<std::int64_t> region_ids;
            for (const auto& [id, name] : v.regions_) {
                region_ids.push_back(id);
            }
            std::vector<std::int64_t> civ_ids;
            for (const auto& [id, name] : v.civilizations_) {
                civ_ids.push_back(id);
            }
            std::stable_sort(civ_ids.begin(), civ_ids.end());
            std::int64_t end_year = 0;
            std::int64_t map_size = 0;
            if (const std::optional<std::string> text = world.meta(db::META_HISTORY_END_YEAR)) {
                if (const std::optional<std::uint64_t> y = db::parse_canonical_u64(*text); y && *y <= 1000000000U) {
                    end_year = static_cast<std::int64_t>(*y);
                }
            }
            if (const std::optional<std::string> text = world.meta(db::META_PARAM_MAP_SIZE)) {
                if (const std::optional<std::uint64_t> n = db::parse_canonical_u64(*text); n && *n <= 65536U) {
                    map_size = static_cast<std::int64_t>(*n);
                }
            }
            MonsterTables mt;
            mt.beasts = v.beast_rows_;
            mt.monster_kinds = v.monster_kind_rows_;
            const std::vector<db::BeastPopulationRow> populations =
                read(db::read_beast_population_rows(world), db::BEAST_POPULATION_TABLE);
            mt.beast_populations = populations;
            mt.monsters = v.monster_rows_;
            mt.sites = v.site_rows_;
            mt.settlements = settlement_rows;
            mt.region_ids = region_ids;
            mt.civ_ids = civ_ids;
            mt.end_year = end_year;
            mt.map_size = map_size;
            mt.events = event_rows;
            mt.participants = participants;
            mt.values = values;
            mt.causes = causes;
            mt.catalogue = &v.catalogue_;
            if (std::optional<std::string> why = monster_tables_problem(mt)) {
                view_fail(*why);
            }
        }
        v.region_name_range_.assign(v.regions_.size(), {0, 0});
        for (std::size_t i = 0; i < v.region_names_.size(); ++i) {
            // Region ids are 1, 2, ... in valid worlds; find by search otherwise.
            const auto it = std::lower_bound(
                v.regions_.begin(), v.regions_.end(), v.region_names_[i].region,
                [](const std::pair<std::int64_t, std::string>& a, std::int64_t r) { return a.first < r; });
            auto& range = v.region_name_range_[static_cast<std::size_t>(it - v.regions_.begin())];
            if (range.first == range.second) {
                range = {i, i};
            }
            range.second = i + 1;
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
        case EntityType::monster: {  // P5-02 10.3
            const MonsterNames* m = find_by_id(monsters_, id);
            return m != nullptr ? &m->name : nullptr;
        }
        case EntityType::site: return find_by_id(sites_, id);
        case EntityType::figure:
        case EntityType::faction: return nullptr;
    }
    return nullptr;
}

const std::string* HistoryView::monster_noun(std::int64_t monster) const noexcept {
    const MonsterNames* m = find_by_id(monsters_, monster);
    if (m == nullptr) {
        return nullptr;
    }
    return find_by_id(monster_kind_nouns_, m->kind);
}

const std::string* HistoryView::beast_plural(std::int64_t beast) const noexcept {
    return find_by_id(beast_plurals_, beast);
}

const std::string* HistoryView::race_plural(std::int64_t id) const noexcept {
    const RaceNames* r = find_by_id(races_, id);
    return r != nullptr ? &r->plural : nullptr;
}

const std::string* HistoryView::race_adjective(std::int64_t id) const noexcept {
    const RaceNames* r = find_by_id(races_, id);
    return r != nullptr ? &r->adjective : nullptr;
}

std::int64_t HistoryView::race_generation_years(std::int64_t id) const noexcept {
    const RaceNames* r = find_by_id(races_, id);
    return r != nullptr ? r->generation_years : 0;
}

std::optional<HistoryView::CivilizationInfo> HistoryView::civilization_info(std::int64_t id) const noexcept {
    const CivilizationInfo* c = find_by_id(civilization_info_, id);
    if (c == nullptr) {
        return std::nullopt;
    }
    return *c;
}

const std::string* HistoryView::region_name_at_event(std::int64_t region, std::int64_t event) const noexcept {
    const auto it = std::lower_bound(regions_.begin(), regions_.end(), region,
                                     [](const std::pair<std::int64_t, std::string>& a, std::int64_t r) {
                                         return a.first < r;
                                     });
    if (it == regions_.end() || it->first != region) {
        return nullptr;
    }
    const std::string* out = &it->second;
    if (region_name_range_.size() == regions_.size()) {
        const auto [first, last] = region_name_range_[static_cast<std::size_t>(it - regions_.begin())];
        for (std::size_t i = first; i < last; ++i) {
            if (region_names_[i].event <= event) {
                out = &region_names_[i].name;
            }
        }
    }
    return out;
}

const std::string* HistoryView::region_name_at_year(std::int64_t region, std::int64_t year) const noexcept {
    const auto it = std::lower_bound(regions_.begin(), regions_.end(), region,
                                     [](const std::pair<std::int64_t, std::string>& a, std::int64_t r) {
                                         return a.first < r;
                                     });
    if (it == regions_.end() || it->first != region) {
        return nullptr;
    }
    const std::string* out = &it->second;
    if (region_name_range_.size() == regions_.size()) {
        const auto [first, last] = region_name_range_[static_cast<std::size_t>(it - regions_.begin())];
        for (std::size_t i = first; i < last; ++i) {
            if (region_names_[i].from_year <= year) {
                out = &region_names_[i].name;
            }
        }
    }
    return out;
}

namespace {

bool is_region_name_text(std::string_view name) noexcept {
    if (name.empty() || name.size() > 64U) {
        return false;
    }
    for (const char c : name) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == ' ' || c == '\'' || c == '-';
        if (!ok) {
            return false;
        }
    }
    return true;
}

bool sorted_has(std::span<const std::int64_t> ids, std::int64_t id) noexcept {
    return std::binary_search(ids.begin(), ids.end(), id);
}

}  // namespace

std::optional<std::string> region_name_rows_problem(std::span<const db::RegionNameRow> rows,
                                                    std::span<const std::int64_t> region_ids,
                                                    std::span<const std::int64_t> culture_ids, std::int64_t end_year,
                                                    std::span<const db::EventRow> events, std::int64_t renamed_kind) {
    const db::RegionNameRow* previous = nullptr;
    for (const db::RegionNameRow& r : rows) {
        const std::string at = "corrupt table 'region_name': row (" + int_text(r.region) + ", " +
                               int_text(r.from_year) + "): ";
        // P5-02 21.3 A1: rows strictly ascending in (region, from_year).
        if (previous != nullptr && std::pair(r.region, r.from_year) <= std::pair(previous->region, previous->from_year)) {
            return at + "not above the previous row";
        }
        previous = &r;
        if (!sorted_has(region_ids, r.region)) {
            return at + "unknown region " + int_text(r.region);
        }
        if (r.from_year < 0 || r.from_year > end_year - 1) {
            return at + "from_year " + int_text(r.from_year) + " outside 0 to " + int_text(end_year - 1);
        }
        if (!is_region_name_text(r.name)) {
            return at + "name is not a name";
        }
        if (r.culture != 0 && !sorted_has(culture_ids, r.culture)) {
            return at + "unknown culture " + int_text(r.culture);
        }
        const bool event_ok = renamed_kind != 0 && r.event >= 1 && static_cast<std::uint64_t>(r.event) <= events.size() &&
                              events[static_cast<std::size_t>(r.event - 1)].kind == renamed_kind &&
                              events[static_cast<std::size_t>(r.event - 1)].region == r.region &&
                              events[static_cast<std::size_t>(r.event - 1)].year == r.from_year;
        if (!event_ok) {
            return at + "event " + int_text(r.event) + " is not a region_renamed of this region and year";
        }
    }
    if (renamed_kind != 0) {
        // P5-02 21.3 A2: one pass marks the events with a row (every row's
        // event is an event, checked above), one pass tests the kind's events.
        std::vector<std::uint8_t> has(events.size() + 1, 0);
        for (const db::RegionNameRow& r : rows) {
            has[static_cast<std::size_t>(r.event)] = 1;
        }
        for (const db::EventRow& e : events) {
            if (e.kind != renamed_kind) {
                continue;
            }
            const bool has_row = e.id >= 1 && static_cast<std::size_t>(e.id) < has.size() &&
                                 has[static_cast<std::size_t>(e.id)] != 0;
            if (!has_row) {
                return "corrupt table 'event': row " + int_text(e.id) + ": region_renamed without a region_name row";
            }
        }
    }
    return std::nullopt;
}

// --- P5-02 11.4 ------------------------------------------------------------------------------

namespace {

bool is_definition_id(std::string_view s) noexcept {
    return s.size() <= 64U && is_key(s);
}

bool is_display_name(std::string_view s) noexcept {
    if (s.empty() || s.size() > 32U || !(s.front() >= 'A' && s.front() <= 'Z')) {
        return false;
    }
    return std::all_of(s.begin(), s.end(),
                       [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == ' '; });
}

bool is_plural(std::string_view s) noexcept {
    if (s.empty() || s.size() > 32U || s.front() == ' ' || s.back() == ' ') {
        return false;
    }
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == ' ' ? s[i + 1] == ' ' : !(s[i] >= 'a' && s[i] <= 'z')) {
            return false;
        }
    }
    return true;
}

bool is_noun(std::string_view s) noexcept {
    if (s.size() > 48U || !(s.substr(0, 2) == "a " || s.substr(0, 3) == "an ")) {
        return false;
    }
    return std::all_of(s.begin(), s.end(), [](char c) { return (c >= 'a' && c <= 'z') || c == ' ' || c == '\''; });
}

// A monster or site name: ASCII letters, space, ' and -, 1 to 64 bytes.
bool is_entity_name(std::string_view s) noexcept {
    return is_region_name_text(s);
}

}  // namespace

std::int64_t settlement_ending_code(std::string_view ruin_kind, std::span<const std::string_view> cause_kinds) noexcept {
    if (ruin_kind == "sacked") {
        return 1;
    }
    if (ruin_kind == "settlement_overrun") {
        return 2;
    }
    if (ruin_kind == "settlement_abandoned" || ruin_kind == "settlement_abandoned_to") {
        bool famine = false;
        for (const std::string_view k : cause_kinds) {
            if (k == "plague" || k == "plague_spread" || k == "plague_outbreak") {
                return 3;
            }
            famine = famine || k == "famine";
        }
        if (famine) {
            return 4;
        }
    }
    return 5;
}

std::optional<std::string> monster_tables_problem(const MonsterTables& t) {
    const EventCatalogue& cat = *t.catalogue;
    const std::int64_t n_events = static_cast<std::int64_t>(t.events.size());
    const auto kind_key = [&](std::int64_t event) -> std::string_view {
        if (event < 1 || event > n_events) {
            return {};
        }
        const KindDef* k = cat.find_id(t.events[static_cast<std::size_t>(event - 1)].kind);
        return k != nullptr ? std::string_view(k->key) : std::string_view();
    };
    const auto year_of = [&](std::int64_t event) {
        return t.events[static_cast<std::size_t>(event - 1)].year;
    };
    // Per event: its participants [first, last) and causes [first, last).
    std::vector<std::pair<std::size_t, std::size_t>> parts(t.events.size(), {0, 0});
    for (std::size_t i = 0; i < t.participants.size(); ++i) {
        const std::int64_t e = t.participants[i].event;
        if (e >= 1 && e <= n_events) {
            auto& r = parts[static_cast<std::size_t>(e - 1)];
            if (r.first == r.second) {
                r = {i, i};
            }
            r.second = i + 1;
        }
    }
    std::vector<std::pair<std::size_t, std::size_t>> causes(t.events.size(), {0, 0});
    for (std::size_t i = 0; i < t.causes.size(); ++i) {
        const std::int64_t e = t.causes[i].event;
        if (e >= 1 && e <= n_events) {
            auto& r = causes[static_cast<std::size_t>(e - 1)];
            if (r.first == r.second) {
                r = {i, i};
            }
            r.second = i + 1;
        }
    }
    // The entity of the first participant of `event` in role `role` of type `type`, 0 none.
    const auto participant = [&](std::int64_t event, std::string_view role) -> std::int64_t {
        if (event < 1 || event > n_events) {
            return 0;
        }
        const KindDef* k = cat.find_id(t.events[static_cast<std::size_t>(event - 1)].kind);
        const RoleDef* r = k != nullptr ? k->find_role(role) : nullptr;
        if (r == nullptr) {
            return 0;
        }
        const auto [a, b] = parts[static_cast<std::size_t>(event - 1)];
        for (std::size_t i = a; i < b; ++i) {
            if (t.participants[i].role == r->seq) {
                return t.participants[i].entity_id;
            }
        }
        return 0;
    };
    const auto names_site = [&](std::int64_t event, std::int64_t site) {
        if (event < 1 || event > n_events) {
            return false;
        }
        const auto [a, b] = parts[static_cast<std::size_t>(event - 1)];
        for (std::size_t i = a; i < b; ++i) {
            if (t.participants[i].entity_type == static_cast<std::int64_t>(EntityType::site) &&
                t.participants[i].entity_id == site) {
                return true;
            }
        }
        return false;
    };
    const auto n_monsters = static_cast<std::int64_t>(t.monsters.size());
    const auto n_sites = static_cast<std::int64_t>(t.sites.size());
    const auto n_beasts = static_cast<std::int64_t>(t.beasts.size());
    const auto n_kinds = static_cast<std::int64_t>(t.monster_kinds.size());
    const std::string range = " outside 0 to " + int_text(t.end_year - 1);
    // beast.
    for (std::size_t i = 0; i < t.beasts.size(); ++i) {
        const db::BeastRow& r = t.beasts[i];
        const std::string at = "corrupt table 'beast': row " + int_text(r.id) + ": ";
        if (r.id != static_cast<std::int64_t>(i) + 1) {
            return at + "id " + int_text(r.id) + " is not " + int_text(static_cast<std::int64_t>(i) + 1);
        }
        if (!is_definition_id(r.key)) return at + "key is not a definition id";
        if (!is_display_name(r.name)) return at + "name is not a name";
        if (!is_plural(r.plural)) return at + "plural is not a plural";
        if (r.menace < 0 || r.menace > 1000) return at + "menace " + int_text(r.menace) + " outside 0 to 1000";
        if (r.food < 0 || r.food > 1000) return at + "food " + int_text(r.food) + " outside 0 to 1000";
    }
    // monster_kind.
    for (std::size_t i = 0; i < t.monster_kinds.size(); ++i) {
        const db::MonsterKindRow& r = t.monster_kinds[i];
        const std::string at = "corrupt table 'monster_kind': row " + int_text(r.id) + ": ";
        if (r.id != static_cast<std::int64_t>(i) + 1) {
            return at + "id " + int_text(r.id) + " is not " + int_text(static_cast<std::int64_t>(i) + 1);
        }
        if (!is_definition_id(r.key)) return at + "key is not a definition id";
        if (!is_display_name(r.name)) return at + "name is not a name";
        if (!is_noun(r.noun)) return at + "noun is not a noun";
        if (r.grouped != 0 && r.grouped != 1) return at + "grouped " + int_text(r.grouped) + " is not 0 or 1";
        if (r.wake != 1 && r.wake != 2) return at + "wake " + int_text(r.wake) + " is not 1 or 2";
        if (r.might < 1 || r.might > 10000000) return at + "might " + int_text(r.might) + " outside 1 to 10000000";
    }
    // beast_population.
    const db::BeastPopulationRow* prev = nullptr;
    for (const db::BeastPopulationRow& r : t.beast_populations) {
        const std::string at = "corrupt table 'beast_population': row (" + int_text(r.region) + ", " +
                               int_text(r.beast) + ", " + int_text(r.year) + "): ";
        if (prev != nullptr &&
            std::tie(r.region, r.beast, r.year) <= std::tie(prev->region, prev->beast, prev->year)) {
            return at + "not above the previous row";
        }
        if (!sorted_has(t.region_ids, r.region)) return at + "unknown region " + int_text(r.region);
        if (r.beast < 1 || r.beast > n_beasts) return at + "unknown beast " + int_text(r.beast);
        if (r.year < 0 || r.year > t.end_year - 1) return at + "year " + int_text(r.year) + range;
        if (r.count < 0 || r.count > 1000000) return at + "count " + int_text(r.count) + " outside 0 to 1000000";
        if (prev != nullptr && prev->region == r.region && prev->beast == r.beast && prev->count == r.count) {
            return at + "count equals the previous row of this region and beast";
        }
        prev = &r;
    }
    // site.
    const auto n_settlements = static_cast<std::int64_t>(t.settlements.size());
    const auto settlement_of = [&](std::int64_t id) -> const db::SettlementRow* {
        if (id < 1 || id > n_settlements || t.settlements[static_cast<std::size_t>(id - 1)].id != id) {
            return nullptr;
        }
        return &t.settlements[static_cast<std::size_t>(id - 1)];
    };
    std::vector<std::uint8_t> origin_of(t.events.size() + 1, 0);  // events that are a site's origin_event (count)
    for (std::size_t i = 0; i < t.sites.size(); ++i) {
        const db::SiteRow& r = t.sites[i];
        const std::string at = "corrupt table 'site': row " + int_text(r.id) + ": ";
        if (r.id != static_cast<std::int64_t>(i) + 1) {
            return at + "id " + int_text(r.id) + " is not " + int_text(static_cast<std::int64_t>(i) + 1);
        }
        if (r.kind != 1 && r.kind != 2) return at + "kind " + int_text(r.kind) + " is not 1 or 2";
        if (!sorted_has(t.region_ids, r.region)) return at + "unknown region " + int_text(r.region);
        if (r.x < 0 || r.y < 0 || r.x >= t.map_size || r.y >= t.map_size) {
            return at + "cell " + int_text(r.x) + " " + int_text(r.y) + " outside the map";
        }
        if (r.settlement != 0) {
            const db::SettlementRow* s = settlement_of(r.settlement);
            if (s == nullptr) return at + "unknown settlement " + int_text(r.settlement);
            if (s->x != r.x || s->y != r.y) {
                return at + "settlement " + int_text(r.settlement) + " is not at the site's cell";
            }
        }
        if (r.kind == 1) {
            if (r.monster < 1 || r.monster > n_monsters) return at + "monster " + int_text(r.monster) + " is not a monster row";
            if (r.founder_civ != 0) return at + "founder_civ " + int_text(r.founder_civ) + " of a lair";
        } else {
            if (!sorted_has(t.civ_ids, r.founder_civ)) return at + "unknown civilization " + int_text(r.founder_civ);
            if (r.monster != 0) return at + "monster " + int_text(r.monster) + " of an outpost";
        }
        if (r.origin_year < 0 || r.origin_year > t.end_year - 1) return at + "origin_year " + int_text(r.origin_year) + range;
        const std::string_view ok = kind_key(r.origin_event);
        const bool origin_ok = (ok == "monster_appeared" || ok == "undead_rose" || ok == "settlement_overrun" ||
                                ok == "outpost_built") &&
                               year_of(r.origin_event) == r.origin_year && names_site(r.origin_event, r.id);
        if (!origin_ok) {
            return at + "origin_event " + int_text(r.origin_event) + " is not an event of its year naming this site";
        }
        if (origin_of[static_cast<std::size_t>(r.origin_event)] < 2) {
            ++origin_of[static_cast<std::size_t>(r.origin_event)];
        }
        if (r.ended_year.has_value() != (r.ending_event != 0) || (r.ending_event != 0) != (r.ending != 0)) {
            return at + "end columns disagree";
        }
        if (r.ended_year) {
            if (*r.ended_year < r.origin_year) return at + "ended_year " + int_text(*r.ended_year) + " before origin_year";
            if (r.ending_event < 1 || r.ending_event > n_events || year_of(r.ending_event) != *r.ended_year) {
                return at + "ending_event " + int_text(r.ending_event) + " is not an event of year " +
                       int_text(*r.ended_year);
            }
        }
        if (r.ending < 0 || r.ending > 6) return at + "ending " + int_text(r.ending) + " outside 0 to 6";
    }
    // monster.
    for (std::size_t i = 0; i < t.monsters.size(); ++i) {
        const db::MonsterRow& r = t.monsters[i];
        const std::string at = "corrupt table 'monster': row " + int_text(r.id) + ": ";
        if (r.id != static_cast<std::int64_t>(i) + 1) {
            return at + "id " + int_text(r.id) + " is not " + int_text(static_cast<std::int64_t>(i) + 1);
        }
        if (r.kind < 1 || r.kind > n_kinds) return at + "unknown kind " + int_text(r.kind);
        if (!is_entity_name(r.name)) return at + "name is not a name";
        if (r.lair_site < 1 || r.lair_site > n_sites || t.sites[static_cast<std::size_t>(r.lair_site - 1)].kind != 1 ||
            t.sites[static_cast<std::size_t>(r.lair_site - 1)].monster != r.id) {
            return at + "lair_site " + int_text(r.lair_site) + " is not a lair of this monster";
        }
        const std::string_view bk = kind_key(r.born_event);
        if (!((bk == "monster_appeared" || bk == "undead_rose") && year_of(r.born_event) == r.born &&
              participant(r.born_event, "monster") == r.id)) {
            return at + "born_event " + int_text(r.born_event) + " is not the appearance of this monster in year " +
                   int_text(r.born);
        }
        if (r.status < 1 || r.status > 4) return at + "status " + int_text(r.status) + " outside 1 to 4";
        const bool alive = r.status == 1 || r.status == 2;
        if (alive != !r.died.has_value() || alive != (r.death_event == 0)) return at + "end columns disagree";
        if (r.status == 3) {
            const bool ok3 = r.slain_by != 0 && kind_key(r.death_event) == "monster_slain" &&
                             year_of(r.death_event) == *r.died && participant(r.death_event, "monster") == r.id &&
                             participant(r.death_event, "slayer") == r.slain_by;
            if (!ok3) return at + "death_event " + int_text(r.death_event) + " is not this monster's slaying";
        } else if (r.slain_by != 0) {
            return at + "slain_by " + int_text(r.slain_by) + " of a monster not slain";
        }
        if (r.status == 4) {
            const bool ok4 = kind_key(r.death_event) == "monster_died" && year_of(r.death_event) == *r.died &&
                             participant(r.death_event, "monster") == r.id;
            if (!ok4) return at + "death_event " + int_text(r.death_event) + " is not this monster's death";
        }
        if (r.hoard < 0) return at + "hoard " + int_text(r.hoard) + " negative";
    }
    // settlement.ending (8.3).
    std::vector<std::int64_t> faded_ruin(t.settlements.size() + 1, 0);
    for (const db::EventRow& e : t.events) {
        if (kind_key(e.id) == "ruin_faded" && e.settlement >= 1 && e.settlement <= n_settlements) {
            const auto [a, b] = causes[static_cast<std::size_t>(e.id - 1)];
            if (a < b) {
                faded_ruin[static_cast<std::size_t>(e.settlement)] = t.causes[a].cause;
            }
        }
    }
    for (const db::SettlementRow& r : t.settlements) {
        std::int64_t expected = 0;
        if (r.status == 3 || r.status == 4) {
            const std::int64_t ruin =
                r.status == 3 ? r.ruin_event
                              : (r.id >= 1 && r.id <= n_settlements ? faded_ruin[static_cast<std::size_t>(r.id)] : 0);
            std::vector<std::string_view> ck;
            if (ruin >= 1 && ruin <= n_events) {
                const auto [a, b] = causes[static_cast<std::size_t>(ruin - 1)];
                for (std::size_t i = a; i < b; ++i) {
                    ck.push_back(kind_key(t.causes[i].cause));
                }
            }
            expected = settlement_ending_code(kind_key(ruin), ck);
        }
        if (r.ending != expected) {
            return "corrupt table 'settlement': row " + int_text(r.id) + ": ending " + int_text(r.ending) +
                   " is not its ruin's ending " + int_text(expected);
        }
    }
    // Events.
    for (const db::EventParticipantRow& p : t.participants) {
        const bool monster = p.entity_type == static_cast<std::int64_t>(EntityType::monster);
        const bool site = p.entity_type == static_cast<std::int64_t>(EntityType::site);
        if ((monster && (p.entity_id < 1 || p.entity_id > n_monsters)) || (site && (p.entity_id < 1 || p.entity_id > n_sites))) {
            return "corrupt table 'event': row " + int_text(p.event) + ": " + (monster ? "monster " : "site ") +
                   int_text(p.entity_id) + " has no row";
        }
    }
    for (const db::EventRow& e : t.events) {
        const std::string_view k = kind_key(e.id);
        if ((k == "monster_appeared" || k == "undead_rose" || k == "settlement_overrun" || k == "outpost_built") &&
            origin_of[static_cast<std::size_t>(e.id)] != 1) {
            return "corrupt table 'event': row " + int_text(e.id) + ": " + std::string(k) + " without its site row";
        }
    }
    for (const db::EventValueRow& v : t.values) {
        if (v.event < 1 || v.event > n_events) {
            continue;
        }
        const KindDef* k = cat.find_id(t.events[static_cast<std::size_t>(v.event - 1)].kind);
        if (k != nullptr && k->value_seq("beast") == v.seq && (v.value < 1 || v.value > n_beasts)) {
            return "corrupt table 'event': row " + int_text(v.event) + ": value beast " + int_text(v.value) +
                   " is not a beast";
        }
    }
    return std::nullopt;
}

// --- render_event (8.5) ------------------------------------------------------------------

namespace {

// P3-04 r19 22.1: the number of clause cuts ("; " or ", ") in a literal.
std::size_t clause_cuts(std::string_view literal) noexcept {
    std::size_t n = 0;
    for (std::size_t i = 0; i + 1 < literal.size(); ++i) {
        if ((literal[i] == ';' || literal[i] == ',') && literal[i + 1] == ' ') {
            ++n;
        }
    }
    return n;
}

}  // namespace

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
        } else if (form == Form::kind) {
            s = view.monster_noun(p.entity_id);  // P5-02 10.3
        } else if (form == Form::adjective) {
            s = view.race_adjective(p.entity_id);
        } else {
            s = type == EntityType::region ? view.region_name_at_event(p.entity_id, event_id)  // r21 23.1
                                           : view.name(type, p.entity_id);
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
    // r19 22.1: the clause of each token (a placeholder belongs to the
    // clause in which it starts), and per clause whether every number of it
    // (each {value} and each '#' of a chosen three-word form) is 1 to 9.
    std::vector<std::size_t> clause_of(parsed.tokens.size(), 0);
    std::vector<std::uint8_t> clause_words(1, 1);
    for (std::size_t i = 0; i < parsed.tokens.size(); ++i) {
        const Token& t = parsed.tokens[i];
        clause_of[i] = clause_words.size() - 1;
        if (t.type == TokenType::literal) {
            clause_words.resize(clause_words.size() + clause_cuts(t.text), 1);
            continue;
        }
        bool is_number = t.type == TokenType::value;
        std::int64_t v = 0;
        if (t.type == TokenType::value) {
            v = value_of(t.text);
        } else if (t.type == TokenType::choice && t.three) {
            v = value_of(t.text);
            const std::string& word = v == 0 ? t.word_zero : v == 1 ? t.word_one : t.word_other;
            is_number = word.find('#') != std::string::npos;
        }
        if (is_number && (v < 1 || v > 9)) {
            clause_words[clause_of[i]] = 0;
        }
    }
    const auto number_text = [&](std::size_t token, std::int64_t v) {
        return clause_words[clause_of[token]] != 0 ? sentence_value_text(v) : grouped_text(v);
    };
    for (std::size_t ti = 0; ti < parsed.tokens.size(); ++ti) {
        const Token& t = parsed.tokens[ti];
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
            case TokenType::value: out += number_text(ti, value_of(t.text)); break;
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
                        out += number_text(ti, v);  // r17 21.1, r19 22.1
                    } else {
                        out += c;
                    }
                }
                break;
            }
            case TokenType::year: out += int_text(e->row.year); break;
            case TokenType::end_year: out += int_text(e->row.end_year); break;
            case TokenType::region:
            case TokenType::region_former: {
                // r21 23.1: the name in force at the event, or just before it.
                const std::int64_t at_event = t.type == TokenType::region ? event_id : event_id - 1;
                const std::string* s = view.region_name_at_event(e->row.region, at_event);
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
            case TokenType::beast: {  // P5-02 10.3: copied as stored
                const std::int64_t b = value_of("beast");
                const std::string* s = view.beast_plural(b);
                if (s == nullptr) {
                    fail(at + "no beast " + int_text(b));
                }
                out += *s;
                break;
            }
            case TokenType::causes: {
                // r12 20.2: the cwords of the causes' kinds, in ascending
                // cause id, each once; joined as {role:all} joins names.
                std::vector<const std::string*> words;
                std::vector<std::int64_t> causes = e->causes;
                std::stable_sort(causes.begin(), causes.end());
                for (const std::int64_t c : causes) {
                    const HistoryView::Event* ce = view.event(c);
                    const KindDef* ck = ce != nullptr ? view.catalogue().find_id(ce->row.kind) : nullptr;
                    if (ck == nullptr) {
                        fail(at + "cause " + int_text(c) + " is not in the log");
                    }
                    for (const auto& [key, word] : t.cause_words) {
                        if (key == ck->key) {
                            const bool listed = std::any_of(words.begin(), words.end(),
                                                            [&word](const std::string* w) { return *w == word; });
                            if (!listed) {
                                words.push_back(&word);
                            }
                            break;
                        }
                    }
                }
                if (words.empty()) {
                    break;
                }
                out += t.word_one;
                for (std::size_t i = 0; i < words.size(); ++i) {
                    if (i > 0) {
                        out += i + 1 == words.size() ? " and " : ", ";
                    }
                    out += *words[i];
                }
                out += t.word_other;
                break;
            }
        }
    }
    return out;
}

}  // namespace worldgen::history
