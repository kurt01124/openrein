"""Tool descriptions — override built-in descriptions for rein-code."""
import datetime

_year = datetime.date.today().year

DESCRIPTIONS: dict[str, str] = {
    "Read": (
        "Reads a file from the local filesystem and returns its contents with line numbers.\n\n"
        "Usage:\n"
        "- Provide an absolute path to the file\n"
        "- Output format: each line is prefixed with its 1-based line number followed by a tab\n"
        "- Reads up to 2000 lines by default; use offset and limit for partial reads of large files\n"
        "- When you already know which part of the file you need, read only that part\n"
        "- To list a directory, use the Bash tool with ls — this tool reads files only\n"
        "- Returns an error if the file cannot be opened; it is safe to attempt reading a path the user provides"
    ),

    "Write": (
        "Writes content to a file, creating it or replacing it entirely.\n\n"
        "Usage:\n"
        "- If the file already exists, read it first with the Read tool before writing\n"
        "- Prefer the Edit tool for partial changes to an existing file — Write replaces everything\n"
        "- Use only to create a new file or when a complete rewrite is genuinely necessary\n"
        "- Parent directories are created automatically\n"
        "- Do not create documentation or README files unless the user explicitly requests it\n"
        "- Only use emojis if the user explicitly requests it"
    ),

    "Edit": (
        "Replaces an exact string in a file with new content.\n\n"
        "Usage:\n"
        "- Read the file with the Read tool before editing — this tool errors if the file has not been read\n"
        "- Copy old_string exactly from the Read output. The line-number prefix (number + tab) is not part of the file — do not include it in old_string or new_string\n"
        "- Preserve the original indentation (tabs/spaces) precisely; any mismatch will cause the edit to fail\n"
        "- Prefer this tool over Write when changing part of an existing file\n"
        "- old_string must be unique in the file; include more surrounding context if needed, or set replace_all=true to update every occurrence\n"
        "- Use replace_all=true when renaming a symbol or repeated value throughout the file\n"
        "- Only use emojis if the user explicitly requests it"
    ),

    "Bash": (
        "Executes a shell command and returns the combined output (stdout + stderr).\n\n"
        "Usage:\n"
        "- Prefer dedicated tools for common operations instead of shell equivalents:\n"
        "    file search    -> Glob  (not find or ls)\n"
        "    content search -> Grep  (not grep or rg)\n"
        "    reading files  -> Read  (not cat, head, tail)\n"
        "    editing files  -> Edit  (not sed, awk)\n"
        "    writing files  -> Write (not echo redirection)\n"
        "- Quote paths that contain spaces: e.g., cd \"my dir/file.txt\"\n"
        "- Use absolute paths and avoid cd to keep the working directory stable\n"
        "- Chain dependent commands with &&; issue independent commands as separate Bash calls\n"
        "- Do not separate multiple commands with newlines inside a single call\n"
        "- Default timeout: 120 seconds (2 minutes). Maximum: 600 seconds (10 minutes)\n"
        "- run_in_background=true defers output collection; omit '&' from the command itself"
    ),

    "Grep": (
        "Searches file contents with regular expressions and returns matches.\n\n"
        "Usage:\n"
        "- Use this tool for all content searches — do not run grep or rg through the Bash tool\n"
        "- Supports full regex syntax (e.g., \"log.*Error\", \"def\\s+\\w+\")\n"
        "- Narrow the search scope with glob (e.g., \"*.py\") or type (e.g., \"py\", \"js\", \"rust\") filters\n"
        "- Output modes: \"files_with_matches\" returns file paths only (default), \"content\" returns matching lines with optional context, \"count\" returns per-file match counts\n"
        "- For open-ended searches spanning many locations, use the Agent tool instead\n"
        "- Escape literal braces in patterns: use interface\\{\\} to match interface{} in Go code\n"
        "- Patterns match within a single line by default; multiline mode is not supported\n"
        "- Use head_limit and offset to page through large result sets"
    ),

    "Glob": (
        "Finds files by name pattern and returns matching paths sorted by modification time.\n\n"
        "Usage:\n"
        "- Accepts standard glob syntax: ** matches across directory levels, * matches within one level\n"
        "- Examples: \"**/*.py\", \"src/**/*.ts\", \"test_*.cpp\"\n"
        "- Use when you know roughly what a file is named but not where it lives\n"
        "- For open-ended exploration requiring multiple searches, use the Agent tool instead\n"
        "- To search file contents rather than names, use the Grep tool"
    ),

    "WebFetch": (
        "Fetches a URL and returns its content as plain text.\n\n"
        "Usage:\n"
        "- HTML tags are stripped; the result is unformatted plain text\n"
        "- Request timeout is 30 seconds\n"
        "- Use max_length to cap the returned text (default 20000 characters)\n"
        "- Suitable for documentation pages, plain-text files, and simple web content\n"
        "- Not designed for large downloads or binary content\n"
        "- For GitHub resources, the gh CLI via Bash (gh pr view, gh issue view) is often more convenient"
    ),

    "WebSearch": (
        "Searches the web and returns a ranked list of results, each with a title, URL, and short summary.\n\n"
        "Usage:\n"
        f"- Use for current events, recent releases, or topics beyond the model's training cutoff\n"
        f"- The current year is {_year} — include it in queries when recency matters (e.g., \"Python 3.13 release notes {_year}\")\n"
        "- Results include title, URL, and excerpt; follow up with WebFetch to read a full page\n"
        "- Set num_results to control how many results are returned (default 10)"
    ),
}


