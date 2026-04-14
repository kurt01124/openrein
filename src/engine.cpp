#include "engine.hpp"
#include "subagent.hpp"
#include "compact.hpp"
#include "tools/tool_base.hpp"
#include "tools/read.hpp"
#include "tools/write.hpp"
#include "tools/edit.hpp"
#include "tools/bash.hpp"
#include "tools/grep.hpp"
#include "tools/glob.hpp"
#include "tools/web_fetch.hpp"
#include "tools/web_search.hpp"
#include "mcp/stdio_transport.hpp"
#include "mcp/http_transport.hpp"
#include <pybind11/stl.h>
#include <unordered_set>
#include <stdexcept>
#include <sstream>

// py_to_json / json_to_py are defined in bindings.cpp
namespace openrein {
json             py_to_json(pybind11::handle obj);
pybind11::object json_to_py(const json& val);
}

namespace openrein {

// ---------------------------------------------------------------------------
// Constructor (C2: tools parameter accepts both bool|list)
// ---------------------------------------------------------------------------

Engine::Engine(std::string system_prompt, py::object tools, int max_turns)
    : system_prompt_(std::move(system_prompt))
    , max_turns_(max_turns)
{
    bool use_builtin = true;
    if (py::isinstance<py::bool_>(tools)) {
        use_builtin = tools.cast<bool>();
    } else if (py::isinstance<py::list>(tools)) {
        // If list[dict] is passed, enable builtins for compatibility
        use_builtin = true;
    }
    // If False, disable builtins

    if (use_builtin) {
        builtin_tools_.push_back(std::make_unique<ReadTool>());
        builtin_tools_.push_back(std::make_unique<WriteTool>());
        builtin_tools_.push_back(std::make_unique<EditTool>());
        builtin_tools_.push_back(std::make_unique<BashTool>());
        builtin_tools_.push_back(std::make_unique<GrepTool>());
        builtin_tools_.push_back(std::make_unique<GlobTool>());
        builtin_tools_.push_back(std::make_unique<WebFetchTool>());
        builtin_tools_.push_back(std::make_unique<WebSearchTool>());
    }
}

// ---------------------------------------------------------------------------
// Conversation management
// ---------------------------------------------------------------------------

void Engine::add(const std::string& role, py::object content) {
    json msg = {
        {"role",    role},
        {"content", py_to_json(content)}
    };
    messages_.push_back(std::move(msg));
}

bool Engine::step(py::object content) {
    // m2: guard against re-calling after done
    if (done_) {
        throw py::value_error(
            "Cannot call step() after engine is done. "
            "Call reset() to start a new conversation."
        );
    }

    // C4: normalize content — unify string/dict/list all into array
    json content_json = normalize_content(py_to_json(content));

    // Add assistant message
    messages_.push_back({{"role", "assistant"}, {"content", content_json}});

    // Collect tool_use blocks (m3: guard against duplicate IDs)
    std::vector<json> tool_uses;
    std::unordered_set<std::string> seen_ids;
    for (const auto& block : content_json) {
        if (block.is_object() && block.value("type", "") == "tool_use") {
            const std::string id = block["id"].get<std::string>();
            if (seen_ids.count(id))
                throw py::value_error("Duplicate tool_use id in single step: " + id);
            seen_ids.insert(id);
            tool_uses.push_back(block);
        }
    }

    // No tool_use -> done
    if (tool_uses.empty()) {
        done_ = true;
        // content_json is always an array, so no need for string branch
        for (auto it = content_json.rbegin(); it != content_json.rend(); ++it) {
            if ((*it).value("type", "") == "text") {
                last_answer_ = (*it)["text"].get<std::string>();
                break;
            }
        }
        return true;
    }

    // Initialize pending_results_
    pending_results_.clear();
    for (const auto& tu : tool_uses) {
        pending_results_.push_back({tu["id"].get<std::string>(), std::nullopt, false});
    }

    // G3: check max_turns (before tool execution)
    turn_count_++;
    if (max_turns_ > 0 && turn_count_ > max_turns_) {
        throw py::value_error(
            "Max turns (" + std::to_string(max_turns_) + ") exceeded."
        );
    }

    // Process each tool_use
    for (size_t i = 0; i < tool_uses.size(); ++i) {
        const auto& tu         = tool_uses[i];
        const std::string tid  = tu["id"].get<std::string>();
        const std::string tname= tu["name"].get<std::string>();
        const json tinput      = tu.value("input", json::object());

        // Check if this is a SubAgent
        bool is_subagent = false;
        for (auto& [name, subagent] : subagents_) {
            if (name == tname) {
                std::string task = tinput.value("task", tinput.dump());
                subagent->activate(tid, task);
                is_subagent = true;
                break;
            }
        }

        if (!is_subagent) {
            // G1: handle is_error via ExecResult
            ExecResult er = execute_tool(tname, tinput);
            pending_results_[i].result   = std::move(er.content);
            pending_results_[i].is_error = er.is_error;
        }
    }

    flush_pending_if_ready();
    return false;
}

void Engine::reset() {
    // m1: also reset SubAgent state
    for (auto& [name, sa] : subagents_) sa->reset();
    messages_.clear();
    pending_results_.clear();
    done_ = false;
    last_answer_ = std::nullopt;
    turn_count_ = 0;
}

// ---------------------------------------------------------------------------
// Tool registration
// ---------------------------------------------------------------------------

py::object Engine::register_tool_decorator(
    const std::string& name,
    const std::string& description,
    py::object schema)
{
    return py::cpp_function(
        [this, name, description, schema](py::object func) mutable -> py::object {
            this->register_tool(func, name, description, schema);
            return func;
        },
        py::name("tool_decorator"),
        py::doc("openrein tool decorator")
    );
}

void Engine::register_tool(
    py::object func,
    const std::string& name,
    const std::string& description,
    py::object schema)
{
    json schema_json;
    if (!schema.is_none()) {
        schema_json = py_to_json(schema);
    } else {
        py::module_ inspect = py::module_::import("inspect");
        py::object  sig     = inspect.attr("signature")(func);
        py::dict    params  = sig.attr("parameters").cast<py::dict>();

        json props = json::object();
        std::vector<std::string> required;

        for (const auto& kv : params) {
            std::string pname = kv.first.cast<std::string>();
            py::object  param = py::reinterpret_borrow<py::object>(kv.second);

            py::object ann   = param.attr("annotation");
            py::object empty = inspect.attr("Parameter").attr("empty");
            std::string type_str = "string";

            if (!ann.is(empty)) {
                std::string ann_name;
                try { ann_name = py::str(ann).cast<std::string>(); } catch (...) {}
                if (ann_name.find("int")   != std::string::npos) type_str = "integer";
                else if (ann_name.find("float") != std::string::npos) type_str = "number";
                else if (ann_name.find("bool")  != std::string::npos) type_str = "boolean";
            }

            props[pname] = {{"type", type_str}};
            py::object defval = param.attr("default");
            if (defval.is(empty)) required.push_back(pname);
        }

        schema_json = {
            {"type",       "object"},
            {"properties", props},
            {"required",   required}
        };
    }

    for (auto it = custom_tools_.begin(); it != custom_tools_.end(); ++it) {
        if (it->name == name) { custom_tools_.erase(it); break; }
    }
    custom_tools_.push_back({name, description, std::move(schema_json), func});
}

void Engine::register_tool(std::shared_ptr<ToolBase> tool) {
    const std::string nm = tool->name();
    contrib_tools_.erase(
        std::remove_if(contrib_tools_.begin(), contrib_tools_.end(),
            [&nm](const auto& t){ return t->name() == nm; }),
        contrib_tools_.end());
    contrib_tools_.push_back(std::move(tool));
}

// ---------------------------------------------------------------------------
// Subagents
// ---------------------------------------------------------------------------

void Engine::add_subagent(const std::string& name, std::shared_ptr<SubAgent> subagent) {
    subagent->set_parent(this, name);
    for (auto it = subagents_.begin(); it != subagents_.end(); ++it) {
        if (it->first == name) { subagents_.erase(it); break; }
    }
    subagents_.emplace_back(name, std::move(subagent));
}

std::vector<std::shared_ptr<SubAgent>> Engine::active_subagents() const {
    std::vector<std::shared_ptr<SubAgent>> result;
    for (const auto& [name, sa] : subagents_) {
        if (sa->is_running()) result.push_back(sa);
    }
    return result;
}

// ---------------------------------------------------------------------------
// MCP server connection
// ---------------------------------------------------------------------------

void Engine::add_mcp_server(const std::string& name, py::object command_or_url) {
    std::unique_ptr<McpTransport> transport;

    if (py::isinstance<py::list>(command_or_url)) {
        // stdio transport: ["python", "server.py"] format
        std::vector<std::string> cmd;
        for (const auto& item : command_or_url.cast<py::list>())
            cmd.push_back(item.cast<std::string>());
        transport = std::make_unique<StdioTransport>(cmd);
    } else {
        // HTTP transport: "http://localhost:3000/mcp" format
        transport = std::make_unique<HttpTransport>(
            command_or_url.cast<std::string>()
        );
    }

    auto client = std::make_shared<McpClient>(std::move(transport));
    client->connect();  // throws on failure

    auto tools = client->list_tools();
    for (const auto& tool : tools) {
        mcp_entries_.push_back({name, tool.name, tool.description,
                                client, tool.input_schema});
    }
}

// ---------------------------------------------------------------------------
// M1: maybe_compact
// ---------------------------------------------------------------------------

bool Engine::maybe_compact(py::object model_fn, int threshold) {
    Compact compact;
    py::list py_msgs = get_messages();
    if (!compact.should_compact(py_msgs, threshold)) return false;

    std::string prompt = compact.make_prompt(py_msgs);
    py::list single_msg;
    single_msg.append(json_to_py({{"role","user"},{"content",prompt}}));

    py::object response;
    try {
        response = model_fn(single_msg);
    } catch (py::error_already_set&) {
        return false;  // keep original if compact fails
    }

    // Parse response
    std::string resp_text;
    json resp_json = normalize_content(py_to_json(response));
    for (const auto& block : resp_json) {
        if (block.value("type","") == "text") {
            resp_text = block.value("text","");
            break;
        }
    }

    std::string summary = compact.parse_response(resp_text);
    messages_ = {{ {"role","user"}, {"content", summary} }};
    return true;
}

// ---------------------------------------------------------------------------
// SubAgent completion callback
// ---------------------------------------------------------------------------

void Engine::on_subagent_done(const std::string& tool_use_id,
                               const std::string& result,
                               bool is_error) {
    // m3: update all matching entries (guard against duplicate IDs)
    for (auto& pr : pending_results_) {
        if (pr.tool_use_id == tool_use_id) {
            pr.result   = result;
            pr.is_error = is_error;
        }
    }
    flush_pending_if_ready();
}

void Engine::flush_pending_if_ready() {
    if (pending_results_.empty()) return;
    for (const auto& pr : pending_results_) {
        if (!pr.result.has_value()) return;
    }

    json tool_results = json::array();
    for (const auto& pr : pending_results_) {
        // G1: include the is_error field only when true (omit when false — per API spec)
        json entry = {
            {"type",        "tool_result"},
            {"tool_use_id", pr.tool_use_id},
            {"content",     *pr.result}
        };
        if (pr.is_error) entry["is_error"] = true;
        tool_results.push_back(std::move(entry));
    }
    messages_.push_back({{"role", "user"}, {"content", tool_results}});
    pending_results_.clear();
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

// C3: tool_schemas — includes builtins when include_builtins=true (default)
py::list Engine::tool_schemas(bool include_builtins) const {
    py::list result;
    if (include_builtins) {
        for (const auto& bt : builtin_tools_)
            result.append(json_to_py(bt->schema()));
    }
    // contrib tools
    for (const auto& ct : contrib_tools_)
        result.append(json_to_py(ct->schema()));
    for (const auto& td : custom_tools_) {
        json schema = {
            {"name",         td.name},
            {"description",  td.description},
            {"input_schema", td.schema}
        };
        result.append(json_to_py(schema));
    }
    for (const auto& [name, sa] : subagents_) {
        json schema = {
            {"name",        name},
            {"description", sa->description_text()},
            {"input_schema", {
                {"type",       "object"},
                {"properties", {
                    {"task", {{"type","string"},{"description","Task to delegate to the agent"}}}
                }},
                {"required",   {"task"}}
            }}
        };
        result.append(json_to_py(schema));
    }
    // MCP tools
    // MCP tools
    for (const auto& entry : mcp_entries_) {
        json schema = {
            {"name",         entry.tool_name},
            {"description",  entry.description},
            {"input_schema", entry.input_schema}
        };
        result.append(json_to_py(schema));
    }
    return result;
}

py::list Engine::get_messages() const {
    py::list result;
    for (const auto& msg : messages_) {
        result.append(json_to_py(msg));
    }
    return result;
}

// C1: set_messages — messages can be replaced directly (e.g. after compact)
void Engine::set_messages(py::list messages) {
    messages_.clear();
    for (const auto& msg : messages) {
        messages_.push_back(py_to_json(msg));
    }
}

py::object Engine::get_last_answer() const {
    if (!last_answer_.has_value()) return py::none();
    return py::str(*last_answer_);
}

std::vector<std::string> Engine::get_tool_names() const {
    std::vector<std::string> names;
    for (const auto& bt : builtin_tools_) names.push_back(bt->name());
    for (const auto& ct : contrib_tools_)  names.push_back(ct->name());
    for (const auto& ct : custom_tools_)  names.push_back(ct.name);
    for (const auto& [n, sa] : subagents_) names.push_back(n);
    for (const auto& entry : mcp_entries_) names.push_back(entry.tool_name);
    return names;
}

void Engine::set_tool_description(const std::string& tool_name, const std::string& desc) {
    for (const auto& bt : builtin_tools_) {
        if (bt->name() == tool_name) {
            bt->set_description(desc);
            return;
        }
    }
    throw py::value_error("Unknown builtin tool: " + tool_name);
}

std::string Engine::get_tool_description(const std::string& tool_name) const {
    for (const auto& bt : builtin_tools_) {
        if (bt->name() == tool_name)
            return bt->effective_description();
    }
    throw py::value_error("Unknown builtin tool: " + tool_name);
}

std::string Engine::repr() const {
    std::ostringstream oss;
    oss << "Engine(system_prompt=" << json(system_prompt_).dump()
        << ", messages=" << messages_.size()
        << ", done=" << (done_ ? "True" : "False")
        << ')';
    return oss.str();
}

// ---------------------------------------------------------------------------
// Internal tool execution
// ---------------------------------------------------------------------------

ExecResult Engine::execute_tool(const std::string& name, const json& input) {
    // 1. Try builtins first (if not_found, continue down the chain)
    ExecResult r = execute_builtin(name, input);
    if (!r.not_found) return r;

    // 2. contrib tools (Python/C++ objects that inherit ToolBase)
    for (const auto& ct : contrib_tools_) {
        if (ct->name() == name) {
            try {
                return ExecResult::ok(ct->call(input));
            } catch (const std::exception& e) {
                return ExecResult::err("Error (contrib): " + std::string(e.what()));
            } catch (...) {
                return ExecResult::err("Error (contrib): unknown exception");
            }
        }
    }

    // 3. Check MCP tools
    for (const auto& entry : mcp_entries_) {
        if (entry.tool_name == name) {
            try {
                json mcp_result = entry.client->call_tool(name, input);
                // Extract text from MCP result.content[]
                if (mcp_result.contains("content") && mcp_result["content"].is_array()) {
                    std::string text;
                    for (const auto& item : mcp_result["content"]) {
                        if (item.value("type","") == "text") {
                            text += item.value("text","");
                        }
                    }
                    return ExecResult::ok(text.empty() ? mcp_result.dump() : text);
                }
                return ExecResult::ok(mcp_result.dump());
            } catch (const std::exception& e) {
                return ExecResult::err("Error (MCP): " + std::string(e.what()));
            }
        }
    }

    // 3. custom
    return execute_custom(name, input);
}

ExecResult Engine::execute_builtin(const std::string& name, const json& input) {
    for (const auto& bt : builtin_tools_) {
        if (bt->name() == name) {
            try {
                return ExecResult::ok(bt->call(input));
            } catch (const std::exception& e) {
                return ExecResult::err("Error: " + std::string(e.what()));
            } catch (...) {
                return ExecResult::err("Error: unknown exception in builtin tool");
            }
        }
    }
    return ExecResult::unknown();
}

ExecResult Engine::execute_custom(const std::string& name, const json& input) {
    for (const auto& td : custom_tools_) {
        if (td.name == name) {
            try {
                py::dict kwargs;
                for (const auto& [k, v] : input.items()) {
                    kwargs[py::str(k)] = json_to_py(v);
                }
                py::object ret = td.callable(**kwargs);
                if (py::isinstance<py::str>(ret)) return ExecResult::ok(ret.cast<std::string>());
                return ExecResult::ok(py::str(ret).cast<std::string>());
            } catch (py::error_already_set& e) {
                return ExecResult::err("Error (Python): " + std::string(e.what()));
            } catch (const std::exception& e) {
                return ExecResult::err("Error: " + std::string(e.what()));
            }
        }
    }
    return ExecResult::err("Error: Unknown tool: " + name);
}

} // namespace openrein
