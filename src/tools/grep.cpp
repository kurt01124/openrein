#include "grep.hpp"
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <vector>
#include <algorithm>
#include <string>
#include <unordered_map>

namespace openrein {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace {

/// Converts a glob pattern (filename portion) to a regex string.
/// '**' → ".*", '*' → "[^/]*", '?' → "[^/]", '{a,b}' → "(a|b)"
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
                if (i + 1 < glob.size() && (glob[i + 1] == '/' || glob[i + 1] == '\\')) ++i;
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

/// Returns true if the file path matches the given glob pattern (checked against the full relative path).
bool path_matches_glob(const std::filesystem::path& root,
                       const std::filesystem::path& entry,
                       const std::string& glob)
{
    if (glob.empty()) return true;

    // Normalize relative path using '/'
    std::filesystem::path rel = entry.lexically_relative(root);
    std::string rel_str;
    for (const auto& part : rel) {
        if (!rel_str.empty()) rel_str += '/';
        rel_str += part.string();
    }

    try {
        std::regex re(glob_to_regex_str(glob),
                      std::regex::ECMAScript | std::regex::icase);
        if (std::regex_match(rel_str, re)) return true;
        if (std::regex_match(entry.filename().string(), re)) return true;
    } catch (...) {
        return true;
    }
    return false;
}

/// Returns true if the file path matches any of the given glob patterns (M4: for type filtering)
bool path_matches_any_glob(const std::filesystem::path& root,
                           const std::filesystem::path& entry,
                           const std::vector<std::string>& globs)
{
    for (const auto& g : globs)
        if (path_matches_glob(root, entry, g)) return true;
    return false;
}

/// Quick binary file check (presence of null bytes).
bool is_binary(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    char buf[512];
    f.read(buf, sizeof(buf));
    const std::streamsize n = f.gcount();
    for (std::streamsize i = 0; i < n; ++i) {
        if (buf[i] == '\0') return true;
    }
    return false;
}

// M4: language type → glob pattern table
static const std::unordered_map<std::string, std::vector<std::string>> TYPE_TO_GLOBS = {
    {"py",    {"*.py", "*.pyi"}},
    {"js",    {"*.js", "*.mjs", "*.cjs"}},
    {"ts",    {"*.ts", "*.tsx"}},
    {"rs",    {"*.rs"}},
    {"cpp",   {"*.cpp", "*.cxx", "*.cc", "*.C"}},
    {"c",     {"*.c", "*.h"}},
    {"go",    {"*.go"}},
    {"java",  {"*.java"}},
    {"rb",    {"*.rb"}},
    {"cs",    {"*.cs"}},
    {"swift", {"*.swift"}},
    {"kt",    {"*.kt", "*.kts"}},
    {"html",  {"*.html", "*.htm"}},
    {"css",   {"*.css", "*.scss", "*.sass"}},
    {"json",  {"*.json"}},
    {"yaml",  {"*.yaml", "*.yml"}},
    {"toml",  {"*.toml"}},
    {"md",    {"*.md", "*.markdown"}},
    {"sh",    {"*.sh", "*.bash", "*.zsh"}},
    {"sql",   {"*.sql"}},
    {"cmake", {"CMakeLists.txt", "*.cmake"}},
    {"hpp",   {"*.hpp", "*.hxx", "*.h"}},
    {"txt",   {"*.txt"}},
};

/// M3: Search a single file with context lines — ripgrep-compatible output format
/// match line:   "path:lineno:text\n"
/// context line: "path-lineno-text\n"
/// range separator: "--\n"
static std::string search_file_with_context(
    const std::filesystem::path& fpath,
    const std::regex& re,
    int before, int after,
    int& match_count)
{
    // Read all lines
    std::vector<std::string> all_lines;
    {
        std::ifstream f(fpath, std::ios::binary);
        if (!f) return "";
        std::string line;
        while (std::getline(f, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            all_lines.push_back(std::move(line));
        }
    }

    const int total = static_cast<int>(all_lines.size());

    // Collect matched line numbers (0-based)
    std::vector<int> matched_nos;
    for (int i = 0; i < total; ++i) {
        if (std::regex_search(all_lines[i], re)) {
            matched_nos.push_back(i);
            ++match_count;
        }
    }
    if (matched_nos.empty()) return "";

    // Merge ranges (combine overlapping intervals)
    std::vector<std::pair<int,int>> ranges;
    for (int m : matched_nos) {
        int lo = std::max(0, m - before);
        int hi = std::min(total - 1, m + after);
        if (!ranges.empty() && lo <= ranges.back().second + 1)
            ranges.back().second = std::max(ranges.back().second, hi);
        else
            ranges.push_back({lo, hi});
    }

    std::string path_str = fpath.string();
    std::ostringstream oss;
    bool first_range = true;
    for (const auto& [lo, hi] : ranges) {
        if (!first_range) oss << "--\n";
        first_range = false;
        for (int i = lo; i <= hi; ++i) {
            bool is_match = std::regex_search(all_lines[i], re);
            char sep = is_match ? ':' : '-';
            oss << path_str << sep << (i + 1) << sep << all_lines[i] << '\n';
        }
    }
    return oss.str();
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// GrepTool implementation
// ---------------------------------------------------------------------------

std::string GrepTool::description() const {
    return
        "A powerful search tool built on ripgrep\n"
        "\n"
        "  Usage:\n"
        "  - ALWAYS use Grep for search tasks. NEVER invoke `grep` or `rg` as a Bash command. The Grep tool has been optimized for correct permissions and access.\n"
        "  - Supports full regex syntax (e.g., \"log.*Error\", \"function\\s+\\w+\")\n"
        "  - Filter files with glob parameter (e.g., \"*.js\", \"**/*.tsx\") or type parameter (e.g., \"js\", \"py\", \"rust\")\n"
        "  - Output modes: \"content\" shows matching lines, \"files_with_matches\" shows only file paths (default), \"count\" shows match counts\n"
        "  - Use Agent tool for open-ended searches requiring multiple rounds\n"
        "  - Pattern syntax: Uses ripgrep (not grep) - literal braces need escaping (use `interface\\{\\}` to find `interface{}` in Go code)\n"
        "  - Multiline matching: By default patterns match within single lines only. For cross-line patterns like `struct \\{[\\s\\S]*?field`, use `multiline: true`\n";
}

json GrepTool::input_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"pattern", {
                {"type", "string"},
                {"description", "Regular expression to search for"}
            }},
            {"path", {
                {"type", "string"},
                {"description", "Directory or file to search (default: \".\")"}
            }},
            {"glob", {
                {"type", "string"},
                {"description", "File filter glob pattern (e.g., \"*.rs\", \"**/*.py\")"}
            }},
            {"type", {
                {"type", "string"},
                {"description",
                    "Language type filter (e.g., \"py\", \"cpp\", \"js\"). "
                    "Takes precedence over the glob parameter."}
            }},
            {"output_mode", {
                {"type", "string"},
                {"enum", {"content", "files_with_matches", "count"}},
                {"description", "Output format (default: \"files_with_matches\")"},
                {"default", "files_with_matches"}
            }},
            {"-i", {
                {"type", "boolean"},
                {"description", "Case-insensitive matching (default: false)"},
                {"default", false}
            }},
            {"-A", {
                {"type", "integer"},
                {"description", "Number of context lines after each match (content mode)"},
                {"default", 0}
            }},
            {"-B", {
                {"type", "integer"},
                {"description", "Number of context lines before each match (content mode)"},
                {"default", 0}
            }},
            {"-C", {
                {"type", "integer"},
                {"description", "Shorthand to set both -A and -B simultaneously (content mode)"},
                {"default", 0}
            }},
            {"context", {
                {"type", "integer"},
                {"description", "Alias for -C"},
                {"default", 0}
            }},
            {"head_limit", {
                {"type", "integer"},
                {"description", "Maximum number of items to return (default 250, 0=unlimited)"},
                {"default", 250}
            }},
            {"offset", {
                {"type", "integer"},
                {"description", "Number of items to skip (default: 0)"},
                {"default", 0}
            }},
            {"multiline", {
                {"type", "boolean"},
                {"description", "Multiline mode — not currently supported (ignored)"},
                {"default", false}
            }}
        }},
        {"required", {"pattern"}}
    };
}

