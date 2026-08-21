# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/), and this project adheres to [Semantic Versioning](https://semver.org/).

## [1.4.0] - 2026-08-21

### Features

- **Shadow & Border-Radius Rendering Rework**: Border radius now applies clipping by default; the shadow layer is rendered independently of component content so it is no longer clipped by the component. The `addChild` logic no longer relies on index computation. Note: `filter: drop-shadow` is unified as box shadow, and glyph-level text shadows are no longer supported.
- **Style Default Alignment**: Unified default style values across Android, iOS, and HarmonyOS (including font styles and default text size); aligned Android text measurement and style parsing with the other platforms. Styles parsing was refactored to be more cohesive in preparation for this alignment.
- **Unified `null` Handling in Incremental Updates**: Component data no longer retains `null` values on Android and HarmonyOS, and first-level property `null` behavior is aligned across all built-in components. On iOS, properties with `NSNull` values are now treated as property removal instead of being silently dropped; a `removeProperties` API was added to the render layer, and styles are no longer flattened into first-level properties.
- **Deep Data Binding for Custom Components**: Custom components now support deep resolution of data bindings within nested properties.
- **(iOS) Deployment Target**: Restored the minimum deployment target from 15.0 back to 13.0.

### Bug Fixes

- (Core) Fixed a low-probability stability issue by returning the surface manager from `findSurfaceManager` as a `shared_ptr` (HarmonyOS usage updated accordingly).
- (iOS) Fixed the `display` style incorrectly overriding the `visibility` style.
- (iOS) Fixed shadow rendering linkage issues.
- (Android) Fixed string-valued `font-weight` falling back to the binary Typeface path.
- (Android) Fixed AudioPlayer and ChoicePicker component issues.
- (Android) Removed unnecessary click and focus configuration on components.
- (HarmonyOS) Fixed the right border disappearing on certain device models.
- (HarmonyOS) Fixed an ArkTS compilation error in `Component.triggerAction`.
- (All) Restored the reset behavior when `action` is empty.
- (All) Fixed the default text size across the three platforms.

### Testing & Quality

- Added rendering test cases for null-diff incremental updates, and filled coverage gaps for previously uncovered style properties and enum values in the catalog.
- Added stability stress test cases and scripts.

---

## [1.3.1] - 2026-08-06

### Features

- **Package Type API**: Added package type set/get APIs to the engine, exposed through the Core engine and the Android, iOS, and HarmonyOS integration layers.
- **Unified Text Rendering Pipeline**: Merged Text and RichText into a single rendering pipeline reusing the shared label implementation, improving rendering consistency and maintainability.
- Hardened JSON parsing with stricter type checks and exception handling.
- Moved Button styling onto the shared base-class styling pipeline.
- Moved Card rounded corners onto the shared clip decision.

### Bug Fixes

- Fixed `overflow: hidden` not taking effect.
- Fixed opacity not taking effect.
- (Android) Unified `border-radius` and `overflow` into a single clip decision.
- (Android) Restored rounded corner rendering on software canvases.
- (Android) Fixed built-in containers unexpectedly clipping their children.
- (Android) Fixed misalignment between the border stroke center (`border-width`) and the rounded-clip center (`border-radius`).
- (iOS) Fixed a UIKit assertion crash caused by `layoutIfNeeded` re-entry in horizontal List, and fixed off-screen update timing of horizontal List.
- (iOS) Fixed a crash by filtering empty values in properties.
- (Android) Fixed the Tab component incorrectly triggering `onTabClick` on first display.
---

## [1.2.0] - 2026-07-08

### Features

- **A2UI Accessibility Support**: Added `accessibility` field and its sub-fields parsing with data binding support across the Core engine, Android, iOS, and HarmonyOS — enabling screen reader compatibility and semantic annotations.
- **List Item Appear Event & First Render Tracking**: Exposed list item appear event and first-render trackInfo to the integration layer for analytics and performance monitoring.
- **Padding Parsing API**: Opened the padding parsing interface for external use, allowing integrators to access parsed padding values directly.
- **linear-gradient Background Support**: Text, Button, List, Checkbox, Divider, and TextField components now support `linear-gradient` backgrounds via a unified base class method.
- **Dashed Underline Support**: Added custom dashed underline style on iOS, Android, and HarmonyOS via `text-decoration` properties.

### Bug Fixes

- (iOS) Fixed root node Image rendering blank — Surface root now receives `createView()` lifecycle.
- (iOS) Fixed shadow rendering too light — set `shadowOpacity` to `1.0` to prevent alpha being multiplied twice.
- (iOS) Fixed `Surface.updateSize()` recursive layout notification causing stack overflow crash.
- (iOS) Fixed `TabsComponent.addChild` closure strongly referencing child components, causing permanent memory leak.
- (iOS) Fixed concurrent `ImageLoader` registration causing ARC reference count race crash.
- (iOS) Fixed concurrent Function registration/deregistration causing Swift `Dictionary` race crash.
- (Android) Fixed Image with explicit `0px` being overridden by intrinsic image size, causing layout jitter.
- (Android) Fixed strikethrough position error and improved line-height handling logic.
- (HarmonyOS) Fixed Row child element overlap and vertical centering anomaly.
- (HarmonyOS) Fixed API 17 crash by replacing `OH_ArkUI_PostFrameCallback` with `dlsym` wrapper.
- (All) Optimized large image loading to reduce UI jank.
- (All) Fixed underline calculation for multi-line text and `thickness` unit conversion.
- (iOS) Fixed strikethrough shorthand syntax not working.
- (All) Fixed image crop size not multiplied by screen density.
- (All) Fixed horizontal List unable to dynamically append child elements.

---

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
