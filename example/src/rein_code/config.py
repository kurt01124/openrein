"""Environment config — API keys, model names, CWD."""
import os
from pathlib import Path

# GLM API
GLM_API_KEY  = os.environ.get("GLM_API_KEY")
GLM_BASE_URL = "https://api.z.ai/api/coding/paas/v4"
GLM_HEADERS  = {"User-Agent": "ai-sdk/openai-compatible/0.1.0"}

# Models
MAIN_MODEL = os.environ.get("REIN_MAIN_MODEL", "glm-5-turbo")
SUB_MODEL  = os.environ.get("REIN_SUB_MODEL",  "glm-4.5-air")

# Session storage path
SESSION_DIR = Path.home() / ".rein-code" / "projects"


def get_cwd(override: str | None = None) -> str:
    """Return the working directory: override if provided, otherwise os.getcwd()."""
    return str(Path(override).resolve()) if override else os.getcwd()


def is_git_repo(cwd: str) -> bool:
    """Return True if the directory is a git repository."""
    return (Path(cwd) / ".git").exists()
