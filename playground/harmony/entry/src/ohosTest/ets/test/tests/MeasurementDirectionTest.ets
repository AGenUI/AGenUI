import { describe, it, expect, beforeEach, afterEach } from '@ohos/hypium';
import { common } from '@kit.AbilityKit';
import { UIContext } from '@kit.ArkUI';
import {
  AGenUI, SurfaceManager, MeasurementManager, MeasurementHandler, MeasureResult,
  A2UIComponent, A2UIComponentProps, CustomView, ISurfaceManagerListener
} from '@agenui/agenui';
import { sleep, TIMEOUT_MS } from '../base/AGenUIBaseTest';
import { FixtureData, loadFixture, getMessagesAsString, getSurfaceId } from '../base/TestFixtureLoader';

const PROBE_TYPE: string = 'MeasurementDirectionProbe';
const PROBE_ID: string = 'measurement-direction-probe';

interface ProbeSnapshot {
  id: string;
  component: string;
}

interface ProbeMeasurement {
  snapshot: ProbeSnapshot;
  widthMode: number;
  maxWidth: number;
  heightMode: number;
  maxHeight: number;
  direction?: string;
}

interface ProbeContentSize {
  surfaceId: string;
  height: number;
}

interface ProbeExpectation {
  measurementIndex: number;
  contentSizeIndex: number;
  surfaceId: string;
  probeId: string;
  direction?: string;
}

@Builder
function buildMeasurementProbe(view: CustomView): void {
  Blank()
    .width(view.getProperty().getViewWidth())
    .height(view.getProperty().getViewHeight())
}

class MeasurementDirectionProbe extends A2UIComponent {
  constructor(surfaceId: string, nodeId: string, componentType: string,
    viewStatePtr: bigint, instanceId: number) {
    super(surfaceId, nodeId, componentType, viewStatePtr,
      wrapBuilder<[CustomView]>(buildMeasurementProbe), instanceId);
  }

  protected onUpdateProperties(_properties: A2UIComponentProps): void {
  }
}

function probeMessages(fixture: FixtureData, suffix: string): string {
  return getMessagesAsString(fixture)
    .split(getSurfaceId(fixture)).join(`${getSurfaceId(fixture)}-${suffix}`)
    .split(PROBE_ID).join(`${PROBE_ID}-${suffix}`);
}

function sendProbeMessages(manager: SurfaceManager, fixture: FixtureData, suffix: string): void {
  manager.beginTextStream();
  manager.receiveTextChunk(probeMessages(fixture, suffix));
  manager.endTextStream();
}

async function waitForProbeMeasurements(
  measurements: ProbeMeasurement[], contentSizes: ProbeContentSize[], target: ProbeExpectation
): Promise<number> {
  const deadline: number = Date.now() + TIMEOUT_MS;
  let currentMeasurements: ProbeMeasurement[] = [];
  let currentSizes: ProbeContentSize[] = [];
  let receivedExpectedDirection: boolean = false;
  while (Date.now() < deadline) {
    currentMeasurements = measurements.slice(target.measurementIndex)
      .filter((measurement: ProbeMeasurement) => measurement.snapshot.id === target.probeId);
    currentSizes = contentSizes.slice(target.contentSizeIndex)
      .filter((size: ProbeContentSize) => size.surfaceId === target.surfaceId && size.height > 0);
    receivedExpectedDirection = currentMeasurements.some((measurement: ProbeMeasurement) =>
      target.direction === undefined || measurement.direction === target.direction);
    if (receivedExpectedDirection && currentSizes.length > 0) {
      break;
    }
    await sleep(10);
  }
  if (!receivedExpectedDirection || currentSizes.length === 0) {
    const observedDirections: string[] = currentMeasurements.map((measurement: ProbeMeasurement) =>
      measurement.direction ?? 'five-parameter handler');
    throw new Error(`Incomplete native layout for ${target.probeId}: expected=${target.direction}, ` +
      `observed=${observedDirections.join(',')}, sizes=${currentSizes.length}; real liba2ui-capi.so is required`);
  }
  expect(currentMeasurements.length).assertLarger(0);
  for (const measurement of currentMeasurements) {
    expect(typeof measurement.snapshot.id).assertEqual('string');
    expect(typeof measurement.snapshot.component).assertEqual('string');
    expect(measurement.snapshot.id).assertEqual(target.probeId);
    expect(measurement.snapshot.component).assertEqual(PROBE_TYPE);
    expect([0, 1, 2].includes(measurement.widthMode)).assertTrue();
    expect([0, 1, 2].includes(measurement.heightMode)).assertTrue();
    if (measurement.widthMode !== 0) {
      expect(Number.isFinite(measurement.maxWidth)).assertTrue();
      expect(measurement.maxWidth).assertLarger(0);
    }
    if (measurement.heightMode !== 0) {
      expect(Number.isFinite(measurement.maxHeight)).assertTrue();
      expect(measurement.maxHeight).assertLargerOrEqual(0);
    }
    if (target.direction !== undefined) {
      expect(measurement.direction).assertEqual(target.direction);
    } else {
      expect(measurement.direction).assertUndefined();
    }
  }
  return currentSizes[currentSizes.length - 1].height;
}

