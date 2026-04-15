#include "web_search.hpp"
#include <pybind11/pybind11.h>
#include <string>
#include <sstream>

namespace py = pybind11;

namespace openrein {

std::string WebSearchTool::description() const {
    return
        "Searches the web and returns a ranked list of results, each with a title, URL, and short summary.\n"
        "\n"
        "Usage:\n"
        "- Use for current events, recent releases, or topics beyond the model's training cutoff\n"
        "- Set the BRAVE_API_KEY environment variable to use the Brave Search API; falls back to DuckDuckGo otherwise\n"
        "- Adjust num_results to control how many results are returned (default 10)";
}

json WebSearchTool::input_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"query", {
                {"type", "string"},
                {"description", "Search query"}
            }},
            {"num_results", {
                {"type", "integer"},
                {"description", "Number of results to return (default 10)"},
                {"default", 10}
            }}
        }},
        {"required", {"query"}}
    };
}

// ---------------------------------------------------------------------------
// Brave Search API call
// ---------------------------------------------------------------------------
static std::string search_brave(const std::string& query, int num,
                                 const std::string& api_key) {
    try {
        py::module_ urllib   = py::module_::import("urllib.request");
        py::module_ urllib_p = py::module_::import("urllib.parse");
        py::module_ json_mod = py::module_::import("json");

        std::string encoded_q = urllib_p.attr("quote")(query).cast<std::string>();
        std::string url = "https://api.search.brave.com/res/v1/web/search?q="
                        + encoded_q + "&count=" + std::to_string(num);

        py::object req = urllib.attr("Request")(url);
        req.attr("add_header")("X-Subscription-Token", api_key);
        req.attr("add_header")("Accept", "application/json");

        py::object resp    = urllib.attr("urlopen")(req, py::none(), 30);
        py::object raw_obj = resp.attr("read")();
        py::object decoded = raw_obj.attr("decode")("utf-8", "replace");
        py::object data    = json_mod.attr("loads")(decoded);

        py::object web     = data.attr("get")("web", py::dict());
        py::object results = web.attr("get")("results", py::list());

        std::ostringstream oss;
        int count = 0;
        for (auto item : results) {
            if (count >= num) break;
            std::string title  = py::str(item.attr("get")("title",       "")).cast<std::string>();
            std::string u      = py::str(item.attr("get")("url",         "")).cast<std::string>();
            std::string desc   = py::str(item.attr("get")("description", "")).cast<std::string>();
            oss << title << "\n" << u << "\n" << desc << "\n---\n";
            count++;
        }

        return oss.str().empty() ? "(no results)" : oss.str();

    } catch (py::error_already_set& e) {
        return "Error (Brave): " + std::string(e.what());
    }
}

// ---------------------------------------------------------------------------
// DuckDuckGo HTML scraping
// ---------------------------------------------------------------------------
static std::string search_duckduckgo(const std::string& query, int num) {
    try {
        py::module_ urllib   = py::module_::import("urllib.request");
        py::module_ urllib_p = py::module_::import("urllib.parse");
        py::module_ re_mod   = py::module_::import("re");

        std::string encoded_q = urllib_p.attr("quote_plus")(query).cast<std::string>();
        std::string url = "https://html.duckduckgo.com/html/?q=" + encoded_q;

        py::object req = urllib.attr("Request")(url);
        req.attr("add_header")("User-Agent",
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
            "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
        req.attr("add_header")("Accept-Language", "en-US,en;q=0.9");

        py::object resp    = urllib.attr("urlopen")(req, py::none(), 30);
        py::object raw_obj = resp.attr("read")();
        std::string html   = raw_obj.attr("decode")("utf-8", "replace").cast<std::string>();

        // Titles: content of class="result__a" tags
        py::object titles = re_mod.attr("findall")(
            R"(class="result__a"[^>]*>([\s\S]*?)</a>)", html
        );
        // URLs: content of class="result__url" tags
        py::object urls = re_mod.attr("findall")(
            R"(class="result__url"[^>]*>\s*([\s\S]*?)</(?:span|a)>)", html
        );
        // Snippets: content of class="result__snippet" tags
        py::object snippets = re_mod.attr("findall")(
            R"(class="result__snippet"[^>]*>([\s\S]*?)</(?:span|a)>)", html
        );

        // Tag-stripping helper
        py::object tag_re = re_mod.attr("compile")("<[^>]+>");

        int n_titles   = (int)py::len(titles);
        int n_urls     = (int)py::len(urls);
        int n_snippets = (int)py::len(snippets);
        int count      = std::min(n_titles, num);

        std::ostringstream oss;
        for (int i = 0; i < count; i++) {
            std::string title, u, snippet;

            try {
                title = tag_re.attr("sub")(" ", py::list(titles)[i])
                             .attr("strip")().cast<std::string>();
            } catch (...) {}

            try {
                if (i < n_urls) {
                    u = tag_re.attr("sub")(" ", py::list(urls)[i])
                              .attr("strip")().cast<std::string>();
                }
            } catch (...) {}

            try {
                if (i < n_snippets) {
                    snippet = tag_re.attr("sub")(" ", py::list(snippets)[i])
                                   .attr("strip")().cast<std::string>();
                }
            } catch (...) {}

            if (!title.empty() || !u.empty()) {
                oss << title << "\n" << u << "\n" << snippet << "\n---\n";
            }
        }

        return oss.str().empty() ? "(no results)" : oss.str();

    } catch (py::error_already_set& e) {
        return "Error (DuckDuckGo): " + std::string(e.what());
    }
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
std::string WebSearchTool::call(const json& input) const {
    std::string query       = input["query"].get<std::string>();
    int         num_results = input.value("num_results", 10);

    try {
        py::module_ os  = py::module_::import("os");
        py::object  key = os.attr("environ").attr("get")("BRAVE_API_KEY");

        if (!key.is_none()) {
            return search_brave(query, num_results, key.cast<std::string>());
        } else {
            return search_duckduckgo(query, num_results);
        }
    } catch (py::error_already_set& e) {
        return "Error: " + std::string(e.what());
    }
}

} // namespace openrein
