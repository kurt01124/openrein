#include "http_transport.hpp"
#include <pybind11/pybind11.h>
#include <stdexcept>

namespace py = pybind11;

namespace openrein {

HttpTransport::HttpTransport(const std::string& url) : url_(url) {}

void HttpTransport::initialize() {
    // HTTP transport is stateless — no initialization needed
}

json HttpTransport::http_post(const json& payload) {
    py::module_ urllib_request = py::module_::import("urllib.request");

    std::string body_str = payload.dump();

    try {
        py::dict headers;
        headers["Content-Type"] = "application/json";

        // Request(url, data, headers)
        py::object request = urllib_request.attr("Request")(
            url_,
            py::bytes(body_str),
            headers
        );

        py::object resp = urllib_request.attr("urlopen")(request, py::none(), 30);
        std::string data = resp.attr("read")()
                               .attr("decode")("utf-8")
                               .cast<std::string>();

        return data.empty() ? json::object() : json::parse(data);

    } catch (py::error_already_set& e) {
        throw std::runtime_error("MCP HTTP error: " + std::string(e.what()));
    } catch (const std::exception& e) {
        throw std::runtime_error("MCP HTTP error: " + std::string(e.what()));
    }
}

json HttpTransport::send_request(const std::string& method,
                                  const json& params, int id) {
    json req = {{"jsonrpc","2.0"}, {"id",id}, {"method",method}};
    if (!params.empty()) req["params"] = params;
    return http_post(req);
}

void HttpTransport::send_notification(const std::string& method,
                                       const json& params) {
    json notif = {{"jsonrpc","2.0"}, {"method",method}};
    if (!params.empty()) notif["params"] = params;
    try { http_post(notif); } catch (...) {}  // ignore notification failures
}

} // namespace openrein
