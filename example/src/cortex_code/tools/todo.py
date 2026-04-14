"""TodoWrite implementation."""


class TodoManager:
    def __init__(self):
        self.todos: list[dict] = []

    def write(self, todos: list) -> str:
        """Replace the todo list and print to console."""
        self.todos = todos
        icons = {"pending": "○", "in_progress": "●", "completed": "✓"}
        lines = []
        for t in todos:
            icon = icons.get(t.get("status", "pending"), "○")
            lines.append(f"  {icon} {t.get('content', '')}")
        if lines:
            print("\n[TODO]\n" + "\n".join(lines) + "\n")
        return (
            "Todos have been modified successfully. "
            "Ensure that you continue to use the todo list to track your progress. "
            "Please proceed with the current tasks if applicable"
        )
