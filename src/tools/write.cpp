#include "write.hpp"
#include <fstream>
#include <filesystem>
#include <string>

namespace openrein {

std::string WriteTool::description() const {
    return
        "Creates or overwrites a file. Intermediate directories are created automatically.\n"
        "If the file already exists it is completely overwritten.";
}

json WriteTool::input_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"file_path", {
                {"type", "string"},
                {"description", "Path to the file to write"}
            }},
            {"content", {
                {"type", "string"},
                {"description", "Content to write to the file"}
            }}
        }},
        {"required", {"file_path", "content"}}
    };
}

std::string WriteTool::call(const json& input) const {
    const std::string path    = input["file_path"].get<std::string>();
    const std::string content = input["content"].get<std::string>();

    namespace fs = std::filesystem;
    fs::path p(path);
    if (p.has_parent_path() && !p.parent_path().empty()) {
        std::error_code ec;
        fs::create_directories(p.parent_path(), ec);
        if (ec) return "Error: Cannot create directory: " + ec.message();
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) return "Error: Cannot open for writing: " + path;

    file << content;
    if (!file) return "Error: Write failed for: " + path;

    return "OK: Written " + std::to_string(content.size()) + " bytes to " + path;
}

} // namespace openrein
