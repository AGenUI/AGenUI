import { AbilityConstant, ConfigurationConstant, UIAbility, Want } from '@kit.AbilityKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { window } from '@kit.ArkUI';
import { buffer } from '@kit.ArkTS';
import { fileIo } from '@kit.CoreFileKit';
import { AGenUI } from '@agenui/agenui';

const DOMAIN = 0x0000;
const TAG = 'AGenUITest';


export default class EntryAbility extends UIAbility {
  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    try {
      this.context.getApplicationContext().setColorMode(ConfigurationConstant.ColorMode.COLOR_MODE_NOT_SET);
    } catch (err) {
      hilog.error(DOMAIN, 'testTag', 'Failed to set colorMode. Cause: %{public}s', JSON.stringify(err));
    }
    hilog.info(DOMAIN, 'testTag', '%{public}s', 'Ability onCreate');

    // Save test parameters to AppStorage for UITestPage
    const testCase = want.parameters?.testCase as string;
    if (testCase) {
      AppStorage.setOrCreate('testCase', testCase);
      hilog.info(DOMAIN, TAG, 'Test mode: testCase=%{public}s', testCase);
    }
    const testDataPath = want.parameters?.testDataPath as string;
    if (testDataPath) {
      AppStorage.setOrCreate('testDataPath', testDataPath);
      hilog.info(DOMAIN, TAG, 'Test mode: testDataPath=%{public}s', testDataPath);
    }
    const testBundleData = want.parameters?.testBundleData as string;
    if (testBundleData) {
      AppStorage.setOrCreate('testBundleData', testBundleData);
      hilog.info(DOMAIN, TAG, 'Test mode: testBundleData length=%{public}d', testBundleData.length);
    }
    const testBundleDataB64 = want.parameters?.testBundleDataB64 as string;
    if (testBundleDataB64) {
      const decoded = buffer.from(testBundleDataB64, 'base64').toString('utf-8');
      AppStorage.setOrCreate('testBundleData', decoded);
      hilog.info(DOMAIN, TAG, 'Test mode: testBundleDataB64 decoded length=%{public}d', decoded.length);
    }
  }

  onDestroy(): void {
    hilog.info(DOMAIN, 'testTag', '%{public}s', 'Ability onDestroy');
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {
    // Main window is created, set main page for this ability
    hilog.info(DOMAIN, 'testTag', '%{public}s', 'Ability onWindowStageCreate');

    // Route to UITestPage if test parameters are present
    const testCase = AppStorage.get<string>('testCase');
    const testDataPath = AppStorage.get<string>('testDataPath');
    const testBundleData = AppStorage.get<string>('testBundleData');
    const page = (testCase || testDataPath || testBundleData) ? 'pages/UITestPage' : 'pages/AGenUIDemoPage';

    // Register custom fonts BEFORE loading any page content.
    // ArkUI font registration must happen before text nodes are created.
    // The SDK uses loadFontSync internally, which does not depend on UIContext.
    this.registerCustomFonts();

    windowStage.loadContent(page, (err) => {
      if (err.code) {
        hilog.error(DOMAIN, 'testTag', 'Failed to load the content. Cause: %{public}s', JSON.stringify(err));
        return;
      }
      hilog.info(DOMAIN, 'testTag', 'Succeeded in loading: %{public}s', page);
    });
  }

  /**
   * Copy rawfile fonts to the sandbox and register them via AGenUI.registerFont.
   *
   * The SDK's registerFont() handles both ArkUI rendering (NODE_FONT_FAMILY) and
   * OH_Drawing measurement registration internally, so the host only needs to
   * call it once per font with a sandbox absolute path.
   *
   * Must be called before windowStage.loadContent() for ArkUI fonts to take
   * effect (see onWindowStageCreate above).
   */
  private registerCustomFonts(): void {
    // Register Nunito via file-path API (exercises OH_Drawing_RegisterFont)
    try {
      const rawData = this.context.resourceManager.getRawFileContentSync('fonts/Nunito-Regular.ttf');
      const sandboxPath = this.context.filesDir + '/Nunito-Regular.ttf';
      const file = fileIo.openSync(sandboxPath, fileIo.OpenMode.CREATE | fileIo.OpenMode.WRITE_ONLY | fileIo.OpenMode.TRUNC);
      try {
        fileIo.writeSync(file.fd, rawData.buffer);
      } finally {
        fileIo.closeSync(file);
      }
      const ok = AGenUI.registerFont('Nunito', sandboxPath);
      hilog.info(DOMAIN, TAG, 'registerFont (path) %{public}s: %{public}s', ok ? 'OK' : 'FAIL', sandboxPath);
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'registerFont (path) FAIL Nunito: %{public}s', String(e));
    }

    // Register remaining fonts via rawfile buffer API
    const names: string[] = ['PlayfairDisplay', 'FiraCode'];
    const files: string[] = ['fonts/PlayfairDisplay-Regular.ttf', 'fonts/FiraCode-Regular.ttf'];

    for (let i = 0; i < names.length; i++) {
      try {
        AGenUI.registerFontFromRawFile(names[i], files[i], this.context);
        hilog.info(DOMAIN, TAG, 'registerFontFromRawFile OK: %{public}s <- %{public}s', names[i], files[i]);
      } catch (e) {
        hilog.error(DOMAIN, TAG, 'registerFontFromRawFile FAIL %{public}s: %{public}s', names[i], String(e));
      }
    }
  }

  onWindowStageDestroy(): void {
    // Main window is destroyed, release UI related resources
    hilog.info(DOMAIN, 'testTag', '%{public}s', 'Ability onWindowStageDestroy');
  }

  onForeground(): void {
    // Ability has brought to foreground
    hilog.info(DOMAIN, 'testTag', '%{public}s', 'Ability onForeground');
  }

  onBackground(): void {
    // Ability has back to background
    hilog.info(DOMAIN, 'testTag', '%{public}s', 'Ability onBackground');
  }
}
