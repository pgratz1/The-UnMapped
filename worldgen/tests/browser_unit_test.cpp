// Spec P4-05 10.2 items 1, 2, 4, 5 and 7 on fixture T3 (worldgen/tests/data/browser/t3-browser.toml), with
// hand-checked values (the reference's check_t3 holds the same ones): render_event_linked's links for {role},
// {role:all} with three participants, {role:plural}, {role:adjective}, a lower-cased "The ", a region renamed
// after the event (its former name), {region_former}, {settlement} and a non-ASCII name (offsets in bytes);
// event_group for the table of 4.4, every compiled kind, "" and "warx"; the civilization entries; event
// details and row texts; select's rules (span overlap at both ends, multi-year events, two entity fields,
// summary_only) and FatalErrors; search (A-Z folding only, a region found by a former name, the limit, the
// empty query); event_place; the block lines of a civilization without capital, an ended and a seceded one and
// one with an adversary of another type (4.2); BrowserIndex::build.
//
// Written from the spec only.

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "browser_support.hpp"
#include "worldgen/core/error.hpp"

namespace {

using namespace brtest;
using brapi::EventGroup;
using brapi::LinkedText;
using brapi::TextLink;
using V = std::vector<std::int64_t>;

constexpr std::int64_t RACE = 1, GOD = 2, CIV = 3, SETTLEMENT = 4, REGION = 5, FIGURE = 6;

// "Khaz" a-circumflex "d": the non-ASCII settlement name of T3 (bytes 4b 68 61 7a c3 a2 64).
const std::string KHAZAD = "Khaz\xC3\xA2"
                           "d";

template <class F>
std::string fatal_text(F&& f) {
    try {
        f();
    } catch (const worldgen::FatalError& e) {
        return e.what();
    }
    return {};
}

TextLink lk(std::uint32_t start, std::uint32_t length, std::int64_t type, std::int64_t id, bool tagged = false) {
    TextLink l;
    l.start = start;
    l.length = length;
    l.type = type;
    l.id = id;
    l.tagged = tagged;
    return l;
}

LinkedText linked(std::string text, std::vector<TextLink> links) {
    LinkedText t;
    t.text = std::move(text);
    t.links = std::move(links);
    return t;
}

std::string link_list(const LinkedText& t) { return link_lines(t, true); }

void check_linked(std::int64_t e, const LinkedText& want) {
    CAPTURE(e);
    const LinkedText got = brapi::render_event_linked(t3().h.view, e);
    CHECK(got.text == want.text);
    CHECK_MESSAGE(got.links == want.links, "got\n" << link_list(got));
    CHECK(got.text == worldgen::history::render_event(t3().h.view, e));
}

brapi::EventFilter flt(std::int64_t civ, std::int64_t settlement, std::int64_t region, std::int64_t from,
                       std::int64_t to, std::uint32_t groups, bool summary_only) {
    brapi::EventFilter f;
    f.civilization = civ;
    f.settlement = settlement;
    f.region = region;
    f.from_year = from;
    f.to_year = to;
    f.groups = groups;
    f.summary_only = summary_only;
    return f;
}

V sel(const brapi::EventFilter& f) { return select(t3(), f); }

std::vector<std::pair<std::int64_t, std::int64_t>> matches(std::string_view q, std::size_t limit) {
    std::vector<std::pair<std::int64_t, std::int64_t>> out;
    for (const brapi::NameMatch& m : t3().index.search(t3().h, q, limit)) out.emplace_back(m.type, m.id);
    return out;
}

std::vector<LinkedText> block(std::int64_t civ, HistoryLevel level) { return t3().index.block(t3().h, civ, level); }

const LinkedText* line_starting(const std::vector<LinkedText>& lines, std::string_view head) {
    for (const LinkedText& t : lines) {
        if (t.text.rfind(head, 0) == 0) return &t;
    }
    return nullptr;
}

constexpr std::uint32_t bit(EventGroup g) { return 1U << static_cast<std::uint32_t>(g); }

}  // namespace

