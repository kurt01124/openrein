#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "engine.hpp"
#include "subagent.hpp"
#include "compact.hpp"
#include "tools/tool_base.hpp"

// ---------------------------------------------------------------------------
// PyToolBase — Trampoline allowing Python to override ToolBase virtual functions
// json type cannot be auto-converted by pybind11 → handled manually via py_to_json/json_to_py
// ---------------------------------------------------------------------------

class PyToolBase : public openrein::ToolBase {
public:
    using openrein::ToolBase::ToolBase;

    std::string name() const override {
        PYBIND11_OVERRIDE_PURE(std::string, openrein::ToolBase, name);
    }
    std::string description() const override {
        PYBIND11_OVERRIDE_PURE(std::string, openrein::ToolBase, description);
    }
    json input_schema() const override {
        py::gil_scoped_acquire gil;
        py::function override_fn = pybind11::get_override(this, "input_schema");
        if (override_fn) {
            py::object result = override_fn();
            return openrein::py_to_json(result);
        }
        return json::object();
    }
    std::string call(const json& input) const override {
        py::gil_scoped_acquire gil;
        py::function override_fn = pybind11::get_override(this, "call");
        if (override_fn) {
            py::object py_input = openrein::json_to_py(input);
            py::object result   = override_fn(py_input);
            try { return result.cast<std::string>(); }
            catch (...) { return py::str(result).cast<std::string>(); }
        }
        return "";
    }
};
#include "tools/read.hpp"
#include "tools/write.hpp"
#include "tools/edit.hpp"
#include "tools/bash.hpp"
#include "tools/grep.hpp"
#include "tools/glob.hpp"
#include "tools/web_fetch.hpp"
#include "tools/web_search.hpp"

namespace py = pybind11;

// ---------------------------------------------------------------------------
// py <-> json conversion (declared extern in engine.cpp / subagent.cpp / compact.cpp)
// ---------------------------------------------------------------------------

namespace openrein {

json py_to_json(py::handle obj) {
    if (obj.is_none())                      return nullptr;
    if (py::isinstance<py::bool_>(obj))     return obj.cast<bool>();
    if (py::isinstance<py::int_>(obj))      return obj.cast<int64_t>();
    if (py::isinstance<py::float_>(obj))    return obj.cast<double>();
    if (py::isinstance<py::str>(obj))       return obj.cast<std::string>();

    if (py::isinstance<py::bytes>(obj))     return obj.cast<std::string>();

    if (py::isinstance<py::dict>(obj)) {
        json result = json::object();
        for (const auto& item : py::cast<py::dict>(obj)) {
            result[item.first.cast<std::string>()] = py_to_json(item.second);
        }
        return result;
    }

    if (py::isinstance<py::list>(obj) || py::isinstance<py::tuple>(obj)) {
        json result = json::array();
        for (const auto& item : obj) {
            result.push_back(py_to_json(item));
        }
        return result;
    }

    // Last resort: str() conversion
    try { return py::str(obj).cast<std::string>(); } catch (...) {}
    return nullptr;
}

py::object json_to_py(const json& val) {
    if (val.is_null())              return py::none();
    if (val.is_boolean())           return py::bool_(val.get<bool>());
    if (val.is_number_integer())    return py::int_(val.get<int64_t>());
    if (val.is_number_float())      return py::float_(val.get<double>());
    if (val.is_string()) {
        const std::string& s = val.get<std::string>();
        // UTF-8 safe conversion — invalid bytes replaced with U+FFFD
        PyObject* u = PyUnicode_DecodeUTF8(s.data(), (Py_ssize_t)s.size(), "replace");
        if (!u) { PyErr_Clear(); return py::str(""); }
        return py::reinterpret_steal<py::str>(u);
    }

    if (val.is_array()) {
        py::list lst;
        for (const auto& item : val) lst.append(json_to_py(item));
        return lst;
    }

    if (val.is_object()) {
        py::dict d;
        for (const auto& [k, v] : val.items()) {
            d[py::str(k)] = json_to_py(v);
        }
        return d;
    }

    return py::none();
}

// ---------------------------------------------------------------------------
// Built-in tool schema list (declared in tool_base.hpp)
// ---------------------------------------------------------------------------

std::vector<json> builtin_tool_schemas() {
    ReadTool  read;
    WriteTool write;
    EditTool  edit;
    BashTool  bash;
    GrepTool  grep;
    GlobTool       glob;
    WebFetchTool   web_fetch;
    WebSearchTool  web_search;
    return {
        read.schema(),
        write.schema(),
        edit.schema(),
        bash.schema(),
        grep.schema(),
        glob.schema(),
        web_fetch.schema(),
        web_search.schema(),
    };
}

} // namespace openrein

// ---------------------------------------------------------------------------
// Convenience function: Python-level default_tools()
// ---------------------------------------------------------------------------

