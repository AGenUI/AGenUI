# 当 A2UI 遇上 MCP：生成式 UI 的一种新模式？

> 最近，A2UI 团队最近发起了一个 [Discussion](https://github.com/a2ui-project/a2ui/discussions/1676)，讨论 A2UI 结合 MCP 进行协议扩展的可行性。在声明式协议和开放式协议共同发展的今天，这个观点确实有点意思。作为 A2UI 协议的深度参与者，我们对此进行了调研和思考，在这里和大家分享。

### 先说结论

A2UI over MCP 的核心是：**A2UI 协议不仅可由 LLM 实时生成，MCP Server 也可以直接构造和下发 A2UI 协议**。这不是两个协议的简单拼接，而是对"A2UI 协议真实赋能业务解题"的一次重新思考。

---

### A2UI vs MCP：两个协议的特点

在聊 A2UI over MCP 之前，我们先快速回顾一下这两个协议各自的定位。

#### A2UI：安全、跨平台的声明式 UI 协议

[A2UI](https://github.com/google/A2UI) 是 Google 开源的一套声明式 UI 协议。它的设计哲学是：**LLM 负责定义"界面长什么样"的结构化描述，客户端渲染器负责把这些描述画出来**。

这种架构天然带来几个好处：

*   **安全**：客户端不执行任何来自服务端的代码，只解析声明式的 JSON 数据
    
*   **跨平台**：同一份 A2UI 协议，可以在 Web、iOS、Android、HarmonyOS 上由各自的渲染器绘制
    
*   **流式渲染**：支持增量更新，LLM 一边生成一边渲染，体验流畅
    

但它也有一个明显的限制：**A2UI 协议本身不支持执行复杂的客户端逻辑**。它是声明式协议，能描述"一个按钮点击后触发某个 action"，但做不到"点击后执行一段计算逻辑再更新 UI"。对于需要复杂交互的业务场景，这是一个实实在在的瓶颈。

#### MCP：灵活、强大的工具协议

[MCP](https://modelcontextprotocol.io/)（Model Context Protocol）是 Anthropic 提出的一套开放协议，本质上是给 AI 模型提供了一种标准化的方式来调用外部工具和访问资源。在此基础上，MCP 进一步推出了 [MCP Apps](https://modelcontextprotocol.io/specification/2025-12-17/server/apps)——通过 iframe 沙盒承载完整的 HTML 应用，支持复杂的交互逻辑。

MCP（及 MCP Apps）的表达力非常强：你可以在里面跑 JavaScript，做状态管理，实现任意复杂的 UI 交互。但它面临一些问题：

*   **安全性**：需要 iframe 沙盒隔离
    
*   **性能开销**：HTML/JS 在沙盒中运行，相比原生渲染有性能损耗
    
*   **体验割裂**：嵌入的 Web 页面和原生 UI 之间可能存在视觉和交互上的不一致
    

简单来说：**A2UI 安全但逻辑受限，MCP 灵活但存在安全和体验代价**。在特点上两者似乎有一种天然的互补关系。

---

### A2UI over MCP：不只是"拼在一起"

理解了各自的优劣，再来看 A2UI over MCP 就很自然了。

Google A2UI 团队的意图很明确：**在 A2UI 的安全性与 MCP 的表达力之间找到融合点，给开发者提供更多生成式 UI 落地时的技术选型空间**。

这里有一个关键的思路转变：传统的 A2UI 落地方式是LLM 生成协议，然后客户端渲染，而 A2UI over MCP 拓宽了这个范式——**MCP Server 可以直接构造 A2UI 协议数据，不需要经过 LLM 的推理和生成**。A2UI 协议从"LLM 输出的声明式UI协议"变成了"一种通用的 UI 描述语言"。

这意味一个 MCP Server 可以像构建 API 响应一样构建 A2UI 协议，客户端渲染器照常解析和绘制。整个过程不需要 LLM 参与，但 UI 依然享有 A2UI 协议的安全性和跨平台能力。

---

### A2UI over MCP的三种集成模式

A2UI over MCP 并不是一种固定的技术方案，而是提供了三种不同的集成模式。每种模式在安全性、表达力和架构复杂度上各有侧重。

#### 模式一：MCP Server 直接下发 A2UI 协议

**一句话概括：MCP Server 构建 A2UI JSON，客户端渲染器画出来。**

这是最直接的集成方式。在 MCP 的 Resource 或 Tool Call 链路中，把 A2UI 协议 JSON 作为响应内容下发。客户端侧只需要接入 A2UI 渲染器即可。

这种模式又分为两种子模式：

**Static 模式**：MCP Server 直接返回预设的 A2UI JSON，没有任何逻辑运算。适合展示固定内容的场景。

```python
@app.read_resource()
async def read_resource(uri: str) -> list[ReadResourceContents]:
    if str(uri) == "a2ui://recipe-form":
        return [ReadResourceContents(
            content=json.dumps(recipe_form_json),  # 预设的 A2UI JSON
            mime_type="application/a2ui+json",
        )]
```

**Tool Call 模式**：客户端携带参数请求，MCP Server 根据参数执行逻辑后组装 A2UI 协议数据返回。可以基于预设模板进行数据填充。

```python
@app.call_tool()
async def handle_call_tool(name: str, arguments: dict) -> CallToolResult:
    if name == "get_recipe_a2ui":
        custom_recipe_json = copy.deepcopy(recipe_a2ui_json)  # 基于模板
        for action in custom_recipe_json:
            if "updateDataModel" in action:
                action["updateDataModel"]["value"] = {
                    "title": recipe_data["title"],    # 注入动态数据
                    "rating": recipe_data["rating"],
                }
        return CallToolResult(content=[EmbeddedResource(
            resource=TextResourceContents(
                uri="a2ui://recipe-card",
                mimeType="application/a2ui+json",
                text=json.dumps(custom_recipe_json),
            )
        )])
```

这种模式的优势在于简单、安全，MCP Server 只负责数据和协议构造，渲染完全在客户端完成。同一套 MCP Server 产出的 A2UI 数据可以在不同平台的渲染器上呈现。

![image.png](https://alidocs.oss-cn-zhangjiakou.aliyuncs.com/res/Q35O851yW8VJWl9V/img/7ceee61a-21ac-4b4c-955d-9236ee78db80.png)

#### 模式二：A2UI 中嵌入 MCP Apps

**一句话概括：A2UI 是容器，MCP Apps 是嵌入的内容。**

在已接入 A2UI 渲染器的页面中，注册一个自定义的 `McpApp` 组件，用 iframe 沙盒承载 MCP Apps 的 HTML 页面。A2UI 协议中通过 `content` 字段下发 MCP Apps 的内容：

```json
{
  "component": {
    "McpApp": {
      "content": {"literalString": "url_encoded:%3C!doctype%20html%3E..."},
      "title": {"literalString": "Calculator"},
      "allowedTools": ["calculate"]
    }
  }
}
```

在这种模式下，A2UI 渲染器充当 MCP Apps 与后端 Server 之间的交互桥梁。MCP Apps 内部的工具调用需要封装为 A2UI Action，经由渲染器转发。

![image.png](https://alidocs.oss-cn-zhangjiakou.aliyuncs.com/res/Q35O851yW8VJWl9V/img/22d8bd06-5830-4f53-9ca7-bf3752daa5fa.png)

这种模式的核心价值是：**让接入了 A2UI 的页面获得更强的表现力和复杂交互能力**。A2UI 原生组件负责主体 UI，遇到需要复杂逻辑的局部区域，交给嵌入的 MCP Apps 处理。

#### 模式三：MCP Apps 中嵌入 A2UI

**一句话概括：MCP Apps 是容器，A2UI 是嵌入的内容。**

这个模式的方向和模式二恰好相反。客户端页面首先是一个 MCP Apps 容器（iframe），内部挂载 A2UI 渲染器（必须是web渲染器）。A2UI 协议数据通过 MCP 的 Resource 或 Tool Call 获取，然后在 MCP Apps 内部渲染。

![image.png](https://alidocs.oss-cn-zhangjiakou.aliyuncs.com/res/Q35O851yW8VJWl9V/img/2c15f19f-a2d5-4659-94bc-362e8f26d168.png)

这种模式最大的优势是赋予了MCP Apps页面的**能力局部刷新**：A2UI 协议天然支持增量更新，页面变更时只需下发需要更新的组件或数据，而不需要重新下发整个 MCP Apps 页面。

此外，它还有一个有意思的应用场景：**对于那些不原生支持 A2UI 的宿主环境，可以通过 MCP Apps 的 iframe 来引入 A2UI 的渲染能力**。老系统只需要支持基本的 MCP Apps iframe 容器，就能获得动态 AI 交互能力。

#### 三种模式的对比

| **维度** | **模式一：MCP 下发 A2UI** | **模式二：A2UI 嵌 MCP Apps** | **模式三：MCP Apps 嵌 A2UI** |
| --- | --- | --- | --- |
| 架构关系 | MCP 是通道，A2UI 是内容 | A2UI 是容器，MCP Apps 是内容 | MCP Apps 是容器，A2UI 是内容 |
| 安全性 | 高（纯声明式） | 需 iframe 沙盒 | 需 iframe 沙盒 |
| 表达力 | A2UI 标准组件 | A2UI + 任意 HTML/JS | 任意 HTML/JS + A2UI 局部渲染 |
| 原生渲染支持 | 支持 | 部分（MCP Apps 区域为 Web） | 仅 Web |

---

### 我们的思考

整体上，我们认为 A2UI over MCP 在生成式UI业务落地中引出一些新的思路，以下亮点是我们值得关注的。

#### "预生产模板"不是妥协，可能是一种标准的落地方式

在将 AGenUI 应用到实际业务场景的过程中，我们也探索了"预生产协议模板"的模式：服务端根据客户端请求下发预设的 A2UI 协议模板，或者执行业务逻辑后对模板进行数据组装，全程不需要 LLM 参与协议的实时生成。

这和 A2UI over MCP 的模式是一样的思路。这种看起"折中"的方案，在未来是否会成为 **A2UI 协议落地的一种标准方式**，值得我们关注。这个思路也更凸显出另外一点：A2UI 协议的价值不仅在于"让 LLM 生成 UI"，更在于它作为一种跨平台、安全、声明式的 UI 描述语言的通用性。

#### "Logic in A2UI"：一个值得探索的方向

模式二（MCP Apps in A2UI）给我们的启发是：**A2UI 渲染器如何在保持安全性的前提下，支持更复杂的业务逻辑？**

MCP Apps in A2UI 提供了一个解题思路：把复杂逻辑交给 iframe 中的 MCP Apps 处理。但对于原生渲染器来说，引入 iframe 和 Web 技术栈意味着额外的复杂度和性能开销。这种解决问题A引入问题B的模式在真正业务落地时要做好权衡。

但是，顺着这个思路我们更进一步思考。本质问题是："A2UI 能不能支持某种形式的逻辑执行？"，我们暂且称之为 "Logic in A2UI"。可能是下发可执行的表达式、轻量脚本，或者某种受限的 DSL。当然，这和 A2UI 协议的安全性设计需要调和，需要谨慎探索。但我们认为这是一个值得关注的方向。

---

### 写在最后

A2UI over MCP 目前仍处于早期阶段，Google 已经在 Discussion 中征集社区意见，具体的 MCP Extension 规范尚未确定，但是我们能够看出 A2UI 协议在易用性、业务友好性方向上的持续探索。我们会持续关注进展，也期待和社区一起参与。

> 如果你对生成式 UI 感兴趣，欢迎关注我们的开源项目 [AGenUI](https://github.com/AGenUI/AGenUI)**——支持 iOS、Android 和 HarmonyOS 的高性能 A2UI 渲染引擎。它基于共享 C++ 核心引擎 + 三端原生渲染架构，完整实现了 A2UI v0.9 协议，能够在移动设备上实时流式渲染 LLM 生成的可交互 UI。欢迎 Star、试用、提 Issue，一起推动生成式 UI 在移动端的落地。**

---

**参考资料**

*   [A2UI over MCP - A2UI 官方指南](https://a2ui.org/guides/a2ui_over_mcp/)
    
*   [MCP Apps in A2UI - A2UI 官方指南](https://a2ui.org/guides/mcp-apps-in-a2ui/)
    
*   [A2UI in MCP Apps - A2UI 官方指南](https://a2ui.org/guides/a2ui-in-mcp-apps/)
    
*   [A2UI + MCP Apps: Combining the best of declarative and custom agentic UIs - Google Developers Blog](https://developers.googleblog.com/a2ui-and-mcp-apps/)
    
*   [A2UI as MCP Extension - GitHub Discussion](https://github.com/a2ui-project/a2ui/discussions/1676)
    
*   [AGenUI - GitHub](https://github.com/AGenUI/AGenUI)