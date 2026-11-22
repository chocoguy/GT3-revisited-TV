#include "platform/tv_discs.h"

#include "base/file.h"

#include <SDL3/SDL_filesystem.h>

#include <algorithm>
#include <cctype>

namespace gt3r::platform {

std::string bundled_disc_dir() {
    const char* base = SDL_GetBasePath(); // ends with a separator
    return base && *base ? std::string(base) + "disc/" : std::string();
}

std::vector<std::string> find_disc_images(const std::vector<std::string>& dirs) {
    std::vector<std::string> found;
    for (std::string dir : dirs) {
        if (dir.empty()) continue;
        if (dir.back() != '/' && dir.back() != '\\') dir += '/';
        for (const HostDirEntry& e : host_dir_entries(dir)) { // sorted by name
            std::string name = e.name;
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            if (!e.dir && (name.ends_with(".iso") || name.ends_with(".chd"))) found.push_back(dir + e.name);
        }
    }
    return found;
}

} // namespace gt3r::platform
