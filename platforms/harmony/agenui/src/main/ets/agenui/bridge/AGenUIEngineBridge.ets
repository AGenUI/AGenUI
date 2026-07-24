/**
 * AGenUI Engine Bridge (global singleton)
 *
 * Owns the global IAGenUIEngine lifecycle and engine-level configuration,
 * including theme, design tokens, day/night mode, skill and function
 * registration, and device information.
 *
 * Use AGenUIEngineSurfaceManagerBridge for per-instance data transport.
 */

import display from '@ohos.display';
import {
  start,
  setPathConfig as nativeSetPathConfig,
  setDeviceInfo,
  setThemeConfig,
  setDesignTokenConfig,
  setDayNightMode,
  registerImageLoader as nativeRegisterImageLoader,
  registerFont as nativeRegisterFont,
  registerFontFromBuffer as nativeRegisterFontFromBuffer,
  setImagePixelMap,
  setImagePixelMapNative,
  onImageLoadFailed,
  createSurfaceManager as nativeCreateSurfaceManager,
  destroySurfaceManager as nativeDestroySurfaceManager,
  registerEtsFunction,
} from 'liba2ui-capi.so';

import internal_a2ui from 'liba2ui-capi.so';

import { common, OpenLinkOptions } from '@kit.AbilityKit';
import { BusinessError } from '@kit.BasicServicesKit';
import { hilog } from '@kit.PerformanceAnalysisKit';

import {
  IImageLoader,
  ImageLoadResult,
  ImageLoaderError,
} from '../ImageLoader';
import {
  createHybridView,
  updateHybridView,
  destroyHybridView,
  onInvokeHybridView,
  HybridViewManager,
} from '../hybrid/HybridViewManager';
import { AGenUILogger } from '../logger/AGenUILogger';

// ==================== Internal Image Loader State ====================

/** Loader wrapper contract used by the C++ bridge layer. */
interface WrappedImageLoader {
  loadImage: (url: string, options: Record<string, Object> | null, requestId: string) => string;
  cancel: (requestId: string) => void;
  clearCache: () => void;
}

/** Globally registered image loader instance. */
let g_imageLoader: IImageLoader | null = null;

// ==================== AGenUIEngineBridge ====================

/**
 * Global AGenUI engine bridge.
 *
 * - Performs one-time global initialization
 * - Applies theme, design token, and day/night configuration
 * - Registers skills, function calls, and image loaders
 * - Creates C++ ISurfaceManager instances for SurfaceManager
 */
export class AGenUIEngineBridge {
  private static _instance: AGenUIEngineBridge | null = null;
  private static _initialized: boolean = false;

  private constructor() {}

  /**
   * Returns the global singleton.
   */
  static getInstance(): AGenUIEngineBridge {
    if (AGenUIEngineBridge._instance === null) {
      AGenUIEngineBridge._instance = new AGenUIEngineBridge();
    }
    return AGenUIEngineBridge._instance;
  }

  // MARK: - Global Initialization

  /**
   * Initializes the SDK once.
   *
   * Registers HybridView ETS callbacks, URL hooks, and device information,
   * then starts the engine.
   *
   * @param context UIAbilityContext
   */
  initialize(context: common.UIAbilityContext): void {
    if (AGenUIEngineBridge._initialized) {
      return;
    }
    AGenUIEngineBridge._initialized = true;

    // Register built-in HybridView ETS callbacks.
    registerEtsFunction('createHybridView', createHybridView);
    registerEtsFunction('updateHybridView', updateHybridView);
    registerEtsFunction('destroyHybridView', destroyHybridView);
    registerEtsFunction('onInvokeHybridView', onInvokeHybridView);
    registerEtsFunction('hasCustomComponent', (componentType: string): boolean => {
      return HybridViewManager.getInstance().hasViewCreator(componentType);
    });

    // Register the URL open callback.
    internal_a2ui.registerOpenUrlCallback((url: string) => {
      try {
        const openLinkOptions: OpenLinkOptions = { appLinkingOnly: false };
        context.openLink(url, openLinkOptions).then(() => {
        }).catch((error: BusinessError<void>) => {
          hilog.error(0x0000, 'AGenUIBridge', 'Failed to open URL: %{public}s, error: %{public}s', url, JSON.stringify(error));
        });
      } catch (error) {
        hilog.error(0x0000, 'AGenUIBridge', 'Exception in openUrl callback: %{public}s, url: %{public}s', JSON.stringify(error), url);
      }
    });

    // Device information.
    const display_ = display.getDefaultDisplaySync();
    setDeviceInfo(display_.width, display_.height, display_.densityPixels);

    // Start the engine with logger
    const logger = AGenUILogger.getInstance();
    start(logger);
  }

