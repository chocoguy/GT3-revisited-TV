#pragma once

// Apple TV: what the player made, kept through a purge. The only folder an app writes on tvOS
// (Library/Caches, the user data folder) is one the system may empty while the app is not
// running. The app's user defaults stay, until the app is deleted, but tvOS ends an app whose
// defaults pass about 1 MB. So the small files that matter (the settings, the GT Mode saves, the
// music playlist, replays while they fit) are packed, each compressed, into one value of the
// user defaults, a few seconds after any of them is written and when the game leaves the
// foreground; what does not fit goes into a file in Library/Preferences (which stayed in tests,
// though Apple promises nothing for it). At a start that finds the user data folder without
// settings and saves, they are written back.
//
// Files are named by their path inside the user data folder, never by the folder's own path,
// which every install changes.

#include "base/bytes.h"
#include "base/file.h"

#include <optional>
#include <string>
#include <vector>

namespace gt3r::platform {

struct PersistFile {
    std::string path; // inside the user data folder, '/' between folders ("saves/eu/Mine.sav")
    Bytes data;
    s64 mtime = 0; // seconds since 1970
};

// The user defaults' share: under tvOS's limit with room for the defaults' other values.
constexpr size_t kPersistBudget = 900 * 1024;
// The Library/Preferences file's: replays are a few hundred KB each.
constexpr size_t kPersistOverflowBudget = 48 * 1024 * 1024;

// The files worth keeping in `dir` (the user data folder, with its trailing separator), most
// important first: config.ini, the saves (saves/, newest first: the game being played is the
// one written last), the music playlist, then the replays (replays/, newest first). Files over
// `largest` bytes are left out.
std::vector<PersistFile> collect_persist_files(const std::string& dir, size_t largest = 8 * 1024 * 1024);

// Packs `files`, in their order, into a blob of at most `budget` bytes: a file that would pass
// it is skipped (the smaller ones after it may still fit) and added to `left_out`.
Bytes pack_persist_files(const std::vector<PersistFile>& files, size_t budget, std::vector<PersistFile>* left_out);
// The files of a blob; nullopt when it is not one or is damaged.
std::optional<std::vector<PersistFile>> unpack_persist_files(ByteSpan blob);

// Writes `files` into `dir` where no file of that path is there yet; nothing is written over.
// The number of files written.
int restore_persist_files(const std::string& dir, const std::vector<PersistFile>& files);

// --- The Apple TV's side (no-ops elsewhere) ---

// At the start, before the settings are read: when `dir` has neither config.ini nor saves, the
// kept files are written back. Then watches `dir`.
void persist_start(const std::string& dir);
// A file was written, renamed or removed (base/file.cpp calls this for every one).
void persist_note_write(const std::string& path);
// Once a frame: keeps the files a few seconds after the last write. `now`: SDL_GetTicks().
void persist_tick(unsigned long long now_ms);
// The game leaves the foreground (it may be ended without another word): keeps them now.
void persist_flush();

} // namespace gt3r::platform
