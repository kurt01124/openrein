"""Core agent loop."""
import openrein
from .config import get_cwd, is_git_repo, MAIN_MODEL
from .prompts import get_system_prompt
from .converter import to_openai_msgs, to_openai_tools, parse_resp
from .session import SessionManager
from .tools import register_all_tools
from .tools.todo import TodoManager
from .tools.plan import PlanModeManager
from .models import glm


class Agent:
    def __init__(self, cwd: str | None = None, session_id: str | None = None):
        self.cwd        = get_cwd(cwd)
        self.session_mgr = SessionManager()
        self.session_id  = session_id or self.session_mgr.new_session()
        self._engine: openrein.Engine | None = None
        self._plan_mgr: PlanModeManager | None = None
        self._todo_mgr: TodoManager | None = None

    # ── Engine initialization ─────────────────────────────────────────────────

    def _make_engine(self, messages: list | None = None) -> openrein.Engine:
        self._plan_mgr = PlanModeManager()
        self._todo_mgr = TodoManager()

        engine = openrein.Engine(
            system_prompt=get_system_prompt(self.cwd, is_git_repo(self.cwd), MAIN_MODEL),
        )
        register_all_tools(engine, self._plan_mgr, self._todo_mgr)
        self._plan_mgr.set_engine(engine)

        if messages:
            engine.messages = messages

        self._engine = engine
        return engine

    # ── Single task execution ─────────────────────────────────────────────────

    def run(self, task: str, engine: openrein.Engine | None = None) -> str:
        """Process one user message and return last_answer."""
        if engine is None:
            prev = self.session_mgr.load(self.cwd, self.session_id)
            engine = self._make_engine(prev)

        engine.add("user", task)
        self._step_loop(engine)
        self.session_mgr.save(self.cwd, self.session_id, engine.messages)
        return engine.last_answer or ""

    # ── Step loop ─────────────────────────────────────────────────────────────

    def _step_loop(self, engine: openrein.Engine) -> None:
        """Call the model until done."""
        while not engine.done:
            msgs  = to_openai_msgs(engine.messages, engine.system_prompt)
            tools = to_openai_tools(engine.tool_schemas())
            resp  = glm.call_main(msgs, tools or None)
            blocks = parse_resp(resp)

            # Plan mode guard: strip Write/Edit tool_use blocks and inject error
            # tool_results directly into messages
            blocked_ids: set[str] = set()
            for b in blocks:
                if b.get("type") == "tool_use":
                    name = b["name"]
                    inp  = b.get("input", {})
                    preview = {
                        k: (str(v)[:50] + "…" if len(str(v)) > 50 else v)
                        for k, v in inp.items()
                    }
                    print(f"[{name}] {preview}")

                    if self._plan_mgr:
                        blocked = self._plan_mgr.block_if_active(name)
                        if blocked:
                            blocked_ids.add(b["id"])

            if blocked_ids:
                # Add the full assistant message (including blocked tool_use) as required by the API
                engine.step(blocks)
                # Inject error tool_results for each blocked tool_use
                for b in blocks:
                    if b.get("type") == "tool_use" and b["id"] in blocked_ids:
                        blocked_msg = (
                            f"Error: {b['name']} is not allowed in plan mode. "
                            "Finish planning with ExitPlanMode first, then implement."
                        )
                        engine.add("user", [{
                            "type":        "tool_result",
                            "tool_use_id": b["id"],
                            "content":     blocked_msg,
                            "is_error":    True,
                        }])
            else:
                engine.step(blocks)

    # ── Interactive REPL ──────────────────────────────────────────────────────

    def loop(self, new_session: bool = False) -> None:
        """Interactive chat mode."""
        if not new_session:
            latest = self.session_mgr.get_latest(self.cwd)
            if latest:
                infos = self.session_mgr.list_sessions(self.cwd)
                info  = infos[0]
                ts    = info.timestamp.strftime("%Y-%m-%d %H:%M")
                first = info.first_message or "(empty)"
                ans   = input(
                    f"Resume previous session? ({ts}, \"{first}...\") [y/N]: "
                ).strip().lower()
                if ans == "y":
                    self.session_id = latest

        prev   = self.session_mgr.load(self.cwd, self.session_id)
        engine = self._make_engine(prev)

        print(f"\ncortex-code v0.1  (exit: exit | new session: /new)\n")

        while True:
            try:
                task = input("You: ").strip()
            except (EOFError, KeyboardInterrupt):
                print("\nExiting.")
                break

            if not task:
                continue
            if task.lower() in ("exit", "quit"):
                break
            if task == "/new":
                self.session_id = self.session_mgr.new_session()
                engine.reset()
                print("Started a new session.\n")
                continue
            if task == "/sessions":
                for info in self.session_mgr.list_sessions(self.cwd):
                    ts = info.timestamp.strftime("%Y-%m-%d %H:%M")
                    print(f"  {info.session_id[:8]}  {ts}  {info.first_message}")
                continue

            answer = self.run(task, engine)
            engine.reset()
            print(f"\nAI: {answer}\n")
