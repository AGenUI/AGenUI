#pragma once
#include <string>

namespace agenui {
class IDataModel;

/**
 * @brief Surface context interface
 *
 * Provides access to surface-level context information (instance ID, surface ID, data model).
 * Implemented by Surface to pass context down to ComponentManager and ComponentModel.
 */
class ISurfaceContext {
public:
    virtual ~ISurfaceContext() = default;
    virtual int getInstanceId() const = 0;
    virtual std::string getSurfaceId() const = 0;
    virtual IDataModel* getDataModel() const = 0;

    /**
     * @brief Return the current surface width in vp.
     *
     * Not const: the implementation may perform a synchronous pull from the
     * host-supplied ISurfaceSizeProvider and cache the result on first call
     * (before any onSurfaceSizeChanged push has arrived).
     *
     * @return Width in vp; 0 if no push has been received yet and the
     *         provider also reports no measurable size.
     */
    virtual float getSurfaceWidth() = 0;

    /**
     * @brief Return the current surface height in vp.
     *
     * Same lazy-fetch semantics as getSurfaceWidth().
     */
    virtual float getSurfaceHeight() = 0;
};
} // namespace agenui
