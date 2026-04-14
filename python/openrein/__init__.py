"""
openrein — Minimal harness engine for building LLM agent apps.

The developer calls the model directly; openrein provides only the
skeleton (Engine, SubAgent, Compact + 8 built-in tools) to reduce
boilerplate.

Quick start:
    import openrein

    engine = openrein.Engine(system_prompt="You are a helpful assistant.")

    @engine.tool("get_time", "Return the current time")
    def get_time() -> str:
        from datetime import datetime
        return datetime.now().isoformat()

    engine.add("user", "What time is it?")

    while True:
        resp = my_model(engine.messages, engine.tool_schemas())
        if engine.step(resp):
            break

    print(engine.last_answer)
"""

"""
openrein — Minimal harness engine for building LLM agent apps.

Writing a contrib tool:

    import openrein

    class MyTool(openrein.ToolBase):
        def name(self) -> str:
            return "MyTool"
        def description(self) -> str:
            return "..."
        def input_schema(self) -> dict:
            return {"type": "object", "properties": {}}
        def call(self, input: dict) -> str:
            return "result"

    engine = openrein.Engine()
    engine.register_tool(MyTool())
"""

from ._openrein import (  # noqa: F401
    Engine as _Engine,
    SubAgent,
    Compact,
    default_tools,
)

try:
    from importlib.metadata import version
    __version__ = version("openrein")
except Exception:
    from ._openrein import __version__  # fallback


class ToolBase:
    """Base class for openrein contrib tools.

    Subclass this in Python to create custom tools without a C++ build.
    """

    _desc_override: str | None = None

    def name(self) -> str:
        """Tool name (required)."""
        raise NotImplementedError(f"{type(self).__name__} must implement name()")

    def description(self) -> str:
        """Default description (required)."""
        raise NotImplementedError(f"{type(self).__name__} must implement description()")

    def input_schema(self) -> dict:
        """Input JSON schema (required)."""
        raise NotImplementedError(f"{type(self).__name__} must implement input_schema()")

    def call(self, input: dict) -> str:
        """Execute the tool — input is a dict, return value is str (required)."""
        raise NotImplementedError(f"{type(self).__name__} must implement call()")

    def set_description(self, desc: str) -> None:
        """Override the description at runtime."""
        self._desc_override = desc

    def effective_description(self) -> str:
        """Return the active description (override if set, otherwise default)."""
        return self._desc_override if self._desc_override else self.description()

    def schema(self) -> dict:
        """Return the full schema dict including name, description, and input_schema."""
        return {
            "name":         self.name(),
            "description":  self.effective_description(),
            "input_schema": self.input_schema(),
        }


class Engine(_Engine):
    """openrein Engine with ToolBase registration support."""

    def register_tool(self, tool_or_func=None, *, name=None, description=None, schema=None):
        """Register a tool.

        Supports two forms:
        1. ToolBase object:  engine.register_tool(MyTool())
        2. Python function:  engine.register_tool(func, name=..., description=..., schema=...)
        """
        if isinstance(tool_or_func, ToolBase):
            tool = tool_or_func
            def _adapter(**kw):
                result = tool.call(kw)
                return str(result) if not isinstance(result, str) else result
            _Engine.register_tool(
                self,
                _adapter,
                name=tool.name(),
                description=tool.effective_description(),
                schema=tool.input_schema(),
            )
        else:
            kwargs = {}
            if name        is not None: kwargs["name"]        = name
            if description is not None: kwargs["description"] = description
            if schema      is not None: kwargs["schema"]      = schema
            _Engine.register_tool(self, tool_or_func, **kwargs)


__all__ = ["Engine", "SubAgent", "Compact", "ToolBase", "default_tools"]
