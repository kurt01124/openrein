"""Session save/load — JSONL format."""
import json
import uuid
import datetime
from dataclasses import dataclass
from pathlib import Path
from . import config


def _sanitize(path: str, max_len: int = 200) -> str:
    """Convert a path to a string safe for use as a filename."""
    s = "".join(c if c.isascii() and c.isalnum() else "-" for c in path)
    if len(s) <= max_len:
        return s
    h = abs(hash(path)) % 0xFFFFFF
    return f"{s[:max_len]}-{h:06x}"


def _project_dir(cwd: str) -> Path:
    d = config.SESSION_DIR / _sanitize(cwd)
    d.mkdir(parents=True, exist_ok=True)
    return d


@dataclass
class SessionInfo:
    session_id: str
    timestamp: datetime.datetime
    first_message: str
    path: Path


class SessionManager:
    def new_session(self) -> str:
        return str(uuid.uuid4())

    def save(self, cwd: str, session_id: str, messages: list) -> None:
        """Overwrite the JSONL file with the current messages."""
        path = _project_dir(cwd) / f"{session_id}.jsonl"
        with path.open("w", encoding="utf-8") as f:
            for msg in messages:
                f.write(json.dumps(msg, ensure_ascii=False) + "\n")

    def load(self, cwd: str, session_id: str) -> list:
        """Load messages from a JSONL file."""
        path = _project_dir(cwd) / f"{session_id}.jsonl"
        if not path.exists():
            return []
        messages = []
        with path.open("r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if line:
                    try:
                        messages.append(json.loads(line))
                    except json.JSONDecodeError:
                        pass
        return messages

    def list_sessions(self, cwd: str) -> list[SessionInfo]:
        """Return sessions sorted by modification time (newest first)."""
        d = _project_dir(cwd)
        infos = []
        for p in sorted(d.glob("*.jsonl"), key=lambda x: x.stat().st_mtime, reverse=True):
            session_id = p.stem
            mtime = datetime.datetime.fromtimestamp(p.stat().st_mtime)
            first = ""
            try:
                with p.open("r", encoding="utf-8") as f:
                    for line in f:
                        line = line.strip()
                        if not line:
                            continue
                        msg = json.loads(line)
                        if msg.get("role") == "user":
                            c = msg.get("content", "")
                            if isinstance(c, str):
                                first = c[:60]
                            elif isinstance(c, list):
                                for b in c:
                                    if b.get("type") == "text":
                                        first = b.get("text", "")[:60]
                                        break
                            break
            except Exception:
                pass
            infos.append(SessionInfo(session_id, mtime, first, p))
        return infos

    def get_latest(self, cwd: str) -> str | None:
        """Return the most recent session_id, or None."""
        infos = self.list_sessions(cwd)
        return infos[0].session_id if infos else None

    def delete(self, cwd: str, session_id: str) -> None:
        """Delete a session file."""
        path = _project_dir(cwd) / f"{session_id}.jsonl"
        if path.exists():
            path.unlink()
