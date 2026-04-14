#include "stdio_transport.hpp"
#include <pybind11/stl.h>
#include <stdexcept>

namespace py = pybind11;
using namespace pybind11::literals;

namespace openrein {

StdioTransport::StdioTransport(const std::vector<std::string>& command)
    : command_(command) {}

StdioTransport::~StdioTransport() {
    if (!started_) return;
    if (!Py_IsInitialized()) return;
    try {
        py::gil_scoped_acquire gil;
        proc_.attr("terminate")();
        proc_ = py::none();  // release reference while holding GIL
    } catch (...) {}
}

void StdioTransport::initialize() {
    if (started_) return;

    py::module_ subprocess = py::module_::import("subprocess");

    py::list cmd_list;
    for (const auto& arg : command_) cmd_list.append(arg);

    try {
        proc_ = subprocess.attr("Popen")(
            cmd_list,
            "stdin"_a    = subprocess.attr("PIPE"),
            "stdout"_a   = subprocess.attr("PIPE"),
            "stderr"_a   = subprocess.attr("PIPE"),
            "text"_a     = true,
            "encoding"_a = "utf-8"
        );
    } catch (py::error_already_set& e) {
        throw std::runtime_error(
            "Failed to start MCP server process: " + std::string(e.what())
        );
    }
    started_ = true;
}

void StdioTransport::write_line(const std::string& json_str) {
    try {
        proc_.attr("stdin").attr("write")(json_str + "\n");
        proc_.attr("stdin").attr("flush")();
    } catch (py::error_already_set& e) {
        throw std::runtime_error("MCP stdio write error: " + std::string(e.what()));
    }
}

std::string StdioTransport::read_line() {
    try {
        py::object line_obj = proc_.attr("stdout").attr("readline")();
        std::string line    = line_obj.cast<std::string>();
        if (line.empty()) {
            throw std::runtime_error("MCP server closed stdout (process may have crashed)");
        }
        return line;
    } catch (py::error_already_set& e) {
        throw std::runtime_error("MCP stdio read error: " + std::string(e.what()));
    }
}

json StdioTransport::send_request(const std::string& method,
                                   const json& params, int id) {
    if (!started_) initialize();

    json req = {{"jsonrpc","2.0"}, {"id",id}, {"method",method}};
    if (!params.empty()) req["params"] = params;

    write_line(req.dump());

    std::string line = read_line();
    try {
        return json::parse(line);
    } catch (const std::exception& e) {
        throw std::runtime_error(
            std::string("MCP invalid JSON response: ") + e.what()
            + " | raw: " + line.substr(0, 200)
        );
    }
}

void StdioTransport::send_notification(const std::string& method,
                                        const json& params) {
    if (!started_) return;
    json notif = {{"jsonrpc","2.0"}, {"method",method}};
    if (!params.empty()) notif["params"] = params;
    try { write_line(notif.dump()); } catch (...) {}  // ignore notification failures
}

} // namespace openrein
