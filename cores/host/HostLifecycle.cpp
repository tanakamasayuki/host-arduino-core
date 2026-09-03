#include "HostLifecycle.h"

namespace HostArduino {
namespace {

LifecycleHook lifecycle_hook = nullptr;
void *lifecycle_hook_user = nullptr;
uint64_t loop_count = 0;

} // namespace

void setLifecycleHook(LifecycleHook hook, void *user)
{
    lifecycle_hook = hook;
    lifecycle_hook_user = user;
}

void clearLifecycleHook()
{
    lifecycle_hook = nullptr;
    lifecycle_hook_user = nullptr;
}

uint64_t loopCount()
{
    return loop_count;
}

namespace lifecycle_detail {

void announce(LifecyclePhase phase)
{
    // Counted before the hook runs, so a driver reading `loopCount()`
    // from `kPostLoop` sees the iteration it is closing out rather than
    // the one before it.
    if (phase == kPostLoop) {
        ++loop_count;
    }
    if (lifecycle_hook) {
        lifecycle_hook(phase, lifecycle_hook_user);
    }
}

} // namespace lifecycle_detail

} // namespace HostArduino
