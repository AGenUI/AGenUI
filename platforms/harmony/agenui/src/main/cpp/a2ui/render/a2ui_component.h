#pragma once

#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>
#include "a2ui_component_types.h"
#include "a2ui_node.h"
#include "a2ui_component_state.h"
#include "a2ui/a2ui_component_render_observable.h"

namespace a2ui {

class ComponentState;

/**
 * Component base class (refactored)
 * 
 * Refactoring highlights:
 * 1. Add ComponentState support to separate the data layer
 * 2. Add the incremental update method updateView()
 * 3. Keep compatibility with the original interfaces
 * 
 * Cross-platform field mapping:
 *   String id                      -> std::string m_id
 *   String componentType           -> std::string m_componentType
 *   Map<String, Object> properties -> ComponentState::m_properties (data layer)
 *   View view                      -> ArkUI_NodeHandle m_nodeHandle (native Harmony UI node)
 *   A2UIComponent parent           -> A2UIComponent* m_parent
 *   List<A2UIComponent> children   -> std::vector<A2UIComponent*> m_children
 *   String surfaceId               -> std::string m_surfaceId
 */
class A2UIComponent {
public:
    A2UIComponent(const std::string& id, const std::string& componentType);
    virtual ~A2UIComponent();

    A2UIComponent(const A2UIComponent&) = delete;
    A2UIComponent& operator=(const A2UIComponent&) = delete;
    
    // ---- Basic information (unchanged)----
    const std::string& getId() const {
        return m_id;
    }
    
    const std::string& getComponentType() const {
        return m_componentType;
    }
    
    /**
     * Get properties (compatibility API)
     * Prefer State when present; otherwise fall back to local m_properties.
     */
    const nlohmann::json& getProperties() const;
    
    A2UIComponent* getParent() const {
        return m_parent;
    }

     A2UINode getNode() {
        return A2UINode(m_nodeHandle);
     }
    
    const std::vector<A2UIComponent*>& getChildren() const {
        return m_children;
    };
    
    const std::string& getSurfaceId() const {
        return m_surfaceId;
    };
    
    ArkUI_NodeHandle getNodeHandle() const {
        return m_nodeHandle;
    };
    
    void setSurfaceId(const std::string& surfaceId) {
        m_surfaceId = surfaceId;
        if (m_state) {
            m_state->setSurfaceId(surfaceId);
        }
    }
    
    // ---- State binding (new)----
    void setState(ComponentState* state) { m_state = state; }
    ComponentState* getState() const { return m_state; }
    
    // ---- Property updates (refactored)----
    
    /**
     * Update properties (compatibility API)
     * If State exists, update State and trigger an incremental refresh
     * If State does not exist, fall back to the original full refresh logic
     */
    void updateProperties(const nlohmann::json& newProps);

    /**
     * @brief Update layout-related properties (new)
     * 
     * @param newProps 
     */
    void updateLayoutProperties(const nlohmann::json& newProps);
    
    /**
     * Incrementally update the view (new)
     * Only update properties marked dirty in State
     */
    virtual void updateView();
    
    // ---- Parent-child relationship (unchanged)----
    virtual void addChild(A2UIComponent* child);
    virtual void removeChild(A2UIComponent* child);
    void removeChildById(const std::string& childId);
    
    // ---- Event dispatch (new, aligned with the cross-platform A2UIComponent.handleClick)----
    
    /**
     * Dispatch the Action event to the SDK layer
     * Matches the logic in the cross-platform A2UIComponent.handleClick():
     *   Build {"action": actionValue} JSON and dispatch ActionMessage through EventDispatcher
     * 
     * @param actionDef Value of the action property (JSON object)
     */
    void dispatchAction(const nlohmann::json& actionDef);

    /**
     * Notify that this component appeared in a container (e.g., List item bound to viewport).
     * Dispatches onComponentAppeared to ArkTS listeners via A2UIMessageListener.
     *
     * @param parentType Container type name (e.g., "List")
     * @param properties Child's raw properties JSON
     */
    void notifyAppeared(const std::string& parentType, const nlohmann::json& properties);

