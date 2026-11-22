#include "platform/tv_persist.h"

#include "base/log.h"

#include <SDL3/SDL.h>
#include <zlib.h>

#include <algorithm>
#include <atomic>
#include <cstring>

#if defined(SDL_PLATFORM_TVOS)
#include <objc/message.h>
#include <objc/runtime.h>
#endif

namespace gt3r::platform {

namespace {

constexpr char kMagic[4] = {'G', 'T', '3', 'P'};
constexpr u32 kVersion = 1;
constexpr size_t kHeaderSize = 12;     // magic, version, file count
constexpr size_t kMaxPathLength = 1024;
constexpr u32 kMaxFileSize = 64u * 1024 * 1024;

void put_u32(Bytes& out, u32 v) {
    for (int i = 0; i < 4; ++i) out.push_back(u8(v >> (8 * i)));
}
void put_u64(Bytes& out, u64 v) {
    for (int i = 0; i < 8; ++i) out.push_back(u8(v >> (8 * i)));
}
bool get_u32(ByteSpan in, size_t& at, u32& v) {
    if (in.size() - at < 4) return false;
    v = 0;
    for (int i = 0; i < 4; ++i) v |= u32(in[at + size_t(i)]) << (8 * i);
    at += 4;
    return true;
}
bool get_u64(ByteSpan in, size_t& at, u64& v) {
    if (in.size() - at < 8) return false;
    v = 0;
    for (int i = 0; i < 8; ++i) v |= u64(in[at + size_t(i)]) << (8 * i);
    at += 8;
    return true;
}

// The files of `dir`/`sub` and of the folders in it, by path inside `dir`.
void collect_tree(const std::string& dir, const std::string& sub, size_t largest, std::vector<PersistFile>& out,
                  int depth = 0) {
    if (depth > 4) return;
    for (const HostDirEntry& e : host_dir_entries(dir + sub)) {
        const std::string rel = sub + "/" + e.name;
        if (e.dir) {
            collect_tree(dir, rel, largest, out, depth + 1);
        } else if (e.size <= largest && !e.name.ends_with(".tmp")) { // (.tmp: a save being written)
            if (std::optional<Bytes> data = read_host_file(dir + rel)) out.push_back({rel, std::move(*data), e.mtime});
        }
    }
}

void newest_first(std::vector<PersistFile>& files, size_t from) {
    std::stable_sort(files.begin() + std::ptrdiff_t(from), files.end(),
                     [](const PersistFile& a, const PersistFile& b) { return a.mtime > b.mtime; });
}

bool safe_path(const std::string& path) {
    if (path.empty() || path.size() > kMaxPathLength || path.front() == '/' || path.find('\\') != std::string::npos ||
        path.find(':') != std::string::npos)
        return false;
    for (size_t at = 0; at <= path.size();) { // no "." or ".." part, no empty one
        const size_t end = std::min(path.find('/', at), path.size());
        const std::string part = path.substr(at, end - at);
        if (part.empty() || part == "." || part == "..") return false;
        at = end + 1;
    }
    return true;
}

} // namespace

std::vector<PersistFile> collect_persist_files(const std::string& dir, size_t largest) {
    std::vector<PersistFile> files;
    auto single = [&](const char* name) {
        const std::optional<s64> mtime = host_file_mtime(dir + name);
        if (!mtime) return;
        if (std::optional<Bytes> data = read_host_file(dir + name); data && data->size() <= largest)
            files.push_back({name, std::move(*data), *mtime});
    };
    single("config.ini");
    size_t from = files.size();
    collect_tree(dir, "saves", largest, files);
    newest_first(files, from);
    single("music_playlist.txt");
    from = files.size();
    collect_tree(dir, "replays", largest, files);
    newest_first(files, from);
    return files;
}

Bytes pack_persist_files(const std::vector<PersistFile>& files, size_t budget, std::vector<PersistFile>* left_out) {
    Bytes out(kMagic, kMagic + 4);
    put_u32(out, kVersion);
    put_u32(out, 0); // the file count, set below
    u32 count = 0;
    for (const PersistFile& f : files) {
        Bytes packed;
        bool fits = safe_path(f.path) && f.data.size() <= kMaxFileSize;
        if (fits) {
            uLongf len = compressBound(uLong(f.data.size()));
            packed.resize(len);
            fits = compress2(packed.data(), &len, f.data.data(), uLong(f.data.size()), Z_BEST_SPEED) == Z_OK;
            packed.resize(fits ? len : 0);
        }
        const size_t entry = 4 + f.path.size() + 8 + 4 + 4 + packed.size();
        if (!fits || out.size() + entry > budget) {
            if (left_out) left_out->push_back(f);
            continue;
        }
        put_u32(out, u32(f.path.size()));
        out.insert(out.end(), f.path.begin(), f.path.end());
        put_u64(out, u64(f.mtime));
        put_u32(out, u32(f.data.size()));
        put_u32(out, u32(packed.size()));
        out.insert(out.end(), packed.begin(), packed.end());
        ++count;
    }
    for (int i = 0; i < 4; ++i) out[8 + size_t(i)] = u8(count >> (8 * i));
    return out;
}

std::optional<std::vector<PersistFile>> unpack_persist_files(ByteSpan blob) {
    if (blob.size() < kHeaderSize || std::memcmp(blob.data(), kMagic, 4) != 0) return std::nullopt;
    size_t at = 4;
    u32 version = 0, count = 0;
    if (!get_u32(blob, at, version) || version != kVersion || !get_u32(blob, at, count)) return std::nullopt;
    std::vector<PersistFile> files;
    for (u32 i = 0; i < count; ++i) {
        u32 path_len = 0, raw_len = 0, packed_len = 0;
        u64 mtime = 0;
        if (!get_u32(blob, at, path_len) || path_len > kMaxPathLength || blob.size() - at < path_len) return std::nullopt;
        PersistFile f;
        f.path.assign(reinterpret_cast<const char*>(blob.data() + at), path_len);
        at += path_len;
        if (!safe_path(f.path) || !get_u64(blob, at, mtime) || !get_u32(blob, at, raw_len) ||
            !get_u32(blob, at, packed_len) || raw_len > kMaxFileSize || blob.size() - at < packed_len)
            return std::nullopt;
        f.mtime = s64(mtime);
        f.data.resize(raw_len);
        uLongf len = raw_len;
        if (uncompress(f.data.data(), &len, blob.data() + at, packed_len) != Z_OK || len != raw_len) return std::nullopt;
        at += packed_len;
        files.push_back(std::move(f));
    }
    return files;
}

int restore_persist_files(const std::string& dir, const std::vector<PersistFile>& files) {
    int written = 0;
    for (const PersistFile& f : files) {
        if (!safe_path(f.path) || host_path_exists(dir + f.path)) continue;
        if (const size_t slash = f.path.rfind('/'); slash != std::string::npos) host_make_dir(dir + f.path.substr(0, slash));
        if (write_host_file(dir + f.path, f.data)) ++written;
        else LOG_WARN("Kept file %s could not be written back: %s", f.path.c_str(), SDL_GetError());
    }
    return written;
}

#if defined(SDL_PLATFORM_TVOS)
namespace {

constexpr const char* kDefaultsKey = "GT3RKeptFiles";
constexpr Uint64 kQuietMs = 3000; // after the last write: a save is a few files in a row

std::string g_dir;                     // the user data folder
std::atomic<bool> g_dirty{false};      // a file was written since the files were last kept
std::atomic<Uint64> g_last_write{0};   // SDL_GetTicks() of that write
std::atomic<bool> g_restoring{false};  // (writing the kept files back is no change to keep)
u64 g_kept_signature = 0;              // of the files last kept: nothing is written twice

// An autorelease pool for the Objective-C objects made here (this is not an Objective-C file).
extern "C" void* objc_autoreleasePoolPush(void);
extern "C" void objc_autoreleasePoolPop(void* pool);
struct Pool {
    void* pool = objc_autoreleasePoolPush();
    ~Pool() { objc_autoreleasePoolPop(pool); }
};

id cls(const char* name) { return reinterpret_cast<id>(objc_getClass(name)); }
id defaults() {
    return reinterpret_cast<id (*)(id, SEL)>(objc_msgSend)(cls("NSUserDefaults"), sel_registerName("standardUserDefaults"));
}
id ns_string(const char* s) {
    return reinterpret_cast<id (*)(id, SEL, const char*)>(objc_msgSend)(cls("NSString"),
                                                                       sel_registerName("stringWithUTF8String:"), s);
}

void store_defaults(const Bytes& blob) {
    id data = reinterpret_cast<id (*)(id, SEL, const void*, unsigned long)>(objc_msgSend)(
        cls("NSData"), sel_registerName("dataWithBytes:length:"), blob.data(), static_cast<unsigned long>(blob.size()));
    reinterpret_cast<void (*)(id, SEL, id, id)>(objc_msgSend)(defaults(), sel_registerName("setObject:forKey:"), data,
                                                              ns_string(kDefaultsKey));
    // Written out now: the app may be ended right after it left the foreground.
    reinterpret_cast<BOOL (*)(id, SEL)>(objc_msgSend)(defaults(), sel_registerName("synchronize"));
}

Bytes load_defaults() {
    id data = reinterpret_cast<id (*)(id, SEL, id)>(objc_msgSend)(defaults(), sel_registerName("dataForKey:"),
                                                                  ns_string(kDefaultsKey));
    if (!data) return {};
    const auto* bytes = reinterpret_cast<const u8* (*)(id, SEL)>(objc_msgSend)(data, sel_registerName("bytes"));
    const unsigned long n = reinterpret_cast<unsigned long (*)(id, SEL)>(objc_msgSend)(data, sel_registerName("length"));
    return bytes ? Bytes(bytes, bytes + n) : Bytes();
}

// <container>/Library/Preferences/GT3RevisitedFiles.bin, from the user data folder
// (<container>/Library/Caches/...); "" when the folder is somewhere else (GT3R_DATA_DIR).
std::string overflow_path() {
    const size_t at = g_dir.find("/Library/Caches/");
    return at == std::string::npos ? std::string() : g_dir.substr(0, at) + "/Library/Preferences/GT3RevisitedFiles.bin";
}

u64 signature(const std::vector<PersistFile>& files) {
    u64 h = 14695981039346656037ull;
    auto mix = [&](const void* p, size_t n) {
        for (size_t i = 0; i < n; ++i) h = (h ^ static_cast<const u8*>(p)[i]) * 1099511628211ull;
    };
    for (const PersistFile& f : files) {
        mix(f.path.data(), f.path.size());
        mix(&f.mtime, sizeof f.mtime);
        mix(f.data.data(), f.data.size());
    }
    return h ? h : 1;
}

void keep_now() {
    g_dirty = false;
    if (g_dir.empty()) return;
    {
        const Pool pool;
        const Uint64 start = SDL_GetTicksNS();
        const std::vector<PersistFile> files = collect_persist_files(g_dir);
        const u64 sig = signature(files);
        if (sig == g_kept_signature) return;
        std::vector<PersistFile> rest, lost;
        const Bytes blob = pack_persist_files(files, kPersistBudget, &rest);
        store_defaults(blob);
        size_t overflow_size = 0;
        if (const std::string path = overflow_path(); !path.empty()) {
            if (rest.empty()) {
                host_remove(path);
            } else {
                const Bytes more = pack_persist_files(rest, kPersistOverflowBudget, &lost);
                g_restoring = true; // (not a file of the user data folder anyway)
                if (!write_host_file(path, more)) LOG_WARN("Kept files: %s could not be written", path.c_str());
                g_restoring = false;
                overflow_size = more.size();
            }
        } else {
            lost = rest;
        }
        for (const PersistFile& f : lost) LOG_WARN("Not kept through a purge (no room): %s", f.path.c_str());
        g_kept_signature = sig;
        LOG_INFO("Kept %zu files: %zu KB in the user defaults, %zu KB beside them, %.0f ms", files.size() - lost.size(),
                 blob.size() / 1024, overflow_size / 1024, double(SDL_GetTicksNS() - start) / 1e6);
    }
}

} // namespace

void persist_start(const std::string& dir) {
    g_dir = dir;
    if (g_dir.empty()) return;
    {
        const Pool pool;
        bool saves = false;
        for (const HostDirEntry& e : host_dir_entries(g_dir + "saves")) saves = saves || e.dir || e.name.ends_with(".sav");
        if (host_path_exists(g_dir + "config.ini") || saves) return;
        // A first start, or the system emptied the folder (or the app was installed anew).
        g_restoring = true;
        int written = 0;
        if (auto files = unpack_persist_files(load_defaults())) written += restore_persist_files(g_dir, *files);
        if (const std::string path = overflow_path(); !path.empty())
            if (std::optional<Bytes> more = read_host_file(path))
                if (auto files = unpack_persist_files(*more)) written += restore_persist_files(g_dir, *files);
        g_restoring = false;
        if (written) LOG_INFO("The user data folder was empty: %d kept files written back", written);
    }
}

void persist_note_write(const std::string& path) {
    if (g_dir.empty() || g_restoring || path.compare(0, g_dir.size(), g_dir) != 0) return;
    g_last_write = SDL_GetTicks();
    g_dirty = true;
}

void persist_tick(unsigned long long now_ms) {
    if (g_dirty && now_ms - g_last_write >= kQuietMs) keep_now();
}

void persist_flush() {
    if (g_dirty) keep_now();
}

#else

void persist_start(const std::string&) {}
void persist_note_write(const std::string&) {}
void persist_tick(unsigned long long) {}
void persist_flush() {}

#endif

} // namespace gt3r::platform
