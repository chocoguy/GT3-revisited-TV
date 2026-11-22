// The Apple TV's kept files (platform/tv_persist.h): the settings, saves and replays packed for
// the user defaults within a budget, and written back into an emptied user data folder. Works in
// the run's temporary folder only.

#include "test_framework.h"

#include "base/file.h"
#include "platform/tv_persist.h"

#include <filesystem>

using namespace gt3r;
using namespace gt3r::platform;
namespace fs = std::filesystem;

namespace {

Bytes bytes_of(const std::string& text) { return Bytes(text.begin(), text.end()); }

bool write_text(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    const Bytes b = bytes_of(text);
    return write_host_file(path.string(), b);
}

std::string read_text(const fs::path& path) {
    std::optional<Bytes> b = read_host_file(path.string());
    return b ? std::string(b->begin(), b->end()) : std::string("<missing>");
}

// Bytes that do not compress (a replay's worth of them fills a budget).
Bytes noise(size_t n, u32 seed) {
    Bytes b(n);
    for (u8& v : b) v = u8((seed = seed * 1664525u + 1013904223u) >> 24);
    return b;
}

} // namespace

TEST(tv_persist_pack_round_trip) {
    const std::vector<PersistFile> files = {
        {"config.ini", bytes_of("[video]\nmsaa = 2\n"), 100},
        {"saves/eu/My game.sav", Bytes(200000, 0), 200}, // mostly zeros, as saves are
        {"saves/current.txt", {}, 300},                  // an empty file is a file
    };
    std::vector<PersistFile> left;
    const Bytes blob = pack_persist_files(files, kPersistBudget, &left);
    CHECK(left.empty());
    CHECK(blob.size() < 4096); // compressed
    const auto back = unpack_persist_files(blob);
    CHECK(back.has_value());
    if (back) {
        CHECK(back->size() == files.size());
        for (size_t i = 0; i < back->size() && i < files.size(); ++i) {
            CHECK((*back)[i].path == files[i].path);
            CHECK((*back)[i].data == files[i].data);
            CHECK((*back)[i].mtime == files[i].mtime);
        }
    }
}

TEST(tv_persist_budget_keeps_the_first_files) {
    const std::vector<PersistFile> files = {
        {"config.ini", bytes_of("a = b\n"), 1},
        {"replays/big.gt3replay", noise(300000, 1), 2}, // does not fit
        {"saves/small.sav", bytes_of("credits 100\n"), 3}, // a smaller file after it still does
    };
    std::vector<PersistFile> left;
    const Bytes blob = pack_persist_files(files, 100000, &left);
    CHECK(blob.size() <= 100000);
    CHECK(left.size() == 1);
    if (left.size() == 1) CHECK(left[0].path == "replays/big.gt3replay");
    const auto back = unpack_persist_files(blob);
    CHECK(back && back->size() == 2);
    if (back && back->size() == 2) {
        CHECK((*back)[0].path == "config.ini");
        CHECK((*back)[1].path == "saves/small.sav");
    }
}

TEST(tv_persist_rejects_damage_and_bad_paths) {
    const Bytes blob = pack_persist_files({{"config.ini", bytes_of("a = b\n"), 1}}, kPersistBudget, nullptr);
    CHECK(unpack_persist_files(blob).has_value());
    CHECK(!unpack_persist_files({}).has_value());
    CHECK(!unpack_persist_files(ByteSpan(blob.data(), blob.size() - 3)).has_value()); // cut short
    Bytes other = blob;
    other[0] = 'X';
    CHECK(!unpack_persist_files(other).has_value());
    // Paths that would leave the folder are never packed.
    std::vector<PersistFile> left;
    const Bytes none = pack_persist_files(
        {{"../outside.txt", bytes_of("x"), 1}, {"/etc/passwd", bytes_of("x"), 1}, {"saves//a.sav", bytes_of("x"), 1}},
        kPersistBudget, &left);
    CHECK(left.size() == 3);
    const auto back = unpack_persist_files(none);
    CHECK(back && back->empty());
}

TEST(tv_persist_collects_in_order_and_restores_without_overwriting) {
    const fs::path root = test::temp_dir("tv_persist");
    const fs::path from = root / "from", to = root / "to";
    CHECK(write_text(from / "config.ini", "[game]\n"));
    CHECK(write_text(from / "saves" / "old.sav", "old"));
    CHECK(write_text(from / "saves" / "eu" / "new.sav", "new"));
    CHECK(write_text(from / "saves" / "new.sav.tmp", "half written"));
    CHECK(write_text(from / "music_playlist.txt", "songs"));
    CHECK(write_text(from / "replays" / "lap.gt3replay", "replay"));
    CHECK(write_text(from / "log.txt", "not kept"));
    CHECK(write_text(from / "mods" / "mod.zip", "not kept"));
    fs::last_write_time(from / "saves" / "old.sav", fs::last_write_time(from / "saves" / "eu" / "new.sav") - std::chrono::hours(1));

    const std::vector<PersistFile> files = collect_persist_files(from.string() + "/");
    CHECK(files.size() == 5);
    if (files.size() == 5) {
        CHECK(files[0].path == "config.ini");
        CHECK(files[1].path == "saves/eu/new.sav"); // the save written last comes first
        CHECK(files[2].path == "saves/old.sav");
        CHECK(files[3].path == "music_playlist.txt");
        CHECK(files[4].path == "replays/lap.gt3replay");
    }

    const auto back = unpack_persist_files(pack_persist_files(files, kPersistBudget, nullptr));
    CHECK(back.has_value());
    if (!back) return;
    CHECK(write_text(to / "saves" / "old.sav", "played since")); // there already: stays as it is
    CHECK(restore_persist_files(to.string() + "/", *back) == 4);
    CHECK(read_text(to / "config.ini") == "[game]\n");
    CHECK(read_text(to / "saves" / "eu" / "new.sav") == "new");
    CHECK(read_text(to / "saves" / "old.sav") == "played since");
    CHECK(read_text(to / "replays" / "lap.gt3replay") == "replay");
    CHECK(restore_persist_files(to.string() + "/", *back) == 0); // a second time: nothing to write
}
