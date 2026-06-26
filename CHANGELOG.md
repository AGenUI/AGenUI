# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/), and this project adheres to [Semantic Versioning](https://semver.org/).

## [1.1.0] - 2026-06-25

### Features

- **List Lazy Loading & Exposure Tracking**: Implemented horizontal List lazy loading on all three platforms (iOS `UICollectionView` / Android `RecyclerView` / Harmony cell reuse), with direction-based rendering path separation. Added List Item exposure tracking for impression analytics.
- **Properties Incremental Update**: Replaced full-style re-rendering with properties-based incremental update on Android and iOS. Protocol adds `id` field, removes `styles` dependency.
- **Component Lifecycle Events**: Aligned lifecycle across three platforms; introduced `onDestroy` on HarmonyOS.
- **Button Child-Component Centering**: Button child components now use centered layout by default, aligning root-view behavior across platforms.
- **Image Auto-Sizing Consistency**: Unified Image measurement logic across three platforms — synchronous measurement returns constraint value for EXACTLY/AT_MOST modes and 0 for UNDEFINED; asynchronous size reporting triggers only when style width or height is unspecified.
- **CSS `gap` Property Support**: Engine now supports the CSS `gap` property for Flex layouts, enabling spacing between child items without manual margins.
- **Text Rendering Consistency**: Fixed text drawing being clipped at view boundaries and `padding` not taking effect, ensuring consistent line-height and spacing across platforms.

### Bug Fixes

- Fixed wild-pointer crash on concurrent `surfaceSize` access by adding lock.
- Fixed Yoga `flex-basis` cache reuse on sibling placeholder nodes.
- Fixed horizontal List item overflow, padding residuals, Tabs display, and Card shadow issues (Android).
- Fixed horizontal List cell-reuse layout anomalies and `CollectionView` separation (iOS).
- Fixed layout not recalculated on window dimension change.
- Fixed `TextComponent` unable to display numeric values.
- Improved `textChunk` streaming: correct field priority, full-text measurement, and protocol completeness. HarmonyOS now supports `textChunk`.
- Fixed `padding-right` / `padding-bottom` not effective in List containers (iOS/Harmony).

---

## [1.0.0] - 2026-05-25

### Layout Engine

- Unified layout computation into the shared C++ core across iOS, Android, and HarmonyOS; eliminated behavioral differences in Flex nesting, alignment, and size inference at the source, significantly improving cross-platform visual consistency.

### Rendering Performance

- Optimized the rendering pipeline by streamlining the diff-based redraw path, restructuring critical data structures, and batching computation passes — reducing overall parsing and drawing overhead.
- Improved fluidity in high-frequency scenarios such as streaming incremental updates and multi-component draws.

### Runtime Logger Interface

- Introduced a pluggable runtime logger abstraction (`IRuntimeLogger`). Integrators can inject a custom logger implementation to fully take over SDK log output.
- Supports dynamic log-level control covering Debug / Info / Warn / Error / Fatal / Performance levels.
- Enables integrators to connect their own log collection, sanitization, sampling, and reporting pipelines.

### Runtime Error Reporting

- Proactively captures protocol-level anomalies (missing fields, type mismatches, JSON parse failures) and surfaces them to the integration layer through a unified error callback.
- Enables integrators to implement graceful degradation, monitoring, and reporting in production environments.

### Stability & Visual Polish

- Introduced cross-platform automated visual comparison testing covering atomic components and composite card scenarios.
- Fixed multiple cross-platform rendering consistency issues.
- Resolved edge cases in complex containers including List, Table, and Image.

---

## [0.9.10] - 2026-05-11

### Improvements

- Component rendering optimizations.

---

## [0.9.9] - 2026-04-15

### Improvements

- Rendering consistency improvements across platforms.
- Streaming parser stability fixes.

---

## [0.9.8] - 2026-03-26

### Added

- Initial open-source release.
- A2UI v0.9 protocol implementation with 22 built-in components.
- Shared C++ core engine with iOS, Android, and HarmonyOS rendering engines.
- Function Call integration framework.
- Design Token and theming support with light/dark mode.
