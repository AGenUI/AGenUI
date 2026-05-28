#pragma once

#include <string>

namespace agenui {

/**
 * @brief Size of a surface in a2ui virtual-pixel (vp) units.
 */
struct SurfaceSize {
    float width  = 0.0f;
    float height = 0.0f;
};

/**
 * @brief Host-supplied source of truth for per-surface size.
 *
 * Injected into ISurfaceManager via setSurfaceSizeProvider(). The engine
 * queries it synchronously on demand when its locally cached size for a
 * given surface has never been initialized (neither by an
 * onSurfaceSizeChanged push nor by a prior provider query).
 *
 * Contract:
 * - getSurfaceSize() must be safe to call synchronously from the engine
 *   worker thread (the same thread that delivers onSurfaceSizeChanged).
 * - Returning {0, 0} (or any non-positive component) is treated as
 *   "not measurable yet"; the engine will skip the in-flight layout pass
 *   and replay it once a positive size arrives via either channel.
 *
 * Lifetime:
 * - The provider is owned by the host; the engine only holds a non-owning
 *   pointer. The host must keep the provider alive at least until the
 *   owning ISurfaceManager is destroyed (or until a subsequent
 *   setSurfaceSizeProvider(nullptr) call detaches it).
 */
class ISurfaceSizeProvider {
public:
    virtual ~ISurfaceSizeProvider() = default;

    /**
     * @brief Return the current size of the given surface.
     * @param surfaceId Surface identifier assigned by the engine.
     * @return Surface size in vp; {0, 0} if not measurable yet.
     */
    virtual SurfaceSize getSurfaceSize(const std::string& surfaceId) = 0;
};

}  // namespace agenui
