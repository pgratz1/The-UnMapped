// lint-selftest-path: worldgen/src/history/selftest_member_held.cpp
// Self-test snippet: scanned as history library code (P3-04 6.9, P5-02
// 6.7). A reference or pointer into a subsystem's monster or site vector
// must not be used after the vector grows in the same block: the append
// may reallocate it. Not compiled.
#include "worldgen/history/monsters.hpp"

namespace worldgen::history {

void MonstersSubsystem::add_site(std::int64_t id) {
    Site x;
    x.id = id;
    sites_.push_back(std::move(x));
}

void MonstersSubsystem::add_through(std::int64_t id) {
    add_site(id);
}

void MonstersSubsystem::bad_refs(std::int64_t id) {
    const Site& s = site(id);  // expect: history-member-held-reference
    sites_.push_back(Site{});
    (void)s.region;

    Monster* m = &monster(id);  // expect: history-member-held-reference
    monsters_.emplace_back();
    m->hoard = 1;

    auto& t = site(id);  // expect: history-member-held-reference
    add_through(id);
    t.ending = 3;
}

// Allowed: a reference into the other vector; fields copied first; the
// reference only in the append's own arguments; re-fetched after; a block
// that ends before the append; a range-for variable; another type.
void MonstersSubsystem::good_refs(std::int64_t id) {
    const Monster& m = monster(id);
    sites_.push_back(Site{});
    (void)m.kind;
    const Site& s = site(id);
    const std::int64_t region = s.region;
    add_site(region);
    const Site& u = site(id);
    sites_.push_back(u);
    Site& again = site(id);
    again.ending = 1;
    {
        const Site& inner = site(id);
        (void)inner.kind;
    }
    sites_.push_back(Site{});
    for (const Site& v : sites_) {
        (void)v.id;
    }
    const db::SiteRow& row = rows_.front();
    sites_.push_back(Site{});
    (void)row.id;
}

}  // namespace worldgen::history
