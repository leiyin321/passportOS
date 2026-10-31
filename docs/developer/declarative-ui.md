[简体中文](declarative-ui.zh_CN.md)

# Declarative MiniApp UI/1

`ui.json` is the entry point for a `runtime: "declarative"` MiniApp. Its machine-readable contract is [`ui-v1.schema.json`](../../protocol/ui-v1.schema.json); `passport pack` and `passport validate` also run bounded host-side checks against the current device runtime.

## Document shape

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

`format`, object-valued `state`, and array-valued `ui` are required. Optional root fields are `title`, `cities`, `datasource`, `actions`, `on_start`, and `timer`. Persisted application state is namespaced by publisher and app ID. Reserved system values such as `display` are injected by the OS and are not persisted.

## Rendering model

- A frame is a bounded vertical text stream plus up to 16 image commands, clipped to the app viewport. The renderer does not create an LVGL widget tree or independent focus tree.
- `text`, `button`, `progress`, `select`, and `list` currently render as text rows. Their names do not imply interactive widgets, progress bars, selection controls, or focus behavior.
- `row`, `column`, and `dialog` recursively group child nodes but currently flatten their children into the same text/image stream; they do not provide flex layout or dialog behavior.
- A text row appends `label` and then `bind`'s value; when there is no binding it appends `text`. A binding is a dotted object path with optional numeric array indexes, such as `response.current.temperature_2m` or `cities[0].name`.
- `icon` draws a shared system icon. `image` resolves a signed package resource using an `app://` ID. Coordinates are relative to the app viewport. The verified asset's compiled dimensions determine image size; node `width` and `height` are descriptive only.
- The complete document is bounded to 16 KiB, 512 parser tokens, depth 12, 128 items per array/object, and 1,024 UTF-8 bytes per string. The UI tree is at most 64 nodes, depth 8, and 16 images.

## Actions and persistence

`actions.up`, `actions.down`, and `actions.ok` run on the corresponding short button action. A value can be one action or an ordered array of actions. Supported action types:

| Type | Fields | Behavior |
| --- | --- | --- |
| `increment` | `field`, optional numeric `amount` | Adds to a numeric state value; absolute result is capped at 999,999 |
| `set` | `field`, `value` | Replaces a state value |
| `save` | optional `field` | Atomically saves the entire state object as one private-data value |
| `select` | `field`, `options`, `index`, optional integer `step` | Selects an item from a top-level array of 1–32 entries and stores it at `field` |
| `conditional` | `field`, `then`, optional `else` | Runs a branch based on the truth value at `field` |
| `fetch` | none | Enqueues the `datasource.url` request; it needs `network.fetch`, a matching Manifest domain, and a `datasource` object |

Action arrays and conditional nesting are bounded to depth 8. State paths cannot modify the OS-owned `display` object. Saves use one bounded private-data value, so design the saved state to fit the Manifest's `persistent_kib` quota. A sequence of actions or multiple saves is not a multi-key transaction.

`on_start` accepts one action or an action array and runs once when the app starts. A `timer` uses `{ "period_ms": 1000, "action": ... }`; periods below 1,000 ms are rejected and the Manifest must declare `timer`. The timer does not guarantee hard real-time scheduling.

## Data requests

Example:

```json
{
  "datasource": {
    "url": "https://api.example.test/value?city={city.name}",
    "fields": ["value"]
  },
  "on_start": { "type": "fetch" }
}
```

Placeholders expand from state and permit only ASCII letters, digits, `-`, `.`, and `_`. The final URL is still subject to the Manifest domain/port policy and the system's DNS/IP checks. Responses must be HTTP 200, valid bounded JSON, and at most 4,096 bytes; the parsed body becomes `state.response`. `datasource.fields` is currently descriptive metadata; the runtime stores the complete bounded response. Failure leaves the previous cached response intact and sets `_status`.

## Validation workflow

```sh
passport init my-app --id org.example.my-app --runtime declarative
passport validate examples/packages/counter.pap --public sdk/publisher-public.pem
passport pack my-app --key local/publisher-private.pem --public local/publisher-public.pem -o dist/my-app.pap
passport validate dist/my-app.pap --public local/publisher-public.pem
```

`passport validate` verifies the package envelope, publisher signature, entry digest, Manifest bounds, and declarative UI/1 shape. It does not execute Lua or prove the UI looks correct on hardware. Use the [simulator and runtime tests](quickstart.md) for behavior checks and device testing for actual rendering/input. The SDK's validation is an early diagnostic; firmware remains authoritative.

The current API intentionally has no page router, arbitrary widgets, independent focus controls, computed expressions, or arbitrary network methods. Unsupported node/action names are rejected by the SDK preflight. Use Lua when an interaction cannot be represented by these bounded actions.
