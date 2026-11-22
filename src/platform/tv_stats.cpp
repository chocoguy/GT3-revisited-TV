#include "platform/tv_stats.h"

#include "base/log.h"

#include <SDL3/SDL_platform_defines.h>

#if defined(SDL_PLATFORM_TVOS)
#include <cmath>
#include <mach/mach.h>
#include <objc/message.h>
#include <objc/runtime.h>
#include <os/proc.h>
#endif

namespace gt3r::platform {

#if defined(SDL_PLATFORM_TVOS)
namespace {

constexpr unsigned long long kPeriodMs = 5000;

// The footprint tvOS holds against its limit (phys_footprint), in MB; 0 when it cannot be read.
double footprint_mb() {
    task_vm_info_data_t info{};
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO, reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS) return 0;
    return double(info.phys_footprint) / (1024.0 * 1024.0);
}

// NSProcessInfo.thermalState: 0 nominal, 1 fair, 2 serious (the clocks come down), 3 critical.
int thermal_state() {
    id info = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend)(reinterpret_cast<id>(objc_getClass("NSProcessInfo")),
                                                             sel_registerName("processInfo"));
    return info ? int(reinterpret_cast<long (*)(id, SEL)>(objc_msgSend)(info, sel_registerName("thermalState"))) : 0;
}

const char* thermal_name(int state) {
    static const char* const kNames[] = {"nominal", "fair", "serious", "critical"};
    return state >= 0 && state < 4 ? kNames[state] : "unknown";
}

} // namespace

void stats_frame(unsigned long long now_ms) {
    static unsigned long long period_start = now_ms, last_frame = now_ms;
    static int frames = 0, thermal = -1;
    static double longest_ms = 0, logged_fps = -1, logged_footprint = -1;
    ++frames;
    longest_ms = std::fmax(longest_ms, double(now_ms - last_frame));
    last_frame = now_ms;
    if (frames % 30 == 0 || thermal < 0) { // (about twice a second)
        if (const int t = thermal_state(); t != thermal) {
            LOG_INFO("Thermal state: %s%s", thermal_name(t), thermal < 0 ? "" : t > thermal ? " (up)" : " (down)");
            thermal = t;
        }
    }
    if (now_ms - period_start < kPeriodMs) return;
    const double fps = double(frames) * 1000.0 / double(now_ms - period_start), footprint = footprint_mb();
    // An idle menu repeats itself: only a line that says something new is written.
    if (std::fabs(fps - logged_fps) >= 2 || std::fabs(footprint - logged_footprint) >= 8 || longest_ms >= 100) {
        LOG_INFO("Stats: %.0f fps (longest frame %.0f ms), footprint %.0f MB, %.0f MB left, thermal %s", fps, longest_ms,
                 footprint, double(os_proc_available_memory()) / (1024.0 * 1024.0), thermal_name(thermal));
        logged_fps = fps;
        logged_footprint = footprint;
    }
    period_start = now_ms;
    frames = 0;
    longest_ms = 0;
}

#else

void stats_frame(unsigned long long) {}

#endif

} // namespace gt3r::platform
