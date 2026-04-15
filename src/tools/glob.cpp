#include "glob.hpp"
#include <filesystem>
#include <regex>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>

namespace openrein {

namespace {

/// Converts a glob pattern to a regex string.
/// '**' → ".*", '*' → "[^/\\]*", '?' → "[^/\\]", '{a,b}' → "(a|b)"
std::string glob_to_regex_str(const std::string& glob) {
    std::string re;
    re.reserve(glob.size() * 2);
    re += '^';
    for (size_t i = 0; i < glob.size(); ++i) {
        char c = glob[i];
        if (c == '*') {
            if (i + 1 < glob.size() && glob[i + 1] == '*') {
                re += ".*";
                ++i;
                if (i + 1 < glob.size() && (glob[i + 1] == '/' || glob[i + 1] == '\\'))
                    ++i;
            } else {
                re += "[^/\\\\]*";
            }
        } else if (c == '?') {
            re += "[^/\\\\]";
        } else if (c == '{') {
            re += '(';
            ++i;
            while (i < glob.size() && glob[i] != '}') {
                if (glob[i] == ',') re += '|';
                else {
                    if (std::string(".^$|()?*+{}[]\\").find(glob[i]) != std::string::npos)
                        re += '\\';
                    re += glob[i];
                }
                ++i;
            }
            re += ')';
        } else if (std::string(".^$|()+{}[]\\").find(c) != std::string::npos) {
            re += '\\';
            re += c;
        } else {
            re += c;
        }
    }
    re += '$';
    return re;
}

} // anonymous namespace

std::string GlobTool::description() const {
    return
        "Finds files by name pattern and returns matching paths sorted by modification time.\n"
        "\n"
        "Usage:\n"
        "- Accepts standard glob syntax: ** matches across directory levels, * matches within one level\n"
        "- Examples: \"**/*.py\", \"src/**/*.ts\", \"test_*.cpp\"\n"
        "- Use when you know roughly what a file is named but not where it lives\n"
        "- To search file contents rather than names, use the Grep tool instead";
}

json GlobTool::input_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"pattern", {
                {"type", "string"},
                {"description", "Glob pattern (e.g., \"**/*.cpp\", \"src/*.hpp\")"}
            }},
            {"path", {
                {"type", "string"},
                {"description", "Root directory to search (default: \".\")"}
            }}
        }},
        {"required", {"pattern"}}
    };
}

std::string GlobTool::call(const json& input) const {
    const std::string pattern     = input["pattern"].get<std::string>();
    const std::string search_path = input.value("path", ".");

    namespace fs = std::filesystem;
    fs::path root(search_path);

    std::error_code ec;
    if (!fs::exists(root, ec)) {
        return "Error: Path does not exist: " + search_path;
    }

    // Compile regex
    std::regex re;
    try {
        re = std::regex(glob_to_regex_str(pattern),
                        std::regex::ECMAScript | std::regex::icase);
    } catch (const std::regex_error& e) {
        return "Error: Invalid glob pattern: " + std::string(e.what());
    }

    // Collect file list + modification times
    struct Entry {
        std::string path;
        std::filesystem::file_time_type mtime;
    };
    std::vector<Entry> matches;

    for (const auto& entry :
         fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied))
    {
        if (!entry.is_regular_file()) continue;

        // Normalize relative path using '/'
        fs::path rel = entry.path().lexically_relative(root);
        std::string rel_str;
        for (const auto& part : rel) {
            if (!rel_str.empty()) rel_str += '/';
            rel_str += part.string();
        }

        bool matched = false;
        try {
            if (std::regex_match(rel_str, re)) matched = true;
            if (!matched && std::regex_match(entry.path().filename().string(), re)) matched = true;
        } catch (...) {}

        if (matched) {
            matches.push_back({normalize_path(entry.path()), entry.last_write_time()});
        }
    }

    // Sort by modification time descending (most recently modified first)
    std::sort(matches.begin(), matches.end(),
              [](const Entry& a, const Entry& b) { return a.mtime > b.mtime; });

    if (matches.empty()) return "(no matches)";

    std::ostringstream oss;
    for (const auto& m : matches) {
        oss << m.path << '\n';
    }
    return oss.str();
}

} // namespace openrein
