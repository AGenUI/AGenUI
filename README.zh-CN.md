<div align="center">

# AGenUI

**AGenUI: 同步支持 iOS、Android 和 HarmonyOS 的高性能 A2UI 渲染引擎**

[![License: Apache 2.0](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-iOS%20%7C%20Android%20%7C%20HarmonyOS-blue)](#)
[![Android SDK](https://img.shields.io/badge/Android-API%2021%2B-green)](#)
[![iOS](https://img.shields.io/badge/iOS-13%2B-lightgrey)](#)
[![HarmonyOS](https://img.shields.io/badge/HarmonyOS%20NEXT-API%2017%2B-red)](#)

[**在线 Demo**](https://genui.amap.com) · [**快速上手**](docs/QuickStart.zh-CN.md) · [**API 参考**](docs/API.zh-CN.md) · [**贡献指南**](CONTRIBUTING.md)

[English](README.md) | 中文

</div>

> 本项目正在积极开发与持续演进中，欢迎贡献代码、反馈意见和参与讨论。

---

## 什么是 AGenUI？

**AGenUI** 是同步支持 iOS、Android 和 HarmonyOS 三端的 A2UI SDK，它基于并完整实现了 [Google 开源的 A2UI v0.9 协议](https://github.com/google/A2UI)，能够在移动设备上实时渲染 LLM 生成的可交互界面 UI 的流式数据。它底层由一个跨平台的**共享 C++ 核心引擎**驱动，三端渲染引擎则基于核心引擎下发的组件协议，使用系统原生能力完成绘制。

**AGenUI** 采用系统原生 UI 能力绘制**可交互的卡片、表单、列表、图片、媒体播放器等**，提供高性能、流畅的操作体验。

<img src="docs/images/a2ui_rendering_effect.png" alt="A2UI 组件效果展示" height="720">

---

## 核心特性

- **实时流式渲染** — 组件描述的结构化数据由 LLM 生成，流式增量出现并实时更新
- **高性能** — iOS、Android 和 HarmonyOS 三端的原生渲染能力，页面滚动等核心场景中刷新帧率维持 120 fps
- **22 个内置组件** — 18 个 A2UI 协议组件 + 4 个 SDK 扩展组件
- **自定义组件 API** — 支持通过自定义组件 API 注册扩展原生组件，LLM 可通过组件名称生成组件描述数据
- **Function Call 集成** — 支持注册端侧工具/函数，LLM 可指定执行特定的工具/函数
- **Design Token 与主题** — 同步支持三端的 Design Token 和主题模式
- **日 / 夜间模式** — 已配备亮色 / 暗色切换功能

---

## 架构设计

AGenUI 采用**共享 C++ 核心引擎 + 三端组件渲染引擎**的设计。
C++ 层实现三端通用的流式数据解析、虚拟组件树管理、绘制数据缓存、主题/样式解析、组件变化 diff 识别等能力。C++ 层将解析后的组件协议同步到三端渲染引擎触发绘制。

<div align="center">
<img src="docs/images/architecture.svg" alt="AGenUI SDK 架构" width="720"/>
</div>

### 工程目录说明
| 路径 | 内容 |
|---|---|
| `engine/` | C++ 核心引擎 — 解析器、差分算法、布局、Function Call 框架 |
| `engine/include/` | 供平台桥接层引用的公共 C++ API |
| `platforms/ios/` | iOS 组件渲染器 + Objective-C 桥接层 |
| `platforms/android/` | Android 组件渲染器 + JNI 桥接层 |
| `platforms/harmony/` | HarmonyOS 组件渲染器 + NAPI 桥接层 |
| `playground/` | 三端演示应用，用于开发与调试 |
| `scripts/` | 各平台构建脚本 |

---

## 组件

<div align="center">
<img src="docs/images/components.svg" alt="AGenUI 内置组件" width="760"/>
</div>

### A2UI 协议组件

以下 18 个组件实现了 A2UI 协议规范，三端均支持。

| 组件 | 说明 |
|---|---|
| `Text` | 样式化文本，支持 h1–h5、body、caption 等变体 |
| `Image` | 网络图片，支持缩放模式与圆角 |
| `Icon` | 通过 Unicode / SVG 映射渲染图标 |
| `Divider` | 水平或垂直分隔线 |
| `Video` | 原生视频播放器，支持拖拽进度、控件自动隐藏 |
| `AudioPlayer` | 音频播放器，含进度条 |
| `Button` | 可点击按钮，触发 Action 事件 |
| `Row` | 水平弹性容器 |
| `Column` | 垂直弹性容器 |
| `Card` | 带阴影的卡片容器 |
| `List` | 可滚动列表，支持静态子项或模板驱动子项 |
| `Tabs` | 标签栏，支持可切换面板 |
| `Modal` | 原生对话框浮层 |
| `TextField` | 文本输入框，支持可选校验 |
| `CheckBox` | 布尔开关 |
| `Slider` | 数值范围输入 |
| `ChoicePicker` | 单选 / 多选选择器 |
| `DateTimeInput` | 日期与时间选择器 |

### SDK 内置扩展组件

以下 4 个组件随 SDK 捆绑发布，不属于 A2UI 协议规范。

| 组件 | 说明 |
|---|---|
| `Table` | 数据表格，使用 Yoga 子布局 |
| `Carousel` | 图片 / 内容轮播 |
| `Web` | 内嵌 WebView |
| `RichText` | HTML 富文本渲染 |

### Playground 示例组件

以下 3 个组件在 Playground 中完成注册和应用，用于演示如何通过 API 接入自定义组件。

| 组件 | 说明 |
|---|---|
| `Chart` | 柱状图、折线图、饼图 |
| `Markdown` | Markdown 渲染，支持流式输出 |
| `Lottie` | Lottie 动画播放 |

你可以在运行时使用相同的 API 注册自己的组件，参见[快速上手](#快速上手)章节。

---

## Catalog 文件

仓库根目录提供了 `agenui_catalog.json`，这是一个符合 [A2UI v0.9 规范](https://github.com/google/A2UI/blob/main/docs/concepts/catalogs.md)的**独立（[freestanding](https://github.com/google/A2UI/blob/main/docs/concepts/catalogs.md#freestanding-catalogs)）Catalog JSON Schema 文件**，所有外部 `$ref` 均已内联，无需任何外部依赖即可独立使用。

**覆盖范围：**

- **25 个组件**：18 个 A2UI 协议标准组件 + 4 个 SDK 内置扩展组件（`Table`、`Carousel`、`Web`、`RichText`），以及 3 个 Playground 示例组件（`Chart`、`Markdown`、`Lottie`）
- **14 个函数**：覆盖校验、格式化、逻辑运算与 URL 跳转
- **公共类型定义**：包含 A2UI v0.9 标准类型，以及 AGenUI 额外扩展的 `Styles`（通用样式属性）和 `TextStyles`（文本专属样式属性）

**如何使用：**

在对接 LLM Agent 时，将此文件内容或其托管 URL 作为 `catalogId` 写入 [A2UI ClientCapabilities](https://github.com/google/A2UI/blob/main/docs/concepts/catalogs.md#a2ui-catalog-negotiation) 的 `supportedCatalogIds` 字段，Agent 将以此 Schema 为约束生成严格合规的 A2UI JSON 消息，确保渲染器能够正确解析和渲染所有下发的 UI 组件。

---

## 快速上手

### 工具链要求

| 平台 | 工具链 |
|---|---|
| Android | Android Studio Hedgehog+、NDK 27.3.13750724、API 35 SDK、JDK 11 |
| iOS | Xcode 15+、CocoaPods、CMake |
| HarmonyOS | DevEco Studio 4.0+、ohpm |

### 从源码构建

所有构建脚本位于 `scripts/` 目录。`engine/` 中的 C++ 引擎会自动编译，无需单独准备。

**Android**

```bash
# Release AAR（默认）
./scripts/android/build.sh

# Debug AAR
./scripts/android/build.sh --debug

# 发布到本地 Maven（~/.m2）
./scripts/android/build.sh --publish-local

# 构建前清理
./scripts/android/build.sh --clean
```

AAR 输出到 `dist/android/release/`。

**iOS**

```bash
# XCFramework（Release，默认）
./scripts/ios/build.sh

# 单架构 Framework，Debug
./scripts/ios/build.sh -t framework -c Debug

# 强制 pod install 后构建
./scripts/ios/build.sh --pod-install
```

**HarmonyOS**

```bash
# HAR 包（Release，默认）
./scripts/harmony/build.sh

# Debug 构建
./scripts/harmony/build.sh --mode debug

# 自定义输出目录
./scripts/harmony/build.sh -o /path/to/output
```

### 使用 Playground 调试

我们在目录 `playground` 中为每个平台建立了独立的 Playground 工程应用，你可以打开并运行 Playground 工程，体验完整的 A2UI 组件渲染效果。Playground 工程直接引用 AGenUI 源码，并支持进行断点调试。

**Android Playground**

- 使用 Android Studio 打开 `playground/android/`
- 触发 gradle 安装依赖并检索代码文件
- 可修改 `gradle.properties` 中的配置调整依赖模式：

```
# 源码模式：修改 c++ engine 和 Android 端渲染引擎代码时立即生效
agenui.sdk.source=true

# AAR 模式：不再引用源码，而是引用已经打包的 SDK 构建包（推荐仅调试 Playground 时使用）
agenui.sdk.source=false
```

**iOS Playground**

- 进入目录 `playground/ios/Playground`
- 运行 `pod install` 安装依赖并生成 Xcode 工程。执行成功后，将在 `playground/ios/Playground` 目录生成 `Playground.xcworkspace`
- 打开工程 `Playground.xcworkspace`，选择模拟器，触发代码编译和运行

*Playground.xcworkspace 已经同时包含 AGenUI 源码和 Playground target*

**HarmonyOS Playground**

- 使用 DevEco Studio 打开 `playground/harmony` 目录
- 触发安装依赖并检索代码文件
- 选择模拟器，触发代码编译和运行

*项目通过 `srcPath` 引用 `platforms/harmony/agenui/`，因此对 AGenUI 源码的任何修改在下次构建时都会自动生效*

### 接入 AGenUI SDK

关于如何将源码构建为可直接集成的产物（见[从源码构建](#从源码构建)）以及如何接入到应用中的详细介绍，请参考：

> SDK 安装与使用指南：[快速开始](docs/QuickStart.zh-CN.md)
>
> 完整 SDK API 参考：[API 参考](docs/API.zh-CN.md)

---

## 贡献指南

欢迎各种形式的贡献——Bug 修复、新组件、平台改进、文档完善和测试覆盖。

提交 Pull Request 前，请阅读 [CONTRIBUTING.md](CONTRIBUTING.md) 了解完整工作流、代码风格规范（C++、Swift、Java 遵循 Google Style Guide，ArkTS 遵循 OpenHarmony 规范）以及 PR 检查清单。

**简要流程：**

1. Fork 仓库，从 `main` 创建分支：`fix/123-my-fix` 或 `feat/my-feature`
2. 完成修改，在适当位置补充测试
3. 在受影响的平台上本地构建并测试
4. 向 `main` 提交 PR，清晰描述*做了什么*以及*为什么*
5. 至少需要一位维护者审批才能合并

对于较大范围的变更——新平台支持、重大引擎重构、新组件品类——请先提 Issue 对齐方案，再开始编写代码。

---

## 许可证

AGenUI 基于 [Apache License, Version 2.0](LICENSE) 发布。
