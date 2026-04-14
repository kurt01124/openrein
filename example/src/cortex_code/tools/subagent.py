"""Agent tool — run a sub-agent task using glm-4.5-air."""
import openrein
from ..converter import to_openai_msgs, to_openai_tools, parse_resp
from ..prompts import SUBAGENT_SYSTEM_PROMPT
from ..models import glm
from .descriptions import DESCRIPTIONS


def run_agent(prompt: str, description: str = "") -> str:
    """Run a sub-agent with a new engine and glm-4.5-air."""
    label = (description or prompt)[:30]
    print(f"  ↳ [Agent: {label}] running...")

    sub_engine = openrein.Engine(system_prompt=SUBAGENT_SYSTEM_PROMPT)
    for name, desc in DESCRIPTIONS.items():
        sub_engine.set_tool_description(name, desc)

    sub_engine.add("user", prompt)

    while not sub_engine.done:
        msgs  = to_openai_msgs(sub_engine.messages, sub_engine.system_prompt)
        tools = to_openai_tools(sub_engine.tool_schemas())
        resp  = glm.call_sub(msgs, tools or None)
        blocks = parse_resp(resp)
        for b in blocks:
            if b.get("type") == "tool_use":
                print(f"    [tool] {b['name']}")
        sub_engine.step(blocks)

    return sub_engine.last_answer or "(done)"
