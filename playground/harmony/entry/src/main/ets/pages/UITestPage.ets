import { AGenUIContainer, SurfaceManager, ISurfaceManagerListener, Surface, AGenUI, A2UIComponent } from '@agenui/agenui';
import { common } from '@kit.AbilityKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import fs from '@ohos.file.fs';
import { CustomMarkdownComponent } from '../components/CustomMarkdownComponent';
import { CustomChartComponent } from '../components/CustomChartComponent';
import { CustomLottieComponent } from '../components/CustomLottieComponent';
import { ToastFunction } from '../functions/ToastFunction';
import { OpenUrlFunction } from '../functions/OpenUrlFunction';
import { PlaygroundRuntimeLogger } from '../utils/PlaygroundRuntimeLogger';

const TAG = 'UITestPage';
const DOMAIN = 0x0000;

@Entry
@Component
struct UITestPage {
  @State currentSurfaceId: string = '';
  @State surfaceVersion: number = 0;
  private surfaceManager: SurfaceManager | null = null;
  private runtimeLogger: PlaygroundRuntimeLogger | null = null;

  aboutToAppear() {
    const context = getContext(this) as common.UIAbilityContext;
    
    // Initialize custom RuntimeLogger and inject to engine
    this.runtimeLogger = new PlaygroundRuntimeLogger(context,
      (level: number, tag: string, func: string, line: number, message: string) => {
        const levelName = PlaygroundRuntimeLogger.getLevelName(level);
        hilog.info(DOMAIN, TAG, '[%{public}s] %{public}s', levelName, message);
      }
    );
    AGenUI.setCustomLogger(this.runtimeLogger);

    this.surfaceManager = new SurfaceManager(context);

    // Register custom components
    AGenUI.registerFunction(new ToastFunction());
    AGenUI.registerFunction(new OpenUrlFunction(context));

    AGenUI.registerComponent('Markdown', (
      surfaceId: string,
      nodeId: string,
      componentType: string,
      viewStatePtr: bigint
    ): A2UIComponent => {
      return new CustomMarkdownComponent(surfaceId, nodeId, componentType, viewStatePtr);
    });
    AGenUI.registerComponent('Chart', (
      surfaceId: string,
      nodeId: string,
      componentType: string,
      viewStatePtr: bigint
    ): A2UIComponent => {
      return new CustomChartComponent(surfaceId, nodeId, componentType, viewStatePtr);
    });
    AGenUI.registerComponent('Lottie', (
      surfaceId: string,
      nodeId: string,
      componentType: string,
      viewStatePtr: bigint
    ): A2UIComponent => {
      return new CustomLottieComponent(surfaceId, nodeId, componentType, viewStatePtr);
    });

    this.surfaceManager.addListener({
      onCreateSurface: (surface: Surface) => {
        this.surfaceVersion++;
        this.currentSurfaceId = surface.surfaceId;
        hilog.info(DOMAIN, TAG, 'Surface created: %{public}s', surface.surfaceId);
      },
      onDeleteSurface: (surface: Surface) => {
        if (this.currentSurfaceId === surface.surfaceId) {
          this.currentSurfaceId = '';
        }
      }
    } as ISurfaceManagerListener);

    // Read test data and render
    const testBundleData = AppStorage.get<string>('testBundleData');
    if (testBundleData) {
      this.renderFromData(testBundleData);
    } else {
      const testCase = AppStorage.get<string>('testCase');
      if (testCase) {
        const bundlePath = `/data/local/tmp/agenui_test/${testCase}/${testCase}_bundle.json`;
        this.loadAndRender(bundlePath);
      } else {
        const testDataPath = AppStorage.get<string>('testDataPath');
        if (testDataPath) {
          this.loadAndRender(testDataPath);
        }
      }
    }
  }

  renderFromData(jsonStr: string) {
    try {
      const bundle: Record<string, Object> = JSON.parse(jsonStr) as Record<string, Object>;
      const version = bundle.version as string;
      const sequence = bundle.sequence as Array<Record<string, Object>>;

      this.surfaceManager!.beginTextStream();
      for (const item of sequence) {
        const msg: Record<string, Object> = JSON.parse(JSON.stringify(item)) as Record<string, Object>;
        msg['version'] = version;
        this.surfaceManager!.receiveTextChunk(JSON.stringify(msg));
      }
      this.surfaceManager!.endTextStream();

      hilog.info(DOMAIN, TAG, 'Rendered %{public}d messages from inline data', sequence.length);
    } catch (err) {
      hilog.error(DOMAIN, TAG, 'Failed to render from data: %{public}s', JSON.stringify(err));
    }
  }

  loadAndRender(path: string) {
    try {
      const content = fs.readTextSync(path);
      this.renderFromData(content);
      hilog.info(DOMAIN, TAG, 'Loaded from file: %{public}s', path);
    } catch (err) {
      hilog.error(DOMAIN, TAG, 'Failed to load file: %{public}s', JSON.stringify(err));
    }
  }

  aboutToDisappear(): void {
    if (this.surfaceManager !== null) {
      this.surfaceManager.destroy();
      this.surfaceManager = null;
    }
    this.runtimeLogger = null;
  }

  build() {
    Scroll() {
      Column() {
        ForEach(
          this.currentSurfaceId ? [this.currentSurfaceId] : [],
          (surfaceId: string) => {
            AGenUIContainer({ surfaceId: surfaceId })
              .width('100%')
          },
          (_surfaceId: string) => `agenui_${this.surfaceVersion}`
        )
      }
      .width('100%')
      .height('100%')
      .justifyContent(FlexAlign.Center)
      .alignItems(HorizontalAlign.Center)
    }
    .width('100%')
    .height('100%')
    .backgroundColor(Color.White)
    .scrollBar(BarState.Off)
    .edgeEffect(EdgeEffect.None)
    .align(Alignment.Center)
  }
}
