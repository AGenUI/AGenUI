/*
 * Manager for extensible ArkTS views mounted from the C API.
 */

import { hilog } from '@kit.PerformanceAnalysisKit';
import internal_a2ui from 'liba2ui-capi.so';
import { UpdateState } from './ViewState';
import { HybridInvokeCallback, HybridBaseView, HybridViewCreator, HYBRID_VIEW_TAG } from './HybridBaseView';
import { HybridWebView } from './HybridWebView';

export function createHybridView(viewTag: string, ctxId: string, nodeId: string, viewStatePtr: bigint, instanceId: number): ComponentContent<HybridBaseView> | null {
  hilog.debug(0, HYBRID_VIEW_TAG, "createHybridView(viewTag:%{public}s,ctxId:%{public}s,nodeId:%{public}s,instanceId:%{public}d)", viewTag, ctxId, nodeId, instanceId);
  return HybridViewManager.getInstance().createArkTsView(viewTag, ctxId, nodeId, viewStatePtr, instanceId);
}

export function updateHybridView(instanceId: number, nodeId: string, updateStates: UpdateState[]) {
  hilog.debug(0, HYBRID_VIEW_TAG, "updateHybridView(instanceId:%{public}d,nodeId:%{public}s,updateStatesLength:%{public}d)", instanceId, nodeId, updateStates.length);
  HybridViewManager.getInstance().updateArkTsView(instanceId, nodeId, updateStates);
}

export function destroyHybridView(instanceId: number, nodeId: string) {
  hilog.debug(0, HYBRID_VIEW_TAG, "destroyHybridView(instanceId:%{public}d,nodeId:%{public}s)", instanceId, nodeId);
  HybridViewManager.getInstance().destroyArkTsView(instanceId, nodeId);
}

export function onInvokeHybridView(instanceId: number, nodeId: string, key: string, params: HybridInvokeCallback) {
  hilog.debug(0, HYBRID_VIEW_TAG, "onInvokeHybridView(instanceId:%{public}d,nodeId:%{public}s,key:%{public}s,params:%{public}s)", instanceId, nodeId, key, String(params));
  HybridViewManager.getInstance().onInvokeArkTsView(instanceId, nodeId, key, params);
}

export class HybridViewManager {
  private static viewManagerInstance: HybridViewManager;

  private viewCreatorRegistry = new Map<string, HybridViewCreator>();
  private hybridViews = new Map<string, HybridBaseView>();

  private constructor() {
  }

  public static getInstance(): HybridViewManager {
    if (!HybridViewManager.viewManagerInstance) {
      HybridViewManager.viewManagerInstance = new HybridViewManager();
    }
    return HybridViewManager.viewManagerInstance;
  }

  /**
   * Registry key for a hybrid view.
   *
   * nodeId is the DSL component id and is only unique **within one surface**.
   * Two surfaces (e.g. two <a2ui> tags whose payloads both use surfaceId
   * "default" and a component named "id_checkbox") would otherwise collide on a
   * single Map entry: the second creation overwrites the first, and every later
   * update/destroy/invoke is routed to whichever view registered last. The owning
   * SurfaceManager's instanceId is globally unique, so it disambiguates the key.
   */
  private static hybridKey(instanceId: number, nodeId: string): string {
    return `${instanceId}:${nodeId}`;
  }

  /**
   * Registers an ArkTS-side view factory.
   * @param viewTag View tag
   * @param viewCreator View factory
   */
  public registerViewCreator(viewTag: string, viewCreator: HybridViewCreator) {
    this.viewCreatorRegistry.set(viewTag, viewCreator);
    // Sync the type into the native global component registry so the C++
    // render layer can create this hybrid type. The typeof guard keeps
    // compatibility with older .so builds without this export.
    if (typeof internal_a2ui.registerComponent === 'function') {
      internal_a2ui.registerComponent(viewTag);
    }
  }

  public unregisterViewCreator(viewTag: string) {
    this.viewCreatorRegistry.delete(viewTag);
    if (typeof internal_a2ui.unregisterComponent === 'function') {
      internal_a2ui.unregisterComponent(viewTag);
    }
  }

  public hasViewCreator(viewTag: string): boolean {
    return this.viewCreatorRegistry.has(viewTag);
  }

  /**
   * Creates an ArkTS view.
   * @param viewTag View tag
   * @param ctxId Context ID
   * @param nodeId Node ID
   * @param viewStatePtr Pointer to the matching C++ ViewState
   * @param instanceId SurfaceManager instance ID
   * @return ComponentContent
   */
  public createArkTsView(viewTag: string, ctxId: string, nodeId: string, viewStatePtr: bigint, instanceId: number): ComponentContent<HybridBaseView> | null {
    let hybridView: HybridBaseView | null = null;
    switch (viewTag) {
      case 'Web': { // Built-in Web component
        hybridView = new HybridWebView(viewTag, ctxId, nodeId, viewStatePtr, instanceId);
      }
        break;
    }
    if (hybridView === null) {
      let creator: HybridViewCreator | undefined = this.viewCreatorRegistry.get(viewTag);
      if (creator) {
        hybridView = creator(ctxId, nodeId, viewTag, viewStatePtr, instanceId);
      }
    }
    if (hybridView) {
      let componentContent = hybridView.toCreateArkUIView();
      if (componentContent === null) {
        hilog.error(0, HYBRID_VIEW_TAG, "createArkTsView failed: toCreateArkUIView returned null for viewTag:%{public}s,nodeId:%{public}s", viewTag, nodeId);
        return null;
      }
      this.hybridViews.set(HybridViewManager.hybridKey(instanceId, nodeId), hybridView);
      return componentContent;
    }
    hilog.debug(0, HYBRID_VIEW_TAG, "createHybridView(viewTag:%{public}s,ctxId:%{public}d,nodeId:%{public}d) is failed!!!", viewTag.toString(), ctxId, nodeId);
    return null;
  }

  /**
   * Updates ArkTS view properties.
   * @param instanceId Owning SurfaceManager's instance ID (part of the registry key)
   * @param nodeId Node ID
   * @param updateStates Collection of property update states
   */
  public updateArkTsView(instanceId: number, nodeId: string, updateStates: UpdateState[]) {
    let view: HybridBaseView | undefined = this.hybridViews.get(HybridViewManager.hybridKey(instanceId, nodeId));
    if (view) {
      view.toUpdateArkTsView(updateStates);
    }
  }

  /**
   * Destroys an ArkTS view.
   * @param instanceId Owning SurfaceManager's instance ID (part of the registry key)
   * @param nodeId Node ID
   */
  public destroyArkTsView(instanceId: number, nodeId: string) {
    const key = HybridViewManager.hybridKey(instanceId, nodeId);
    let view: HybridBaseView | undefined = this.hybridViews.get(key);
    if (view) {
      view.toDestroyArkTsView();
      this.hybridViews.delete(key);
    }
  }

  /**
   * Invokes an ArkTS view event.
   * @param instanceId Owning SurfaceManager's instance ID (part of the registry key)
   * @param nodeId Node ID
   * @param key Event key
   * @param params Event parameters
   */
  public onInvokeArkTsView(instanceId: number, nodeId: string, key: string, params: HybridInvokeCallback) {
    let view: HybridBaseView | undefined = this.hybridViews.get(HybridViewManager.hybridKey(instanceId, nodeId));
    if (view) {
        view.toInvokeArkTsView(key, params);
    }
  }

}
