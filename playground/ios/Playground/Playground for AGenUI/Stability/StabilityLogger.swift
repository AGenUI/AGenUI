import Foundation

/// Writes stability test metrics in JSONL format.
/// Each round appends one JSON line and flushes to disk (updating mtime for heartbeat detection).
class StabilityLogger {
    private let outputDir: URL
    private let logFileURL: URL
    private var fileHandle: FileHandle?
    private let queue = DispatchQueue(label: "com.agenui.stability.logger")
    private let dateFormatter: DateFormatter = {
        let df = DateFormatter()
        df.dateFormat = "yyyy-MM-dd'T'HH:mm:ss.SSS"
        df.locale = Locale(identifier: "en_US_POSIX")
        return df
    }()

    var logFilePath: String { logFileURL.path }

    init(outputDir: URL) {
        self.outputDir = outputDir
        self.logFileURL = outputDir.appendingPathComponent("stability_log.jsonl")
        setupFile()
    }

    private func setupFile() {
        let fm = FileManager.default
        if !fm.fileExists(atPath: outputDir.path) {
            try? fm.createDirectory(at: outputDir, withIntermediateDirectories: true)
        }
        if !fm.fileExists(atPath: logFileURL.path) {
            fm.createFile(atPath: logFileURL.path, contents: nil)
        }
        // Append mode: launch.sh clears the file before first launch,
        // but crash-restarts by monitor.sh must preserve previous data.
        fileHandle = try? FileHandle(forWritingTo: logFileURL)
        fileHandle?.seekToEndOfFile()
    }

    func logRound(round: Int, scenario: String, durationMs: Int, status: String,
                  fixture: String?, error: String?, memoryMb: Double) {
        var dict: [String: Any] = [
            "ts": dateFormatter.string(from: Date()),
            "round": round,
            "scenario": scenario,
            "duration_ms": durationMs,
            "memory_mb": Double(Int(memoryMb * 10)) / 10.0,
            "status": status
        ]
        if let f = fixture { dict["fixture"] = f }
        if let e = error { dict["error"] = e }

        writeLine(dict)
    }

    func logEvent(event: String, detail: String) {
        let dict: [String: Any] = [
            "ts": dateFormatter.string(from: Date()),
            "event": event,
            "detail": detail
        ]
        writeLine(dict)
    }

    private func writeLine(_ dict: [String: Any]) {
        guard let data = try? JSONSerialization.data(withJSONObject: dict, options: [.sortedKeys]),
              var line = String(data: data, encoding: .utf8) else { return }
        line += "\n"
        guard let lineData = line.data(using: .utf8) else { return }

        queue.sync {
            self.fileHandle?.write(lineData)
            self.fileHandle?.synchronizeFile()
        }
    }

    func close() {
        queue.sync {
            self.fileHandle?.synchronizeFile()
            self.fileHandle?.closeFile()
            self.fileHandle = nil
        }
    }

    deinit {
        close()
    }
}
