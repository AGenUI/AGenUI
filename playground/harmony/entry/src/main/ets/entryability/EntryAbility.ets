import { AbilityConstant, ConfigurationConstant, UIAbility, Want } from '@kit.AbilityKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { window } from '@kit.ArkUI';
import { buffer } from '@kit.ArkTS';

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

    windowStage.loadContent(page, (err) => {
      if (err.code) {
        hilog.error(DOMAIN, 'testTag', 'Failed to load the content. Cause: %{public}s', JSON.stringify(err));
        return;
      }
      hilog.info(DOMAIN, 'testTag', 'Succeeded in loading: %{public}s', page);
    });
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
