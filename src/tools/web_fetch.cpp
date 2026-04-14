#include "web_fetch.hpp"
#include <pybind11/pybind11.h>
#include <string>
#include <sstream>

namespace py = pybind11;

namespace openrein {

std::string WebFetchTool::description() const {
    return
        "Fetches the contents of a web page from a URL and returns it as text.\n"
        "HTML is converted to plain text by stripping tags. Timeout: 30 seconds.";
}

json WebFetchTool::input_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"url", {
                {"type", "string"},
                {"description", "URL to fetch"}
            }},
            {"max_length", {
                {"type", "integer"},
                {"description", "Maximum number of characters to return (default 20000)"},
                {"default", 20000}
            }}
        }},
        {"required", {"url"}}
    };
}

// Strips HTML tags from the given string and returns plain text.
// Calls Python's re / html modules via pybind11.
static std::string strip_html(const std::string& html) {
    py::module_ re_mod   = py::module_::import("re");
    py::module_ html_mod = py::module_::import("html");

    // 1. Remove <script> / <style> blocks (including their content)
    int flags = re_mod.attr("DOTALL").cast<int>() |
                re_mod.attr("IGNORECASE").cast<int>();
    py::object text = re_mod.attr("sub")(
        R"(<(script|style)[\s\S]*?</(script|style)>)",
        " ", html, 0, flags
    );

    // 2. Remove remaining HTML tags
    text = re_mod.attr("sub")("<[^>]+>", " ", text);

    // 3. Decode HTML entities (&amp; &lt; etc.)
    text = html_mod.attr("unescape")(text);

    // 4. Collapse consecutive whitespace/newlines into a single space
    text = re_mod.attr("sub")(R"(\s+)", " ", text);

    // 5. Strip leading and trailing whitespace
    text = text.attr("strip")();

    return text.cast<std::string>();
}

std::string WebFetchTool::call(const json& input) const {
    std::string url        = input["url"].get<std::string>();
    int         max_length = input.value("max_length", 20000);

    try {
        py::module_ urllib = py::module_::import("urllib.request");

        // urlopen(url, data=None, timeout=30)
        py::object resp    = urllib.attr("urlopen")(url, py::none(), 30);
        py::object raw_obj = resp.attr("read")();

        // Check Content-Type to determine whether response is HTML
        py::object headers = resp.attr("headers");
        std::string ctype;
        try {
            ctype = py::str(
                headers.attr("get")("Content-Type", "")
            ).cast<std::string>();
        } catch (...) {}

        // Decode bytes → str
        std::string decoded;
        try {
            decoded = raw_obj.attr("decode")("utf-8", "replace").cast<std::string>();
        } catch (...) {
            decoded = raw_obj.cast<std::string>();
        }

        std::string text;
        if (ctype.find("html") != std::string::npos || ctype.empty()) {
            text = strip_html(decoded);
        } else {
            text = decoded;
        }

        if ((int)text.size() > max_length) {
            text = text.substr(0, max_length)
                 + "\n\n[...truncated at " + std::to_string(max_length) + " chars]";
        }

        return text.empty() ? "(empty response)" : text;

    } catch (py::error_already_set& e) {
        return "Error: " + std::string(e.what());
    } catch (const std::exception& e) {
        return "Error: " + std::string(e.what());
    }
}

} // namespace openrein
