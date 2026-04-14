#pragma once
#include <string>

namespace openrein {

/// Tool execution result.
/// content is always populated; if is_error=true, it contains the error message.
/// not_found=true means no tool with that name was found (continue search chain).
struct ExecResult {
    std::string content;
    bool        is_error  = false;
    bool        not_found = false;

    static ExecResult ok(std::string s)    { return {std::move(s), false, false}; }
    static ExecResult err(std::string msg) { return {std::move(msg), true,  false}; }
    static ExecResult unknown()            { return {"",            false, true};  }
};

} // namespace openrein
