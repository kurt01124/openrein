"""Tool registration — register_all_tools(engine, plan_mgr, todo_mgr)."""
import openrein
from .descriptions import (
    DESCRIPTIONS, AGENT_DESCRIPTION, TODO_DESCRIPTION,
    ASK_USER_DESCRIPTION, ENTER_PLAN_MODE_DESCRIPTION, EXIT_PLAN_MODE_DESCRIPTION,
)
from .todo import TodoManager
from .ask import ask_user
from .plan import PlanModeManager
from .subagent import run_agent


def register_all_tools(
    engine: openrein.Engine,
    plan_mgr: PlanModeManager,
    todo_mgr: TodoManager,
) -> None:
    """Register all tools on the engine."""

    # 1. Override descriptions for the 8 built-in tools
    for name, desc in DESCRIPTIONS.items():
        engine.set_tool_description(name, desc)

    # 2. Agent
    engine.register_tool(
        run_agent,
        name="Agent",
        description=AGENT_DESCRIPTION,
        schema={
            "type": "object",
            "properties": {
                "prompt":      {"type": "string", "description": "Task for the agent (self-contained, full context)"},
                "description": {"type": "string", "description": "3-5 word summary of what the agent will do"},
            },
            "required": ["prompt"],
        },
    )

    # 3. TodoWrite
    engine.register_tool(
        todo_mgr.write,
        name="TodoWrite",
        description=TODO_DESCRIPTION,
        schema={
            "type": "object",
            "properties": {
                "todos": {
                    "type": "array",
                    "description": "The updated todo list",
                    "items": {
                        "type": "object",
                        "properties": {
                            "content":    {"type": "string", "description": "Imperative form: 'Fix auth bug'"},
                            "activeForm": {"type": "string", "description": "Present continuous: 'Fixing auth bug'"},
                            "status":     {"type": "string", "enum": ["pending", "in_progress", "completed"]},
                        },
                        "required": ["content", "status"],
                    },
                },
            },
            "required": ["todos"],
        },
    )

    # 4. AskUserQuestion
    engine.register_tool(
        ask_user,
        name="AskUserQuestion",
        description=ASK_USER_DESCRIPTION,
        schema={
            "type": "object",
            "properties": {
                "questions": {
                    "type": "array",
                    "items": {
                        "type": "object",
                        "properties": {
                            "question":    {"type": "string"},
                            "header":      {"type": "string", "maxLength": 12},
                            "options":     {
                                "type": "array",
                                "items": {
                                    "type": "object",
                                    "properties": {
                                        "label":       {"type": "string"},
                                        "description": {"type": "string"},
                                    },
                                    "required": ["label"],
                                },
                            },
                            "multiSelect": {"type": "boolean", "default": False},
                        },
                        "required": ["question", "header", "options"],
                    },
                },
            },
            "required": ["questions"],
        },
    )

    # 5. EnterPlanMode
    engine.register_tool(
        plan_mgr.enter,
        name="EnterPlanMode",
        description=ENTER_PLAN_MODE_DESCRIPTION,
        schema={
            "type": "object",
            "properties": {
                "task_description": {"type": "string", "description": "Brief description of the task to plan"},
            },
        },
    )

    # 6. ExitPlanMode
    engine.register_tool(
        plan_mgr.exit,
        name="ExitPlanMode",
        description=EXIT_PLAN_MODE_DESCRIPTION,
        schema={
            "type": "object",
            "properties": {
                "plan": {"type": "string", "description": "Complete implementation plan in markdown"},
            },
            "required": ["plan"],
        },
    )
