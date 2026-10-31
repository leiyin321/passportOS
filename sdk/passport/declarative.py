"""Host-side preflight checks for the implemented Passport UI/1 runtime."""
import json
import math
import re


def _pairs(items):
    result = {}
    for key, value in items:
        if key in result:
            raise ValueError("declarative UI: duplicate JSON key")
        result[key] = value
    return result


def _safe_json(raw):
    if len(raw) > 16384 or b"\0" in raw:
        raise ValueError("declarative UI: entry size or NUL")
    tokens = 0
    depth = 0
    quoted = escaped = False
    for byte in raw:
        char = chr(byte)
        if quoted:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                quoted = False
        elif char == '"':
            quoted = True
            tokens += 1
        elif char in "{[":
            depth += 1
            tokens += 1
        elif char in "}]":
            depth -= 1
        elif char in ",:":
            tokens += 1
        if depth > 12 or depth < 0 or tokens > 512:
            raise ValueError("declarative UI: JSON complexity limit")
    if quoted or escaped or depth:
        raise ValueError("declarative UI: malformed JSON")
    try:
        document = json.loads(raw, object_pairs_hook=_pairs,
                              parse_constant=lambda _: (_ for _ in ()).throw(
                                  ValueError("declarative UI: non-finite number")))
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ValueError("declarative UI: malformed JSON") from error

    def inspect(value, level=0):
        if level > 12:
            raise ValueError("declarative UI: JSON nesting limit")
        if isinstance(value, dict):
            if len(value) > 128 or any(not isinstance(k, str) or len(k) > 64 for k in value):
                raise ValueError("declarative UI: object limit")
            for child in value.values():
                inspect(child, level + 1)
        elif isinstance(value, list):
            if len(value) > 128:
                raise ValueError("declarative UI: array limit")
            for child in value:
                inspect(child, level + 1)
        elif isinstance(value, str) and len(value.encode("utf-8")) > 1024:
            raise ValueError("declarative UI: string limit")
        elif isinstance(value, float) and not math.isfinite(value):
            raise ValueError("declarative UI: non-finite number")
    inspect(document)
    return document


def _path(value, label):
    grammar = r"[A-Za-z0-9_]{1,64}(?:\[[0-9]{1,3}\])?(?:\.[A-Za-z0-9_]{1,64}(?:\[[0-9]{1,3}\])?)*"
    if not isinstance(value, str) or len(value) > 128 or not re.fullmatch(grammar, value):
        raise ValueError(f"declarative UI: invalid {label} path")


