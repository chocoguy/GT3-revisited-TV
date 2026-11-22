#pragma once

// Apple TV: the log in a file. Nothing shows a console there and the app's files are reached
// only from a Mac over the network, so everything the game logs (and whatever else is written to
// stderr or stdout: an abort's last words) goes to <dir>/log.txt, which tools/tvos/tv.py fetches.
// The run before is kept as log.prev.txt: the log of a run that ended badly is still there
// after the game was started again.

#include <string>

namespace gt3r::platform {

// tvOS: starts the log file in `dir` (the user data folder, with its trailing separator), each
// line with the seconds since the start and its level (D, I, W, E). False when the file could not
// be made (the log then stays where SDL sends it), and on every other platform, where it does
// nothing.
bool start_log_file(const std::string& dir);

} // namespace gt3r::platform
