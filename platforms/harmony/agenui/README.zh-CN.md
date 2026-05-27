<div align="center">

# AGenUI — HarmonyOS SDK

**AGenUI：高性能鸿蒙 A2UI 渲染引擎**

![License: Apache 2.0](https://img.shields.io/badge/License-Apache%202.0-blue.svg)
![HarmonyOS](https://img.shields.io/badge/HarmonyOS%20NEXT-API%2017%2B-red)

[**在线 Demo**](https://genui.amap.com)


</div>

> 本项目正在积极开发与持续演进中，欢迎贡献代码、反馈意见和参与讨论。

---

## 什么是 AGenUI？

**AGenUI** 是同步支持 iOS、Android 和 HarmonyOS 三端的 A2UI SDK，它基于并完整实现了 Google 开源的 A2UI v0.9 协议，能够在移动设备上实时渲染 LLM 生成的可交互界面 UI 的流式数据。它底层由一个跨平台的**共享 C++ 核心引擎**驱动，HarmonyOS 渲染引擎则基于核心引擎下发的组件协议，使用系统原生能力完成绘制。

**AGenUI** 采用系统原生 UI 能力绘制**可交互的卡片、表单、列表、图片、媒体播放器等**，提供高性能、流畅的操作体验。

<img src="https://raw.githubusercontent.com/AGenUI/AGenUI/main/docs/images/a2ui_rendering_effect.png" alt="A2UI 组件效果展示" height="720">

---

## 核心特性

- **实时流式渲染** — 组件描述的结构化数据由 LLM 生成，流式增量出现并实时更新
- **高性能** — HarmonyOS 原生渲染能力，页面滚动等核心场景中刷新帧率维持 120 fps
- **22 个内置组件** — 18 个 A2UI 协议组件 + 4 个 SDK 扩展组件
- **自定义组件 API** — 支持通过自定义组件 API 注册扩展原生组件，LLM 可通过组件名称生成组件描述数据
- **Function Call 集成** — 支持注册端侧工具/函数，LLM 可指定执行特定的工具/函数
- **Design Token 与主题** — 支持 Design Token 和主题模式
- **日 / 夜间模式** — 已配备亮色 / 暗色切换功能

---

## 架构设计

AGenUI 采用**共享 C++ 核心引擎 + HarmonyOS 组件渲染引擎**的设计。
C++ 层实现三端通用的流式数据解析、虚拟组件树管理、绘制数据缓存、主题/样式解析、组件变化 diff 识别等能力。C++ 层将解析后的组件协议同步到 HarmonyOS 渲染引擎触发绘制。

<div align="center">
<img src="https://raw.githubusercontent.com/AGenUI/AGenUI/main/docs/images/architecture.svg" alt="AGenUI SDK 架构" width="720"/>
</div>

### HarmonyOS SDK 目录说明

| 路径 | 内容 |
|---|---|
| `src/main/cpp/` | HarmonyOS C++ 组件渲染器 + NAPI 桥接层 |
| `src/main/cpp/a2ui/render/components/` | 原生组件实现（22+ 个组件） |
| `src/main/cpp/a2ui/bridge/` | 平台桥接层（图片加载、URL、Function Call 等） |
| `src/main/ets/` | ArkTS 封装层 — 对外公开的 SDK API |
| `src/main/ets/agenui/bridge/` | ArkTS 到 C++ 层的桥接实现 |
| `src/main/ets/agenui/hybrid/` | 混合视图支持（WebView 等） |

---

## 组件

<div align="center">
<img src="https://raw.githubusercontent.com/AGenUI/AGenUI/main/docs/images/components.svg" alt="AGenUI 内置组件" width="760"/>
</div>

### A2UI 协议组件

以下 18 个组件实现了 A2UI 协议规范。

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

以下 3 个组件在 Playground 中完成注册和应用，用于演示如何通过 API 接入自定义组件，**不包含在 SDK 中**。

| 组件 | 说明 |
|---|---|
| `Chart` | 柱状图、折线图、饼图 |
| `Markdown` | Markdown 渲染，支持流式输出 |
| `Lottie` | Lottie 动画播放 |

你可以在运行时使用相同的 API 注册自己的组件，参见下文[接入 AGenUI SDK](#接入-agenui-sdk)章节。

---

## 快速上手

### 前提条件

- DevEco Studio 4.0 或更高版本
- HarmonyOS NEXT API 17 或以上

### 安装
```shell
ohpm install @agenui/agenui
```

### 依赖
```json
{
  "dependencies": {
    "@agenui/agenui": "0.9.9"
  }
}
```

### 接入 AGenUI SDK

**1. 创建 SurfaceManager 并渲染 UI**

实现 `ISurfaceManagerListener` 类以接收 Surface 生命周期回调：

```typescript
import { AGenUI, AGenUIContainer, SurfaceManager, ISurfaceManagerListener, Surface } from '@agenui/agenui';
import { common } from '@kit.AbilityKit';

class SurfaceListenerImpl implements ISurfaceManagerListener {
  private page: MyPage | null = null;

  constructor(page: MyPage) {
    this.page = page;
  }

  onCreateSurface(surface: Surface): void {
    if (this.page) {
      // 将 surfaceId 绑定到 AGenUIContainer
      this.page.surfaceId = surface.surfaceId;
    }
  }

  onDeleteSurface(surface: Surface): void {
    if (this.page) {
      this.page.surfaceId = '';
    }
  }

  onReceiveActionEvent(event: string): void {
    // 处理组件交互事件
  }

  onError(surface: Surface | null, code: number, message: string): void {
    // 错误码定义见 agenui_errorcode_define.h
  }
}

@Entry
@Component
struct MyPage {
  @State surfaceId: string = '';
  private surfaceManager: SurfaceManager | null = null;

  aboutToAppear(): void {
    const context = getContext(this) as common.UIAbilityContext;
    this.surfaceManager = new SurfaceManager(context);
    this.surfaceManager.addListener(new SurfaceListenerImpl(this));
  }

  build() {
    Column() {
      if (this.surfaceId) {
        AGenUIContainer({ surfaceId: this.surfaceId })
          .width('100%').height('100%')
      }
    }
  }
}
```

**2. 接收来自 LLM 流的 A2UI 协议数据**

每收到一个数据块时调用 `receiveTextChunk()`，引擎会增量重组并解析：

```typescript
// 发送三条 A2UI 协议消息
surfaceManager.receiveTextChunk(createSurfaceJson);    // {"createSurface": {...}}
surfaceManager.receiveTextChunk(updateComponentsJson); // {"updateComponents": {...}}
surfaceManager.receiveTextChunk(updateDataModelJson);  // {"updateDataModel": {...}}
```

**3. 注册主题（可选）**

```typescript
const success: boolean = AGenUI.registerDefaultTheme(themeJson, designToken);

// 切换日/夜间模式
AGenUI.setDayNightMode('dark'); // 'light' 或 'dark'
```

**4. 释放资源**

页面销毁时释放资源：

```typescript
aboutToDisappear(): void {
  this.surfaceManager?.destroy();
  this.surfaceManager = null;
}
```

---

## 贡献指南

欢迎各种形式的贡献——Bug 修复、新组件、平台改进、文档完善和测试覆盖。

提交 Pull Request 前，请阅读项目的贡献规范，了解完整工作流、代码风格规范（ArkTS 遵循 OpenHarmony 规范，C++ 遵循 Google Style Guide）以及 PR 检查清单。

**简要流程：**

1. Fork 仓库，从 `main` 创建分支：`fix/123-my-fix` 或 `feat/my-feature`
2. 完成修改，在适当位置补充测试
3. 在本地构建并测试
4. 向 `main` 提交 PR，清晰描述*做了什么*以及*为什么*
5. 至少需要一位维护者审批才能合并

对于较大范围的变更——新组件、重大引擎重构、新组件品类——请先提 Issue 对齐方案，再开始编写代码。

---

## 社区与联系

- **GitHub Issues**：[Bug 反馈和功能建议](https://github.com/AGenUI/AGenUI/issues)
- **GitHub Discussions**：[问答与通用讨论](https://github.com/AGenUI/AGenUI/discussions)
- **邮箱**：[tengjixiang.ttjx@alibaba-inc.com](mailto:tengjixiang.ttjx@alibaba-inc.com)
- **钉钉群**：技术交流群
- **微信群**：技术交流群

<div align="center">
<table>
  <tr>
    <th>钉钉群</th>
    <th>微信群</th>
  </tr>
  <tr>
    <td align="center"><img src="https://raw.githubusercontent.com/AGenUI/AGenUI/main/docs/images/dingtalk_qrcode.jpg" alt="AGenUI 钉钉群二维码" width="220"></td>
    <td align="center"><img src="https://raw.githubusercontent.com/AGenUI/AGenUI/main/docs/images/wechat_qrcode.jpg" alt="AGenUI 微信群二维码" width="220"></td>
  </tr>
</table>
</div>

---

## 许可证

AGenUI 基于 Apache License, Version 2.0 发布。
