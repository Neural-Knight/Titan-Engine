#pragma once

#include <thread>

namespace titan {

// True if pinned. False and no-op on platforms without a pinning API.
bool pinCurrentThreadToCpu(int cpu);
bool pinThread(std::thread& thread, int cpu);

}  // namespace titan
