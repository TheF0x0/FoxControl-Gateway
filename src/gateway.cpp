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
            _is_online(false),
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

        _server.Get("/status", handle_status);
        _server.Get("/getstate", handle_getstate);
        _server.Post("/authenticate", handle_authenticate);
        _server.Post("/fetch", handle_fetch);
        _server.Post("/setstate", handle_setstate);
        _server.Post("/setonline", handle_setonline);
        _server.Post(fmt::format("/{}", _endpoint), handle_enqueue);

        _server.set_default_headers({ // @formatter:off
            std::make_pair("Access-Control-Allow-Origin", "*"),
            std::make_pair("Access-Control-Allow-Methods", "*"),
            std::make_pair("Access-Control-Allow-Headers", "*")
        }); // @formatter:on

        spdlog::info("Listening on {}:{}/{}", _address, _port, _endpoint);
        _server.listen(_address, static_cast<kstd::i32>(_port)); // This will block
    }

    auto Gateway::send_error(httplib::Response& res, kstd::i32 status, const std::string_view& message) noexcept -> void {
        auto res_body = nlohmann::json::object();

        res_body["status"] = false;
        res_body["error"] = message;

        res.status = status;
        res.set_content(res_body.dump(), FOX_JSON_MIME_TYPE);
    }

    auto Gateway::validate_password(const nlohmann::json& json) -> bool {
        if (!json.contains("password")) {
            return false;
        }

        auto& self = *s_instance;
        const auto& password = static_cast<std::string>(json["password"]);

        if (password != self._password) {
            return false;
        }

        return true;
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

    auto Gateway::handle_error(const httplib::Request& req, httplib::Response& res) -> void {
        spdlog::warn("Received invalid request");

        res.status = 404;

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

    auto Gateway::handle_authenticate(const httplib::Request& req, httplib::Response& res) -> void {
        spdlog::warn("Received authenticate request");

        auto req_body = nlohmann::json::parse(req.body);

        auto res_body = nlohmann::json::object();
        res_body["status"] = validate_password(req_body);

        res.status = 200;
        res.set_content(res_body.dump(), FOX_JSON_MIME_TYPE);
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
                    <h3>Is Online: {}</h3>
                    <h3>Is On: {}</h3>
                    <h3>Target Speed: {}</h3>
                    <h3>Actual Speed: {}</h3>
                    <h3>Mode: {}</h3>
                </body>
            </html>
        )*", task_count, total_task_count, total_processed_count, self._is_online, is_on, target_speed, actual_speed, static_cast<kstd::u8>(mode)), FOX_HTML_MIME_TYPE);
    }

    auto Gateway::handle_fetch(const httplib::Request& req, httplib::Response& res) -> void {
        using namespace nlohmann::literals;
        spdlog::debug("Received fetch request");

        auto& self = *s_instance;
        auto req_body = nlohmann::json::parse(req.body);

        if (!req_body.is_object()) {
            send_error(res, 500, "Invalid request body type");
            return;
        }

        if (!validate_password(req_body)) {
            send_error(res, 401, "Invalid password");
            return;
        }

        auto res_body = nlohmann::json::object();
        res_body["tasks"] = self.dequeue_and_compile();

        res.status = 200;
        res.set_content(res_body.dump(), FOX_JSON_MIME_TYPE);
    }

    auto Gateway::handle_setonline(const httplib::Request& req, httplib::Response& res) -> void {
        auto req_body = nlohmann::json::parse(req.body);

        if (!req_body.is_object()) {
            send_error(res, 500, "Invalid request body type");
            return;
        }

        if (!validate_password(req_body)) {
            send_error(res, 401, "Invalid password");
            return;
        }

        if (!req_body.contains("is_online")) {
            send_error(res, 500, "Invalid property type");
            return;
        }

        auto& self = *s_instance;

        const auto previous_state = static_cast<bool>(self._is_online);
        const auto new_state = req_body["is_online"];

        auto res_body = nlohmann::json::object();
        res_body["status"] = new_state != previous_state;
        res_body["previous"] = previous_state;

        self._is_online = new_state;

        res.status = 200;
        res.set_content(res_body.dump(), FOX_JSON_MIME_TYPE);
    }

    auto Gateway::handle_setstate(const httplib::Request& req, httplib::Response& res) -> void {
        spdlog::debug("Received setstate request");

        auto& self = *s_instance;
        const auto req_body = nlohmann::json::parse(req.body);

        if (!req_body.is_object()) {
            send_error(res, 500, "Invalid request body type");
            return;
        }

        if (!validate_password(req_body)) {
            send_error(res, 401, "Invalid password");
            return;
        }

        if (!req_body.contains("state")) {
            send_error(res, 500, "Missing state object");
            return;
        }

        const auto& state_obj = req_body["state"];

        if (!state_obj.is_object()) {
            send_error(res, 500, "Invalid state object type");
            return;
        }

        self._state_mutex.lock();
        self._state.deserialize(state_obj);
        self._state_mutex.unlock();

        res.status = 200;
    }

    auto Gateway::handle_getstate(const httplib::Request& req, httplib::Response& res) -> void {
        spdlog::debug("Received getstate request");

        auto& self = *s_instance;

        auto res_body = nlohmann::json::object();
        self._state_mutex.lock_shared();
        self._state.serialize(res_body);
        self._state_mutex.unlock_shared();

        res_body["is_online"] = static_cast<bool>(self._is_online);

        res.status = 200;
        res.set_content(res_body.dump(), FOX_JSON_MIME_TYPE);
    }

    auto Gateway::handle_enqueue(const httplib::Request& req, httplib::Response& res) -> void {
        spdlog::debug("Received endpoint request");

        const auto req_body = nlohmann::json::parse(req.body);

        if (!req_body.is_object()) {
            send_error(res, 500, "Invalid request body type");
            return;
        }

        if (!validate_password(req_body)) {
            send_error(res, 401, "Invalid password");
            return;
        }

        if (!req_body.contains("tasks")) {
            send_error(res, 500, "Missing tasks list");
            return;
        }

        const auto& tasks = req_body["tasks"];

        if (!tasks.is_array()) {
            send_error(res, 500, "Invalid tasks list type");
            return;
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
        res.set_content(res_body.dump(), FOX_JSON_MIME_TYPE);
    }
}