// lint-selftest-path: worldgen/src/history/selftest_figure_held.cpp
// Self-test snippet: scanned as history library code (P3-04 6.9, P5-06 5).
// FiguresSubsystem keeps its figures in the member vector figures_, grown by
// new_figure and so by every function of the file that calls it, and its
// lines in lines_, which line(civ) itself resizes when civ is past the end;
// a Figure& or Line& must not be used after such a call in the same block.
// Not compiled.
#include "worldgen/history/figures.hpp"

namespace worldgen::history {

FiguresSubsystem::Line& FiguresSubsystem::line(std::int64_t civ) {
    if (static_cast<std::size_t>(civ) > lines_.size()) {
        lines_.resize(static_cast<std::size_t>(civ));
    }
    return lines_[static_cast<std::size_t>(civ - 1)];
}

std::int64_t FiguresSubsystem::new_figure(StepContext& ctx, std::int64_t role, std::int64_t c) {
    figures_.push_back(Figure{});
    (void)ctx;
    (void)role;
    (void)c;
    return static_cast<std::int64_t>(figures_.size());
}

std::int64_t FiguresSubsystem::name_reign(StepContext& ctx, std::int64_t c, std::int64_t record) {
    (void)record;
    return new_figure(ctx, 1, c);
}

void FiguresSubsystem::bad_figures(StepContext& ctx, std::int64_t id, std::int64_t c) {
    Figure& f = figure(id);  // expect: history-member-held-reference
    (void)name_reign(ctx, c, 0);
    f.status = 3;

    const Figure* g = &figure(id);  // expect: history-member-held-reference
    figures_.emplace_back();
    (void)g->role;

    Line& l = line(c);  // expect: history-member-held-reference
    (void)line(c + 1);
    l.has = false;
}

// Allowed: fields copied first; re-fetched after the call; the reference
// unused after it.
void FiguresSubsystem::good_figures(StepContext& ctx, std::int64_t id, std::int64_t c) {
    const Figure f0 = figure(id);
    (void)new_figure(ctx, 3, c);
    Figure& f = figure(id);
    f.status = f0.status;
    const Line& l = line(c);
    const bool has = l.has;
    (void)line(c + 1);
    (void)has;
}

}  // namespace worldgen::history
