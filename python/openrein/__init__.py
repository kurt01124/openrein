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

Skill usage:
    engine.skill_add("/path/to/skills/")   # directory or single .md file
    engine.add("system", engine.skill_prompt_listing())
    engine.skill_inject("commit")          # inject skill prompt as user message

Custom tool (ToolBase):
    class MyTool(openrein.ToolBase):
        def name(self) -> str: return "MyTool"
        def description(self) -> str: return "..."
        def input_schema(self) -> dict: return {"type": "object", "properties": {}}
        def call(self, input: dict) -> str: return "result"

    engine = openrein.Engine()
    engine.register_tool(MyTool())
"""

from pkgutil import extend_path

__path__ = extend_path(__path__, __name__)

import os
from dataclasses import dataclass

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


@dataclass
class Skill:
    """A single skill loaded from a Markdown file.

    Attributes:
        name:          Skill name (filename without .md extension).
        description:   Short description (from frontmatter ``description`` key).
        when_to_use:   Hint for the LLM on when to trigger this skill.
        allowed_tools: Optional list of tool names this skill may use.
        prompt:        The skill body (everything after the frontmatter).
        source_path:   Absolute path to the source .md file.
    """
    name: str
    description: str
    when_to_use: str
    allowed_tools: list[str]
    prompt: str
    source_path: str


def _parse_skill(path: str) -> Skill:
    """Parse a single Skill Markdown file.

    Frontmatter is delimited by ``---`` lines.  No external YAML library
    required — we parse only the four known keys manually.
    """
    with open(path, encoding="utf-8") as fh:
        raw = fh.read()

    name = os.path.splitext(os.path.basename(path))[0]
    frontmatter: dict[str, str] = {}
    body = raw

    lines = raw.split("\n")
    if lines and lines[0].strip() == "---":
        end = -1
        for i in range(1, len(lines)):
            if lines[i].strip() == "---":
                end = i
                break
        if end != -1:
            fm_lines = lines[1:end]
            body = "\n".join(lines[end + 1:]).lstrip("\n")
            for fm_line in fm_lines:
                if ":" in fm_line:
                    key, _, val = fm_line.partition(":")
                    frontmatter[key.strip()] = val.strip()

    description  = frontmatter.get("description", "")
    when_to_use  = frontmatter.get("when_to_use", "")

    allowed_raw  = frontmatter.get("allowed_tools", "")
    allowed_tools: list[str] = []
    if allowed_raw:
        # Accept both [A, B] list syntax and plain "A, B" string.
        allowed_raw = allowed_raw.strip("[]")
        allowed_tools = [t.strip().strip("'\"") for t in allowed_raw.split(",") if t.strip()]

    return Skill(
        name=name,
        description=description,
        when_to_use=when_to_use,
        allowed_tools=allowed_tools,
        prompt=body,
        source_path=os.path.abspath(path),
    )


def _collect_skill_files(path: str) -> list[str]:
    """Return a list of .md file paths from *path* (file or directory)."""
    path = os.path.abspath(path)
    if os.path.isfile(path):
        return [path] if path.endswith(".md") else []
    if os.path.isdir(path):
        return sorted(
            os.path.join(path, f)
            for f in os.listdir(path)
            if f.endswith(".md") and os.path.isfile(os.path.join(path, f))
        )
    return []


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
    """openrein Engine with ToolBase registration and Skill support."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        # Ordered list of PATH strings added via skill_add().
        self._skill_paths: list[str] = []
        # name → Skill map (later PATH wins on collision).
        self._skills: dict[str, Skill] = {}

    # ------------------------------------------------------------------
    # ToolBase registration
    # ------------------------------------------------------------------

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

    # ------------------------------------------------------------------
    # Skill interface
    # ------------------------------------------------------------------

    @property
    def skills(self) -> list[str]:
        """Return the list of registered skill PATH strings (files or directories)."""
        return list(self._skill_paths)

    def skill_add(self, path: str) -> None:
        """Add a skill file or a directory of skill files.

        When a name collision occurs the new skill (later PATH) wins.
        Re-adding the same path is a no-op.
        """
        abs_path = os.path.abspath(path)
        if abs_path not in self._skill_paths:
            self._skill_paths.append(abs_path)
        for fpath in _collect_skill_files(abs_path):
            skill = _parse_skill(fpath)
            self._skills[skill.name] = skill

    def skill_remove(self, path: str) -> None:
        """Remove a skill file or directory previously added with skill_add().

        Skills whose source_path is under *path* are removed from the registry.
        After removal, remaining paths are re-scanned so earlier definitions
        of the same name are restored if available.
        """
        abs_path = os.path.abspath(path)
        if abs_path in self._skill_paths:
            self._skill_paths.remove(abs_path)
        # Rebuild skill map from remaining paths to restore shadowed skills.
        self._skills.clear()
        for p in self._skill_paths:
            for fpath in _collect_skill_files(p):
                skill = _parse_skill(fpath)
                self._skills[skill.name] = skill

    def skill_list(self) -> list[Skill]:
        """Return all registered Skill objects (sorted by name)."""
        return sorted(self._skills.values(), key=lambda s: s.name)

    def skill_prompt_listing(self) -> str:
        """Return a text block suitable for injection into a system prompt.

        Format::

            Available skills:
            - commit: stage and commit changes (use when: 사용자가 커밋 요청할 때)
            - ...
        """
        skills = self.skill_list()
        if not skills:
            return ""
        lines = ["Available skills:"]
        for s in skills:
            line = f"- {s.name}: {s.description}"
            if s.when_to_use:
                line += f" (use when: {s.when_to_use})"
            lines.append(line)
        return "\n".join(lines)

    def skill_inject(self, name: str, args: str = "") -> None:
        """Inject a skill's prompt as a user message.

        Args:
            name: Skill name (without .md extension).
            args: Optional extra text appended after the skill prompt.

        Raises:
            KeyError: If *name* is not a registered skill.
        """
        if name not in self._skills:
            raise KeyError(f"Skill not found: {name!r}")
        skill = self._skills[name]
        prompt = skill.prompt
        if args:
            prompt = f"{prompt}\n\n{args}"
        self.add("user", prompt)


__all__ = ["Engine", "SubAgent", "Compact", "ToolBase", "Skill", "default_tools"]
