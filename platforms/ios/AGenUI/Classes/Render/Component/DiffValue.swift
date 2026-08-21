//
//  DiffValue.swift
//  AGenUI
//
// Created on 2026/8/12.
//

import Foundation

/// Replaces NSNull at the type level for incremental property diffs.
///
/// C++ diff produces JSON `null` as a delete signal. JSONSerialization turns
/// that into `NSNull`, which is dangerous in Swift: `as! String` on NSNull
/// crashes, `JSONSerialization` can throw an uncatchable NSException.
/// `DiffValue` eliminates that risk by converting NSNull → `.deleted` at the
/// Surface boundary, so leaf code only ever sees `.value(Any)` or `.deleted`.
///
/// Aligned with Android's `null` transparency and Harmony's `is_null()` check:
/// all three platforms make the delete signal explicit at the leaf.
public enum DiffValue {
    /// Set the property to this value.
    case value(Any)
    /// Delete signal: clear the property to its type-appropriate empty value.
    case deleted

    /// Convert a raw `[String: Any]` diff (may contain NSNull) into `[String: DiffValue]`.
    /// NSNull → `.deleted`, everything else → `.value(v)`.
    public static func from(_ raw: [String: Any]) -> [String: DiffValue] {
        var result: [String: DiffValue] = [:]
        result.reserveCapacity(raw.count)
        for (key, value) in raw {
            result[key] = (value is NSNull) ? .deleted : .value(value)
        }
        return result
    }
}

/// Convert `[String: DiffValue]` back to raw `[String: Any]`.
/// `.value(v)` → v, `.deleted` → NSNull.
/// Used at trust boundaries (onPropertiesUpdate callback, ObjC bridge) where
/// raw dictionaries are still required.
extension Dictionary where Key == String, Value == DiffValue {
    public func toRaw() -> [String: Any] {
        var raw: [String: Any] = [:]
        raw.reserveCapacity(count)
        for (key, dv) in self {
            switch dv {
            case .value(let v): raw[key] = v
            case .deleted: raw[key] = NSNull()
            }
        }
        return raw
    }
}
