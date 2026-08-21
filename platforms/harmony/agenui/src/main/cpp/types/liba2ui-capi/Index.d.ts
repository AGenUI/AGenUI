/**
 * Surface lifecycle listener.
 */
export interface ISurfaceListener {
  /**
   * Called when a surface is created.
   */
  onCreateSurface(surfaceId: string, messageId: string, rawProtocolContent: string): void;

  /**
   * Called when a surface is destroyed.
   */
  onDeleteSurface(surfaceId: string): void;
  
  /**
   * Optional callback routed from component actions.
   */
  onActionEventRouted?: (content: string) => void;

  /**
   * Called when the engine rejects a payload. surfaceId is empty when the
   * error cannot be bound to any Surface. Code definitions: agenui_errorcode_define.h.
   */
  onError?: (code: number, surfaceId: string, message: string) => void;

  /**
   * Called when a component appears in a container (e.g., horizontal List item bound to viewport).
   * Bind == Display: fired when the NodeAdapter binds a child, no viewport intersection test.
   * Recycled items fire again when re-bound.
   * @param properties JSON string of child's raw properties (use JSON.parse to access)
   */
  onComponentAppeared?: (surfaceId: string, parentComponentId: string, parentType: string, properties: string) => void;

  /**
   * Called when the root component's properties are updated (e.g. trackInfo arrives).
   * @param surfaceId Surface identifier
   * @param props JSON string of root component properties
   */
  onRootComponentUpdate?: (surfaceId: string, props: string) => void;

  /**
   * Called after receiveTextChunk completes when the C++ yoga layout has computed a
   * non-zero content height. Used to break the height-zero deadlock.
   */
  onContentSizeChanged?: (surfaceId: string, width: number, height: number) => void;
}

/**
 * Async image loader callback.
 */
export interface ImageLoaderCallback {
  /**
   * Loads an image and returns a local path or base64 payload.
   */
  (url: string): Promise<string>;
}

/** Starts the AGenUI engine. */
export const start: (logger?: object) => void;

/** Stops the AGenUI engine and all SurfaceManager instances. */
export const stop: () => void;

/**
 * Sets the minimum log level forwarded to the C++ engine.
 * @param level 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR, 4=FATAL, 5=PERFORMANCE.
 */
export const setMinLogLevel: (level: number) => void;

/** Registers the default theme and DesignToken configuration. */
export const registerDefaultTheme: (theme: string, designToken: string) => boolean;

/** Sets the day/night mode. */
export const setDayNightMode: (mode: string) => void;

/** Sets whether the host app is a debug build. Defaults to false. */
export const setDebug: (isDebug: boolean) => void;

/** Gets whether the host app is a debug build. Returns false if never set. */
export const isDebug: () => boolean;

/** Registers a custom component factory. */
export const registerComponent: (type: string, creator: (nodeId: string, props: object) => object) => void;

/** Returns the AGenUI SDK version. */
export const getVersion: () => string;

/** Creates a SurfaceManager instance. */
export const createSurfaceManager: () => number;

/** Destroys a SurfaceManager instance. */
export const destroySurfaceManager: (instanceId: number) => void;

/** Sends mock data to the engine. */
export const sendMockData: (mockData: string) => void;

/** Sets path configuration. */
export const setPathConfig: (configJson: string) => boolean;

/**
 * Removes an event listener.
 * @deprecated Use unregisterA2UISurfaceListener instead.
 */
export const removeEventListener: (listener: object) => void;

/** Requests a surface using streamed event data. */
export const requestSurface: (instanceId: number, requestContent: string) => void;

/** Registers a surface listener. */
export const registerA2UISurfaceListener: (instanceId: number, listener: ISurfaceListener) => void;

/** Unregisters a surface listener. */
export const unregisterA2UISurfaceListener: (instanceId: number, listener: ISurfaceListener) => void;

/** Binds a surface to a NodeContent object. */
export const bindSurface: (instanceId: number, surfaceId: string, nodeContent: object) => boolean;

/** Unbinds a surface. */
export const unbindSurface: (instanceId: number, surfaceId: string) => boolean;

/** Clears the A2UI container. */
export const clearA2UiContainer: (instanceId: number) => void;

/** Registers the open-url callback. */
export const registerOpenUrlCallback: (callback: (url: string) => void) => void;

/** Registers the skill invoker callback. */
export const registerSkillInvokerCallback: (callback: (skillName: string, argsJson: string) => string) => void;

/** Registers an ETS function. */
export const registerEtsFunction: (name: string, f: Function) => void;

/** Sets device screen metrics. */
export const setDeviceInfo: (width: number, height: number, density: number) => void;

/** Reads a single ComponentState property. */
export const hybridFactoryGetAttribute: (ptr: bigint, key: string) => string;

/** Returns the full ComponentState property snapshot as JSON. */
export const hybridFactoryGetPropertiesJson: (ptr: bigint) => string;

/** Reports the rendered size of a component to the engine. Supports Markdown, Web, and other custom components. */
export const reportComponentRenderSize: (instanceId: number, surfaceId: string, nodeId: string, type: string, height: number, width: number, ptr: bigint) => void;

/** Measurement result returned by a component measurement callback. */
export interface MeasureResult {
  width: number;
  height: number;
  calcType?: number;  // 0=Sync (default), 1=Async
}

/** Registers an ETS measurement callback for a given component type. */
export const registerMeasurement: (instanceId: number, type: string, callback: (paramJson: string, widthMode: number, maxWidth: number, heightMode: number, maxHeight: number) => MeasureResult) => void;

