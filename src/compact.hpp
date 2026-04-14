#pragma once
#include <string>
#include <pybind11/pybind11.h>
#include "json_fwd.hpp"

namespace py = pybind11;

namespace openrein {

/// Context compact utility.
///
/// The developer invokes the LLM directly.
/// This class handles token estimation, compact prompt generation, and response parsing.
class Compact {
public:
    Compact() = default;

    /// Estimate the approximate token count of messages (chars / 4 approximation).
    int estimate_tokens(py::list messages) const;

    /// Returns true if estimate_tokens(messages) >= threshold.
    bool should_compact(py::list messages, int threshold = 80000) const;

    /// Return the LLM prompt string requesting compaction of messages.
    std::string make_prompt(py::list messages) const;

    /// Extract the summary text inside the <result> tag from the compact model response.
    std::string parse_response(const std::string& response_text) const;

    /// Return a new list with messages replaced by [{"role":"user","content":summary}].
    py::list apply(py::list messages, const std::string& summary) const;
};

} // namespace openrein