std::string GrepTool::call(const json& input) const {
    const std::string pattern     = input["pattern"].get<std::string>();
    const std::string search_path = input.value("path", ".");
    const std::string glob_pat    = input.value("glob", "");
    const std::string type_filter = input.value("type", "");
    const std::string output_mode = input.value("output_mode", "files_with_matches");
    const bool        ignore_case = input.value("-i", false);
    const int         head_limit  = input.value("head_limit", 250);
    const int         offset      = input.value("offset", 0);

    // context lines: -C / context takes precedence over -A/-B
    const int ctx_C   = std::max(input.value("-C", 0), input.value("context", 0));
    const int ctx_A   = (ctx_C > 0) ? ctx_C : input.value("-A", 0);
    const int ctx_B   = (ctx_C > 0) ? ctx_C : input.value("-B", 0);
    const bool use_ctx = (output_mode == "content") && (ctx_A > 0 || ctx_B > 0);

    // Compile regex
    std::regex re;
    try {
        auto flags = std::regex::ECMAScript;
        if (ignore_case) flags |= std::regex::icase;
        re = std::regex(pattern, flags);
    } catch (const std::regex_error& e) {
        return "Error: Invalid regex pattern: " + std::string(e.what());
    }

    namespace fs = std::filesystem;
    fs::path root(search_path);
    std::error_code ec;
    if (!fs::exists(root, ec)) {
        return "Error: Path does not exist: " + search_path;
    }

    // M4: resolve type filter → effective glob list
    std::vector<std::string> effective_globs;
    if (!type_filter.empty()) {
        auto it = TYPE_TO_GLOBS.find(type_filter);
        if (it != TYPE_TO_GLOBS.end()) {
            effective_globs = it->second;
        }
        // Unknown type: allow all (effective_globs remains empty)
    } else if (!glob_pat.empty()) {
        effective_globs = {glob_pat};
    }

    // Collect file list
    std::vector<fs::path> files;
    if (fs::is_regular_file(root, ec)) {
        files.push_back(root);
    } else {
        for (const auto& entry :
             fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied))
        {
            if (!entry.is_regular_file()) continue;

            bool pass = effective_globs.empty()
                        || path_matches_any_glob(root, entry.path(), effective_globs);
            if (pass) files.push_back(entry.path());
        }
        std::sort(files.begin(), files.end());
    }

    // Search
    std::vector<std::string> content_lines;   // output_mode == "content"
    std::vector<std::string> matched_files;   // files_with_matches / count
    std::unordered_map<std::string, int> file_match_counts;  // for count mode
    int total_matched_lines = 0;
    const int effective_limit = (head_limit <= 0) ? INT_MAX : head_limit;

    for (const auto& fpath : files) {
        if (is_binary(fpath)) continue;

        if (output_mode == "content" && use_ctx) {
            // context mode: read entire file and merge ranges
            int file_count = 0;
            std::string result = search_file_with_context(fpath, re, ctx_B, ctx_A, file_count);
            if (!result.empty()) {
                // split result into lines and append to content_lines
                std::istringstream ss(result);
                std::string line;
                while (std::getline(ss, line)) {
                    if (static_cast<int>(content_lines.size()) < effective_limit + offset)
                        content_lines.push_back(std::move(line));
                }
                total_matched_lines += file_count;
                const std::string path_str = fpath.string();
                matched_files.push_back(path_str);
                file_match_counts[path_str] = file_count;
            }
            continue;
        }

        // Standard line-by-line search
        std::ifstream f(fpath, std::ios::binary);
        if (!f) continue;

        std::string line;
        int lineno = 0;
        bool file_matched = false;
        int file_count = 0;

        while (std::getline(f, line)) {
            ++lineno;
            if (!line.empty() && line.back() == '\r') line.pop_back();

            if (std::regex_search(line, re)) {
                ++total_matched_lines;
                ++file_count;
                file_matched = true;

                if (output_mode == "content") {
                    if (static_cast<int>(content_lines.size()) < effective_limit + offset)
                        content_lines.push_back(
                            fpath.string() + ':' + std::to_string(lineno) + ':' + line
                        );
                }
            }
        }

        if (file_matched) {
            const std::string path_str = fpath.string();
            matched_files.push_back(path_str);
            file_match_counts[path_str] = file_count;
        }
    }

    // Apply offset + head_limit then format output
    std::ostringstream oss;
    if (output_mode == "content") {
        int idx = 0;
        int written = 0;
        for (const auto& line : content_lines) {
            if (idx++ < offset) continue;
            if (written >= effective_limit) break;
            oss << line << '\n';
            ++written;
        }
        if (content_lines.empty()) oss << "(no matches)";

    } else if (output_mode == "count") {
        // ripgrep-compatible: "path:N" format (fixes m5)
        int idx = 0;
        int written = 0;
        for (const auto& p : matched_files) {
            if (idx++ < offset) continue;
            if (written >= effective_limit) break;
            oss << p << ':' << file_match_counts.at(p) << '\n';
            ++written;
        }
        if (matched_files.empty()) oss << "(no matches)";

    } else {
        // files_with_matches
        int idx = 0;
        int written = 0;
        for (const auto& p : matched_files) {
            if (idx++ < offset) continue;
            if (written >= effective_limit) break;
            oss << p << '\n';
            ++written;
        }
        if (matched_files.empty()) oss << "(no matches)";
    }

    return oss.str();
}

} // namespace openrein