TEST_SUITE("browser") {

TEST_CASE("P4-05 10.2 item 1: render_event_linked on T3") {
    check_linked(1, linked("Wealstuld awakened the Humans in Arn Vale.",  // {creator}, {race:plural}, {@region}
                           {lk(0, 9, GOD, 1), lk(23, 6, RACE, 1), lk(33, 8, REGION, 1)}));
    check_linked(2, linked("The Humans founded the Kingdom of Arn in Arn Vale.",  // a lower-cased "the " linked
                           {lk(4, 6, RACE, 1), lk(19, 18, CIV, 1), lk(41, 8, REGION, 1)}));
    check_linked(4, linked("The Kingdom of Arn founded Yewford in the Holt of Yews.",  // the name before 1300
                           {lk(0, 18, CIV, 1), lk(27, 7, SETTLEMENT, 2), lk(38, 16, REGION, 2)}));
    check_linked(10, linked("Stonehelm founded " + KHAZAD + " in Deep Halls.",  // 7 bytes, not 6 characters
                            {lk(0, 9, CIV, 3), lk(18, 7, SETTLEMENT, 4), lk(29, 10, REGION, 4)}));
    check_linked(11, linked("Dwarven smiths of Stonehelm forged a crown.",  // {race:adjective}
                            {lk(0, 7, RACE, 2), lk(18, 9, CIV, 3)}));
    check_linked(15, linked("After the slaughter, the Holt of Yews became known as the Red Holt.",  // {@region_former}
                            {lk(21, 16, REGION, 2), lk(54, 12, REGION, 2)}));
    check_linked(16, linked("The Kingdom of Arn besieged " + KHAZAD + " of Stonehelm from 1350 to 1360; it held.",
                            {lk(0, 18, CIV, 1), lk(28, 7, SETTLEMENT, 4), lk(39, 9, CIV, 3)}));  // {@settlement}
    check_linked(19, linked("The Kingdom of Arn, Belmark and Stonehelm met at Arnton from 1500 to 1502.",  // :all x 3
                            {lk(0, 18, CIV, 1), lk(20, 7, CIV, 2), lk(32, 9, CIV, 3), lk(49, 6, SETTLEMENT, 1)}));
    check_linked(20, linked("Cor Wold became known as Ash Wold.", {lk(0, 8, REGION, 5), lk(25, 8, REGION, 5)}));
    check_linked(21, linked("Ash Wold became known as Grey Wold.", {lk(0, 8, REGION, 5), lk(25, 9, REGION, 5)}));
    check_linked(7, linked("A comet crossed the sky.", {}));
    // values and years get no link
    check_linked(14, linked("The Kingdom of Arn defeated Stonehelm in the Holt of Yews; 40 of the defeated and 10 of "
                            "the victors fell.",
                            {lk(0, 18, CIV, 1), lk(28, 9, CIV, 3), lk(41, 16, REGION, 2)}));
    for (std::int64_t e = 1; e <= event_count(t3().h); ++e) {
        CAPTURE(e);
        const LinkedText t = brapi::render_event_linked(t3().h.view, e);
        CHECK(links_well_formed(t));
        for (const TextLink& l : t.links) CHECK_FALSE(l.tagged);
    }
    // FatalErrors are render_event's
    const std::string want = fatal_text([] { (void)worldgen::history::render_event(t3().h.view, 24); });
    CHECK_FALSE(want.empty());
    CHECK(fatal_text([] { (void)brapi::render_event_linked(t3().h.view, 24); }) == want);
    CHECK(fatal_text([] { (void)brapi::render_event_linked(t3().h.view, 0); }) ==
          fatal_text([] { (void)worldgen::history::render_event(t3().h.view, 0); }));
}

TEST_CASE("P4-05 10.2 item 2: event_group for every key of 4.4, every compiled kind, \"\" and \"warx\"") {
    const std::pair<EventGroup, std::vector<std::string_view>> table[] = {
        {EventGroup::realms,
         {"awakening", "civ_founded", "civ_seceded", "civ_ended", "civ_conquered", "capital_moved",
          "succession_crisis", "quiet_era", "quiet_stretch", "region_renamed"}},
        {EventGroup::settlements,
         {"settlement_founded", "colony_founded", "became_village", "became_town", "became_city",
          "settlement_dwindled", "settlement_abandoned", "settlement_abandoned_to", "ruin_faded", "refugees_settled",
          "refuge_hidden"}},
        {EventGroup::war,
         {"flare_up", "claim", "claim_settlements", "claim_dropped", "war_declared", "battle", "battle_at_pass",
          "siege", "sacked", "settlement_captured", "peace", "truce", "war_won", "region_ceded", "settlements_ceded",
          "tribute", "search_abandoned", "refuge_held"}},
        {EventGroup::calamity, {"famine", "plague", "plague_spread", "plague_outbreak"}},
        // P5-02's kinds 44 to 55: the monsters group of section 9, appended as code 5 (lead's ruling at the
        // P4-05 integration)
        {EventGroup::monsters,
         {"monster_appeared", "undead_rose", "monster_woke", "monster_slept", "monster_raid", "settlement_overrun",
          "monster_slain", "monster_driven_off", "monster_died", "beasts_bold", "outpost_built",
          "outpost_abandoned"}},
    };
    std::size_t n = 0;
    for (const auto& [g, keys] : table) {
        for (const std::string_view k : keys) {
            CAPTURE(k);
            CHECK(brapi::event_group(k) == g);
            ++n;
        }
    }
    CHECK(n == 55U);
    // every compiled kind has a row: none falls to other (10.2 item 2), P5-02's kinds 44 to 55 in monsters
    const auto& kinds = worldgen::history::compiled_event_catalogue().kinds;
    REQUIRE(kinds.size() == 55U);
    for (const auto& k : kinds) {
        CAPTURE(k.key);
        CHECK(brapi::event_group(k.key) != EventGroup::other);
        CHECK((brapi::event_group(k.key) == EventGroup::monsters) == (k.id >= 44));
    }
    for (const std::string_view k : {std::string_view(""), std::string_view("warx"), std::string_view("Battle"),
                                     std::string_view("battle "), std::string_view("unknown_key"),
                                     std::string_view("plague\0", 7)}) {
        CHECK(brapi::event_group(k) == EventGroup::other);
    }
    CHECK(static_cast<int>(EventGroup::realms) == 0);
    CHECK(static_cast<int>(EventGroup::settlements) == 1);
    CHECK(static_cast<int>(EventGroup::war) == 2);
    CHECK(static_cast<int>(EventGroup::calamity) == 3);
    CHECK(static_cast<int>(EventGroup::other) == 4);
    CHECK(static_cast<int>(EventGroup::monsters) == 5);
    CHECK(brapi::ALL_GROUPS == 0x3FU);
    CHECK(brapi::event_group_word(EventGroup::realms) == "realms");
    CHECK(brapi::event_group_word(EventGroup::settlements) == "settlements");
    CHECK(brapi::event_group_word(EventGroup::war) == "war");
    CHECK(brapi::event_group_word(EventGroup::calamity) == "calamity");
    CHECK(brapi::event_group_word(EventGroup::other) == "other");
    CHECK(brapi::event_group_word(EventGroup::monsters) == "monsters");
}

TEST_CASE("P4-05 10.2 item 7: BrowserIndex::build on T3 and on fixtures that load") {
    for (const std::string& name : {T3, std::string("h2-minimal"), std::string("h1-war-and-ruin"), tltest::T1}) {
        CAPTURE(name);
        const db::WorldReader r = tltest::open_reader(fixture_world(name));
        const wh::LoadedHistory h = tltest::load(r);
        const auto b = brapi::BrowserIndex::build(h);
        CHECK(b.ok());
    }
}

TEST_CASE("P4-05 4.5: T3's civilization entries and event entries") {
    const auto civs = t3().index.civilizations();
    REQUIRE(civs.size() == 3U);
    auto ce = [](std::int64_t id, std::int64_t race, std::int64_t founded, std::int64_t ended, std::int64_t ending,
                 std::int64_t events, std::string name, std::string race_name) {
        brapi::CivilizationEntry c;
        c.id = id;
        c.race = race;
        c.founded_year = founded;
        c.ended_year = ended;
        c.ending_event = ending;
        c.events = events;
        c.name = std::move(name);
        c.race_name = std::move(race_name);
        return c;
    };
    CHECK(civs[0] == ce(1, 1, 0, -1, 0, 15, "The Kingdom of Arn", "Human"));
    CHECK(civs[1] == ce(2, 1, 500, -1, 0, 4, "Belmark", "Human"));  // seceded
    CHECK(civs[2] == ce(3, 2, 1000, 1800, 22, 9, "Stonehelm", "Dwarf"));  // conquered
    for (const brapi::CivilizationEntry& c : civs) {
        CHECK(c.events == static_cast<std::int64_t>(t3().h.timeline.events_of_civilization(c.id).size()));
    }

    const auto events = t3().index.events();
    REQUIRE(events.size() == 23U);
    const EventGroup want_group[] = {
        EventGroup::realms,      EventGroup::realms,   EventGroup::settlements, EventGroup::settlements,
        EventGroup::settlements, EventGroup::realms,   EventGroup::other,       EventGroup::realms,
        EventGroup::realms,      EventGroup::settlements, EventGroup::other,    EventGroup::war,
        EventGroup::war,         EventGroup::war,      EventGroup::realms,      EventGroup::war,
        EventGroup::war,         EventGroup::war,      EventGroup::other,       EventGroup::realms,
        EventGroup::realms,      EventGroup::realms,   EventGroup::calamity};
    for (std::size_t i = 0; i < events.size(); ++i) {
        CAPTURE(i);
        CHECK(events[i].id == static_cast<std::int64_t>(i + 1));
        CHECK(events[i].group == want_group[i]);
        // the summary set of the report's digest: every event but the omen (salience 10) and the claim
        CHECK(events[i].summary == (i + 1 != 7U && i + 1 != 12U));
        CHECK(events[i].summary == (t3().index.digest().summary[i] == 1));
    }
    const brapi::EventEntry& s = events[15];  // e16, the siege
    CHECK(s.year == 1350);
    CHECK(s.end_year == 1360);
    CHECK(s.kind == 12);
    CHECK(s.salience == 25);
    CHECK(s.region == 4);
    CHECK(s.settlement == 4);
    CHECK(events[6].region == 0);
    CHECK(events[6].settlement == 0);
    CHECK(t3().index.digest().summary == brapi::history_report_digest(t3().h).summary);
}

TEST_CASE("P4-05 4.5: T3's event details, row texts and places (4.6)") {
    const Opened& o = t3();
    const brapi::EventDetail d = o.index.event_detail(o.h, 16);
    CHECK(d.entry.id == 16);
    CHECK(d.kind_key == "siege");
    CHECK(d.sentence == brapi::render_event_linked(o.h.view, 16));
    CHECK(d.note.empty());
    REQUIRE(d.participants.size() == 2U);
    CHECK(d.participants[0].role == "attacker");
    CHECK(d.participants[0].type == CIV);
    CHECK(d.participants[0].id == 1);
    CHECK(d.participants[0].name == "The Kingdom of Arn");  // as stored, not lower-cased
    CHECK(d.participants[1].role == "defender");
    CHECK(d.participants[1].id == 3);
    CHECK(d.participants[1].name == "Stonehelm");
    using Values = std::vector<std::pair<std::string, std::int64_t>>;
    CHECK(d.values == Values{{"fell", 0}});
    CHECK(d.causes == V{13});
    CHECK(d.effects == V{22});

    const brapi::EventDetail d13 = o.index.event_detail(o.h, 13);
    CHECK(d13.note == "Claimed since 1200.");
    CHECK(d13.effects == V{14, 16});  // two effects
    CHECK(d13.causes == V{12});
    CHECK(o.index.event_detail(o.h, 2).effects == V{3, 4});
    CHECK(o.index.event_detail(o.h, 5).causes == V{3});  // the chain e1 -> e2 -> e3 -> e5
    CHECK(o.index.event_detail(o.h, 3).causes == V{2});
    CHECK(o.index.event_detail(o.h, 2).causes == V{1});
    CHECK(o.index.event_detail(o.h, 1).causes.empty());
    const brapi::EventDetail d19 = o.index.event_detail(o.h, 19);
    REQUIRE(d19.participants.size() == 3U);
    CHECK(d19.participants[2].role == "parties");
    CHECK(d19.participants[2].id == 3);
    CHECK(d19.values.empty());
    const brapi::EventDetail d18 = o.index.event_detail(o.h, 18);
    CHECK((d18.values == Values{{"dead", 20}, {"fled", 5}}));
    CHECK(d18.entry.group == EventGroup::war);

    CHECK(o.index.row_text(o.h, 13) ==
          "1250  Stonehelm declared war on the Kingdom of Arn over the Holt of Yews. (Claimed since 1200.)");
    CHECK(o.index.row_text(o.h, 16) ==
          "1350-1360  The Kingdom of Arn besieged " + KHAZAD + " of Stonehelm from 1350 to 1360; it held.");
    CHECK(o.index.row_text(o.h, 7) == "700  A comet crossed the sky.");
    CHECK(o.index.row_text(o.h, 19) ==
          "1500-1502  The Kingdom of Arn, Belmark and Stonehelm met at Arnton from 1500 to 1502.");

    using P = std::optional<std::array<std::int64_t, 2>>;
    CHECK(brapi::event_place(o.markers, o.h, 7) == P{});                              // neither
    CHECK(brapi::event_place(o.markers, o.h, 11) == P{});
    CHECK(brapi::event_place(o.markers, o.h, 12) == P{std::array<std::int64_t, 2>{300, 60}});   // r2's seed
    CHECK(brapi::event_place(o.markers, o.h, 16) == P{std::array<std::int64_t, 2>{400, 400}});  // s4
    CHECK(brapi::event_place(o.markers, o.h, 19) == P{std::array<std::int64_t, 2>{100, 100}});  // s1
    CHECK(brapi::event_place(o.markers, o.h, 20) == P{std::array<std::int64_t, 2>{60, 460}});   // r5's seed

    CHECK(fatal_text([&] { (void)o.index.event_detail(o.h, 24); }) == "browser: no event 24");
    CHECK(fatal_text([&] { (void)o.index.event_detail(o.h, 0); }) == "browser: no event 0");
    CHECK(fatal_text([&] { (void)o.index.row_text(o.h, 24); }) == "browser: no event 24");
    CHECK(fatal_text([&] { (void)o.index.row_text(o.h, -1); }) == "browser: no event -1");
    CHECK_FALSE(fatal_text([&] { (void)brapi::event_place(o.markers, o.h, 24); }).empty());
    CHECK_FALSE(fatal_text([&] { (void)brapi::event_place(o.markers, o.h, 0); }).empty());
}

TEST_CASE("P4-05 10.2 item 4: select on T3") {
    const std::uint32_t ALL = brapi::ALL_GROUPS;
    // span overlap at both ends of the multi-year siege (1350-1360) and inside the summit (1500-1502)
    CHECK(sel(flt(0, 0, 0, 1350, 1350, ALL, false)) == V{16});
    CHECK(sel(flt(0, 0, 0, 1360, 1360, ALL, false)) == V{16});
    CHECK(sel(flt(0, 0, 0, 1349, 1349, ALL, false)) == V{});
    CHECK(sel(flt(0, 0, 0, 1361, 1389, ALL, false)) == V{});
    CHECK(sel(flt(0, 0, 0, 1355, 1390, ALL, false)) == V{16, 17});
    CHECK(sel(flt(0, 0, 0, 1501, 1501, ALL, false)) == V{19});
    CHECK(sel(flt(0, 0, 0, 1502, 1600, ALL, false)) == V{19, 20});
    // open ends (-1)
    CHECK(sel(flt(0, 0, 0, 1800, -1, ALL, false)) == V{22, 23});
    CHECK(sel(flt(0, 0, 0, -1, 0, ALL, false)) == V{1, 2, 3});
    CHECK(sel(flt(0, 0, 0, -1, -1, ALL, false)).size() == 23U);
    // entity lists: participants, and the event's own region and settlement columns
    CHECK(sel(flt(2, 0, 0, -1, -1, ALL, false)) == V{6, 17, 18, 19});
    CHECK(sel(flt(0, 3, 0, -1, -1, ALL, false)) == V{5, 6, 18});
    CHECK(sel(flt(0, 1, 0, -1, -1, ALL, false)) == V{3, 5, 19, 23});  // s1 as a column and as origin of e5
    CHECK(sel(flt(0, 0, 5, -1, -1, ALL, false)) == V{20, 21});
    // two and three entity fields together: all must hold
    CHECK(sel(flt(1, 1, 0, -1, -1, ALL, false)) == V{3, 5, 19, 23});
    CHECK(sel(flt(0, 3, 3, -1, -1, ALL, false)) == V{5, 6, 18});
    CHECK(sel(flt(3, 0, 2, -1, -1, ALL, false)) == V{12, 13, 14});
    CHECK(sel(flt(2, 3, 3, -1, -1, ALL, false)) == V{6, 18});
    CHECK(sel(flt(2, 0, 5, -1, -1, ALL, false)) == V{});
    // groups
    CHECK(sel(flt(3, 0, 2, -1, -1, bit(EventGroup::war), false)) == V{12, 13, 14});
    CHECK(sel(flt(0, 0, 0, -1, -1, bit(EventGroup::other), false)) == V{7, 11, 19});
    CHECK(sel(flt(0, 0, 0, -1, -1, bit(EventGroup::calamity) | bit(EventGroup::settlements), false)) ==
          V{3, 4, 5, 10, 23});
    CHECK(sel(flt(0, 0, 0, -1, -1, 0U, false)) == V{});
    // summary_only: the report's summary set (every event but e7 and e12)
    CHECK(sel(flt(3, 0, 0, -1, -1, ALL, false)) == V{9, 10, 11, 12, 13, 14, 16, 19, 22});
    CHECK(sel(flt(3, 0, 0, -1, -1, ALL, true)) == V{9, 10, 11, 13, 14, 16, 19, 22});
    CHECK(sel(brapi::EventFilter{}).size() == 21U);
    CHECK(sel(flt(0, 0, 0, 700, 1200, ALL, true)) == V{8, 9, 10, 11});
    // a filter the world cannot answer
    const Opened& o = t3();
    CHECK(fatal_text([&] { (void)sel(flt(4, 0, 0, -1, -1, ALL, false)); }) == "browser: no civilization 4");
    CHECK(fatal_text([&] { (void)sel(flt(0, 5, 0, -1, -1, ALL, false)); }) == "browser: no settlement 5");
    CHECK(fatal_text([&] { (void)sel(flt(0, 0, 6, -1, -1, ALL, false)); }) == "browser: no region 6");
    CHECK(fatal_text([&] { (void)sel(flt(1, 0, 6, -1, -1, ALL, false)); }) == "browser: no region 6");
    CHECK(fatal_text([&] { (void)sel(flt(-2, 0, 0, -1, -1, ALL, false)); }) == "browser: no civilization -2");
    (void)o;
    // ascending, and the same for the same filter
    const V a = sel(flt(1, 0, 0, -1, -1, ALL, false));
    CHECK(std::is_sorted(a.begin(), a.end()));
    CHECK(std::adjacent_find(a.begin(), a.end()) == a.end());
    CHECK(sel(flt(1, 0, 0, -1, -1, ALL, false)) == a);
    CHECK(a.size() == 15U);
}

TEST_CASE("P4-05 10.2 item 5: search on T3") {
    using M = std::vector<std::pair<std::int64_t, std::int64_t>>;
    // a region_name row that is neither the present nor the geographic name
    CHECK(matches("ash", 50) == M{{REGION, 5}});
    CHECK(matches("YEWS", 50) == M{{REGION, 2}});  // the geographic name of a renamed region
    CHECK(matches("red", 50) == M{{REGION, 2}});   // its name at E
    // civilizations, then settlements, then regions
    CHECK(matches("ar", 50) == M{{CIV, 1}, {CIV, 2}, {SETTLEMENT, 1}, {REGION, 1}});
    CHECK(matches("ar", 2) == M{{CIV, 1}, {CIV, 2}});
    CHECK(matches("ar", 0) == M{});
    CHECK(matches("", 50) == M{});
    CHECK(matches(NOTHING, 50) == M{});
    CHECK(matches("KHAZ", 50) == M{{SETTLEMENT, 4}});
    // A-Z only: the a-circumflex (c3 a2) is not folded from A-circumflex (c3 82)
    CHECK(matches("khaz\xC3\xA2", 50) == M{{SETTLEMENT, 4}});
    CHECK(matches("KHAZ\xC3\x82", 50) == M{});
    CHECK(matches("THE KINGDOM", 50) == M{{CIV, 1}});
    CHECK(matches("tHe HoLt", 50) == M{{REGION, 2}});
    // at most one match per (type, id), and the name reported is the first that matches
    const auto wold = t3().index.search(t3().h, "wold", 50);
    REQUIRE(wold.size() == 1U);
    CHECK(wold[0].name == "Grey Wold");  // the name at E is tried first
    const auto ash = t3().index.search(t3().h, "ASH", 50);
    REQUIRE(ash.size() == 1U);
    CHECK(ash[0].name == "Ash Wold");
    CHECK(ash[0].type == REGION);
    const auto yews = t3().index.search(t3().h, "yews", 50);
    REQUIRE(yews.size() == 1U);
    CHECK(yews[0].name == "The Holt of Yews");
    const auto k = t3().index.search(t3().h, "z\xE2", 50);
    CHECK(k.empty());  // "z" + 0xE2 (a Latin-1 a-circumflex): not the UTF-8 bytes
    const auto kh = t3().index.search(t3().h, "khaz", 50);
    REQUIRE(kh.size() == 1U);
    CHECK(kh[0].name == KHAZAD);
}

TEST_CASE("P4-05 10.2 item 3: T3's block lines (no capital, ended, seceded, an adversary of another type)") {
    // c1 at full: an enmity with f1 (entity type 6, no name). Reading B1 of the reference: an entity with no
    // name shows its tag with an untagged link, so that B4's transform gives the report form back.
    const std::vector<LinkedText> c1 = block(1, HistoryLevel::full);
    REQUIRE_FALSE(c1.empty());
    CHECK(c1[0] == linked("c1 The Kingdom of Arn", {}));
    const LinkedText* en = line_starting(c1, "    enmity: ");
    REQUIRE(en != nullptr);
    CHECK(en->text == "    enmity: The Kingdom of Arn, f1, since 700, e7");
    const std::vector<TextLink> en_links = {lk(12, 18, CIV, 1, true), lk(32, 2, FIGURE, 1, false),
                                            lk(47, 2, 0, 7, false)};
    CHECK_MESSAGE(en->links == en_links, link_list(*en));
    CHECK(b4_report_form(*en) == "    enmity: The Kingdom of Arn (c1), f1, since 700, e7");
    const LinkedText* race = line_starting(c1, "  race: ");
    REQUIRE(race != nullptr);
    CHECK(*race == linked("  race: Human", {lk(8, 5, RACE, 1, true)}));

    // c2: seceded, no capital
    const std::vector<LinkedText> c2 = block(2, HistoryLevel::summary);
    const LinkedText* sec = line_starting(c2, "  seceded from: ");
    REQUIRE(sec != nullptr);
    CHECK(*sec == linked("  seceded from: The Kingdom of Arn, e6", {lk(16, 18, CIV, 1, true), lk(36, 2, 0, 6)}));
    const LinkedText* cap = line_starting(c2, "  capital: ");
    REQUIRE(cap != nullptr);
    CHECK(*cap == linked("  capital: none", {}));

    // c3: ended (short at summary: no home, peaks or events line), its last capital a non-ASCII name
    const std::vector<LinkedText> c3 = block(3, HistoryLevel::summary);
    REQUIRE(c3.size() >= 4U);
    CHECK(c3[0] == linked("c3 Stonehelm", {}));
    CHECK(c3[1] == linked("  race: Dwarf", {lk(8, 5, RACE, 2, true)}));
    CHECK(c3[2] == linked("  founded: 1000 in The Age of Dwarves (a2), e9", {lk(44, 2, 0, 9)}));
    CHECK(c3[3] == linked("  last capital: " + KHAZAD, {lk(16, 7, SETTLEMENT, 4, true)}));
    CHECK(line_starting(c3, "  home: ") == nullptr);
    const LinkedText* fate = line_starting(c3, "  fate: ");
    REQUIRE(fate != nullptr);
    CHECK_MESSAGE(*fate == linked("  fate: ended in 1800, e22: Stonehelm fell to the Kingdom of Arn.",
                                  {lk(23, 3, 0, 22), lk(28, 9, CIV, 3), lk(46, 18, CIV, 1)}),
                  link_list(*fate));
    const LinkedText* m13 = line_starting(c3, "    1250 e13 ");
    REQUIRE(m13 != nullptr);
    CHECK_MESSAGE(*m13 == linked("    1250 e13 war_declared: Stonehelm declared war on the Kingdom of Arn over the "
                                 "Holt of Yews. (Claimed since 1200.)",
                                 {lk(9, 3, 0, 13), lk(27, 9, CIV, 3), lk(53, 18, CIV, 1), lk(77, 16, REGION, 2)}),
                  link_list(*m13));
    const std::vector<LinkedText> c3f = block(3, HistoryLevel::full);
    const LinkedText* home = line_starting(c3f, "  home: ");
    REQUIRE(home != nullptr);
    CHECK(*home == linked("  home: Deep Halls, mountain, upland", {lk(8, 10, REGION, 4, true)}));  // name at E

    // block() is block_line_display over civilization_block at the index's digest
    for (const brapi::CivilizationEntry& c : t3().index.civilizations()) {
        for (const HistoryLevel level : {HistoryLevel::summary, HistoryLevel::full}) {
            const auto lines = brapi::civilization_block(t3().h, t3().index.digest(), c.id, level);
            const auto shown = block(c.id, level);
            REQUIRE(lines.size() == shown.size());
            for (std::size_t i = 0; i < lines.size(); ++i) {
                CHECK(brapi::block_line_display(t3().h, lines[i]) == shown[i]);
            }
        }
    }

    // segments (4.2): a note is " (" literal, text, ")" literal; the fate line has an event and a sentence segment
    const auto seg3 = brapi::civilization_block(t3().h, t3().index.digest(), 3, HistoryLevel::summary);
    bool saw_note = false, saw_fate = false;
    for (const brapi::BlockLine& l : seg3) {
        const std::string text = brapi::block_line_report_text(t3().h, l);
        if (text.rfind("    1250 e13 ", 0) == 0) {
            REQUIRE(l.size() >= 3U);
            const brapi::Segment& a = l[l.size() - 3];
            const brapi::Segment& b = l[l.size() - 2];
            const brapi::Segment& c = l[l.size() - 1];
            CHECK(a.kind == brapi::SegmentKind::literal);
            CHECK(a.text == " (");
            CHECK(b.kind == brapi::SegmentKind::text);
            CHECK(b.text == "Claimed since 1200.");
            CHECK(c.kind == brapi::SegmentKind::literal);
            CHECK(c.text == ")");
            saw_note = true;
        }
        if (text.rfind("  fate: ", 0) == 0) {
            bool ev = false, sentence = false;
            for (const brapi::Segment& s : l) {
                ev = ev || (s.kind == brapi::SegmentKind::event && s.id == 22);
                sentence = sentence || (s.kind == brapi::SegmentKind::sentence && s.id == 22);
            }
            CHECK(ev);
            CHECK(sentence);
            CHECK(text == "  fate: ended in 1800, e22: Stonehelm fell to the Kingdom of Arn.");
            saw_fate = true;
        }
    }
    CHECK(saw_note);
    CHECK(saw_fate);
    const auto first = brapi::civilization_block(t3().h, t3().index.digest(), 1, HistoryLevel::full);
    REQUIRE_FALSE(first.empty());
    CHECK(brapi::block_line_report_text(t3().h, first[0]) == "c1 The Kingdom of Arn");
    for (const brapi::Segment& s : first[0]) CHECK(s.kind != brapi::SegmentKind::entity);  // the heading: no link

    // FatalErrors
    const Opened& o = t3();
    CHECK(fatal_text([&] { (void)o.index.block(o.h, 4, HistoryLevel::full); }) == "browser: no civilization 4");
    CHECK(fatal_text([&] { (void)o.index.block(o.h, 0, HistoryLevel::summary); }) == "browser: no civilization 0");
    CHECK(fatal_text([&] { (void)o.index.block(o.h, 1, static_cast<HistoryLevel>(0)); }) == "browser: bad level");
    CHECK(fatal_text([&] { (void)o.index.block(o.h, 1, static_cast<HistoryLevel>(3)); }) == "browser: bad level");
    CHECK_FALSE(fatal_text([&] { (void)brapi::civilization_block(o.h, o.index.digest(), 4, HistoryLevel::full); })
                    .empty());
}

}  // TEST_SUITE("browser")
