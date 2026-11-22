#pragma once

// Apple TV: what a run cost, in the log. tvOS ends an app whose memory footprint passes its
// limit with no crash report and nothing the app can catch, and the fanless box lowers its
// clocks when it runs hot, so the footprint, the room left under the limit, the thermal state
// and the frame rate are logged: every few seconds while they change, and at once when the
// thermal state does. A log that just stops then says how close the run was
// (tools/tvos/tv.py reports).

namespace gt3r::platform {

// Once a frame. `now_ms`: SDL_GetTicks(). Does nothing on other platforms.
void stats_frame(unsigned long long now_ms);

} // namespace gt3r::platform
