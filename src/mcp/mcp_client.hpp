#pragma once
#include <string>
#include <vector>
#include <memory>
#include "../json_fwd.hpp"

namespace openrein {

/// Single tool descriptor from an MCP server
struct McpTool {
    std::string name;
    std::string description;
    json        input_schema;  ///< {"type":"object","properties":{...},"required":[...]}
};

/// Abstract base class for JSON-RPC 2.0 transports
class McpTransport {
public:
    virtual ~McpTransport() = default;

    /// Initialize the server (start process or establish connection)
    virtual void initialize() = 0;

    /// Send a JSON-RPC request and return the response JSON
    virtual json send_request(const std::string& method,
                              const json& params, int id) = 0;

    /// Send a JSON-RPC notification (no response expected, failures ignored)
    virtual void send_notification(const std::string& method,
                                   const json& params) = 0;
};

/// MCP client — initialize handshake + tool listing + tool invocation
class McpClient {
public:
    explicit McpClient(std::unique_ptr<McpTransport> transport);

    /// Perform the initialize handshake (must be called before any other method)
    void connect();

    /// Query the server's tool list (tools/list)
    std::vector<McpTool> list_tools();

    /// Invoke a tool (tools/call) and return the result JSON
    json call_tool(const std::string& name, const json& input);

private:
    std::unique_ptr<McpTransport> transport_;
    int next_id_ = 1;
};

} // namespace openrein
