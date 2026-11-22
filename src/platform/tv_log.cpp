#include "platform/tv_log.h"

#include <SDL3/SDL.h>

#if defined(SDL_PLATFORM_TVOS)
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace gt3r::platform {

#if defined(SDL_PLATFORM_TVOS)
namespace {

void SDLCALL write_line(void*, int, SDL_LogPriority priority, const char* message) {
    const char level = priority >= SDL_LOG_PRIORITY_ERROR  ? 'E'
                       : priority == SDL_LOG_PRIORITY_WARN ? 'W'
                       : priority == SDL_LOG_PRIORITY_INFO ? 'I'
                                                           : 'D';
    std::fprintf(stderr, "%9.3f %c %s\n", double(SDL_GetTicks()) / 1000.0, level, message);
}

} // namespace
#endif

bool start_log_file(const std::string& dir) {
#if defined(SDL_PLATFORM_TVOS)
    if (dir.empty()) return false;
    SDL_CreateDirectory(dir.c_str());
    const std::string path = dir + "log.txt";
    std::rename(path.c_str(), (dir + "log.prev.txt").c_str()); // no earlier run: nothing to keep
    const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return false;
    std::fflush(stdout);
    std::fflush(stderr);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    close(fd);
    // A line at a time: the file is whole up to the last line when the system ends the app.
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    std::setvbuf(stderr, nullptr, _IOLBF, 0);
    SDL_SetLogOutputFunction(write_line, nullptr);
    return true;
#else
    (void)dir;
    return false;
#endif
}

} // namespace gt3r::platform
