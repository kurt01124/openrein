"""EnterPlanMode / ExitPlanMode implementation."""
from __future__ import annotations
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    import openrein


class PlanModeManager:
    def __init__(self):
        self.in_plan_mode: bool = False
        self._engine: "openrein.Engine | None" = None

    def set_engine(self, engine: "openrein.Engine") -> None:
        self._engine = engine

    def enter(self, task_description: str = "") -> str:
        self.in_plan_mode = True
        if self._engine is not None:
            self._engine.add(
                "user",
                (
                    "<system-reminder>\n"
                    "Plan mode is active. You MUST NOT make any edits, run write tools, "
                    "or otherwise modify the system. This supercedes any other instructions.\n\n"
                    "Allowed tools: Read, Grep, Glob, Bash (read-only commands only), "
                    "WebFetch, WebSearch, AskUserQuestion\n"
                    "Forbidden: Write, Edit — these will return an error if called.\n\n"
                    "Workflow:\n"
                    "1. Thoroughly explore the codebase using Glob, Grep, and Read tools\n"
                    "2. Understand existing patterns and architecture\n"
                    "3. Design an implementation approach\n"
                    "4. Use AskUserQuestion if you need to clarify requirements or approaches\n"
                    "5. Call ExitPlanMode(plan='...') with your complete plan when ready\n"
                    "</system-reminder>"
                ),
            )
        msg = "Entered plan mode."
        if task_description:
            msg += f" Task: {task_description}"
        msg += " Explore the codebase and design your plan. Call ExitPlanMode when ready."
        return msg

    def exit(self, plan: str = "") -> str:
        sep = "=" * 60
        print(f"\n{sep}\n[Plan]\n{plan}\n{sep}")
        ans = input("Proceed with this plan? [y/n]: ").strip().lower()
        self.in_plan_mode = False
        if ans == "y":
            return "Plan approved. You may now proceed with implementation."
        else:
            feedback = input("Feedback: ").strip()
            return (
                f"Plan rejected. User feedback: {feedback}. "
                "Please revise your plan and call ExitPlanMode again when ready."
            )

    def block_if_active(self, tool_name: str) -> str | None:
        """Block Write/Edit in plan mode. Returns None if allowed."""
        if self.in_plan_mode and tool_name in ("Write", "Edit"):
            return (
                f"Error: {tool_name} is not allowed in plan mode. "
                "Finish planning with ExitPlanMode first, then implement."
            )
        return None
