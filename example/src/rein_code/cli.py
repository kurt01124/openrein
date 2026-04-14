"""CLI entry point."""
import argparse
import sys
from .agent import Agent


def main():
    sys.stdout.reconfigure(encoding="utf-8")

    parser = argparse.ArgumentParser(
        prog="rein-code",
        description="Claude Code-style AI coding agent",
    )
    parser.add_argument("task", nargs="?", help="Task to run (omit for interactive mode)")
    parser.add_argument("--cwd",     help="Working directory (default: current directory)")
    parser.add_argument("--new",     action="store_true", help="Force a new session")
    parser.add_argument("--session", help="Session ID to resume")
    args = parser.parse_args()

    agent = Agent(cwd=args.cwd, session_id=args.session)

    if args.task:
        # Single-task mode
        print(f"Task: {args.task}\n{'=' * 60}")
        answer = agent.run(args.task)
        print(f"\n{'=' * 60}\n{answer}")
    else:
        # Interactive REPL
        agent.loop(new_session=args.new)
