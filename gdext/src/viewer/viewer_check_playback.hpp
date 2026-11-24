#pragma once

// The playback part of the headless check (spec P4-06 11.2, checks P0 to
// P10). Called by run_viewer_check after the browser checks. The timing
// reference is the check's own implementation of 4.2 over its own stop set
// (event years from the history reports, holder changes from its own
// Timeline, phase starts from [phases]); it never reads PlaybackState.

#include "viewer/viewer_check.hpp"
#include "viewer/viewer_check_browser.hpp"

namespace unmapped {

class ViewerScreen;

// P0: a world without history.
void check_playback_without_history(ViewerScreen& screen, const CheckExpect& expect);
// P1: at open, before any other check changes the state.
void check_playback_at_open(ViewerScreen& screen, const CheckExpect& expect);
// P2 to P10.
void check_playback(ViewerScreen& screen, const ViewerCheckOptions& options, const CheckExpect& expect);

}  // namespace unmapped