AGENT_DESCRIPTION = (
    "Launch a new agent to handle complex, multi-step tasks autonomously.\n\n"
    "The Agent tool launches specialized agents (subprocesses) that autonomously handle complex tasks.\n\n"
    "When NOT to use the Agent tool:\n"
    "- If you want to read a specific file path, use the Read tool or the Glob tool instead of the Agent tool, to find the match more quickly\n"
    "- If you are searching for a specific class definition like \"class Foo\", use the Glob tool instead, to find the match more quickly\n"
    "- If you are searching for code within a specific file or set of 2-3 files, use the Read tool instead of the Agent tool, to find the match more quickly\n\n"
    "Usage notes:\n"
    "- Always include a short description (3-5 words) summarizing what the agent will do\n"
    "- Launch multiple agents concurrently whenever possible, to maximize performance; to do that, use a single message with multiple tool uses\n"
    "- When the agent is done, it will return a single message back to you. The result returned by the agent is not visible to the user. To show the user the result, you should send a text message back to the user with a concise summary of the result.\n"
    "- Clearly tell the agent whether you expect it to write code or just to do research (search, file reads, web fetches, etc.), since it is not aware of the user's intent\n\n"
    "## Writing the prompt\n\n"
    "Brief the agent like a smart colleague who just walked into the room — it hasn't seen this conversation, doesn't know what you've tried, doesn't understand why this task matters.\n"
    "- Explain what you're trying to accomplish and why.\n"
    "- Describe what you've already learned or ruled out.\n"
    "- Give enough context about the surrounding problem that the agent can make judgment calls rather than just following a narrow instruction.\n\n"
    "**Never delegate understanding.** Don't write \"based on your findings, fix the bug\" or \"based on the research, implement it.\" Those phrases push synthesis onto the agent instead of doing it yourself. Write prompts that prove you understood: include file paths, line numbers, what specifically to change."
)

TODO_DESCRIPTION = (
    "Update the todo list for the current session. To be used proactively and often to track progress and pending tasks.\n\n"
    "Use this tool proactively in these scenarios:\n"
    "1. Complex multi-step tasks - When a task requires 3 or more distinct steps or actions\n"
    "2. Non-trivial and complex tasks - Tasks that require careful planning or multiple operations\n"
    "3. When you start working on a task - Mark it as in_progress BEFORE beginning work. Ideally you should only have one todo as in_progress at a time\n"
    "4. After completing a task - Mark it as completed and add any new follow-up tasks discovered during implementation\n\n"
    "Task states: pending / in_progress / completed\n"
    "IMPORTANT: Exactly ONE task must be in_progress at any time.\n\n"
    "When NOT to use this tool:\n"
    "1. There is only a single, straightforward task\n"
    "2. The task is trivial and tracking it provides no organizational benefit\n"
    "3. The task can be completed in less than 3 trivial steps"
)

