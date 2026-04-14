#pragma once
#include "mcp_client.hpp"
#include <string>

namespace openrein {

/// HTTP transport — sends JSON-RPC 2.0 POST requests via Python urllib.request.
/// POSTs requests to the server URL and returns the JSON response.
class HttpTransport : public McpTransport {
public:
    explicit HttpTransport(const std::string& url);

    void initialize() override;   ///< stateless — no-op
    json send_request(const std::string& method,
                      const json& params, int id) override;
    void send_notification(const std::string& method,
                           const json& params) override;

private:
    std::string url_;
    json        http_post(const json& payload);
};

} // namespace openrein