def _state_key(value, label):
    if (not isinstance(value, str) or not 1 <= len(value) <= 64 or
            value == "display" or value.startswith("display.") or
            not all(c in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_" for c in value)):
        raise ValueError(f"declarative UI: {label} must be a writable top-level state key")


def _action(action, depth=0):
    if depth > 8:
        raise ValueError("declarative UI: action nesting limit")
    if isinstance(action, list):
        if len(action) > 64:
            raise ValueError("declarative UI: action list limit")
        for item in action:
            _action(item, depth + 1)
        return
    if not isinstance(action, dict):
        raise ValueError("declarative UI: action must be an object or array")
    kind = action.get("type")
    fields = {
        "increment": {"type", "field", "amount"},
        "set": {"type", "field", "value"},
        "save": {"type", "field"}, "fetch": {"type"},
        "select": {"type", "field", "options", "index", "step"},
        "conditional": {"type", "field", "then", "else"},
    }
    if kind not in fields or set(action) - fields[kind]:
        raise ValueError("declarative UI: unknown action or action field")
    if kind in ("increment", "set", "select"):
        _state_key(action.get("field", ""), "field")
    elif kind == "conditional":
        _path(action.get("field", ""), "field")
    elif kind == "save" and "field" in action:
        _state_key(action["field"], "field")
    if kind == "increment":
        amount = action.get("amount", 1)
        if type(amount) not in (int, float) or abs(amount) > 999999:
            raise ValueError("declarative UI: invalid increment amount")
    elif kind == "set":
        if "value" not in action:
            raise ValueError("declarative UI: set action requires value")
    elif kind == "select":
        options = action.get("options")
        if not isinstance(options, str) or not options or len(options) > 64:
            raise ValueError("declarative UI: select requires a top-level options name")
        if not isinstance(action.get("index"), str):
            raise ValueError("declarative UI: select requires index")
        _state_key(action["index"], "index")
        if type(action.get("step", 1)) is not int or abs(action.get("step", 1)) > 2147483647:
            raise ValueError("declarative UI: select step must be an integer")
    elif kind == "conditional":
        _action(action.get("then"), depth + 1)
        if "else" in action:
            _action(action["else"], depth + 1)
    elif kind not in ("save", "fetch"):
        raise ValueError(f"declarative UI: unsupported action {kind!r}")


def _nodes(nodes, depth=0, counts=None):
    if counts is None:
        counts = {"nodes": 0, "images": 0, "asset_refs": set()}
    if depth > 8 or not isinstance(nodes, list):
        raise ValueError("declarative UI: children must be arrays, depth <= 8")
    for node in nodes:
        counts["nodes"] += 1
        if counts["nodes"] > 64 or not isinstance(node, dict):
            raise ValueError("declarative UI: node limit or invalid node")
        kind = node.get("type")
        allowed = {
            "row": {"type", "children"}, "column": {"type", "children"},
            "dialog": {"type", "children"},
            "icon": {"type", "name", "x", "y", "size"},
            "image": {"type", "src", "x", "y", "size", "width", "height"},
            "text": {"type", "label", "text", "bind"},
            "button": {"type", "label", "text", "bind"},
            "progress": {"type", "label", "text", "bind"},
            "select": {"type", "label", "text", "bind"},
            "list": {"type", "label", "text", "bind"},
        }
        if kind not in allowed or set(node) - allowed[kind]:
            raise ValueError("declarative UI: unknown node or node field")
        if kind in ("row", "column", "dialog"):
            _nodes(node.get("children"), depth + 1, counts)
        elif kind in ("icon", "image"):
            counts["images"] += 1
            if counts["images"] > 16:
                raise ValueError("declarative UI: visible image limit")
            for key, maximum, default in (("x", 192, (counts["images"] - 1) % 4 * 50),
                                          ("y", 156, (counts["images"] - 1) // 4 * 40)):
                value = node.get(key, default)
                if type(value) is not int or not 0 <= value <= maximum:
                    raise ValueError(f"declarative UI: invalid image {key}")
            size = node.get("size", 24)
            if type(size) is not int or not 12 <= size <= 48:
                raise ValueError("declarative UI: image size must be 12..48")
            if kind == "icon" and not isinstance(node.get("name"), str):
                raise ValueError("declarative UI: icon requires name")
            if kind == "image":
                source = node.get("src")
                if not isinstance(source, str) or not source.startswith("app://"):
                    raise ValueError("declarative UI: image src must use app://")
                resource = source[6:]
                if len(resource) > 48 or not re.fullmatch(r"[a-z0-9]+(?:[/_-][a-z0-9]+)*", resource):
                    raise ValueError("declarative UI: invalid image resource ID")
                counts["asset_refs"].add(resource)
        elif kind in ("text", "button", "progress", "select", "list"):
            for key in ("label", "text", "bind"):
                if key in node and not isinstance(node[key], str):
                    raise ValueError(f"declarative UI: {key} must be a string")
            if "bind" in node:
                _path(node["bind"], "binding")
        else:
            raise ValueError(f"declarative UI: unsupported node {kind!r}")
    return counts


def validate_ui(raw):
    """Validate bounds and operations understood by the current device renderer."""
    if isinstance(raw, str):
        raw = raw.encode("utf-8")
    document = _safe_json(raw)
    if not isinstance(document, dict) or document.get("format") != "passport.ui/1":
        raise ValueError("declarative UI: expected passport.ui/1 object")
    if set(document) - {"format", "title", "state", "ui", "actions", "on_start", "timer", "datasource", "cities"}:
        raise ValueError("declarative UI: unknown top-level field")
    if not isinstance(document.get("state"), dict):
        raise ValueError("declarative UI: state must be an object")
    if "title" in document and not isinstance(document["title"], str):
        raise ValueError("declarative UI: title must be a string")
    if "cities" in document and (not isinstance(document["cities"], list) or len(document["cities"]) > 32):
        raise ValueError("declarative UI: cities must be an array")
    counts = _nodes(document.get("ui"))
    actions = document.get("actions", {})
    if isinstance(actions, dict) and not set(actions) - {"up", "down", "ok"}:
        for action in actions.values():
            _action(action)
    else:
        raise ValueError("declarative UI: actions must be an object")
    if "on_start" in document:
        _action(document["on_start"])
    timer = document.get("timer")
    if timer is not None:
        if (not isinstance(timer, dict) or type(timer.get("period_ms")) is not int or
                not 1000 <= timer["period_ms"] <= 2147483647):
            raise ValueError("declarative UI: timer period must be an integer >= 1000 ms")
        _action(timer.get("action"))
    datasource = document.get("datasource")
    if datasource is not None and (not isinstance(datasource, dict) or
                                   set(datasource) - {"url", "fields"} or
                                   not isinstance(datasource.get("url"), str) or
                                   len(datasource.get("url", "")) > 500 or
                                   not isinstance(datasource.get("fields", []), list) or
                                   len(datasource.get("fields", [])) > 64 or
                                   any(not isinstance(v, str) or len(v) > 128 for v in datasource.get("fields", []))):
        raise ValueError("declarative UI: invalid datasource")
    required = {"ui"}
    if actions:
        required.add("input")
    if timer:
        required.add("timer")
    action_nodes = []
    action_nodes.extend(actions.values())
    if "on_start" in document:
        action_nodes.append(document["on_start"])
    if timer:
        action_nodes.append(timer["action"])

    fetch_needed = False
    def collect(action):
        nonlocal fetch_needed
        if isinstance(action, list):
            for child in action:
                collect(child)
        elif isinstance(action, dict):
            if action.get("type") == "save":
                required.add("storage.private")
            if action.get("type") == "fetch":
                fetch_needed = True
                required.add("network.fetch")
                if datasource is None:
                    raise ValueError("declarative UI: fetch action requires datasource")
            if action.get("type") == "conditional":
                collect(action.get("then"))
                if "else" in action:
                    collect(action["else"])

    for action in action_nodes:
        _validate_select_options(action, document)
        collect(action)
    if any(node.get("type") == "image" for node in _walk_nodes(document["ui"])):
        required.add("graphics")
    return {"nodes": counts["nodes"], "images": counts["images"],
            "permissions": required,
            "asset_refs": counts["asset_refs"],
            "fetch_url": datasource.get("url") if fetch_needed else None}


def _validate_select_options(action, document):
    if isinstance(action, list):
        for child in action:
            _validate_select_options(child, document)
    elif isinstance(action, dict):
        if action.get("type") == "select":
            options = document.get(action["options"])
            if not isinstance(options, list) or not 1 <= len(options) <= 32:
                raise ValueError("declarative UI: select options must name a top-level array of 1..32 items")
        elif action.get("type") == "conditional":
            _validate_select_options(action.get("then"), document)
            if "else" in action:
                _validate_select_options(action["else"], document)


def _walk_nodes(nodes):
    for node in nodes:
        yield node
        if node.get("type") in ("row", "column", "dialog"):
            yield from _walk_nodes(node["children"])
