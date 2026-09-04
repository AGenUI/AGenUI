//
//  RuntimeConfig.swift
//  AGenUI
//
// Created on 2026/9/2.
//

import Foundation

final class RuntimeConfig {
    static let shared = RuntimeConfig()

    static let borderPixelAlignmentKey = "borderPixelAlignment"

    private var values: [String: Bool] = [:]
    private let lock = NSLock()

    private init() {}

    @discardableResult
    func setRuntimeConfig(_ configJson: String) -> Bool {
        guard let data = configJson.data(using: .utf8),
              let object = (try? JSONSerialization.jsonObject(with: data)) as? [String: Any] else {
            return false
        }
        lock.lock()
        defer { lock.unlock() }
        for (key, value) in object {
            if let boolValue = value as? Bool {
                values[key] = boolValue
            } else if let number = value as? NSNumber {
                values[key] = number.intValue == 1
            }
        }
        return true
    }

    /// Reads a bool switch, returning the default when unset.
    func bool(forKey key: String, default defaultValue: Bool) -> Bool {
        lock.lock()
        defer { lock.unlock() }
        return values[key] ?? defaultValue
    }
}
