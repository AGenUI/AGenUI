# [v0.9.8] - 2026-03-26
## 新增
- 初始版本发布

---

## 特性
- 支持 HarmonyOS NEXT API 20
- 高性能渲染
- 灵活的组件系统

---

# [v0.9.9] - 2026-05-11
## 
- 组件效果优化

---

# [v1.0.0] - 2026-05-25

## 布局引擎
- 将布局计算统一到 iOS、Android 和 HarmonyOS 共享的 C++ 核心中；从源头消除了 Flex 嵌套、对齐和尺寸推断的行为差异，显著提升了跨平台视觉一致性。

## 渲染性能
- 优化渲染管线，精简基于 diff 的重绘路径，重构关键数据结构，批量合并计算过程——降低了整体解析和绘制开销。
- 提升了流式增量更新、多组件绘制等高频场景下的流畅度。

## 运行时日志接口
- 引入可插拔的运行时日志抽象（`IRuntimeLogger`）。集成方可注入自定义日志实现，完全接管 SDK 日志输出。
- 支持动态日志级别控制，涵盖 Debug / Info / Warn / Error / Fatal / Performance 级别。
- 允许集成方对接自有的日志采集、脱敏、采样和上报链路。

## 运行时错误上报
- 主动捕获协议层异常（字段缺失、类型不匹配、JSON 解析失败），并通过统一的错误回调上报至集成层。
- 使集成方能够在生产环境中实现优雅降级、监控和上报。

## 稳定性与视觉优化
- 引入跨平台自动化视觉对比测试，覆盖原子组件和组合卡片场景。
- 修复多项跨平台渲染一致性问题。
- 解决了 List、Table、Image 等复杂容器中的边界情况。

---

# [v1.0.2] - 2026-06-24

## 性能和稳定性优化
- 优化全链路绘制性能，排查和修复稳定性问题

## 问题修复
- 修复若干已知问题

---

# [v1.1.0] - 2026-06-26

## 新特性

- **List 懒加载 + 曝光埋点**：三端实现横向 List 懒加载（iOS `UICollectionView` / Android `RecyclerView` / 鸿蒙 cell 复用），按方向分离渲染路径。新增 List Item 曝光埋点。
- **Properties 增量更新**：Android/iOS 实现基于 properties 的增量更新，替代全量 style 重渲染。协议新增 `id` 字段，移除 `styles` 依赖。
- **组件生命周期事件**：三端生命周期对齐；鸿蒙端引入 `onDestroy` 方法。
- **Button 子组件居中对齐**：Button 子组件默认采用居中布局，统一三端根视图行为。
- **Image 自动尺寸测量统一**：统一三端 Image 测量逻辑——有明确约束时同步返回约束值，宽高未指定时返回 0 并在图片加载完成后异步上报实际尺寸，解决未指定宽高时图片显示异常问题。
- **CSS `gap` 属性支持**：引擎支持 CSS `gap` 属性，Flex 布局中子元素间距无需手动设置 margin。
- **Text 渲染一致性修复**：修复文字绘制被视图边界裁剪、padding 不生效等问题，确保三端行高与间距表现一致。

## Bug 修复

- 获取 `surfaceSize` 增加锁，修复低概率野指针崩溃。
- 修复 Yoga `flex-basis` 缓存在兄弟 placeholder 节点上的复用问题。
- 修复 Android 水平 List item 溢出、padding 残留、Tabs 显示异常、卡片无阴影等问题。
- 修复 iOS 横向 List cell 复用布局异常，拆分 CollectionView 与竖向子视图。
- 修复窗口尺寸变化时布局未重新计算。
- 修复 TextComponent 无法显示数值类型的问题。
- 完善 `textChunk` 流式效果：字段优先级、全量文本测量、协议完整性。鸿蒙端新增 `textChunk` 支持。
- 修复 iOS/鸿蒙 List 中 `padding-right` / `padding-bottom` 不生效。

---

# [v1.2.0] - 2026-07-08

### 新特性

- **A2UI 无障碍字段支持**：在 Core 引擎、Android、iOS 和鸿蒙端全平台新增 `accessibility` 字段及其二级字段的解析，支持数据绑定，可接入屏幕阅读器和语义标注。
- **List Item 出现事件 & 首屏渲染埋点**：向集成层透出 list item appear 事件和 first-render trackInfo，用于数据分析和性能监控。
- **Padding 解析接口开放**：开放 padding 解析接口，集成方可直接获取解析后的 padding 值。
- **linear-gradient 渐变背景支持**：Text、Button、List、Checkbox、Divider、TextField 组件的 background-color 统一使用基类方法处理，支持 `linear-gradient` 渐变色。

### Bug 修复

- (iOS) 修复 root 节点 Image 渲染空白——Surface root 补调 `createView()` 生命周期。
- (iOS) 修复阴影偏淡问题——设定 `shadowOpacity` 为 `1.0`，防止 alpha 被乘两次。
- (iOS) 修复 `Surface.updateSize()` 递归布局通知导致栈溢出崩溃。
- (iOS) 修复 `TabsComponent.addChild` 闭包强引用子组件导致永久内存泄漏。
- (iOS) 修复并发 `ImageLoader` 注册导致 ARC 引用计数竞争崩溃。
- (iOS) 修复并发 Function 注册/注销导致 Swift `Dictionary` 竞争崩溃。
- (Android) 修复 Image 显式 `0px` 被图片固有尺寸覆盖导致的布局抖动。
- (Android) 修复删除线位置错误问题，改进行高处理逻辑。
- (鸿蒙) 修复 Row 子元素重叠、垂直居中对齐异常。
- (鸿蒙) 修复 API 17 崩溃——使用 `dlsym` wrapper 替换 `OH_ArkUI_PostFrameCallback`。

---