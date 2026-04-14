#pragma once
#include <string>
#include <vector>
#include <optional>
#include <pybind11/pybind11.h>
#include "json_fwd.hpp"
#include "exec_result.hpp"

namespace py = pybind11;

namespace openrein {

class Engine;

struct SubAgentToolDef {
    std::string name;
    std::string description;
    json        schema;
    py::object  callable;
};

enum class SubAgentStatus { Idle, Running };

/// Developer-defined subagent.
///
/// description: serves as parent LLM tool schema + child system_prompt.
/// model: (messages, tools) -> response_content callable.
class SubAgent {
public:
    SubAgent(std::string description, py::object model);

    // --- Execution ---
    bool step(py::object content);

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

    // --- State accessors ---
    py::list   tool_schemas() const;
    py::list   get_messages() const;
    py::list   get_messages_with_system() const;
    bool       is_running() const { return status_ == SubAgentStatus::Running; }
    std::string get_description() const { return description_; }
    py::object  get_model() const { return model_; }
    py::object  get_last_answer() const;
    std::string repr() const;

    // --- Internal (called by Engine) ---
    void activate(std::string tool_use_id, std::string task);
    void set_parent(Engine* parent, std::string registered_name);
    void reset();  // Reset to Idle state (called from engine.reset())

    const std::string& description_text() const { return description_; }
    const std::string& registered_name() const { return registered_name_; }

private:
    // Definition
    std::string description_;
    py::object  model_;
    std::vector<SubAgentToolDef> tool_defs_;

    // Runtime state
    SubAgentStatus status_ = SubAgentStatus::Idle;
    std::vector<json> messages_;
    std::optional<std::string> tool_use_id_;
    std::optional<std::string> last_answer_;

    // Parent connection
    Engine*     parent_ = nullptr;
    std::string registered_name_;

    ExecResult execute_tool(const std::string& name, const json& input);
};

} // namespace openrein
