#pragma once

// The content edits of the P5-02 tests (the reference's monsters_small_data): history.monsters fields replaced,
// every monster kind's fields scaled or set, and the fixture monsters-small (19.3). Shared by the monsters and the
// renaming tests (P5-12 13's rename-small is monsters-small with the renaming edits from 0.23.0).

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>

#include "p3_06_conflict_support.hpp"
#include "worldgen/data/content.hpp"

namespace monsters_content {

namespace wd = worldgen::data;

// history.monsters with fields replaced, and every monster kind's fields replaced by `kind_edit` (past the loader).
inline wd::ContentSet with_monsters(const wd::ContentSet& base, const std::map<std::string, wd::Value>& tuning,
                             const std::map<std::string, std::int64_t>& kind_scale,
                             const std::map<std::string, wd::Value>& kind_set,
                             const std::map<std::string, wd::Value>& peoples = {}) {
    wd::ContentSet c = base;
    for (wd::KindContent& k : c.kinds) {
        if (k.kind == "history") {
            for (wd::Definition& d : k.definitions) {
                const std::map<std::string, wd::Value>* edits =
                    d.id == "monsters" ? &tuning : d.id == "peoples" ? &peoples : nullptr;
                if (edits == nullptr || edits->empty()) continue;
                wd::Record r;
                for (std::size_t i = 0; i < d.record.size(); ++i) {
                    const auto it = edits->find(d.record.name_at(i));
                    r.add(d.record.name_at(i), it != edits->end() ? it->second : d.record.value_at(i));
                }
                d.record = std::move(r);
            }
        }
        if (k.kind == "monster" && (!kind_scale.empty() || !kind_set.empty())) {
            for (wd::Definition& d : k.definitions) {
                wd::Record r;
                for (std::size_t i = 0; i < d.record.size(); ++i) {
                    const std::string& n = d.record.name_at(i);
                    if (const auto s = kind_scale.find(n); s != kind_scale.end()) {
                        r.add(n, wd::Value::integer(std::min<std::int64_t>(10000000, d.record.value_at(i).as_int() *
                                                                                          s->second)));
                    } else if (const auto v = kind_set.find(n); v != kind_set.end()) {
                        r.add(n, v->second);
                    } else {
                        r.add(n, d.record.value_at(i));
                    }
                }
                d.record = std::move(r);
            }
        }
    }
    return c;
}

inline wd::Value pressure_of(std::int64_t low, std::int64_t normal, std::int64_t high) {
    wd::Record r;
    r.add("low", wd::Value::integer(low));
    r.add("normal", wd::Value::integer(normal));
    r.add("high", wd::Value::integer(high));
    return wd::Value::record(std::move(r));
}

// 19.3 monsters-small (reading X1 of the reference: the stress content with the monster edits).
inline const wd::ContentSet& monsters_small_content() {
    static const wd::ContentSet c =
        with_monsters(conflict_test::stress_content(), {{"living_permille", wd::Value::integer(1000)}},
                      {{"wake_rate", 2000}}, {{"overrun_permille", wd::Value::integer(1000)}});
    return c;
}
}  // namespace monsters_content
