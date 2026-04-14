#include "read.hpp"
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>

namespace openrein {

std::string ReadTool::description() const {
    return
        "Reads a file from the local filesystem and returns it with line numbers.\n"
        "Large files are automatically truncated (default 2000 lines). Partial reading via offset/limit is supported.";
}

json ReadTool::input_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"file_path", {
                {"type", "string"},
                {"description", "Absolute or relative path to the file to read"}
            }},
            {"offset", {
                {"type", "integer"},
                {"description", "Line number to start reading from (0-indexed, default 0)"},
                {"default", 0}
            }},
            {"limit", {
                {"type", "integer"},
                {"description", "Maximum number of lines to read (default 2000)"},
                {"default", 2000}
            }}
        }},
        {"required", {"file_path"}}
    };
}

std::string ReadTool::call(const json& input) const {
    const std::string path = input["file_path"].get<std::string>();
    const int offset = input.value("offset", 0);
    const int limit  = input.value("limit",  2000);

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return "Error: Cannot open file: " + path;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        // Strip Windows CRLF
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(std::move(line));
    }

    const int total = static_cast<int>(lines.size());
    const int start = std::min(std::max(offset, 0), total);
    const int end   = std::min(start + std::max(limit, 1), total);

    std::ostringstream oss;
    for (int i = start; i < end; ++i) {
        oss << (i + 1) << '\t' << lines[i] << '\n';
    }

    if (end < total) {
        oss << "\n[...truncated: showing lines " << (start + 1)
            << '-' << end << " of " << total
            << ". Use offset=" << end << " to continue reading...]";
    }

    const std::string result = oss.str();
    return result.empty() ? "(empty file)" : result;
}

} // namespace openrein