  // MARK: - C++ ISurfaceManager Factory

  /**
   * Creates a C++ ISurfaceManager instance and returns its instanceId.
   */
  createSurfaceManager(): number {
    return nativeCreateSurfaceManager();
  }

  /**
   * Destroys a C++ ISurfaceManager instance.
   */
  destroySurfaceManager(instanceId: number): void {
    nativeDestroySurfaceManager(instanceId);
  }

  // MARK: - Theme Configuration

  /**
   * Loads theme configuration.
   */
  loadThemeConfig(themeConfigJson: string): boolean {
    return setThemeConfig(themeConfigJson);
  }

  /**
   * Loads design token configuration.
   */
  loadDesignTokenConfig(designTokenConfigJson: string): boolean {
    return setDesignTokenConfig(designTokenConfigJson);
  }

  /**
   * Sets the day/night mode
   */
  setDayNightMode(mode: string): void {
    setDayNightMode(mode);
  }

  /**
   * Sets path configuration
   */
  setPathConfig(configJson: string): boolean {
    return nativeSetPathConfig(configJson);
  }

  // MARK: - Image Loader

  /**
   * Registers a global image loader
   *
   * Wraps IImageLoader in the format expected by the C++ bridge layer.
   */
  registerImageLoader(loader: IImageLoader): void {
    g_imageLoader = loader;

    const wrappedLoader: WrappedImageLoader = {
      loadImage: (url: string, options: Record<string, Object> | null, requestId: string): string => {
        if (g_imageLoader === null) {
          return '';
        }
        g_imageLoader.loadImage(
          url,
          options,
          (_rid: string, result: ImageLoadResult | null, error: ImageLoaderError | null) => {
            // Defensive check: result and error must not both be null simultaneously.
            if (result === null && error === null) {
              hilog.error(0x0000, 'AGenUIBridge', 'loadImage callback received both result and error as null, requestId=%{public}s. Treating as failure.', requestId);
              onImageLoadFailed(requestId, false);
              return;
            }
            if (result !== null && result.image != null) {
              const pd = result.image;
              try {
                const nativePixelMap = pd.pixelMap;
                if (nativePixelMap != null) {
                  setImagePixelMapNative(requestId, nativePixelMap as object);
                  return;
                }
                if (!pd.bytes || pd.bytes.byteLength === 0 || pd.width <= 0 || pd.height <= 0) {
                  hilog.error(0x0000, 'AGenUIBridge', `invalid PixelData: bytes=%{public}s %{public}d x %{public}d`, `${pd.bytes?.byteLength}`, pd.width, pd.height);
                  onImageLoadFailed(requestId, false);
                  return;
                }
                const buf = pd.bytes.buffer.slice(
                  pd.bytes.byteOffset,
                  pd.bytes.byteOffset + pd.bytes.byteLength
                ) as ArrayBuffer;
                const srcFmt: number = pd.pixelFormat as number;
                setImagePixelMap(requestId, buf, pd.width, pd.height, srcFmt, pd.alphaType as number);
              } catch (e) {
                hilog.error(0x0000, 'AGenUIBridge', 'setImagePixelMap failed: %{public}s', `${e}`);
                onImageLoadFailed(requestId, false);
              }
            } else {
              onImageLoadFailed(requestId, error ? error.isCancelled : false);
            }
          }
        );
        return requestId;
      },
      cancel: (requestId: string): void => {
        g_imageLoader?.cancel(requestId);
      },
      clearCache: (): void => {
        g_imageLoader?.clearCache();
      },
    };

    nativeRegisterImageLoader(wrappedLoader as Object);
    hilog.info(0x0000, 'AGenUIBridge', 'Image loader registered successfully');
  }

