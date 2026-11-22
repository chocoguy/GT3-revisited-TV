#pragma once

// Apple TV: where the game finds its disc image. tvOS has no file picker and no folder the
// player reaches, so an image is either built into the app (GT3R_TVOS_DISC, in the bundle's
// disc/ folder: it stays as long as the app does) or sent from a Mac to the user data folder
// (tools/tvos/tv.py push-disc; tvOS may empty that folder while the app is not running). Both
// folders get a new path with every install, so the images are looked for at each start
// instead of being remembered by path.

#include <string>
#include <vector>

namespace gt3r::platform {

// The folder of the disc image built into the app: <bundle>/disc/ ("" without a bundle path).
std::string bundled_disc_dir();

// The disc images (.iso, .chd, by name) in `dirs`, in that order and by name within a folder,
// as full paths. Folders that are not there are skipped; "" is skipped.
std::vector<std::string> find_disc_images(const std::vector<std::string>& dirs);

} // namespace gt3r::platform
