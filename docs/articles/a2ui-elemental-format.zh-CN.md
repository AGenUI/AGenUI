# 更"LLM友好"的 A2UI 生成格式：Elemental的功能特性

## 前言

[A2UI Elemental](https://github.com/a2ui-project/a2ui/tree/main/specification/proposals/elemental) 是 A2UI 官方在7月初引入的实验性特性。它采用类 HTML 的自定义标签协议来描述UI布局，目的是提高 A2UI 协议生成效率，降低协议生成门槛。从这一点来说它似乎和 Express DSL 的作用有些类似，但它们在数据载体格式上又有一些不同。A2UI Elemental 并非正式的协议规范，目前仍然是一个实验性质的提案。它的易用性、业务落地的真实效果等等还需要经过一定时间的检验。

本文我们将解读一下 A2UI Elemental的核心特征、使用方式，以及对我们接入 A2UI 协议的参考和启示。

### Elemental 介绍

它是一种面向模型生成侧优化的声明式 UI 编码。从技术路线上看，它和 Express DSL 是相同的，即生成侧使用模型友好的、紧凑的协议，再使用编译器转换为标准的 A2UI JSON 协议供端侧渲染器消费。目标都是降低模型生成 A2UI 的成本、提高生成准确率。区别在于表达形态：一个是紧凑的 DSL，一个是类HTML的自定义标签协议。

```plaintext
<body id="dashboard-surface">
  <ui-card id="root">
    <ui-column id="main_column" align="stretch">
    // ...
    </ui-column>
  </ui-card>
</body>
```

### Elemental 的功能特性

Elemental 出现的背景之一是：标准A2UI JSON协议对于模型生成可能不太友好。A2UI JSON 协议的扁平邻接表和父子节点id的前向引用使得大模型不得不关注唯一id的生成、长程的父子关系引用等。

而 HTML 格式的嵌套层级、模型内化等特性天然就避免了上面的问题。以下是我们根据官方文档总结的Elemental的几个核心特性。

1.  **模型友好**：HTML/XML格式是 LLM 在预训练阶段内化的“自然语言”，比生成自定义的A2UI JSON schema更加轻松顺手。出错率也更低
    
2.  **Token 效率**：HTML 标签、属性、自闭合标签常被识别为单个 token，比 JSON 格式表达的协议更加节省
    
3.  **天然树状层级**：HTML 本身是树状的嵌套结构，模型可以很自然地理解包含关系。不必理解长程的邻接表引用
    
4.  **自描述结构**：协议中直接注明属性名和取值，而非 Express DSL 中按照属性定义顺序确认属性值的方式
    
5.  **流式渲染**：HTML 格式协议可以流式解析和渲染
    

---

## Elemental 的语法规则

### 基本语法结构

```html
<a2ui>
  <body id="dashboard-surface">
    <link rel="catalog" href="https://a2ui.org/.../catalog.json" />
    <!-- 组件/数据绑定写在这里 -->
  </body>
</a2ui>
```

*   最外层 `<a2ui>...</a2ui>` 是约定的标签，表示这个标签内部是UI描述协议。需要注意的是，标签`<a2ui>`在官方文档中没有提及，但是在编译器代码中进行了处理
    
*   `<body id>` 是对surface的定义。其中 `id` 就对应于 `surfaceId`；`<link rel="catalog">` 的 `href` 对应于引用的catalog
    

### 组件的定义规则

组件标签一律增加`ui-` 前缀，后面接schema中定义的组件名称，组件名称的规则为kebab-case（小写并以连字符分隔多词）。比如：`<ui-card>`、`<ui-button>`、`<ui-icon>`、`<ui-text-input>`。每个组件定义时建议定义`id`属性，并指定为唯一的值。在编译器转换阶段，如果组件不包含`id`，则会默认生成一个唯一的`id`。

组件的属性直接具名定义，可以不按照约定顺序定义属性，比如：

```plaintext
<ui-row id="header_row" justify="spaceBetween" align="center">
```

Elemental采用树状的嵌套层级定义节点包含关系，这是区别于A2UI JSON和Express DSL的最显著特征。

```plaintext
<ui-row id="header_row" justify="spaceBetween" align="center">
  <ui-row id="header_left" align="center">
    <ui-icon id="flight_indicator" name="send" />
    <ui-text id="flight_number" text="{$/flightNumber}" />
  </ui-row>
</ui-row>
```

### 属性类型的定义规则

根据属性值类型的不同，定义了不同的表示方式。

| **A2UI 属性值** | **Elemental 写法** | **示例** |
| --- | --- | --- |
| 字符串 / 枚举 | 原始类型 | `align="stretch"`、`variant="body"` |
| 数字 / 布尔 / null | 引号包花括号 `{...}` | `count="{4}"`、`checked="{true}"` |
| 数据绑定 | `{$/...}` | `value="{$/user/name}"`、`{$name}` |
| 函数 / 表达式 | `{fn(...)}` | `text="{formatCurrency(value: $/total, currency: 'USD')}"` |

此外，Elemental定义了`<script>`标签用于表示一串原生的JSON协议，目前主要用于dataModel数据协议的下发。这部分下面会再介绍。比如：

```plaintext
<script type="application/json" slot="columns">[...]</script>
```

### updateDataModel事件的表示方法

updateDataModel事件在Elemental格式中，被`<script>`标签包括，内部直接使用原始的JSON协议。比如原始 A2UI JSON 的 updateDataModel 协议内容为：

```plaintext
{
  "version": "v1.0",
  "updateDataModel": {
    "surfaceId": "gallery-flight-status",
    "value": {
      "flightNumber": "OS 87",
      "date": "2025-12-15",
      "origin": "Vienna",
      "destination": "New York"
    }
  }
}
```

转换为 Elemental 格式后，表示方式为：

```plaintext
<body id="gallery-flight-status">
  <script type="application/json">
    {
      "flightNumber": "OS 87",
      "date": "2025-12-15",
      "origin": "Vienna",
      "destination": "New York"
    }
  </script>
</body>
```

### 将Catalog 转换为"模型友好"型

既然要让模型生成 类 HTML 格式的协议，那么也不能将json格式的catalog直接作为prompt的一部分输入给模型了。并且，将catalog压缩，也可以节省一定的输入token。

Elemental 提案中，它把 catalog 的 JSON Schema 翻译成 TypeScript 格式作为输入prompt的部分内容。

比如，下面是system prompt的一部分。

```typescript
## Workflow Description:
# A2UI Elemental Output Contract

You must output the user interface using A2UI Elemental HTML5-like markup.
You MUST surround the entire block with the sentinel tags `<a2ui>` and `</a2ui>`.
Inside the sentinel tags, surround the UI layout with `<body>` and `</body>` tags, including a `<link rel="catalog" href="https://a2ui.org/specification/v1_0/catalogs/basic/catalog.json">` at the start.

## HTML5 Markup Rules

1. Prefix component tags with `ui-` in kebab-case (e.g., `<ui-text-field />`).
2. Provide a unique `id` attribute for every component. The top-level root element must have `id="root"`.
```

而对于组件、属性描述等，则转换为TypeScript格式。

```typescript
type DataBinding = string;
type A2UIElement = string; // ID of the referenced component
type FunctionCall = string; // A catalog function call expression, e.g. "{formatString('Title: ${/path}')}" or "{regex(pattern: '^[A-Z]')}" 

// Tag: <ui-audio-player>
interface AudioPlayer {
  id?: string;
  // The URL of the audio to be played.
  url: string | DataBinding;
  // ...
}
```

### Elemental 的生成与验证

Elemental 在 A2UI 的 `agent_sdks` 中已经有完整的编译器、反编译器和 prompt 生成器实现，   但官方仓库没有提供像 Express DSL 那样便捷的命令行验证入口。为此我们补充了三个脚本，并已   [贡献给 A2UI 官方（PR #2099）](https://github.com/a2ui-project/a2ui/pull/2099)：

*   `run_compiler.py`：将Elemental 标记转换为标准 A2UI v1.0 JSON
    
*   `run_decompiler.py`：将标准 A2UI JSON 转换为 Elemental 标记
    
*   `run_prompt_generator.py`：将basic catalog 转换为 Elemental 的 system prompt（含组件的 TSX 接口签名）
    

该PR中同样提交了作为`Elemental developer guide`的README文档。下载PR的脚本后，按照README中的描述，执行以下命令即可进行协议转换的验证。

*   进入elemental目录
    

```plaintext
cd specification/proposals/elemental
```

*   将01\_flight-status.json转换为Elemental格式并保存到flight.elemental文件中
    

```plaintext
uv run --project ../../../agent_sdks/python/a2ui_agent \
  scripts/run_decompiler.py
  ../../v1_0/catalogs/basic/examples/01_flight-status.json 
  > flight.elemental
```

*   将flight.elemental转换为标准A2UI JSON格式
    

```plaintext
uv run --project ../../../agent_sdks/python/a2ui_agent \
    scripts/run_compiler.py flight.elemental
```

*   将basic catalog转换为输入给LLM的prompt
    

```plaintext
uv run --project ../../../agent_sdks/python/a2ui_agent \
  scripts/run_prompt_generator.py --catalog
  ../../v1_0/catalogs/basic/catalog.json
```

---

## Elemental的收益提升

基于A2UI 官方发布的[评测数据](https://github.com/a2ui-project/a2ui/blob/main/eval/baselines/elemental/run_meta.json)，我们可以看到相对于A2UI JSON格式，Elemental有哪些方面的提升。

| **格式** | **语法/结构准确率** | **语义准确度** | **输出 token中位数** | **输入token中位数** |
| --- | --- | --- | --- | --- |
| A2UI JSON | 0.980 | 0.902 | 975 | 10485 |
| Elemental | 0.980 | 0.941 | 539 | 5671 |

相比较于A2UI JSON，Elemental 输出 token 降低约 45%，输入 token 降低约 46%。在保证语法准确度不降低的情况下，语义准确度也提升。可以说是一个“全面的”提升。从评测结果来看，可能也印证了之前的一个判断：大模型输出类 HTML 格式更自然更准确。

---

## 总结

作为 A2UI 应用领域又一个“节能提效”的提案，从评测数据上看 Elemental 在各方面都有一定程度的提升。但是在决策是否在真实业务场景中使用时，我们必须摸清楚它的所有特性和约束。

首先，Elemental 面向的是协议生成侧，即大模型以及与模型交互的server agent。协议消费侧，即渲染器可能接收的还是标准A2UI JSON。所以，server agent可能需要完成协议的编译和转换，这是一个额外的工作量新增。

其次，一面是标准的A2UI JSON，另一面是Elemental。虽然它们有相同的组件schema、属性定义、事件定义等，但是在表达方式有较大的差异（**JSON vs HTML，邻接表 vs 嵌套层级**）。开发者不得不同时掌握这两类协议才能更好地将Elemental应用在真实业务场景。

最后，Elemental 现在依然是提案阶段，可能面临调整，而且在协议演进、规则检查等方面还需要更加完善的建设。但是，作为在"生成式 UI 成本优化"这个方向上的又一次务实探索，值得进行研究和验证。

> 如果你对生成式 UI 感兴趣，欢迎关注我们的开源项目 [AGenUI](https://github.com/AGenUI/AGenUI)——**支持 iOS、Android 和 HarmonyOS 的高性能 A2UI 渲染引擎**。它基于共享 C++ 核心引擎 + 三端原生渲染架构，完整实现了 A2UI v0.9 协议，能够在移动设备上实时流式渲染 LLM 生成的可交互 UI。欢迎 Star、试用、提 Issue，一起推动生成式 UI 在移动端的落地。