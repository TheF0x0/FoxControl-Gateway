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
    Gateway* Gateway::s_instance = nullptr;

    Gateway::Gateway(std::string address, std::string endpoint, kstd::u32 port, kstd::u32 backlog, std::string password) noexcept:
            _address(std::move(address)),
            _endpoint(std::move(endpoint)),
            _port(port),
            _backlog(backlog),
            _password(std::move(password)),
            _is_running(true),
            _state(),
            _total_task_count(0),
            _total_processed_count(0) {
        s_instance = this;

        register_commands();
        _command_thread = std::thread(command_loop, this);
        run_server();
    }

    Gateway::~Gateway() noexcept {
        _is_running = false;
        _command_thread.join();
    }

    auto Gateway::dequeue_and_compile() noexcept -> nlohmann::json {
        _tasks_mutex.lock_shared();
        const auto task_count = _tasks.size();
        _tasks_mutex.unlock_shared();

        auto array = nlohmann::json::array();

        for (size_t i = 0; i < task_count; ++i) {
            auto task = nlohmann::json::object();
            dequeue_task()->serialize(task);
            array.push_back(task);
        }

        return array;
    }

    auto Gateway::register_commands() noexcept -> void {
        _commands["help"] = [this] {
            for (const auto& pair: _commands) {
                spdlog::info(pair.first);
            }
        };

        _commands["exit"] = [this] {
            spdlog::info("Shutting down gracefully");
            _is_running = false;
            _server.stop();
        };

        _commands["clear"] = [this] {
            spdlog::info("Clearing task queue");
            _tasks_mutex.lock();
            _tasks.clear();
            _tasks_mutex.unlock();
        };

        _commands["info"] = [this] {
            _tasks_mutex.lock_shared();
            spdlog::info("{} tasks queued in total", _tasks.size());
            _tasks_mutex.unlock_shared();

            spdlog::info("{} tasks in total", _total_task_count);
            spdlog::info("{} tasks processed", _total_processed_count);
        };
    }

    auto Gateway::run_server() noexcept -> void {
        spdlog::info("Starting HTTP server");

        _server.set_error_handler(handle_error);
        _server.set_exception_handler(handle_exception);

        _server.Get("/status", handle_status);
        _server.Get("/fetch", handle_fetch);
        _server.Get("/getstate", handle_getstate);
        _server.Post("/setstate", handle_setstate);
        _server.Post(fmt::format("/{}", _endpoint), handle_endpoint);

        spdlog::info("Listening on {}:{}/{}", _address, _port, _endpoint);
        _server.listen(_address, static_cast<kstd::i32>(_port)); // This will block
    }

    auto Gateway::validate_password(const nlohmann::json& json) -> void {
        if (!json.contains("password")) {
            throw std::runtime_error("Missing password");
        }

        auto& self = *s_instance;
        const auto& password = static_cast<std::string>(json["password"]);

        if (password != self._password) {
            throw AuthenticationError("Invalid password");
        }
    }

    auto Gateway::command_loop(Gateway* self) noexcept -> void {
        using namespace std::chrono_literals;

        spdlog::info("Starting command thread");
        std::string command;

        while (self->_is_running) {
            std::getline(std::cin, command);

            if (command.empty()) {
                continue;
            }

            const auto itr = self->_commands.find(command);

            if (itr == self->_commands.end()) {
                spdlog::info("Unrecognized command, try help");
                continue;
            }

            itr->second();
        }

        spdlog::info("Stopping command thread");
    }

    auto Gateway::handle_exception(const httplib::Request& req, httplib::Response& res, std::exception_ptr error_ptr) -> void {
        spdlog::error("Request caused an exception");
        std::string error_message = "Unknown error";
        bool invalid_password = false;

        try {
            std::rethrow_exception(error_ptr); // NOLINT
        }
        catch (const AuthenticationError& error) {
            error_message = error.what();
            invalid_password = true;
        }
        catch (const std::exception& error) {
            error_message = error.what();
        }
        catch (...) { /* Need to cover this */ }

        res.status = invalid_password ? 401 : 500;
        res.set_header("Access-Control-Allow-Origin", "*");

        res.set_content(fmt::format(R"*(
            <html lang="en">
                <head>
                    <title>🦊 Oops..</title>
                    <meta charset="UTF-8" />
                </head>
                <body>
                    <h1>Something broke 🦊</h1>
                    <h3>Something went horribly wrong while processing your request.<h3>
                    {}
                </body>
            </html>
        )*", error_message), FOX_HTML_MIME_TYPE);
    }

    auto Gateway::handle_error(const httplib::Request& req, httplib::Response& res) -> void {
        spdlog::warn("Received invalid request");

        res.status = 404;
        res.set_header("Access-Control-Allow-Origin", "*");

        res.set_content(R"*(
            <html lang="en">
                <head>
                    <title>🦊 Oops..</title>
                    <meta charset="UTF-8" />
                </head>
                <body>
                    <h1>Nothing here but us foxes 🦊</h1>
                    <h3>This is not the page you were looking for.</h3>
                </body>
            </html>
        )*", FOX_HTML_MIME_TYPE);
    }

    auto Gateway::handle_status(const httplib::Request& req, httplib::Response& res) -> void {
        spdlog::debug("Received status request");
        auto& self = *s_instance;

        self._tasks_mutex.lock_shared();
        const auto task_count = self._tasks.size();
        self._tasks_mutex.unlock_shared();

        self._state_mutex.lock_shared();
        const auto is_on = self._state.is_on;
        const auto target_speed = self._state.target_speed;
        const auto actual_speed = self._state.actual_speed;
        const auto mode = self._state.mode;
        self._state_mutex.unlock_shared();

        const auto total_task_count = static_cast<size_t>(self._total_task_count);
        const auto total_processed_count = static_cast<size_t>(self._total_processed_count);

        res.status = 200;
        res.set_header("Access-Control-Allow-Origin", "*");

        res.set_content(fmt::format(R"*(
            <html lang="en">
                <head>
                    <title>🦊 Status</title>
                    <meta charset="UTF-8" />
                </head>
                <body>
                    <h1>🦊 Status</h1>
                    <hr>
                    <h2>Task Queue</h2>
                    <h3>Queued Tasks: {}</h3>
                    <h3>Total Tasks: {}</h3>
                    <h3>Total Processed: {}</h3>
                    <hr>
                    <h2>Device State</h2>
                    <h3>Is On: {}</h3>
                    <h3>Target Speed: {}</h3>
                    <h3>Actual Speed: {}</h3>
                    <h3>Mode: {}</h3>
                </body>
            </html>
        )*", task_count, total_task_count, total_processed_count, is_on, target_speed, actual_speed, static_cast<kstd::u8>(mode)), FOX_HTML_MIME_TYPE);
    }

    auto Gateway::handle_fetch(const httplib::Request& req, httplib::Response& res) -> void {
        using namespace nlohmann::literals;
        spdlog::debug("Received fetch request");

        auto& self = *s_instance;
        auto res_body = nlohmann::json::object();
        res_body["tasks"] = self.dequeue_and_compile();

        res.status = 200;
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(res_body.dump(), FOX_JSON_MIME_TYPE);
    }

    auto Gateway::handle_setstate(const httplib::Request& req, httplib::Response& res) -> void {
        spdlog::debug("Received setstate request");

        auto& self = *s_instance;
        const auto req_body = nlohmann::json::parse(req.body);

        if (!req_body.is_object()) {
            throw std::runtime_error("Invalid request body type");
        }

        validate_password(req_body);

        if (!req_body.contains("state")) {
            throw std::runtime_error("Missing state object");
        }

        const auto& state_obj = req_body["state"];

        if (!state_obj.is_object()) {
            throw std::runtime_error("Invalid state object type");
        }

        self._state_mutex.lock();
        self._state.deserialize(state_obj);
        self._state_mutex.unlock();

        res.status = 200;
        res.set_header("Access-Control-Allow-Origin", "*");
    }

    auto Gateway::handle_getstate(const httplib::Request& req, httplib::Response& res) -> void {
        spdlog::debug("Received getstate request");

        auto& self = *s_instance;

        auto res_body = nlohmann::json::object();
        self._state_mutex.lock_shared();
        self._state.serialize(res_body);
        self._state_mutex.unlock_shared();

        res.status = 200;
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(res_body.dump(), FOX_JSON_MIME_TYPE);
    }

    auto Gateway::handle_endpoint(const httplib::Request& req, httplib::Response& res) -> void {
        spdlog::debug("Received endpoint request");

        const auto req_body = nlohmann::json::parse(req.body);

        if (!req_body.is_object()) {
            throw std::runtime_error("Invalid request body type");
        }

        validate_password(req_body);

        if (!req_body.contains("tasks")) {
            throw std::runtime_error("Missing tasks object");
        }

        const auto& tasks = req_body["tasks"];

        if (!tasks.is_array()) {
            throw std::runtime_error("Invalid task list type");
        }

        auto& self = *s_instance;
        size_t queued_count = 0;

        for (const auto& task: tasks) {
            if (!task.is_object() || !task.contains("type")) {
                continue;
            }

            dto::Task task_dto{};
            task_dto.deserialize(task);

            if (self.enqueue_task(task_dto)) {
                spdlog::debug("Enqueued task");
                ++queued_count;
            }
        }

        auto res_body = nlohmann::json::object();
        res_body["status"] = queued_count == tasks.size();
        res_body["queued"] = queued_count;

        res.status = 200;
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(res_body.dump(), FOX_JSON_MIME_TYPE);
    }
}