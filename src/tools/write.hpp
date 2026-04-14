#pragma once
#include "tool_base.hpp"

namespace openrein {

struct WriteTool : ToolBase {
    std::string name()        const override { return "Write"; }
    std::string description() const override;
    json        input_schema()const override;
    std::string call(const json& input) const override;
};

} // namespace openrein
