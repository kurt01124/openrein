#pragma once
#include "mcp_client.hpp"
#include <vector>
#include <string>
#include <pybind11/pybind11.h>

namespace openrein {

/// stdio transport — manages an MCP server process via Python subprocess.Popen.
/// Writes JSON-RPC requests to the child process's stdin and reads responses from stdout.
class StdioTransport : public McpTransport {
public:
    explicit StdioTransport(const std::vector<std::string>& command);
    ~StdioTransport() override;

    void initialize() override;
    json send_request(const std::string& method,
                      const json& params, int id) override;
    void send_notification(const std::string& method,
                           const json& params) override;

private:
    std::vector<std::string>  command_;
    pybind11::object          proc_;       ///< subprocess.Popen object (null-initialized)
    bool                      started_ = false;

    void write_line(const std::string& json_str);
    std::string read_line();
};

} // namespace openrein