    /**
     * Synchronize UI state changes back to the data model.
     * Mirrors the cross-platform syncState helper used by interactive components
     * (TextField, ChoicePicker, Slider, etc).
     *
     * @param changeJson Changed content, e.g. {"value": "kotlin"}
     */
    void syncState(const nlohmann::json& changeJson);

    /**
     * Set the click listener (aligned with the cross-platform setupClickListener)
     * Register or unregister click events automatically based on the action property
     */
    void setupClickListener();
    
    /**
     * Check whether clicks are disabled (subclasses may override)
     * Mirrors the disabled behavior in the cross-platform ButtonComponent.
     */
    virtual bool isClickDisabled() const { return false; }
    
    // ---- Lifecycle ----
    void destroy();
    virtual bool shouldAutoAddChildView() const;
    virtual bool shouldApplyChildLayoutPosition(const A2UIComponent* child) const;
    /**
     * Whether to apply the child width and height computed by the parent container
     * to the child's ArkUI node.
     * Returns true by default. Containers such as Tabs can return false to let
     * the child size be governed by ArkUI flex layout instead.
     */
    virtual bool shouldApplyChildLayoutSize(const A2UIComponent* child) const;

    /**
     * Called when the framework is about to apply a child's Yoga-computed position.
     * The default implementation calls child->getNode().setPosition(x, y).
     * Containers (e.g. List) may override this to apply only the relevant axis
     * (e.g. List applies x for cross-axis centering but resets y to 0 so ArkUI
     * ListItem stacking is not disrupted).
     */
    virtual void onApplyChildPosition(A2UIComponent* child, float x, float y) {
        child->getNode().setPosition(x, y);
    }
    /**
     * Called after a child's Yoga-computed size has been applied to its ArkUI node.
     * Containers (e.g. List) may override this to propagate the size to wrapper
     * nodes such as ListItem.
     */
    virtual void onChildLayoutSizeChanged(A2UIComponent* /*child*/) {}
    /**
     * Called when a child component is mounted into this container.
     * Override in containers that need post-mount notification (e.g. Tabs, Modal).
     */
    virtual void onChildMounted(A2UIComponent* /*child*/) {}
    bool hasPendingAppearAnimation() const { return m_pendingAppearAnimation; }
    void prepareAppearAnimation(const nlohmann::json& properties);
    void playAppearAnimationIfNeeded();

    /**
     * Inject the animation flag from the owning surface (called by A2UISurface during addComponent)
     *   - Surface animated=false -> disable animations for all components on the surface
     *   - Surface animated=true  -> allow components on the surface to animate normally
     */
    void setSurfaceAnimated(bool animated) { m_surfaceAnimated = animated; }
    bool isSurfaceAnimated() const { return m_surfaceAnimated; }

    /**
     * Inject the component render completion observer from the owning surface.
     */
    void setComponentRenderObservable(agenui::IComponentRenderObservable* observable) { m_componentRenderObservable = observable; }
    agenui::IComponentRenderObservable* getComponentRenderObservable() const { return m_componentRenderObservable; }

protected:
    /**
     * Shared action click callback (static function)
     * Mirrors the cross-platform setupClickListener with view.setOnClickListener(v -> handleClick()).
     */
    static void onActionClickCallback(ArkUI_NodeEvent* event);

    /**
     * Parse color strings (aligned with the cross-platform A2UIUtils.parseColor)
     * Supported formats:
     *   #RRGGBB        -> 0xFFRRGGBB (opaque)
     *   #RRGGBBAA      -> 0xAARRGGBB (alpha included, bytes reordered)
     *   rgba(r,g,b,a)  -> a is a 0.0-1.0 float
     *   rgb(r,g,b)     -> opaque
     * Return 0x00000000 on parse failure (transparent, aligned with the cross-platform Color.TRANSPARENT)
     */
    static uint32_t parseColor(const std::string& colorStr);