export default function measurementDirectionTest() {
  describe('MeasurementDirectionTest', () => {
    let context: common.UIAbilityContext;
    let ltrManager: SurfaceManager;
    let rtlManager: SurfaceManager;
    let measurementManager: MeasurementManager;
    let measurements: ProbeMeasurement[] = [];
    let contentSizes: ProbeContentSize[] = [];

    beforeEach(() => {
      const uiContext: UIContext | undefined = AppStorage.get<UIContext>('context');
      if (!uiContext) {
        throw new Error('MeasurementDirectionTest requires the TestAbility UIContext');
      }
      context = uiContext.getHostContext() as common.UIAbilityContext;
      ltrManager = new SurfaceManager(context);
      rtlManager = new SurfaceManager(context);
      ltrManager.setLayoutDirection('ltr');
      rtlManager.setLayoutDirection('rtl');
      measurements = [];
      contentSizes = [];
      const listener: ISurfaceManagerListener = {
        onContentSizeChanged: (surfaceId: string, _width: number, height: number): void => {
          const size: ProbeContentSize = { surfaceId, height };
          contentSizes.push(size);
        }
      };
      ltrManager.addListener(listener);
      rtlManager.addListener(listener);
      AGenUI.registerComponent(PROBE_TYPE, (surfaceId: string, nodeId: string,
        componentType: string, viewStatePtr: bigint, instanceId: number): A2UIComponent => {
        return new MeasurementDirectionProbe(surfaceId, nodeId, componentType, viewStatePtr, instanceId);
      });
      measurementManager = new MeasurementManager(ltrManager.getInstanceId());
      const handler: MeasurementHandler = (paramJson: string, widthMode: number,
        maxWidth: number, heightMode: number, maxHeight: number, direction: string): MeasureResult => {
        const measurement: ProbeMeasurement = {
          snapshot: JSON.parse(paramJson) as ProbeSnapshot,
          widthMode, maxWidth, heightMode, maxHeight, direction
        };
        measurements.push(measurement);
        const result: MeasureResult = { width: 64, height: direction === 'rtl' ? 48 : 32 };
        return result;
      };
      measurementManager.register(PROBE_TYPE, handler);
    });

    afterEach(() => {
      ltrManager.destroy();
      rtlManager.destroy();
      measurementManager.dispose();
      AGenUI.unRegisterComponent(PROBE_TYPE);
    });

    it('MEASURE_DIR_01_alternating_managers_keep_their_direction', 0, async () => {
      const fixture: FixtureData = await loadFixture(
        context.resourceManager, 'measurement/01_direction_probe.json'
      );
      const managers: SurfaceManager[] = [ltrManager, rtlManager, ltrManager, rtlManager];
      const directions: string[] = ['ltr', 'rtl', 'ltr', 'rtl'];
      for (let passIndex: number = 0; passIndex < managers.length; passIndex++) {
        const suffix: string = `alternating-${passIndex}`;
        const target: ProbeExpectation = {
          measurementIndex: measurements.length,
          contentSizeIndex: contentSizes.length,
          surfaceId: `${getSurfaceId(fixture)}-${suffix}`,
          probeId: `${PROBE_ID}-${suffix}`,
          direction: directions[passIndex]
        };
        sendProbeMessages(managers[passIndex], fixture, suffix);
        await waitForProbeMeasurements(measurements, contentSizes, target);
      }
    });

    it('MEASURE_DIR_02_hot_switch_remeasures_existing_surface', 0, async () => {
      const fixture: FixtureData = await loadFixture(
        context.resourceManager, 'measurement/01_direction_probe.json'
      );
      const suffix: string = 'hot-switch';
      const target: ProbeExpectation = {
        measurementIndex: measurements.length,
        contentSizeIndex: contentSizes.length,
        surfaceId: `${getSurfaceId(fixture)}-${suffix}`,
        probeId: `${PROBE_ID}-${suffix}`,
        direction: 'ltr'
      };
      sendProbeMessages(ltrManager, fixture, suffix);
      const initialHeight: number = await waitForProbeMeasurements(measurements, contentSizes, target);

      target.measurementIndex = measurements.length;
      target.contentSizeIndex = contentSizes.length;
      target.direction = 'rtl';
      ltrManager.setLayoutDirection('rtl');
      const rtlHeight: number = await waitForProbeMeasurements(measurements, contentSizes, target);
      expect(rtlHeight).assertLarger(initialHeight);

      target.measurementIndex = measurements.length;
      target.contentSizeIndex = contentSizes.length;
      target.direction = 'ltr';
      ltrManager.setLayoutDirection('ltr');
      const restoredHeight: number = await waitForProbeMeasurements(measurements, contentSizes, target);
      expect(restoredHeight).assertEqual(initialHeight);
    });

    it('MEASURE_DIR_03_five_parameter_handler_remains_assignable', 0, async () => {
      const legacyHandler: MeasurementHandler = (paramJson: string, widthMode: number,
        maxWidth: number, heightMode: number, maxHeight: number): MeasureResult => {
        const measurement: ProbeMeasurement = {
          snapshot: JSON.parse(paramJson) as ProbeSnapshot,
          widthMode, maxWidth, heightMode, maxHeight
        };
        measurements.push(measurement);
        const result: MeasureResult = { width: 64, height: 32 };
        return result;
      };
      measurementManager.register(PROBE_TYPE, legacyHandler);
      const fixture: FixtureData = await loadFixture(
        context.resourceManager, 'measurement/01_direction_probe.json'
      );
      const managers: SurfaceManager[] = [ltrManager, rtlManager];
      for (let managerIndex: number = 0; managerIndex < managers.length; managerIndex++) {
        const suffix: string = `legacy-${managerIndex}`;
        const target: ProbeExpectation = {
          measurementIndex: measurements.length,
          contentSizeIndex: contentSizes.length,
          surfaceId: `${getSurfaceId(fixture)}-${suffix}`,
          probeId: `${PROBE_ID}-${suffix}`
        };
        sendProbeMessages(managers[managerIndex], fixture, suffix);
        await waitForProbeMeasurements(measurements, contentSizes, target);
      }
    });
  });
}
