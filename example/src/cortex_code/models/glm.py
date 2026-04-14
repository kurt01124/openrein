"""GLM client."""
from openai import OpenAI
from .. import config


_client: OpenAI | None = None


def _get_client() -> OpenAI:
    global _client
    if _client is None:
        _client = OpenAI(
            api_key=config.GLM_API_KEY,
            base_url=config.GLM_BASE_URL,
            default_headers=config.GLM_HEADERS,
        )
    return _client


def call_main(messages: list, tools: list | None = None):
    """Call the main model (glm-5-turbo)."""
    kwargs = {
        "model": config.MAIN_MODEL,
        "messages": messages,
        "max_tokens": 8192,
    }
    if tools:
        kwargs["tools"] = tools
    return _get_client().chat.completions.create(**kwargs)


def call_sub(messages: list, tools: list | None = None):
    """Call the sub-agent model (glm-4.5-air)."""
    kwargs = {
        "model": config.SUB_MODEL,
        "messages": messages,
        "max_tokens": 4096,
    }
    if tools:
        kwargs["tools"] = tools
    return _get_client().chat.completions.create(**kwargs)
