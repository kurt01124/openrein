#include "subagent.hpp"
#include "engine.hpp"
#include "tools/tool_base.hpp"
#include <pybind11/stl.h>
#include <sstream>

// py_to_json / json_to_py are defined in bindings.cpp
namespace openrein {
json             py_to_json(pybind11::handle obj);
pybind11::object json_to_py(const json& val);
}

namespace openrein {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

SubAgent::SubAgent(std::string description, py::object model)
    : description_(std::move(description))
    , model_(std::move(model))
{}

// ---------------------------------------------------------------------------
// Execution
// ---------------------------------------------------------------------------

bool SubAgent::step(py::object content) {
    // C4: normalize content
    json content_json = normalize_content(py_to_json(content));

    // Add assistant message
    messages_.push_back({{"role", "assistant"}, {"content", content_json}});

    // Collect tool_use blocks
    std::vector<json> tool_uses;
    if (content_json.is_array()) {
        for (const auto& block : content_json) {
            if (block.is_object() && block.value("type", "") == "tool_use") {
                tool_uses.push_back(block);
            }
        }
    }

    // No tool_use -> done
    // (always an array after normalize_content -> no need for string branch)
    if (tool_uses.empty()) {
        std::string answer;
        for (auto it = content_json.rbegin(); it != content_json.rend(); ++it) {
            if ((*it).value("type", "") == "text") {
                answer = (*it)["text"].get<std::string>();
                break;
            }
        }
        last_answer_ = answer;
        status_      = SubAgentStatus::Idle;

        // Report result to parent engine
        if (parent_ && tool_use_id_.has_value()) {
            parent_->on_subagent_done(*tool_use_id_, answer);
        }
        return true;
    }

    // Execute tools (G1: handle is_error via ExecResult)
    json tool_results = json::array();
    for (const auto& tu : tool_uses) {
        const std::string tid   = tu["id"].get<std::string>();
        const std::string tname = tu["name"].get<std::string>();
        const json tinput       = tu.value("input", json::object());

        ExecResult er = execute_tool(tname, tinput);
        json entry = {
            {"type",        "tool_result"},
            {"tool_use_id", tid},
            {"content",     er.content}
        };
        if (er.is_error) entry["is_error"] = true;
        tool_results.push_back(std::move(entry));
    }

    messages_.push_back({{"role", "user"}, {"content", tool_results}});
    return false;
}

// ---------------------------------------------------------------------------
// Tool registration
// ---------------------------------------------------------------------------

py::object SubAgent::register_tool_decorator(
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
        py::doc("openrein subagent tool decorator")
    );
}

void SubAgent::register_tool(
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

    for (auto it = tool_defs_.begin(); it != tool_defs_.end(); ++it) {
        if (it->name == name) { tool_defs_.erase(it); break; }
    }
    tool_defs_.push_back({name, description, std::move(schema_json), func});
}

// ---------------------------------------------------------------------------
// State accessors
// ---------------------------------------------------------------------------

py::list SubAgent::tool_schemas() const {
    py::list result;
    for (const auto& td : tool_defs_) {
        json schema = {
            {"name",         td.name},
            {"description",  td.description},
            {"input_schema", td.schema}
        };
        result.append(json_to_py(schema));
    }
    return result;
}

py::list SubAgent::get_messages() const {
    py::list result;
    for (const auto& msg : messages_) {
        result.append(json_to_py(msg));
    }
    return result;
}

py::list SubAgent::get_messages_with_system() const {
    py::list result;
    result.append(json_to_py({{"role", "system"}, {"content", description_}}));
    for (const auto& msg : messages_) result.append(json_to_py(msg));
    return result;
}

py::object SubAgent::get_last_answer() const {
    if (!last_answer_.has_value()) return py::none();
    return py::str(*last_answer_);
}

std::string SubAgent::repr() const {
    std::ostringstream oss;
    oss << "SubAgent(description="
        << json(description_).dump()
        << ", running=" << (is_running() ? "True" : "False")
        << ')';
    return oss.str();
}

// ---------------------------------------------------------------------------
// Internal (called by Engine)
// ---------------------------------------------------------------------------

void SubAgent::activate(std::string tool_use_id, std::string task) {
    tool_use_id_ = std::move(tool_use_id);
    messages_    = {{{"role", "user"}, {"content", task}}};
    status_      = SubAgentStatus::Running;
    last_answer_ = std::nullopt;
}

void SubAgent::set_parent(Engine* parent, std::string registered_name) {
    parent_          = parent;
    registered_name_ = std::move(registered_name);
}

void SubAgent::reset() {
    messages_.clear();
    status_      = SubAgentStatus::Idle;
    tool_use_id_ = std::nullopt;
    last_answer_ = std::nullopt;
}

// ---------------------------------------------------------------------------
// Internal tool execution
// ---------------------------------------------------------------------------

ExecResult SubAgent::execute_tool(const std::string& name, const json& input) {
    for (const auto& td : tool_defs_) {
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
