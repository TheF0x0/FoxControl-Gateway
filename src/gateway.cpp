/**
 * @author F0x0
 * @since 05/04/2023
 */

#include <sstream>
#include <httplib.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include "gateway.hpp"

#define FOX_HTML_MIME_TYPE "text/html"
#define FOX_JSON_MIME_TYPE "application/json"

namespace fox {
    atomic_queue::AtomicQueueB2<Task> Gateway::_tasks(2);
    std::atomic_size_t Gateway::_total_task_count = 0;

    Gateway::Gateway(std::string address, std::string endpoint, kstd::u32 port) noexcept:
            _address(std::move(address)),
            _endpoint(std::move(endpoint)),
            _port(port) {
        httplib::Server server;

        server.set_error_handler(handle_error);

        server.Get("/status", handle_status);
        server.Get("/fetch", handle_fetch);
        server.Post(fmt::format("/{}", _endpoint), handle_endpoint);

        spdlog::info("Listening on {}:{}/{}", _address, _port, _endpoint);
        server.listen(_address, static_cast<kstd::i32>(_port));
    }

    auto Gateway::handle_error(const httplib::Request& req, httplib::Response& res) noexcept -> void {
        spdlog::warn("Received invalid request");

        res.set_content(R"*(
<html lang="en">
    <head>
        <title>🦊 Oops..</title>
        <meta charset="UTF-8" />
    </head>
    <body>
        <h1>Nothing here but us foxes 🦊</h1>
        This is not the page you were looking for.
    </body>
</html>
        )*", FOX_HTML_MIME_TYPE);
    }

    auto Gateway::handle_status(const httplib::Request& req, httplib::Response& res) noexcept -> void {
        spdlog::debug("Received status request");

        const auto task_count = _tasks.was_size();
        const auto total_task_count = static_cast<size_t>(_total_task_count);

        res.set_content(fmt::format(R"*(
<html lang="en">
    <head>
        <title>🦊 Status</title>
        <meta charset="UTF-8" />
    </head>
    <body>
        <h1>🦊 Status</h1>
        <h3>Queued Tasks: {}</h3>
        <h3>Total Tasks: {}</h3>
    </body>
</html>
        )*", task_count, total_task_count), FOX_HTML_MIME_TYPE);
    }

    auto Gateway::handle_fetch(const httplib::Request& req, httplib::Response& res) noexcept -> void {
        spdlog::debug("Received fetch request");
        res.set_content("{}", FOX_JSON_MIME_TYPE);
    }

    auto Gateway::handle_endpoint(const httplib::Request& req, httplib::Response& res) noexcept -> void {
        spdlog::debug("Received endpoint request");
        res.set_content("{}", FOX_JSON_MIME_TYPE);
    }
}