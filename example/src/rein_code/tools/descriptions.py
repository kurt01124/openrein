"""Tool descriptions."""

DESCRIPTIONS: dict[str, str] = {
    "Read": (
        "Reads a file from the local filesystem. You can access any file directly by using this tool.\n"
        "Assume this tool is able to read all files on the machine. If the User provides a path to a file assume that path is valid. It is okay to read a file that does not exist; an error will be returned.\n\n"
        "Usage:\n"
        "- The file_path parameter must be an absolute path, not a relative path\n"
        "- By default, it reads up to 2000 lines starting from the beginning of the file\n"
        "- When you already know which part of the file you need, only read that part. This can be important for larger files.\n"
        "- Results are returned using cat -n format, with line numbers starting at 1\n"
        "- This tool can only read files, not directories. To read a directory, use an ls command via the Bash tool.\n"
        "- You will regularly be asked to read screenshots. If the user provides a path to a screenshot, ALWAYS use this tool to view the file at the path. This tool will work with all temporary file paths.\n"
        "- If you read a file that exists but has empty contents you will receive a system reminder warning in place of file contents."
    ),

    "Write": (
        "Writes a file to the local filesystem.\n\n"
        "Usage:\n"
        "- This tool will overwrite the existing file if there is one at the provided path.\n"
        "- If this is an existing file, you MUST use the Read tool first to read the file's contents. This tool will fail if you did not read the file first.\n"
        "- Prefer the Edit tool for modifying existing files — it only sends the diff. Only use this tool to create new files or for complete rewrites.\n"
        "- NEVER create documentation files (*.md) or README files unless explicitly requested by the User.\n"
        "- Only use emojis if the user explicitly requests it. Avoid writing emojis to files unless asked."
    ),

    "Edit": (
        "Performs exact string replacements in files.\n\n"
        "Usage:\n"
        "- You must use your `Read` tool at least once in the conversation before editing. This tool will error if you attempt an edit without reading the file.\n"
        "- When editing text from Read tool output, ensure you preserve the exact indentation (tabs/spaces) as it appears AFTER the line number prefix. The line number prefix format is: line number + tab. Everything after that is the actual file content to match. Never include any part of the line number prefix in the old_string or new_string.\n"
        "- ALWAYS prefer editing existing files in the codebase. NEVER write new files unless explicitly required.\n"
        "- Only use emojis if the user explicitly requests it. Avoid adding emojis to files unless asked.\n"
        "- The edit will FAIL if `old_string` is not unique in the file. Either provide a larger string with more surrounding context to make it unique or use `replace_all` to change every instance of `old_string`.\n"
        "- Use `replace_all` for replacing and renaming strings across the file. This parameter is useful if you want to rename a variable for instance."
    ),

    "Bash": (
        "Executes a given bash command and returns its output.\n\n"
        "The working directory persists between commands, but shell state does not. The shell environment is initialized from the user's profile (bash or zsh).\n\n"
        "IMPORTANT: Avoid using this tool to run `find`, `grep`, `cat`, `head`, `tail`, `sed`, `awk`, or `echo` commands, unless explicitly instructed or after you have verified that a dedicated tool cannot accomplish your task. Instead, use the appropriate dedicated tool as this will provide a much better experience for the user:\n\n"
        " - File search: Use Glob (NOT find or ls)\n"
        " - Content search: Use Grep (NOT grep or rg)\n"
        " - Read files: Use Read (NOT cat/head/tail)\n"
        " - Edit files: Use Edit (NOT sed/awk)\n"
        " - Write files: Use Write (NOT echo >/cat <<EOF)\n"
        " - Communication: Output text directly (NOT echo/printf)\n"
        "While the Bash tool can do similar things, it's better to use the built-in tools as they provide a better user experience and make it easier to review tool calls and give permission.\n\n"
        "# Instructions\n"
        " - If your command will create new directories or files, first use this tool to run `ls` to verify the parent directory exists and is the correct location.\n"
        " - Always quote file paths that contain spaces with double quotes in your command (e.g., cd \"path with spaces/file.txt\")\n"
        " - Try to maintain your current working directory throughout the session by using absolute paths and avoiding usage of `cd`. You may use `cd` if the User explicitly requests it.\n"
        " - You may specify an optional timeout in milliseconds (up to 600000ms / 10 minutes). By default, your command will timeout after 120000ms (2 minutes).\n"
        " - You can use the `run_in_background` parameter to run the command in the background. Only use this if you don't need the result immediately and are OK being notified when the command completes later. You do not need to check the output right away - you'll be notified when it finishes. You do not need to use '&' at the end of the command when using this parameter.\n"
        " - When issuing multiple commands:\n"
        "  - If the commands are independent and can run in parallel, make multiple Bash tool calls in a single message. Example: if you need to run \"git status\" and \"git diff\", send a single message with two Bash tool calls in parallel.\n"
        "  - If the commands depend on each other and must run sequentially, use a single Bash call with '&&' to chain them together.\n"
        "  - Use ';' only when you need to run commands sequentially but don't care if earlier commands fail.\n"
        "  - DO NOT use newlines to separate commands (newlines are ok in quoted strings)."
    ),

    "Grep": (
        "A powerful search tool built on ripgrep\n\n"
        "  Usage:\n"
        "  - ALWAYS use Grep for search tasks. NEVER invoke `grep` or `rg` as a Bash command. The Grep tool has been optimized for correct permissions and access.\n"
        "  - Supports full regex syntax (e.g., \"log.*Error\", \"function\\s+\\w+\")\n"
        "  - Filter files with glob parameter (e.g., \"*.js\", \"**/*.tsx\") or type parameter (e.g., \"js\", \"py\", \"rust\")\n"
        "  - Output modes: \"content\" shows matching lines, \"files_with_matches\" shows only file paths (default), \"count\" shows match counts\n"
        "  - Use Agent tool for open-ended searches requiring multiple rounds\n"
        "  - Pattern syntax: Uses ripgrep (not grep) - literal braces need escaping (use `interface\\{\\}` to find `interface{}` in Go code)\n"
        "  - Multiline matching: By default patterns match within single lines only. For cross-line patterns like `struct \\{[\\s\\S]*?field`, use `multiline: true`\n"
    ),

    "Glob": (
        "- Fast file pattern matching tool that works with any codebase size\n"
        "- Supports glob patterns like \"**/*.js\" or \"src/**/*.ts\"\n"
        "- Returns matching file paths sorted by modification time\n"
        "- Use this tool when you need to find files by name patterns\n"
        "- When you are doing an open ended search that may require multiple rounds of globbing and grepping, use the Agent tool instead"
    ),

    "WebFetch": (
        "Fetches content from a specified URL and returns it as text.\n\n"
        "Usage notes:\n"
        "  - IMPORTANT: If an MCP-provided web fetch tool is available, prefer using that tool instead of this one, as it may have fewer restrictions.\n"
        "  - The URL must be a fully-formed valid URL\n"
        "  - HTTP URLs will be automatically upgraded to HTTPS\n"
        "  - This tool is read-only and does not modify any files\n"
        "  - Results may be truncated if the content is very large\n"
        "  - For GitHub URLs, prefer using the gh CLI via Bash instead (e.g., gh pr view, gh issue view, gh api)."
    ),

    "WebSearch": (
        "- Allows Claude to search the web and use the results to inform responses\n"
        "- Provides up-to-date information for current events and recent data\n"
        "- Returns search result information formatted as search result blocks, including links as markdown hyperlinks\n"
        "- Use this tool for accessing information beyond Claude's knowledge cutoff\n\n"
        "CRITICAL REQUIREMENT - You MUST follow this:\n"
        "  - After answering the user's question, you MUST include a \"Sources:\" section at the end of your response\n"
        "  - In the Sources section, list all relevant URLs from the search results as markdown hyperlinks: [Title](URL)\n"
        "  - This is MANDATORY - never skip including sources in your response\n\n"
        "Usage notes:\n"
        "  - IMPORTANT: Use the correct year in search queries. The current year is 2026.\n"
        "  - Example: If the user asks for \"latest React docs\", search for \"React documentation 2026\", NOT last year."
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
