# 更低成本地生成A2UI协议：Express DSL的功能特性

### 前言

在将A2UI应用到生产环境中时，会遇到一个绕不开的问题：如何让LLM高效、低成本、稳定地生成高质量的协议。这个问题中的几个评估要素包括生成耗时、token消耗、语法准确率（静态规则）、语义准确率（满足意图）等。这个问题直接影响了A2UI协议在真实业务落地中的成本和效果。

在2026年6月份，A2UI官方引入了一个实验性的特性，[Express DSL](https://github.com/a2ui-project/a2ui/tree/main/specification/proposals/express)。似乎为上面的问题提供了一个解题路径，但是解决问题的效果怎么样还不得而知。下面我们就来探索一下Express DSL的规则、使用方法和优化效果。

#### Express DSL的定位

它这不是一个新协议，而是一种LLM生成的、高度压缩的的DSL。即模型生成紧凑的 Express DSL，再由一个编译器把它编译回标准的 A2UI v1.0 JSON。官方文档对它的一句话定义是：

> "A2UI Express is a compact, model-optimized declarative syntax... It acts as an intermediate, highly compressed representation that on-device large language models generate to describe user interfaces. A host-side compiler parses this syntax and compiles it into standard A2UI v1.0 wire protocol payloads." —— `specification/proposals/express/a2ui_express.md`

#### Express DSL的当前进展

我们需要特别说明Express DSL的当前状态。

*   核心介绍文档在 `specification/proposals/` 目录下，而不是已认证的 `specification/v1_0/` 目录。也就是说，它是一个**提案（proposal）**，而非正式规范。
    
*   在官方提供的python版本agent sdk中。必须设置环境变量 `A2UI_EXPRESS_ENABLED` 为true，才能启用Express功能。
    
*   官方在合入时刻意做了收敛。比如不影响原有agent的主链路；把实现放进 `a2ui.experimental.express` 命名空间中等。
    

虽然这个功能并非A2UI的正式规范，依然在演进和验证阶段，但是作为真实业务落地问题的一个探索型解法，还是值得研究的。

---

### Express DSL的设计目标和原则

官方在技术规范里明确列出了 A2UI Express 的四个核心设计目标，这也正是它的设计意图所在。

*   **减少token消耗。** 生成式模型在产出冗长 JSON 时会消耗大量输出 token。Express DSL去掉了结构键、括号和重复引号，官方测试结果显示，相比原生 A2UI协议，输出 token消耗明显降低。
    

原始A2UI协议：

```plaintext
{
  "id": "root",
  "component": "Card",
  "child": "main_column"
},
{
  "id": "main_column",
  "component": "Column",
  "children": [
    "header_row",
    "route_row"
  ],
  "align": "stretch"
}
```

Express DSL：

```plaintext
root = Card(main_column)
main_column = Column([header_row, route_row], "stretch")
```

*   **端侧小模型优化。** Express DSL支持在端侧小模型生成，比如官方提到的模型Gemma 4 E2B / E4B等。这类模型的上下文窗口有限、推理预算紧张。得益于Express DSL清晰的规则，较少的协议闭合规则等，让模型能在较小的推理空间内完成生成。
    
*   **兼容流式输出。** Express DSL 采用行式（line-oriented）语法，即每个组件定义在单独的一行，使得编译器可以逐行解析、逐步构建组件树，在模型还没输出完时就能渐进式渲染。
    
*   **协议对齐。** Express DSL与标准 A2UI v1.0 保持对齐，比如完整支持数据绑定、客户端校验规则、本地事件处理等。
    

从这四点来看。A2UI官方在希望保持A2UI协议完整性、标准化能力的基础上，用一个中间 DSL，把"生成UI"这件事变得更便宜、更快、更适合在端侧的小模型上运行。

---

### 协议规则与使用方式

#### 基本语法

Express DSL的所有 UI 布局都包裹在 `<a2ui>` 和 `</a2ui>` 标签里，用来和普通对话文本区分开。在标签内部的正文部分，每一行都代表了一个组件或数据绑定定义：

```plaintext
<a2ui>
$数据绑定key = 数据绑定value
变量名 = 组件名(参数1, 参数2, ...)
</a2ui>
```

**核心规则**：

*   必须存在`root` 变量，代表整个协议代表的页面树的根节点。这与标准A2UI协议要求一致。
    
*   **禁止内联嵌套**：组件构造只能出现在赋值语句右侧，不能作为参数直接塞进另一个组件。要引用子组件，必须先给它单独声明一个变量，再用变量名引用。这条规则是为了消除复杂括号带来的语法错误，并支持逐行流式编译。
    
*   **变量名遵循 Unicode 标识符规范**：字母或下划线开头，后接字母、数字、下划线。
    

#### 协议示例

下面是官方示例中[飞机行程卡片](https://github.com/a2ui-project/a2ui/blob/main/specification/proposals/express/examples/01_flight-status.a2ui)（FlightStatus）的Express DSL代码：

```plaintext
<a2ui>
$/arrivalTime = "2025-12-15T14:30:00Z"
$/date = "2025-12-15"
$/departureTime = "2025-12-15T10:15:00Z"
$/destination = "New York"
$/flightNumber = "OS 87"
$/origin = "Vienna"
$/status = "On Time"
root = Card(main_column)
main_column = Column([header_row, route_row, divider, times_row], _, "stretch")
header_row = Row([header_left, date], "spaceBetween", "center")
header_left = Row([flight_indicator, flight_number], _, "center")
flight_indicator = Icon("send")
flight_number = Text($/flightNumber)
date = Text(formatDate($/date, "E, MMM d"), "caption")
route_row = Row([origin, arrow, destination], _, "center")
origin = Text(formatString("## ${/origin}"))
arrow = Text("## →")
destination = Text(formatString("## ${/destination}"))
... ...
</a2ui>
```

参考：飞机行程卡[原始A2UI协议](https://github.com/a2ui-project/a2ui/blob/main/specification/v1_0/catalogs/basic/examples/01_flight-status.json)。

需要注意的是，在官方提供的脚本和agent\_sdk中的逻辑中，当编译器把上述协议还原成标准 A2UI v1.0 协议时，是一个 `createSurface` 事件内部包括`components`和`dataModel`字段（v1.0新支持的特性），而非单独的`createSurface`、`updateComponents`和`updateDataModel`事件。

#### 规则速查表

| **维度** | **写法** | **说明** |
| --- | --- | --- |
| 组件定义 | `varname = ComponentName(arg1 ...)` |  |
| 字符串 | `"文本"` / `"""多行"""` / `r"正则\d+"` | 支持转义串与原始串（raw string），正则很方便 |
| 数字 / 布尔 / 空 | `42` / `true` / `null` |  |
| 列表 | `[child1, child2]` | 映射到容器的子槽位 |
| 数据绑定 | `$/user/email`/ `$lastName` | `$` 前缀绑定到数据模型 |
| 数据填充 | `$/title = "启用通知"` | 直接给数据路径赋值，写进 `dataModel` |
| 动态列表模板 | `List(_template($/breeds, tpl), "horizontal")` | `_template` 辅助函数生成模板子列表 |
| 服务端事件 | `Event("save_deal", {rep: $/form/rep})` |  |
| 客户端函数 | `openUrl("https://...")` | 直接按 catalog 签名调用 |
| 删除surface | `deleteSurface("surface-1")` | 独立语句，无需赋值 |

#### 省 token 的核心机制

Express DSL省 token 的核心在于它省略了所有属性名（key），完全靠"参数位置"来映射。并且省略了所有json格式中的换行、括号等。

上段话中提到：属性要靠"参数位置"来映射。下面我们来解析下它的含义。

按照Express DSL的规则，组件属性的解析顺序完全由在 catalog 定义的顺序决定。Express 不硬编码任何组件名或属性，换 catalog、扩展组件都不用改编译器代码。

比如以下A2UI协议：

```plaintext
{
  "id": "tc_root",
  "component": "Row",
  "children": [
    "tc_card"
  ],
  "align": "stretch",
  "justify": "spaceBetween"
}
```

转换后的Express DSL协议为：

```plaintext
tc_root = Row([tc_card], "spaceBetween", "stretch")
```

很明显，从以上协议转换规则来看，`justify`在`align`之前。那么，如果没有注明`justify`属性呢？Express DSL也定义了规则应对这种情况：

*   中间要跳过的可选参数用下划线 `_` 占位。
    

*   尾部的可选参数可以直接省略。
    

如果`justify`没有指定，那么转换后的Express DSL就是：

```plaintext
tc_root = Row([tc_card], _, "stretch")
```

需要注意的是，以上规则带来一个潜在的问题问题：如果对A2UI进行扩展，引入了新的属性。那么接入Express DSL时必须做相应的适配，否则新扩展的属性无法被正确解析。

#### 输入 catalog 的压缩方法

既然要让模型生成 Express DSL，那喂给模型的 catalog 也不能再是原始 JSON Schema 了。并且将catalog压缩，也是节省输入token的一个手段。

官方提供了 `ExpressPromptGenerator`，把 catalog 编译成紧凑的格式塞进LLM的prompt中。验证命令为：

```plaintext
// 进入目录specification/proposals/express
A2UI_EXPRESS_ENABLED=true uv run --project ../../../agent_sdks/python/a2ui_agent scripts/run_prompt_generator.py --catalog ../../v1_0/catalogs/basic/catalog.json
```

命令执行成功后，会生成一段prompt，其中就包含了对组件、属性、函数等的说明。

```plaintext
• Button(child (component ID), variant? (static only), action (static only), weight? (static only))
  - child: The ID of the child component...
  - variant: ... Must be one of: 'default', 'primary', 'borderless'
```

#### 何时转换到标准 A2UI 协议

对于Express DSL，不免有一个疑问：是在server agent侧还是客户端侧将Express DSL转换为A2UI协议呢？

当前官方设计是：

*   模型直接输出 Express DSL 
    
*   server agent 侧编译器转换为标准 v1.0 协议
    
*   将 A2UI 协议下发到客户端进行渲染
    

这种处理方式的优势是显而易见的：

1.  职责分离。server agent + LLM负责生成完整的A2UI协议，内部可以实现规则检测、循环验证等手段保证协议准确性
    
2.  保证客户端渲染器能力纯粹性，只面向A2UI协议，不引入其他协议规则
    

---

### Express DSL的收益

基于A2UI在官方在提交提案时展示的数据，Express DSL在节省token方面还是有比较大的提升的。

使用轻量模型 `gemini-3.1-flash-lite`、47 个样本，对比"标准A2UI"与"Express DSL"两种策略：

| **策略** | **语法准确率** | **语义准确率** | **平均延迟** | **输出 token** | **总 token** |
| --- | --- | --- | --- | --- | --- |
| 标准A2UI | 87.23% | 93.62% | 4.99s | 35,022 | 527,882 |
| Express DSL | 91.49% | 84.04% | 1.09s（↓78%） | 9,912（↓72%） | 232,748（↓56%） |

首先，协议生成延迟和token消耗大幅降低，这个方面达成了Express DSL设计的目的。同时，语法准确率（即生成协议的静态规则准确性）还有一定的提升，这个属于意外的惊喜。

但是，语义准确率反而降低了。语义准确率是通过另外一个更高级的LLM对生成的协议和输入的需求进行评分，评估结果满足需求意图的程度。从结果来看，引入简化、清晰规则的Express DSL后，反映静态规则的语法更准确了，反映模糊意图的语义更模糊了。

我们团队实地验证过Express DSL的优化效果，和A2UI官方提供的结论基本一致。我们必须充分了解Express DSL特点的两面性，这能帮助我们更好地决策它适合什么业务场景。

---

### Express DSL优缺点汇总

#### Express DSL的优点

*   **节省token、低延迟**
    
*   **端侧友好**。支持端侧小模型 ，适合离线、隐私、边缘场景。
    
*   **降低了模型门槛**：使用编译器进行规则检测和转化，语法准确率提升，降低对强模型的依赖度
    
*   **流式渐进渲染**：每个组件单独一行的定义方式天然支持流式渲染
    
*   **端侧渲染器零侵入**：将Express DSL的生成和转换封装在服务端，保持端侧渲染层纯净
    

#### Express DSL的缺点和风险

*   **语义准确率下降**。可能无法适合复杂样式的生成。
    
*   **转换规则复杂度** 。当前官方提供的示例中，不产出独立 `updateComponents`事件，不太符合固有使用习惯
    
*   **协议理解成本**。开发者在理解A2UI协议后，不得不掌握Express DSL规则，提高了成本
    
*   **适配成本**。新增事件、属性、修改规则后可能要适配编译器
    
*   **仍是实验特性**：Express DSL属于 `proposal`，非正式协议，有可能变动
    

#### 推荐考虑接入的场景

*   调用量大，对token 成本敏感；或倾向使用端侧 / 离线小模型
    
*   生成的样式较为简单，接受一定程度的语义差异
    
*   能够接受在服务端加一道"编译 + 校验 + 重试"的能力，以适配弱模型
    

#### 不推荐接入的场景

*   主要使用前沿大模型，对token消耗没有过多限制
    
*   需要生成复杂样式，对语法准确性、语义准确性有同等的要求
    
*   对协议进行了扩展，且持续演进。不希望引入额外的Express DSL维护成本
    
*   需要稳定性 / 标准化保证（Express DSL仍是提案和实验特性）。
    

### 总结

Express DSL 是A2UI官方在"生成式 UI 的成本优化，业务落地友好型"这个方向上的一次务实探索。它不在于解决"让模型生成的更对更好看"的问题，而在于"让模型生成的成本更低"。虽然它还处在实验阶段，但是值得保持关注和小范围试点验证。

> 如果你对生成式 UI 感兴趣，欢迎关注我们的开源项目 [AGenUI](https://github.com/AGenUI/AGenUI)——**支持 iOS、Android 和 HarmonyOS 的高性能 A2UI 渲染引擎**。它基于共享 C++ 核心引擎 + 三端原生渲染架构，完整实现了 A2UI v0.9 协议，能够在移动设备上实时流式渲染 LLM 生成的可交互 UI。欢迎 Star、试用、提 Issue，一起推动生成式 UI 在移动端的落地。