    /**
     * Parse a color value that may be a FunctionCall token reference.
     * Supported formats:
     *   1. String: Direct color value (e.g., "#FFFFFF", "#RRGGBBAA")
     *      -> Parsed directly by parseColor()
     *   2. JSON object with {"call": "token", "args": {"name": "TokenName"}}
     *      -> Extracts token name, resolves via TokenParser, then parsed by parseColor()
     * Return fallbackValue on parse failure or empty string.
     */
    static uint32_t parseColorWithToken(const nlohmann::json& colorValue, uint32_t fallbackValue);

public:
    float getX() const { return m_x; }
    float getY() const { return m_y; }
    float getWidth() const { return m_width; }
    float getHeight() const { return m_height; }

protected:
    void setHeight(float height);
    
    // Saved raw style information for width and height.
    const std::string& getStyleInfo() const { return m_styleInfo; }


protected:
    /**
     * Subclass resource-release hook, called by destroy() before children are
     * recursively destroyed and before the ArkUI node is disposed.
     * Subclasses override this to release their own resources (players, timers,
     * internal ArkUI nodes, etc.).
     */
    virtual void onDestroy() {}

    /**
     * Single-property update hook (new)
     * Subclasses override this method to implement incremental updates for specific properties
     */
    virtual void onUpdateProperty(const std::string& key, const nlohmann::json& value);
    
    /**
     * Full-property update hook (retained)
     * Used for initialization or compatibility with legacy logic
     */
    virtual void onUpdateProperties(const nlohmann::json& properties);
    
    /**
     * Apply the background-image style
     * Create an IMAGE child node under the component node, with the same size and the lowest z-index
     * @param styles Style JSON object
     */
    void applyBackgroundImage(const nlohmann::json& styles);

    /**
     * Apply the visibility style.
     *   visibility: hidden  -> ARKUI_VISIBILITY_HIDDEN (invisible but still occupies layout space)
     *   visibility: visible -> ARKUI_VISIBILITY_VISIBLE (default)
     * Mirrors Android StyleHelper.applyDisplay's visibility branch and CSS semantics.
     * No-op when the "visibility" key is absent so previous values are preserved.
     * @param styles Style JSON object
     */
    void applyVisibility(const nlohmann::json& styles);
    
    /**
     * Apply background-color style
     * Parse and apply background color from styles
     * @param properties Properties JSON object (contains "styles" field)
     */
    void applyBackgroundColor(const nlohmann::json& properties);
    
    /**
     * Apply border styles uniformly
     * Parse and apply border-radius, border-width, border-color from styles
     * @param properties Properties JSON object (contains "styles" field)
     */
    void applyBorderStyles(const nlohmann::json& properties);
    
    virtual float resolveAppearTargetOpacity(const nlohmann::json& properties) const;
    
private:
    // Layout-related information is stored here temporarily and will be consolidated into State later
    float m_x = 0;
    float m_y = 0;
    float m_width = 0;
    float m_height = 0;
    std::string m_styleInfo;  // Stored raw style information (width/height values)
    
protected:
    std::string m_id;
    std::string m_componentType;
    nlohmann::json m_properties;  // Retained for compatibility (used when State is unavailable)
    std::vector<A2UIComponent*> m_children;
    std::string m_surfaceId;
    
    ComponentState* m_state = nullptr;  // New: bound state
    A2UIComponent* m_parent = nullptr;
    ArkUI_NodeHandle m_nodeHandle = nullptr;
    bool m_actionClickRegistered = false;  // Whether the shared action click event is registered
    
    // background-image node (first child, lowest z-index)
    ArkUI_NodeHandle m_backgroundImageHandle = nullptr;
    std::string m_backgroundImageUrl;  // Store the current background image URL to avoid redundant updates
    std::string m_backgroundImageRequestId;  // Current external loader request ID (empty when unused)
    bool m_pendingAppearAnimation = false;
    bool m_hasPlayedAppearAnimation = false;
    float m_appearTargetOpacity = 1.0f;
    bool m_surfaceAnimated = true;  // Animation flag of the owning surface (injected by A2UISurface)
    agenui::IComponentRenderObservable* m_componentRenderObservable = nullptr;  // Component render completion observer (injected by A2UISurface, non-owning)
};

} // namespace a2ui
