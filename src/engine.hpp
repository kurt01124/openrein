#pragma once
#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <unordered_set>
#include <pybind11/pybind11.h>
#include "json_fwd.hpp"
#include "mcp/mcp_client.hpp"
#include "exec_result.hpp"

namespace py = pybind11;

namespace openrein {

class  SubAgent;
struct ToolBase;

struct CustomToolDef {
    std::string name;
    std::string description;
    json        schema;
    py::object  callable;
};

/// MCP server tool entry attached to the Engine
struct McpEntry {
    std::string              server_name;
    std::string              tool_name;
    std::string              description;
    std::shared_ptr<McpClient> client;
    json                     input_schema;
};

/// openrein core engine.
/// Responsible for message state management + tool_use execution loop.
/// The developer invokes the model directly.
class Engine {
public:
    /// tools: True(default, enables builtins) | False | list[dict](compat, enables builtins)
    /// max_turns: -1(default, unlimited) | positive int -> ValueError when step count with tool_use exceeds limit
    explicit Engine(std::string system_prompt = "",
                    py::object tools = py::bool_(true),
                    int max_turns = -1);

    // --- Conversation management ---
    void add(const std::string& role, py::object content);
    bool step(py::object content);
    void reset();

    // --- Tool registration ---
    py::object register_tool_decorator(
        const std::string& name,
        const std::string& description,
        py::object schema = py::none());

    void register_tool(
        py::object func,
        const std::string& name,
        const std::string& description,
        py::object schema = py::none());

    /// Register a Python/C++ tool object that inherits from ToolBase (contrib interface).
    void register_tool(std::shared_ptr<ToolBase> tool);

    // --- Subagents ---
    void add_subagent(const std::string& name, std::shared_ptr<SubAgent> subagent);
    std::vector<std::shared_ptr<SubAgent>> active_subagents() const;

    // --- MCP server ---
    /// command_or_url: list[str] -> stdio transport, str -> HTTP transport
    void add_mcp_server(const std::string& name, py::object command_or_url);

    // --- Compact ---
    bool maybe_compact(py::object model_fn, int threshold = 80000);

    // --- SubAgent completion callback ---
    void on_subagent_done(const std::string& tool_use_id,
                          const std::string& result,
                          bool is_error = false);

    // --- Accessors ---
    /// include_builtins=true(default): return all builtin + custom + subagent
    /// include_builtins=false: return only custom + subagent
    py::list   tool_schemas(bool include_builtins = true) const;

    /// Replace the description of a builtin tool at runtime.
    /// tool_name: "Read", "Write", "Edit", "Bash", "Grep", "Glob", "WebFetch", "WebSearch"
    void set_tool_description(const std::string& tool_name, const std::string& desc);

    /// Return the current effective description of a builtin tool (override or default).
    std::string get_tool_description(const std::string& tool_name) const;
    py::list   get_messages() const;
    void       set_messages(py::list messages);
    bool       is_done() const { return done_; }
    py::object get_last_answer() const;
    std::vector<std::string> get_tool_names() const;
    std::string get_system_prompt() const { return system_prompt_; }
    void        set_system_prompt(std::string v) { system_prompt_ = std::move(v); }
    int         get_max_turns()  const { return max_turns_; }
    int         get_turn_count() const { return turn_count_; }
    std::string repr() const;

private:
    std::string system_prompt_;
    std::vector<json> messages_;
    std::vector<CustomToolDef> custom_tools_;
    std::vector<std::pair<std::string, std::shared_ptr<SubAgent>>> subagents_;
    std::vector<McpEntry> mcp_entries_;
    bool done_ = false;
    std::optional<std::string> last_answer_;
    int  max_turns_  = -1;   // -1 = unlimited
    int  turn_count_ = 0;    // incremented when step is called with tool_use

    // Built-in tools (Read/Write/Edit/Bash/Grep/Glob)
    std::vector<std::unique_ptr<ToolBase>> builtin_tools_;

    // contrib tools (Python/C++ objects that inherit ToolBase)
    std::vector<std::shared_ptr<ToolBase>> contrib_tools_;

    // Collect multiple tool_use results from the same assistant turn and add as a single user message
    struct PendingResult {
        std::string              tool_use_id;
        std::optional<std::string> result;    // nullopt = waiting for subagent result
        bool                     is_error = false;
    };
    std::vector<PendingResult> pending_results_;

    ExecResult execute_tool(const std::string& name, const json& input);
    ExecResult execute_builtin(const std::string& name, const json& input);
    ExecResult execute_custom(const std::string& name, const json& input);
    void       flush_pending_if_ready();
};

} // namespace openrein
