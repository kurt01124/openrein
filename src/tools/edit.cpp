#include "edit.hpp"
#include <fstream>
#include <sstream>
#include <string>

namespace openrein {

std::string EditTool::description() const {
    return
        "Performs exact string replacements in files.\n"
        "\n"
        "Usage:\n"
        "- You must use your `Read` tool at least once in the conversation before editing. This tool will error if you attempt an edit without reading the file.\n"
        "- When editing text from Read tool output, ensure you preserve the exact indentation (tabs/spaces) as it appears AFTER the line number prefix. The line number prefix format is: line number + tab. Everything after that is the actual file content to match. Never include any part of the line number prefix in the old_string or new_string.\n"
        "- ALWAYS prefer editing existing files in the codebase. NEVER write new files unless explicitly required.\n"
        "- Only use emojis if the user explicitly requests it. Avoid adding emojis to files unless asked.\n"
        "- The edit will FAIL if `old_string` is not unique in the file. Either provide a larger string with more surrounding context to make it unique or use `replace_all` to change every instance of `old_string`.\n"
        "- Use `replace_all` for replacing and renaming strings across the file. This parameter is useful if you want to rename a variable for instance.";
}

json EditTool::input_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"file_path", {
                {"type", "string"},
                {"description", "Path to the file to modify"}
            }},
            {"old_string", {
                {"type", "string"},
                {"description", "Original string to replace (must match exactly)"}
            }},
            {"new_string", {
                {"type", "string"},
                {"description", "Replacement string"}
            }},
            {"replace_all", {
                {"type", "boolean"},
                {"description", "Whether to replace all occurrences (default false)"},
                {"default", false}
            }}
        }},
        {"required", {"file_path", "old_string", "new_string"}}
    };
}

std::string EditTool::call(const json& input) const {
    const std::string path        = input["file_path"].get<std::string>();
    const std::string old_str     = input["old_string"].get<std::string>();
    const std::string new_str     = input["new_string"].get<std::string>();
    const bool        replace_all = input.value("replace_all", false);

    // Read entire file
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return "Error: Cannot open file: " + path;
    std::ostringstream oss;
    oss << ifs.rdbuf();
    std::string content = oss.str();
    ifs.close();

    if (old_str.empty()) return "Error: old_string must not be empty";

    const size_t first = content.find(old_str);
    if (first == std::string::npos) {
        return "Error: old_string not found in " + path;
    }

    size_t count = 0;
    if (!replace_all) {
        // Check uniqueness
        const size_t second = content.find(old_str, first + 1);
        if (second != std::string::npos) {
            return
                "Error: old_string appears multiple times. "
                "Provide more surrounding context to make it unique, "
                "or set replace_all=true.";
        }
        content.replace(first, old_str.size(), new_str);
        count = 1;
    } else {
        std::string result;
        result.reserve(content.size());
        size_t pos = 0;
        size_t found;
        while ((found = content.find(old_str, pos)) != std::string::npos) {
            result.append(content, pos, found - pos);
            result.append(new_str);
            pos = found + old_str.size();
            ++count;
        }
        result.append(content, pos, std::string::npos);
        content = std::move(result);
    }

    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs) return "Error: Cannot write file: " + path;
    ofs << content;
    if (!ofs) return "Error: Write failed for: " + path;

    return "OK: Replaced " + std::to_string(count) + " occurrence(s) in " + path;
}

} // namespace openrein
