#pragma once

#include <atomic>

namespace agenui {

enum class BuildType {
    Release = 0,
    Debug = 1
};

void setBuildTypeInternal(BuildType type);

BuildType getBuildType();

inline bool isDebugBuild() { return getBuildType() == BuildType::Debug; }

} // namespace agenui