  // MARK: - Custom Font Registration
  //
  // Both methods below register a font into the OH_Drawing global
  // FontCollection via OH_Drawing_RegisterFontBuffer. This is the only
  // registration AGenUI needs — it covers CAPI text rendering and C++
  // text measurement.
  //
  // OpenHarmony has other FontCollection instances:
  //   - text.FontCollection (loadFontSync) — for ArkUI declarative Text
  //   - ArkUI TxtFontCollection (font.registerFont) — for CAPI NODE_FONT_FAMILY
  // Neither is used here: font.registerFont does not support sandbox paths
  // (silently fails), and loadFontSync only serves ArkUI declarative Text
  // components which AGenUI does not use.
  //
  // Must be called before windowStage.loadContent() for the font to take
  // effect on text nodes created afterwards.

  /**
   * Register a custom font from a sandbox file path.
   *
   * The native layer reads the file into memory and calls
   * OH_Drawing_RegisterFontBuffer.
   *
   * @param familyName The CSS font-family name to reference in A2UI payloads
   * @param path Sandbox absolute path (e.g. "/data/.../files/fonts/MyFont.ttf")
   * @returns true if registration succeeds
   */
  registerFont(familyName: string, path: string): boolean {
    if (!familyName || familyName.trim().length === 0) {
      hilog.error(0x0000, 'AGenUI', 'registerFont: familyName cannot be empty');
      return false;
    }
    if (!path || path.trim().length === 0) {
      hilog.error(0x0000, 'AGenUI', 'registerFont: path cannot be empty');
      return false;
    }

    const nativeOk = nativeRegisterFont(familyName, path);
    if (!nativeOk) {
      hilog.error(0x0000, 'AGenUIBridge',
        'registerFont: native registration failed for "%{public}s"', familyName);
    }
    return nativeOk;
  }

  /**
   * Register a custom font directly from a rawfile path, without copying
   * to the sandbox.
   *
   * Reads font bytes via ResourceManager#getRawFileContentSync and passes
   * them directly to OH_Drawing_RegisterFontBuffer via NAPI — no
   * intermediate file I/O.
   *
   * @param familyName The CSS font-family name to reference in A2UI payloads
   * @param rawfilePath Path relative to rawfile/, e.g. 'fonts/MyFont.ttf'
   * @param context UIAbilityContext for resourceManager access
   * @returns true if registration succeeds
   */
  registerFontFromRawFile(familyName: string, rawfilePath: string, context: common.UIAbilityContext): boolean {
    if (!familyName || familyName.trim().length === 0) {
      hilog.error(0x0000, 'AGenUI', 'registerFontFromRawFile: familyName cannot be empty');
      return false;
    }
    if (!rawfilePath || rawfilePath.trim().length === 0) {
      hilog.error(0x0000, 'AGenUI', 'registerFontFromRawFile: rawfilePath cannot be empty');
      return false;
    }

    // Read font bytes from rawfile (zero file-system copy).
    let data: Uint8Array;
    try {
      data = context.resourceManager.getRawFileContentSync(rawfilePath);
    } catch (e) {
      hilog.error(0x0000, 'AGenUIBridge',
        'registerFontFromRawFile: getRawFileContentSync failed for "%{public}s": %{public}s',
        rawfilePath, String(e));
      return false;
    }

    if (data.byteLength === 0) {
      hilog.error(0x0000, 'AGenUIBridge',
        'registerFontFromRawFile: rawfile is empty "%{public}s"', rawfilePath);
      return false;
    }

    // Register into OH_Drawing global FontCollection via buffer NAPI.
    const nativeOk = nativeRegisterFontFromBuffer(familyName, data.buffer);
    if (!nativeOk) {
      hilog.error(0x0000, 'AGenUIBridge',
        'registerFontFromRawFile: native buffer registration failed for "%{public}s"', familyName);
    }
    return nativeOk;
  }
}
