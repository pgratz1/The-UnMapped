#pragma once

// The browser part of the headless check (spec P4-05 8.2, checks B0 to
// B12). Called by run_viewer_check; `expect` records one check line.

#include <functional>

#include <godot_cpp/variant/string.hpp>

#include "viewer/viewer_check.hpp"

namespace unmapped {

class ViewerScreen;

using CheckExpect = std::function<void(bool ok, const godot::String& what)>;

// B0: a world without history.
void check_browser_without_history(ViewerScreen& screen, const CheckExpect& expect);
// B1 to B12: a world with history; options.history_report (full) and
// options.history_summary (summary) are the references.
void check_browser(ViewerScreen& screen, const ViewerCheckOptions& options, const CheckExpect& expect);

}  // namespace unmapped
