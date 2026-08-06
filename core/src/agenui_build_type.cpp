#include "agenui_build_type.h"

namespace agenui {

static std::atomic<BuildType> g_buildType{BuildType::Release};

void setBuildTypeInternal(BuildType type) {
    g_buildType.store(type, std::memory_order_relaxed);
}

BuildType getBuildType() {
    return g_buildType.load(std::memory_order_relaxed);
}

} // namespace agenui