static py::list default_tools_schemas() {
    py::list result;
    for (const auto& s : openrein::builtin_tool_schemas()) {
        result.append(openrein::json_to_py(s));
    }
    return result;
}

// ---------------------------------------------------------------------------
// PYBIND11_MODULE
// ---------------------------------------------------------------------------

PYBIND11_MODULE(_openrein, m) {
    m.doc() = "openrein — LLM agent app harness engine (C++ core)";
    m.attr("__version__") = "0.1.0";

    // -----------------------------------------------------------------------
    // ToolBase — Public interface for contrib tool developers to subclass in Python
    // -----------------------------------------------------------------------
    py::class_<openrein::ToolBase, PyToolBase, std::shared_ptr<openrein::ToolBase>>(
            m, "ToolBase", R"(
Base class for openrein contrib tools.

Subclass in Python to create custom tools.

Example:
    class MyTool(openrein.ToolBase):
        def name(self): return "MyTool"
        def description(self): return "..."
        def input_schema(self): return {"type": "object", "properties": {}}
        def call(self, input_json): return "result"

    engine.register_tool(MyTool())
)")
        .def(py::init<>())
        .def("name",                  &openrein::ToolBase::name,
             "Return tool name (must be implemented)")
        .def("description",           &openrein::ToolBase::description,
             "Return default description (must be implemented)")
        .def("input_schema",          &openrein::ToolBase::input_schema,
             "Return input JSON schema (must be implemented)")
        .def("call",                  &openrein::ToolBase::call,
             py::arg("input"),
             "Execute tool — input is dict, return value is str (must be implemented)")
        .def("set_description",       &openrein::ToolBase::set_description,
             py::arg("desc"),
             "Replace description at runtime")
        .def("effective_description", &openrein::ToolBase::effective_description,
             "Return current effective description (override if set, otherwise default)")
        .def("schema",                &openrein::ToolBase::schema,
             "Return full schema dict including name/description/input_schema");

    // -----------------------------------------------------------------------
    // Engine
    // -----------------------------------------------------------------------
    py::class_<openrein::Engine>(m, "Engine", R"(
LLM agent engine — message state + tool_use execution loop.

The developer invokes the model directly; this engine handles only
state management and tool execution.

Args:
    system_prompt: System prompt passed to the model (default: "")
    tools: Whether to enable built-in tools (Read/Write/Edit/Bash/Grep/Glob) (default: True)
)")
        // C2: tools parameter accepts both bool|list
        // G3: add max_turns parameter
        .def(py::init<std::string, py::object, int>(),
             py::arg("system_prompt") = "",
             py::arg("tools")         = py::bool_(true),
             py::arg("max_turns")     = -1,
             "tools: True(default, enables builtins) | False | list[dict](compat, enables builtins)\n"
             "max_turns: -1(default, unlimited) | positive int -> ValueError when step count with tool_use exceeds limit")
        .def("add",        &openrein::Engine::add,
             py::arg("role"), py::arg("content"),
             "Add a message to the conversation. role: 'user' | 'assistant'")
        .def("step",       &openrein::Engine::step,
             py::arg("content"),
             "Process model response. True=done, False=continue. Raises ValueError if called after done")
        .def("reset",      &openrein::Engine::reset,
             "Reset conversation history + SubAgent state")
        .def("tool",       &openrein::Engine::register_tool_decorator,
             py::arg("name"), py::arg("description"),
             py::kw_only(), py::arg("schema") = py::none(),
             "Return a decorator that registers a Python function as a tool")
        .def("register_tool",
             py::overload_cast<py::object, const std::string&, const std::string&, py::object>(
                 &openrein::Engine::register_tool),
             py::arg("func"), py::kw_only(),
             py::arg("name"), py::arg("description"),
             py::arg("schema") = py::none(),
             "Register a tool directly without a decorator (Python function)")
        .def("register_tool",
             py::overload_cast<std::shared_ptr<openrein::ToolBase>>(
                 &openrein::Engine::register_tool),
             py::arg("tool"),
             "Register a Python/C++ tool object that inherits from ToolBase (contrib interface)")
        // C3: tool_schemas — includes builtins when include_builtins=True (default)
        .def("tool_schemas",  &openrein::Engine::tool_schemas,
             py::arg("include_builtins") = true,
             "All tool schemas. If include_builtins=False, returns only custom+subagent tools")
        .def("add_subagent",  &openrein::Engine::add_subagent,
             py::arg("name"), py::arg("subagent"),
             "Register a subagent")
        .def("active_subagents", &openrein::Engine::active_subagents,
             "List of subagents currently in Running state")
        // Phase5: MCP server connection
        .def("add_mcp_server", &openrein::Engine::add_mcp_server,
             py::arg("name"), py::arg("command_or_url"),
             "Connect to an MCP server.\n"
             "command_or_url: list[str] -> stdio (spawn process),\n"
             "                str -> HTTP (JSON-RPC POST to URL)")
        // M1: maybe_compact
        .def("maybe_compact",  &openrein::Engine::maybe_compact,
             py::arg("model_fn"), py::arg("threshold") = 80000,
             "If estimated tokens exceed threshold, compact messages using model_fn.\n"
             "Returns True=compacted, False=skipped. model_fn: (messages) -> content")
        .def_property("system_prompt",
             &openrein::Engine::get_system_prompt,
             &openrein::Engine::set_system_prompt)
        // C1: messages readable/writable (for applying compact etc.)
        .def_property("messages",
             &openrein::Engine::get_messages,
             &openrein::Engine::set_messages,
             "Conversation message list. Can be replaced directly (e.g. after compact)")
        .def_property_readonly("done",        &openrein::Engine::is_done)
        .def_property_readonly("last_answer", &openrein::Engine::get_last_answer)
        .def_property_readonly("tool_names",  &openrein::Engine::get_tool_names)
        // G3: max_turns / turn_count accessors
        .def("set_tool_description", &openrein::Engine::set_tool_description,
             py::arg("tool_name"), py::arg("description"),
             "Replace the description of a builtin tool at runtime.\n"
             "tool_name: 'Read','Write','Edit','Bash','Grep','Glob','WebFetch','WebSearch'")
        .def("get_tool_description", &openrein::Engine::get_tool_description,
             py::arg("tool_name"),
             "Return the current description of a builtin tool (override or default)")
        .def_property_readonly("max_turns",   &openrein::Engine::get_max_turns,
             "Configured max_turns value (-1=unlimited)")
        .def_property_readonly("turn_count",  &openrein::Engine::get_turn_count,
             "Number of step() calls so far that included tool_use")
        .def("__repr__", &openrein::Engine::repr);

    // -----------------------------------------------------------------------
    // SubAgent
    // -----------------------------------------------------------------------
    py::class_<openrein::SubAgent, std::shared_ptr<openrein::SubAgent>>(
            m, "SubAgent", R"(
Developer-defined subagent.

description: serves as parent LLM tool schema + child system_prompt
model: (messages, tools) -> response_content callable
)")
        .def(py::init<std::string, py::object>(),
             py::arg("description"), py::arg("model"))
        .def("step",       &openrein::SubAgent::step,
             py::arg("content"),
             "Process model response. True=done (result auto-forwarded to parent), False=continue")
        .def("reset",      &openrein::SubAgent::reset,
             "Reset messages and state to Idle")
        .def("tool",       &openrein::SubAgent::register_tool_decorator,
             py::arg("name"), py::arg("description"),
             py::kw_only(), py::arg("schema") = py::none())
        .def("register_tool", &openrein::SubAgent::register_tool,
             py::arg("func"), py::kw_only(),
             py::arg("name"), py::arg("description"),
             py::arg("schema") = py::none())
        .def("tool_schemas",  &openrein::SubAgent::tool_schemas)
        .def("is_running",    &openrein::SubAgent::is_running)
        .def_property_readonly("messages",              &openrein::SubAgent::get_messages)
        .def_property_readonly("messages_with_system", &openrein::SubAgent::get_messages_with_system,
             "Message list with description prepended as the system role.\n"
             "Usage: my_model(sub.messages_with_system, sub.tool_schemas())")
        .def_property_readonly("description",  &openrein::SubAgent::get_description)
        .def_property_readonly("system_prompt",&openrein::SubAgent::get_description)
        .def_property_readonly("model",        &openrein::SubAgent::get_model)
        .def_property_readonly("last_answer",  &openrein::SubAgent::get_last_answer)
        .def("__repr__", &openrein::SubAgent::repr);

    // -----------------------------------------------------------------------
    // Compact
    // -----------------------------------------------------------------------
    py::class_<openrein::Compact>(m, "Compact", R"(
Context compact utility.

The developer invokes the LLM directly.
This class provides token estimation, compact prompt generation, and response parsing.
)")
        .def(py::init<>())
        .def("estimate_tokens",  &openrein::Compact::estimate_tokens,
             py::arg("messages"),
             "Estimate approximate token count of messages (chars / 4 approximation)")
        .def("should_compact",   &openrein::Compact::should_compact,
             py::arg("messages"),
             py::arg("threshold") = 80000,
             "Whether the compact threshold has been exceeded")
        .def("make_prompt",      &openrein::Compact::make_prompt,
             py::arg("messages"),
             "Return the compact request prompt string")
        .def("parse_response",   &openrein::Compact::parse_response,
             py::arg("response_text"),
             "Extract the <result> tag content from the compact model response")
        .def("apply",            &openrein::Compact::apply,
             py::arg("messages"), py::arg("summary"),
             "Return a new message list with messages replaced by the summary");

    // -----------------------------------------------------------------------
    // Convenience functions
    // -----------------------------------------------------------------------
    m.def("default_tools", &default_tools_schemas,
          "Return list[dict] of schemas for the 6 built-in tools (Read/Write/Edit/Bash/Grep/Glob)");
}
