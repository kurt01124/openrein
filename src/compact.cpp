#include "compact.hpp"
#include <sstream>
#include <string>
#include <regex>

// py_to_json / json_to_py are defined in bindings.cpp
namespace openrein {
json        py_to_json(pybind11::handle obj);
pybind11::object json_to_py(const json& val);
}

namespace openrein {

int Compact::estimate_tokens(py::list messages) const {
    // Aggregate text content only to avoid UTF-8 multibyte overcount
    size_t text_chars = 0;
    for (const auto& msg : messages) {
        json j = py_to_json(msg);
        const auto& content = j["content"];
        if (content.is_string()) {
            text_chars += content.get<std::string>().size();
        } else if (content.is_array()) {
            for (const auto& block : content) {
                const std::string btype = block.value("type", "");
                if (btype == "text") {
                    text_chars += block.value("text", "").size();
                } else if (btype == "tool_result") {
                    const auto& c = block["content"];
                    if (c.is_string()) text_chars += c.get<std::string>().size();
                }
            }
        }
    }
    return static_cast<int>(text_chars / 3);  // more conservative estimate than /4
}

bool Compact::should_compact(py::list messages, int threshold) const {
    return estimate_tokens(messages) >= threshold;
}

std::string Compact::make_prompt(py::list messages) const {
    // Serialize messages to text and build compact request prompt
    std::ostringstream oss;
    oss <<
        "The following is a conversation history of an AI assistant. "
        "Please summarize the conversation concisely while retaining "
        "key information, completed tasks, and important decisions.\n\n"
        "<conversation>\n";

    for (const auto& msg : messages) {
        json j = py_to_json(msg);
        const std::string role = j.value("role", "unknown");
        oss << "[" << role << "]\n";

        const auto& content = j["content"];
        if (content.is_string()) {
            oss << content.get<std::string>() << "\n\n";
        } else if (content.is_array()) {
            for (const auto& block : content) {
                const std::string type = block.value("type", "");
                if (type == "text") {
                    oss << block["text"].get<std::string>() << "\n";
                } else if (type == "tool_use") {
                    oss << "[Tool call: " << block.value("name", "?") << "]\n";
                } else if (type == "tool_result") {
                    const auto& res_content = block["content"];
                    if (res_content.is_string()) {
                        oss << "[Tool result: " << res_content.get<std::string>().substr(0, 200) << "...]\n";
                    }
                }
            }
            oss << "\n";
        }
    }

    oss <<
        "</conversation>\n\n"
        "Please summarize the above conversation and write it inside the <result> tag. "
        "The summary will be used as future conversation context.";

    return oss.str();
}

std::string Compact::parse_response(const std::string& response_text) const {
    // Extract content inside <result>...</result> tags
    static const std::regex re(R"(<result>([\s\S]*?)</result>)",
                                std::regex::ECMAScript);
    std::smatch m;
    if (std::regex_search(response_text, m, re)) {
        std::string s = m[1].str();
        // Strip leading/trailing whitespace
        const size_t start = s.find_first_not_of(" \t\n\r");
        const size_t end   = s.find_last_not_of(" \t\n\r");
        if (start != std::string::npos) return s.substr(start, end - start + 1);
    }
    // If no <result> tag found, return the entire response
    return response_text;
}

py::list Compact::apply(py::list /*messages*/, const std::string& summary) const {
    // Replace with [{"role": "user", "content": summary}]
    json new_msg = {
        {"role", "user"},
        {"content", summary}
    };
    py::list result;
    result.append(json_to_py(new_msg));
    return result;
}

} // namespace openrein
