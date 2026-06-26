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
export const reportComponentRenderSize: (surfaceId: string, nodeId: string, type: string, height: number, width: number, ptr: bigint) => void;

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
export const onSurfaceSizeChanged: (surfaceId: string, width: number, height: number) => void;

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

/** Reports image load failure or cancellation from ETS. */
export const onImageLoadFailed: (requestId: string, isCancelled: boolean) => void;

/** Looks up the instanceId corresponding to the given surfaceId. Returns 0 if not found. */
export const findInstanceIdBySurfaceId: (surfaceId: string) => number;
