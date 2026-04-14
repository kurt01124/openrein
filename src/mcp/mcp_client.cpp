#include "mcp_client.hpp"
#include <stdexcept>

namespace openrein {

McpClient::McpClient(std::unique_ptr<McpTransport> transport)
    : transport_(std::move(transport)) {}

void McpClient::connect() {
    transport_->initialize();

    // MCP initialize handshake
    json init_params = {
        {"protocolVersion", "2024-11-05"},
        {"capabilities",    json::object()},
        {"clientInfo",      {{"name", "openrein"}, {"version", "0.1.0"}}}
    };
    json resp = transport_->send_request("initialize", init_params, next_id_++);
    if (resp.contains("error")) {
        throw std::runtime_error(
            "MCP initialize error: " + resp["error"].dump()
        );
    }

    // send initialized notification (no response expected)
    transport_->send_notification("notifications/initialized", json::object());
}

std::vector<McpTool> McpClient::list_tools() {
    json resp = transport_->send_request("tools/list", json::object(), next_id_++);
    if (resp.contains("error")) {
        throw std::runtime_error(
            "MCP tools/list error: " + resp["error"].dump()
        );
    }

    std::vector<McpTool> tools;
    if (!resp.contains("result") || !resp["result"].contains("tools"))
        return tools;

    for (const auto& t : resp["result"]["tools"]) {
        McpTool tool;
        tool.name        = t.value("name", "");
        tool.description = t.value("description", "");
        // MCP spec: "inputSchema" (camelCase) → internal: "input_schema"
        tool.input_schema = t.contains("inputSchema")
            ? t["inputSchema"]
            : json{{"type","object"},
                   {"properties",json::object()},
                   {"required",  json::array()}};
        tools.push_back(std::move(tool));
    }
    return tools;
}

json McpClient::call_tool(const std::string& name, const json& input) {
    json params = {
        {"name",      name},
        {"arguments", input}
    };
    json resp = transport_->send_request("tools/call", params, next_id_++);
    if (resp.contains("error")) {
        throw std::runtime_error(
            "MCP tools/call error: " + resp["error"].dump()
        );
    }
    return resp.contains("result") ? resp["result"] : json::object();
}

} // namespace openrein
