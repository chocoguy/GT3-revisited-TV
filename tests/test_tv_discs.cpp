// The Apple TV's disc images (platform/tv_discs.h): looked for in the app's bundle, then in the
// user data folder. Works in the run's temporary folder only.

#include "test_framework.h"

#include "base/file.h"
#include "platform/tv_discs.h"

#include <filesystem>

using namespace gt3r;
namespace fs = std::filesystem;

TEST(tv_discs_found_in_folder_order) {
    const fs::path root = test::temp_dir("tv_discs");
    const fs::path bundle = root / "bundle" / "disc", data = root / "data";
    fs::create_directories(bundle);
    fs::create_directories(data / "mods.iso"); // a folder is no image
    const u8 byte[1] = {0};
    for (const fs::path& p : {bundle / "GT3.CHD", data / "b.iso", data / "a.iso", data / "config.ini", data / "iso"})
        CHECK(write_host_file(p.string(), ByteSpan(byte, 1)));

    const std::vector<std::string> found =
        platform::find_disc_images({(bundle.string() + "/"), "", (root / "missing").string(), data.string()});
    CHECK(found.size() == size_t(3));
    if (found.size() == 3) {
        CHECK(fs::path(found[0]).filename().string() == std::string("GT3.CHD")); // the bundle's first
        CHECK(fs::path(found[1]).filename().string() == std::string("a.iso"));
        CHECK(fs::path(found[2]).filename().string() == std::string("b.iso"));
        CHECK(host_path_exists(found[1])); // a full path, whether or not the folder had its separator
    }
    CHECK(platform::find_disc_images({}).empty());
}
