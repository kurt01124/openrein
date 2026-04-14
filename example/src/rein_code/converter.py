"""Anthropic ↔ OpenAI format conversion."""
import json


def _safe(v) -> str:
    """UTF-8 safe string conversion."""
    if isinstance(v, bytes):
        return v.decode("utf-8", "replace")
    s = str(v) if not isinstance(v, str) else v
    return s.encode("utf-8", "replace").decode("utf-8")


def to_openai_msgs(messages: list, system: str = "") -> list:
    """Anthropic messages → OpenAI messages."""
    result = []
    if system:
        result.append({"role": "system", "content": system})
    for msg in messages:
        role, content = msg["role"], msg["content"]
        if isinstance(content, str):
            result.append({"role": role, "content": _safe(content)})
            continue
        if role == "assistant":
            text = "".join(b.get("text", "") for b in content if b.get("type") == "text")
            tcs  = [
                {
                    "id": b["id"], "type": "function",
                    "function": {"name": b["name"], "arguments": json.dumps(b.get("input", {}))},
                }
                for b in content if b.get("type") == "tool_use"
            ]
            entry = {"role": "assistant", "content": _safe(text) or None}
            if tcs:
                entry["tool_calls"] = tcs
            result.append(entry)
        elif role == "user":
            for b in content:
                if b.get("type") == "tool_result":
                    result.append({
                        "role": "tool",
                        "tool_call_id": b["tool_use_id"],
                        "content": _safe(b.get("content", "")),
                    })
                elif b.get("type") == "text":
                    result.append({"role": "user", "content": _safe(b["text"])})
    return result


def to_openai_tools(schemas: list) -> list:
    """Anthropic tool schemas → OpenAI function schemas."""
    return [
        {
            "type": "function",
            "function": {
                "name": t["name"],
                "description": t.get("description", ""),
                "parameters": t.get("input_schema", {"type": "object", "properties": {}}),
            },
        }
        for t in schemas
    ]


def parse_resp(resp) -> list:
    """OpenAI ChatCompletion response → Anthropic content blocks."""
    msg = resp.choices[0].message
    blocks = []
    text = msg.content or getattr(msg, "reasoning_content", None) or ""
    if text:
        blocks.append({"type": "text", "text": text})
    for tc in getattr(msg, "tool_calls", None) or []:
        try:
            inp = json.loads(tc.function.arguments)
        except Exception:
            inp = {}
        blocks.append({
            "type":  "tool_use",
            "id":    tc.id,
            "name":  tc.function.name,
            "input": inp,
        })
    return blocks
