/**
 * StabilityTestPage — displays a semi-transparent metrics overlay
 * showing real-time stability test status (matching Android MetricsOverlayView).
 * Uses AppStorage to receive metrics from StabilityTestAbility.
 */
@Entry
@Component
struct StabilityTestPage {
  @StorageProp('stability_scenario') scenario: string = 'IDLE';
  @StorageProp('stability_round') round: number = 0;
  @StorageProp('stability_maxRounds') maxRounds: number = 0;
  @StorageProp('stability_elapsedMs') elapsedMs: number = 0;
  @StorageProp('stability_errors') errors: number = 0;
  @StorageProp('stability_isRunning') isRunning: boolean = false;
  @StorageProp('stability_status') status: string = 'READY';

  build() {
    Stack({ alignContent: Alignment.TopStart }) {
      // Dark background
      Column()
        .width('100%')
        .height('100%')
        .backgroundColor('#1A1A1A')

      // Semi-transparent metrics overlay panel
      Column() {
        Text(this.abbreviate(this.scenario))
          .fontSize(11)
          .fontColor('#FFFFFF')
          .fontFamily('monospace')
          .margin({ bottom: 2 })

        Text(this.formatRound())
          .fontSize(11)
          .fontColor('#FFFFFF')
          .fontFamily('monospace')
          .margin({ bottom: 2 })

        Text(this.formatTimer())
          .fontSize(11)
          .fontColor('#FFFFFF')
          .fontFamily('monospace')
          .margin({ bottom: 2 })

        Text(this.formatStatus())
          .fontSize(11)
          .fontColor(this.statusColor())
          .fontFamily('monospace')
          .margin({ bottom: 2 })
      }
      .padding({ left: 12, right: 12, top: 10, bottom: 10 })
      .borderRadius(12)
      .backgroundColor('rgba(0, 0, 0, 0.7)')
      .margin({ left: 16, top: 60 })
      .alignItems(HorizontalAlign.Start)
    }
    .width('100%')
    .height('100%')
  }

  private abbreviate(scenario: string): string {
    if (!scenario) return 'IDLE';
    let s = scenario.replace('REALISTIC_', 'R:');
    return s.length > 16 ? s.substring(0, 16) : s;
  }

  private formatRound(): string {
    if (this.maxRounds > 0) {
      return `R:${this.round}/${this.maxRounds}`;
    }
    return `R:${this.round}`;
  }

  private formatTimer(): string {
    const totalMs = this.elapsedMs;
    const h = Math.floor(totalMs / 3600000);
    const m = Math.floor((totalMs % 3600000) / 60000);
    const s = Math.floor((totalMs % 60000) / 1000);
    return `${this.pad2(h)}:${this.pad2(m)}:${this.pad2(s)}`;
  }

  private pad2(n: number): string {
    return n < 10 ? `0${n}` : `${n}`;
  }

  private formatStatus(): string {
    if (this.errors > 0) {
      return `ERR:${this.errors}`;
    }
    return this.isRunning ? 'RUNNING' : this.status;
  }

  private statusColor(): string {
    if (this.errors > 0) return '#FF6666';
    if (this.isRunning) return '#4CFF4C';
    return '#FFCC33';
  }
}
