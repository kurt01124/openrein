#pragma once
#include "tool_base.hpp"

namespace openrein {

struct ReadTool : ToolBase {
    std::string name()        const override { return "Read"; }
    std::string description() const override;
    json        input_schema()const override;
    std::string call(const json& input) const override;
};

} // namespace openrein
