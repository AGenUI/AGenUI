import UIKit

/// Floating semi-transparent overlay showing real-time stability test metrics.
class MetricsOverlayView: UIView {
    private let roundLabel = UILabel()
    private let timerLabel = UILabel()
    private let memoryLabel = UILabel()
    private let statusLabel = UILabel()
    private let scenarioLabel = UILabel()

    override init(frame: CGRect) {
        super.init(frame: frame)
        setupUI()
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    private func setupUI() {
        backgroundColor = UIColor.black.withAlphaComponent(0.7)
        layer.cornerRadius = 12
        clipsToBounds = true

        let stack = UIStackView(arrangedSubviews: [scenarioLabel, roundLabel, timerLabel, memoryLabel, statusLabel])
        stack.axis = .vertical
        stack.spacing = 4
        stack.translatesAutoresizingMaskIntoConstraints = false
        addSubview(stack)

        NSLayoutConstraint.activate([
            stack.topAnchor.constraint(equalTo: topAnchor, constant: 10),
            stack.leadingAnchor.constraint(equalTo: leadingAnchor, constant: 12),
            stack.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -12),
            stack.bottomAnchor.constraint(equalTo: bottomAnchor, constant: -10),
        ])

        let labels = [scenarioLabel, roundLabel, timerLabel, memoryLabel, statusLabel]
        for label in labels {
            label.font = UIFont.monospacedSystemFont(ofSize: 11, weight: .medium)
            label.textColor = .white
        }
        statusLabel.textColor = UIColor(red: 0.3, green: 1.0, blue: 0.3, alpha: 1.0)

        // Default values
        scenarioLabel.text = "IDLE"
        roundLabel.text = "R:0"
        timerLabel.text = "00:00:00"
        memoryLabel.text = "0 MB"
        statusLabel.text = "READY"
    }

    func update(scenario: String, round: Int, maxRounds: Int, elapsed: TimeInterval, memoryMb: Double, peakMb: Double, errors: Int, isRunning: Bool) {
        let h = Int(elapsed) / 3600
        let m = (Int(elapsed) % 3600) / 60
        let s = Int(elapsed) % 60

        scenarioLabel.text = abbreviate(scenario)
        roundLabel.text = maxRounds > 0 ? "R:\(round)/\(maxRounds)" : "R:\(round)"
        timerLabel.text = String(format: "%02d:%02d:%02d", h, m, s)
        memoryLabel.text = String(format: "%.0fMB (pk:%.0f)", memoryMb, peakMb)

        if errors > 0 {
            statusLabel.text = "ERR:\(errors)"
            statusLabel.textColor = UIColor(red: 1.0, green: 0.4, blue: 0.4, alpha: 1.0)
        } else {
            statusLabel.text = isRunning ? "RUNNING" : "DONE"
            statusLabel.textColor = isRunning
                ? UIColor(red: 0.3, green: 1.0, blue: 0.3, alpha: 1.0)
                : UIColor(red: 1.0, green: 0.8, blue: 0.2, alpha: 1.0)
        }
    }

    private func abbreviate(_ scenario: String) -> String {
        let s = scenario.replacingOccurrences(of: "REALISTIC_", with: "R:")
        if s.count > 16 {
            return String(s.prefix(16))
        }
        return s
    }
}