ASK_USER_DESCRIPTION = (
    "Asks the user multiple choice questions to gather information, clarify ambiguity, understand preferences, make decisions or offer them choices.\n\n"
    "Use this tool when you need to:\n"
    "1. Gather user preferences or requirements\n"
    "2. Clarify ambiguous instructions\n"
    "3. Get decisions on implementation choices as you work\n"
    "4. Offer choices to the user about what direction to take.\n\n"
    "Usage notes:\n"
    "- Users will always be able to select \"Other\" to provide custom text input\n"
    "- Use multiSelect: true to allow multiple answers to be selected for a question\n"
    "- If you recommend a specific option, make that the first option in the list and add \"(Recommended)\" at the end of the label\n\n"
    "Plan mode note: In plan mode, use this tool to clarify requirements or choose between approaches BEFORE finalizing your plan. Do NOT use this tool to ask \"Is my plan ready?\" or \"Should I proceed?\" - use ExitPlanMode for plan approval."
)

ENTER_PLAN_MODE_DESCRIPTION = (
    "Use this tool proactively when you're about to start a non-trivial implementation task. "
    "Getting user sign-off on your approach before writing code prevents wasted effort and ensures alignment. "
    "This tool transitions you into plan mode where you can explore the codebase and design an implementation approach for user approval.\n\n"
    "## When to Use This Tool\n\n"
    "**Prefer using EnterPlanMode** for implementation tasks unless they're simple. Use it when ANY of these conditions apply:\n\n"
    "1. **New Feature Implementation**: Adding meaningful new functionality\n"
    "2. **Multiple Valid Approaches**: The task can be solved in several different ways\n"
    "3. **Code Modifications**: Changes that affect existing behavior or structure\n"
    "4. **Architectural Decisions**: The task requires choosing between patterns or technologies\n"
    "5. **Multi-File Changes**: The task will likely touch more than 2-3 files\n"
    "6. **Unclear Requirements**: You need to explore before understanding the full scope\n\n"
    "## When NOT to Use This Tool\n\n"
    "Only skip EnterPlanMode for simple tasks:\n"
    "- Single-line or few-line fixes (typos, obvious bugs, small tweaks)\n"
    "- Adding a single function with clear requirements\n"
    "- Tasks where the user has given very specific, detailed instructions\n\n"
    "## What Happens in Plan Mode\n\n"
    "In plan mode, you'll:\n"
    "1. Thoroughly explore the codebase using Glob, Grep, and Read tools\n"
    "2. Understand existing patterns and architecture\n"
    "3. Design an implementation approach\n"
    "4. Present your plan to the user for approval\n"
    "5. Use AskUserQuestion if you need to clarify approaches\n"
    "6. Exit plan mode with ExitPlanMode when ready to implement"
)

EXIT_PLAN_MODE_DESCRIPTION = (
    "Use this tool when you are in plan mode and have finished designing your plan and are ready for user approval.\n\n"
    "## How This Tool Works\n"
    "- This tool presents your plan to the user for review and approval\n"
    "- Include your complete implementation plan in the `plan` parameter\n\n"
    "## Before Using This Tool\n"
    "Ensure your plan is complete and unambiguous:\n"
    "- If you have unresolved questions about requirements or approach, use AskUserQuestion first\n"
    "- Once your plan is finalized, use THIS tool to request approval\n\n"
    "**Important:** Do NOT use AskUserQuestion to ask \"Is this plan okay?\" or \"Should I proceed?\" - "
    "that's exactly what THIS tool does. ExitPlanMode inherently requests user approval of your plan."
)
