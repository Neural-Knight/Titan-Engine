#include "titan/platform/thread_affinity.hpp"

#ifdef __linux__
#include <pthread.h>
#endif

namespace titan {

#ifdef __linux__
namespace {
bool pinHandleToCpu(pthread_t handle, int cpu)
{
    if (cpu < 0 || cpu >= CPU_SETSIZE)
        return false;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    return pthread_setaffinity_np(handle, sizeof(cpu_set_t), &cpuset) == 0;
}
}  // namespace

bool pinCurrentThreadToCpu(int cpu)
{
    return pinHandleToCpu(pthread_self(), cpu);
}

bool pinThread(std::thread& thread, int cpu)
{
    return pinHandleToCpu(thread.native_handle(), cpu);
}
#else
bool pinCurrentThreadToCpu(int)
{
    return false;  // not available on this platform
}

bool pinThread(std::thread&, int)
{
    return false;  // not available on this platform
}
#endif

}  // namespace titan
