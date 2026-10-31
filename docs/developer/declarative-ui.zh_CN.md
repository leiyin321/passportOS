[English](declarative-ui.md)

# 声明式 MiniApp UI/1

`runtime: "declarative"` 的 MiniApp 使用 `ui.json` 作为入口。机器可读规范见 [`ui-v1.schema.json`](../../protocol/ui-v1.schema.json)；`passport pack` 和 `passport validate` 还会按当前设备运行时执行有界宿主预检。

## 文档结构

```json
{
  "format": "passport.ui/1",
  "title": "Pocket Counter",
  "state": { "count": 0 },
  "ui": [
    { "type": "text", "label": "Count: ", "bind": "count" },
    { "type": "text", "bind": "_status" }
  ],
  "actions": {
    "up": { "type": "increment", "field": "count" },
    "down": { "type": "increment", "field": "count", "amount": -1 },
    "ok": { "type": "save" }
  }
}
```

必须提供 `format`、对象类型的 `state` 和数组类型的 `ui`。可选根字段为 `title`、`cities`、`datasource`、`actions`、`on_start` 和 `timer`。持久化状态按发布者和 App ID 隔离。`display` 等系统字段由 OS 注入，不会写入应用数据。

## 绘制模型

- 每帧由有界的纵向文本流和最多 16 个图像命令组成，并裁切到应用视口。运行时不会创建 LVGL 控件树或独立焦点树。
- `text`、`button`、`progress`、`select`、`list` 当前都只绘制为文本行；名称不表示它们具有交互控件、进度条、选择器或焦点行为。
- `row`、`column`、`dialog` 会递归包含子节点，但目前只把子节点展平到同一文本/图像流中；不提供弹性布局或对话框行为。
- 文本行先添加 `label`，然后添加 `bind` 对应值；无绑定时添加 `text`。绑定路径支持点分对象路径和可选数字数组索引，例如 `response.current.temperature_2m` 或 `cities[0].name`。
- `icon` 绘制共享系统图标；`image` 使用 `app://` ID 读取签名包资源。坐标相对于应用视口。实际图像尺寸来自已验证资源；节点中的 `width` 和 `height` 仅供描述。
- 完整文档最大 16 KiB、512 个解析 token、嵌套深度 12；每个数组/对象最多 128 项，每个字符串最大 1,024 UTF-8 字节。UI 树最多 64 个节点、深度 8、16 张图像。

## 操作与持久化

`actions.up`、`actions.down`、`actions.ok` 在对应短按事件发生时运行。每项可以是一个操作，也可以是按顺序执行的操作数组。当前支持：

| 类型 | 字段 | 行为 |
| --- | --- | --- |
| `increment` | `field`、可选数字 `amount` | 对数值状态加值；绝对结果上限为 999,999 |
| `set` | `field`、`value` | 替换一个状态值 |
| `save` | 可选 `field` | 将整个状态对象作为一个私有数据值原子保存 |
| `select` | `field`、`options`、`index`、可选整数 `step` | 从顶层 1–32 项数组中选择，并写入 `field` |
| `conditional` | `field`、`then`、可选 `else` | 根据 `field` 的布尔值执行分支 |
| `fetch` | 无 | 排队请求 `datasource.url`；需要 `network.fetch`、匹配的 Manifest 域名和 `datasource` 对象 |

操作数组和条件嵌套深度上限为 8。用于写入的 `field`/`index` 必须是顶层简单状态键；读取绑定和条件可使用点分路径。应用不得改写 OS 管理的 `display` 对象。`save` 保存整个状态，因此需要控制数据量以符合 Manifest 的 `persistent_kib` 配额。多个操作或多次保存不构成跨键事务。

`on_start` 接收单个操作或操作数组，在应用启动时运行一次。`timer` 格式为 `{ "period_ms": 1000, "action": ... }`；小于 1,000 ms 会被拒绝，Manifest 还必须声明 `timer`。定时器不提供硬实时保证。

## 数据请求

```json
{
  "datasource": {
    "url": "https://api.example.test/value?city={city.name}",
    "fields": ["value"]
  },
  "on_start": { "type": "fetch" }
}
```

占位符从状态中展开，只允许 ASCII 字母、数字、`-`、`.`、`_`。最终 URL 仍受 Manifest 域名/端口规则及系统 DNS/IP 检查约束。响应必须是 HTTP 200、合法且有界的 JSON，最大 4,096 字节；解析结果写入 `state.response`。当前 `datasource.fields` 仅作描述信息，运行时保存完整的有界响应。请求失败时保留之前缓存的响应并设置 `_status`。

## 校验流程

```sh
passport init my-app --id org.example.my-app --runtime declarative
passport validate examples/packages/counter.pap --public sdk/publisher-public.pem
passport pack my-app --key local/publisher-private.pem --public local/publisher-public.pem -o dist/my-app.pap
passport validate dist/my-app.pap --public local/publisher-public.pem
```

`passport validate` 会验证包封装、发布者签名、入口摘要、Manifest 限额及声明式 UI/1 结构。它不会执行 Lua，也不能证明界面在真机显示正确。行为测试使用[模拟器和运行时测试](quickstart.zh_CN.md)，实际绘制/按键仍需目标设备验收。SDK 预检用于尽早发现问题；固件校验仍是最终权威。

当前 API 没有页面路由、任意控件、独立焦点、表达式计算或任意网络方法。SDK 预检会拒绝不支持的节点/操作。交互超出这些有界操作时，请使用 Lua runtime。