/** Unregisters an ETS measurement callback for a given component type. */
export const unregisterMeasurement: (instanceId: number, type: string) => void;

/** Notifies the native layer that the surface size changed. */
export const onSurfaceSizeChanged: (instanceId: number, surfaceId: string, width: number, height: number) => void;

/**
 * Sets the legacy theme config.
 * @deprecated Use registerDefaultTheme instead.
 */
export const setThemeConfig: (config: string) => boolean;

/**
 * Sets the legacy DesignToken config.
 * @deprecated Use registerDefaultTheme instead.
 */
export const setDesignTokenConfig: (config: string) => boolean;

/** Registers a platform function with per-skill configuration and callback. */
export const registerFunction: (name: string, config: string, callback: (context: { instanceId: number; surfaceId: string }, paramsJson: string) => string) => void;

/** Unregisters a platform function. */
export const unregisterFunction: (name: string) => void;

/**
 * Declares a component property as a container of dynamic values, so the parser
 * descends into it and resolves nested {path:} bindings and {call:} function calls
 * at any depth. Must be called before the first render.
 */
export const registerDeepParseProperty: (componentType: string, propertyName: string) => boolean;

/** Sets the theme mode. */
export const setThemeMode: (mode: string) => void;

/** Forwards a UI action to the surface manager. */
export const submitUIAction: (instanceId: number, surfaceId: string, sourceComponentId: string, contextJson: string) => void;

/** Forwards UI data model changes to the surface manager. */
export const submitUIDataModel: (instanceId: number, surfaceId: string, componentId: string, change: string) => void;

/** Destroys the specified surface. */
export const destroySurface: (instanceId: number, surfaceId: string) => void;

/** Forwards raw A2UI protocol data. */
export const receiveTextChunk: (instanceId: number, data: string) => void;

/** Starts a streamed text session. */
export const beginTextStream: (instanceId: number) => void;

/**
 * Ends a streamed text session and resets parser state.
 * Call this after normal close, response end, user abort, or network disconnect cleanup.
 */
export const endTextStream: (instanceId: number) => void;

/** Registers the ETS image loader object. */
export const registerImageLoader: (loader: object) => void;

/** Applies raw image pixel data to the matching ArkUI image node. */
export const setImagePixelMap: (requestId: string, buffer: ArrayBuffer, width: number, height: number, pixelFormat: number, alphaType: number) => void;

/** Applies a native PixelMap object to the matching ArkUI image node. */
export const setImagePixelMapNative: (requestId: string, pixelMap: object) => void;

/** Reports image load failure or cancellation from ETS. */
export const onImageLoadFailed: (requestId: string, isCancelled: boolean) => void;

/** Registers a custom font from a file path. The familyName becomes available for font-family CSS resolution. */
export const registerFont: (familyName: string, filePath: string) => boolean;

/** Registers a custom font from a raw buffer. Passes bytes directly to OH_Drawing_RegisterFontBuffer. */
export const registerFontFromBuffer: (familyName: string, buffer: ArrayBuffer) => boolean;

/** Re-evaluates host-backed function call values for all bound components in the instance. */
export const invalidateFunctionCallValues: (instanceId: number) => void;

/** Starts blank-screen detection on a specific surface with the given delay and component count threshold. */
export const surfaceStartBlankCheck: (instanceId: number, surfaceId: string, delayMs: number, minComponentCount: number) => void;

/** Cancels pending blank-screen detection on a specific surface. */
export const surfaceCancelBlankCheck: (instanceId: number, surfaceId: string) => void;

/**
 * Parses a CSS color string (hex 3/4/6/8, rgb/rgba, hsl/hsla, hwb, named colors)
 * via the shared C++ ColorParser. Returns the ARGB uint32 for a legal solid
 * color -- including explicit transparent ('transparent' / rgba(0,0,0,0)),
 * which yields 0, and 'currentcolor', which yields its solid placeholder
 * (0xFF000000, aligned with iOS/Android). Returns undefined only on parse
 * failure or gradient input, so callers can distinguish "unresolvable" from
 * a legal explicit transparent.
 */
export const parseColor: (cssValue: string) => number | undefined;

/** One side of a parsed CSS edge-insets shorthand. Mirrors core `agenui::EdgeInsetValue`. */
export interface EdgeInsetSideValue {
  value: number;      // raw number; 0 when unit is auto
  unit: number;       // 0=PX,1=PERCENT,2=EM,3=REM,4=VW,5=VH,6=VMIN,7=VMAX,8=CM,9=MM,10=IN,11=PT,12=PC,13=AUTO
  isCalc: boolean;
  calcExpr?: string;  // present only when isCalc===true
}

/** Parsed CSS edge-insets shorthand, expanded to four sides in CSS order. */
export interface EdgeInsetsValue {
  top: EdgeInsetSideValue;
  right: EdgeInsetSideValue;
  bottom: EdgeInsetSideValue;
  left: EdgeInsetSideValue;
}

/**
 * Parses a CSS edge-insets shorthand ("10px", "10px 20px", "10px 20px 30px",
 * "10px 20% auto calc(1px + 2%)") with the standard 1/2/3/4-value expansion, via the
 * shared C++ EdgeInsetsParser -- the same parser Android reaches through JNI and iOS
 * links directly, so the grammar is identical on all three platforms.
 * Returns null when the value cannot be parsed.
 */
export const parseEdgeInsets: (cssValue: string) => EdgeInsetsValue | null;
