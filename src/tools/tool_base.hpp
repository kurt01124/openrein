#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <pybind11/pybind11.h>
#include "../json_fwd.hpp"

namespace openrein {

/// Common interface for all built-in tools.
struct ToolBase {
    virtual ~ToolBase() = default;
    virtual std::string name() const = 0;
    virtual std::string description() const = 0;   // default description (hardcoded in C++)
    virtual json input_schema() const = 0;
    virtual std::string call(const json& input) const = 0;

    /// Replace description at runtime. If empty, the default is used.
    void set_description(const std::string& desc) { description_override_ = desc; }

    /// Current effective description (override if set, otherwise default).
    std::string effective_description() const {
        return description_override_.empty() ? description() : description_override_;
    }

    json schema() const {
        return {
            {"name",         name()},
            {"description",  effective_description()},
            {"input_schema", input_schema()},
        };
    }

private:
    std::string description_override_;
};

/// Convert pybind11 type to json (implemented in bindings.cpp)
json py_to_json(pybind11::handle obj);

/// Convert json to pybind11 type (implemented in bindings.cpp)
pybind11::object json_to_py(const json& val);

/// Return built-in tool schema list.
std::vector<json> builtin_tool_schemas();

// ---------------------------------------------------------------------------
// Shared inline utilities
// ---------------------------------------------------------------------------

/// Normalize model response content to always be an array (ContentBlock[]).
///   - array   -> as-is
///   - string  -> [{"type":"text","text":"..."}]
///   - object  -> [object]
///   - other   -> [{"type":"text","text":""}]
inline json normalize_content(const json& raw) {
    if (raw.is_array())  return raw;
    if (raw.is_string()) return json::array(
        {{ {"type","text"}, {"text", raw.get<std::string>()} }}
    );
    if (raw.is_object()) return json::array({ raw });
    return json::array({{ {"type","text"}, {"text",""} }});
}

/// Normalize backslashes in a file path to forward slashes (Windows compatibility).
/// Prevents escape errors when an LLM uses paths in JSON.
inline std::string normalize_path(const std::filesystem::path& p) {
    std::string s = p.string();
    for (auto& c : s) if (c == '\\') c = '/';
    return s;
}

} // namespace openrein
