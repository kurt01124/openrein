#include "edit.hpp"
#include <fstream>
#include <sstream>
#include <string>

namespace openrein {

std::string EditTool::description() const {
    return
        "Replaces an exact string in a file with new content.\n"
        "\n"
        "Usage:\n"
        "- Read the file with the Read tool before editing — this tool errors if the file has not been read\n"
        "- Copy old_string exactly from the Read output. The line-number prefix (number + tab) is not part of the file — do not include it in old_string or new_string\n"
        "- Preserve the original indentation (tabs/spaces) precisely; any mismatch will cause the edit to fail\n"
        "- Prefer this tool over Write when changing part of an existing file\n"
        "- old_string must be unique in the file; include more surrounding context if needed, or set replace_all=true to update every occurrence\n"
        "- Use replace_all=true when renaming a symbol or repeated value throughout the file";
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
