#pragma once
#include "tool_base.hpp"

namespace openrein {

struct WebSearchTool : ToolBase {
    std::string name()        const override { return "WebSearch"; }
    std::string description() const override;
    json        input_schema()const override;
    std::string call(const json& input) const override;
};

} // namespace openrein
