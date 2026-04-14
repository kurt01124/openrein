# openrein

A minimal harness engine for building LLM agent apps in Python, powered by a C++ core via pybind11.

openrein handles the repetitive parts — message state, tool dispatch, sub-agent coordination, context compaction — while leaving model calls entirely to the developer.

## Features

- **Engine** — message state + tool_use execution loop
- **SubAgent** — nested agent with its own tool registry and system prompt
- **Compact** — token estimation and context compaction utilities
- **8 built-in tools** — Read, Write, Edit, Bash, Grep, Glob, WebFetch, WebSearch
- **MCP support** — connect stdio or HTTP MCP servers via `add_mcp_server()`
- **ToolBase** — pure Python base class for writing custom tools without a C++ build

## Requirements

- Python >= 3.9
- CMake >= 3.18
- A C++17 compiler (MSVC, GCC, Clang)
- pybind11 >= 2.13

## Installation

### From PyPI

```bash
pip install openrein
```

### Build from source

```bash
git clone https://github.com/kurt01124/openrein.git
cd openrein
pip install -e ".[dev]"
```

### Build a wheel

Use the provided build scripts to produce a `.whl` file in `dist/`:

**Windows:**
```bat
scripts\build.bat
```

**Linux / macOS:**
```bash
chmod +x scripts/build.sh
./scripts/build.sh
```

Both scripts install the required build tools (`scikit-build-core`, `pybind11`, `build`) automatically and output the wheel to `dist/`.

## Quick Start

```python
import anthropic
import openrein

client = anthropic.Anthropic()
engine = openrein.Engine(system_prompt="You are a helpful assistant.")

@engine.tool("get_time", "Return the current ISO timestamp")
def get_time() -> str:
    from datetime import datetime
    return datetime.now().isoformat()

engine.add("user", "What time is it?")

while True:
    resp = client.messages.create(
        model="claude-opus-4-5",
        max_tokens=1024,
        system=engine.system_prompt,
        tools=engine.tool_schemas(),
        messages=engine.messages,
    )
    if engine.step(resp.model_dump()):
        break

print(engine.last_answer)
```

## Custom Tools (ToolBase)

Extend `ToolBase` to add tools in pure Python — no C++ required:

```python
import openrein

class WordCount(openrein.ToolBase):
    def name(self) -> str:
        return "word_count"

    def description(self) -> str:
        return "Count the number of words in a string"

    def input_schema(self) -> dict:
        return {
            "type": "object",
            "properties": {"text": {"type": "string"}},
            "required": ["text"],
        }

    def call(self, input: dict) -> str:
        return str(len(input["text"].split()))

engine = openrein.Engine()
engine.register_tool(WordCount())
```

## SubAgent

```python
import openrein

def my_model(messages, tools):
    # your model call here
    ...

sub = openrein.SubAgent(description="Summarize text", model=my_model)

@sub.tool("summarize", "Summarize the given text")
def summarize(text: str) -> str:
    return text[:100] + "..."

engine = openrein.Engine()
engine.add_subagent("summarizer", sub)
```

## MCP Server

```python
engine = openrein.Engine()

# stdio transport (spawns a process)
engine.add_mcp_server("filesystem", ["npx", "-y", "@modelcontextprotocol/server-filesystem", "/tmp"])

# HTTP transport
engine.add_mcp_server("my-server", "http://localhost:8080/mcp")
```

## Built-in Tools

| Tool | Description |
|------|-------------|
| `Read` | Read a file from the filesystem |
| `Write` | Write content to a file |
| `Edit` | Make targeted edits to a file |
| `Bash` | Execute a shell command |
| `Grep` | Search file contents with regex |
| `Glob` | Find files by pattern |
| `WebFetch` | Fetch content from a URL |
| `WebSearch` | Search the web |

To use built-in tools:

```python
engine = openrein.Engine(tools=True)          # enable all built-ins (default)
schemas = openrein.default_tools()            # get schemas only
engine = openrein.Engine(tools=False)         # disable built-ins
```

## Context Compaction

```python
compact = openrein.Compact()

if compact.should_compact(engine.messages, threshold=80000):
    prompt = compact.make_prompt(engine.messages)
    summary_text = my_model_call(prompt)
    engine.messages = compact.apply(engine.messages, summary_text)
```

Or use the convenience method on Engine:

```python
engine.maybe_compact(my_model_fn, threshold=80000)
```

## Example: cortex-code

`example/` contains **cortex-code**, a Claude Code-style AI coding agent CLI built on openrein.  
It uses GLM (ZhipuAI) as the model backend via an OpenAI-compatible API.

### Setup

cortex-code depends on openrein, which must be built and installed first.

**Step 1 — Build and install openrein:**

```bash
# Windows
scripts\build.bat
pip install dist/openrein-*.whl

# Linux / macOS
./scripts/build.sh
pip install dist/openrein-*.whl
```

**Step 2 — Install cortex-code:**

```bash
cd example
pip install -e .
```

**Step 3 — Set your API key:**

```bash
export GLM_API_KEY=<your-key>   # Linux / macOS
set GLM_API_KEY=<your-key>      # Windows
```

### Run

**Interactive mode (REPL):**
```bash
cortex-code
```

**Single task:**
```bash
cortex-code "Refactor the auth module to use JWT"
cortex-code "Write tests for utils.py"
```

**Options:**

| Flag | Description |
|------|-------------|
| `--cwd PATH` | Set working directory (default: current) |
| `--new` | Force a new session |
| `--session ID` | Resume a specific session |

### Model configuration

| Environment variable | Default | Description |
|----------------------|---------|-------------|
| `GLM_API_KEY` | — | ZhipuAI API key (required) |
| `CORTEX_MAIN_MODEL` | `glm-5-turbo` | Main agent model |
| `CORTEX_SUB_MODEL` | `glm-4.5-air` | Sub-agent model |

---

> **Legal notice — cortex-code example**
>
> The system prompt used in `example/` is derived from prompts that have been publicly
> disclosed and widely circulated. It is included here **solely for educational and
> illustrative purposes** to demonstrate how openrein can be used to build a coding
> agent.
>
> This example is provided **as-is**, without warranty of any kind. The author makes
> **no representations** regarding the intellectual property status of the prompt text,
> and **accepts no liability** for any commercial use, derivative works, or legal claims
> arising from use of the example. Anyone who uses this example in a commercial product
> or service does so **entirely at their own risk and responsibility**.

## License

MIT
