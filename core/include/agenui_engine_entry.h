#pragma once

#include "agenui_engine.h"
#include "agenui_version.h"

namespace agenui {

/**
 * @brief Gets the AGenUI SDK version string
 * @return Version string
 */
inline const char* getAGenUIVersion() {
    return AGENUI_VERSION;
}

/**
 * @brief Creates and initializes an AGenUI Engine instance
 * @return Engine instance pointer. Only one instance exists
 */
IAGenUIEngine* initAGenUIEngine();

/**
 * @brief Gets the created AGenUI Engine instance
 * @return Engine instance pointer, nullptr if not created
 */
IAGenUIEngine* getAGenUIEngine();

/**
 * @brief Destroys the AGenUI Engine instance
 */
void destroyAGenUIEngine();

} // namespace agenui
