/**
 * @author F0x0
 * @since 05/04/2023
 */

#include <ctime>
#include <sstream>
#include <httplib.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include "gateway.hpp"

#define FOX_HTML_MIME_TYPE "text/html"
#define FOX_JSON_MIME_TYPE "application/json"

namespace fox {
    Gateway* Gateway::s_instance = nullptr;

    Gateway::Gateway(std::string address, kstd::u32 port, kstd::u32 backlog, std::string password) noexcept:
            _address(std::move(address)),
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

        // Web endpoints
        _server.Get("/status", handle_status);

        // Client endpoints
        _server.Post("/getstate", handle_getstate);
        _server.Post("/authenticate", handle_authenticate);
        _server.Post("/enqueue", handle_enqueue);

        // Server endpoints
        _server.Post("/fetch", handle_fetch);
        _server.Post("/setstate", handle_setstate);
        _server.Post("/setonline", handle_setonline);
        _server.Post("/newsession", handle_newsession);

        _server.set_default_headers({ // @formatter:off
            std::make_pair("Access-Control-Allow-Origin", "*"),
            std::make_pair("Access-Control-Allow-Methods", "*"),
            std::make_pair("Access-Control-Allow-Headers", "*"),
            std::make_pair("Cache-Control", "private,max-age=0") // https://developers.cloudflare.com/cache/about/cache-control/
        }); // @formatter:on

        spdlog::info("Listening on {}:{}", _address, _port);
        _server.listen(_address, static_cast<kstd::i32>(_port)); // This will block
    }

    auto Gateway::generate_password(kstd::usize length) noexcept -> std::string {
        constexpr std::string_view allowed_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789,.-_/()#+!?";
        constexpr auto num_allowed_chars = allowed_chars.size();

        std::random_device device;
        std::mt19937 generator(device());
        std::uniform_int_distribution<kstd::usize> dist(0, num_allowed_chars - 1);
        std::string result;
        result.resize(length);

        for(kstd::usize i = 0; i < length; i++) {
            result[i] = allowed_chars[dist(generator)];
        }

        return result;
    }

    auto Gateway::send_error(httplib::Response& res, kstd::i32 status, const std::string_view& message) noexcept -> void {
        auto res_body = nlohmann::json::object();

        res_body["status"] = false;
        res_body["error"] = message;
        res_body["timestamp"] = static_cast<kstd::u64>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());

        res.status = status;
        res.set_content(res_body.dump(), FOX_JSON_MIME_TYPE);
    }

    auto Gateway::validate_server_password(const nlohmann::json& json) -> bool {
        if (!json.contains("password")) {
            return false;
        }

        auto& self = *s_instance;
        const auto& password = static_cast<std::string>(json["password"]);

        self._password_mutex.lock_shared();

        if (password.empty() || password != self._password) {
            self._password_mutex.unlock_shared();
            return false;
        }

        self._password_mutex.unlock_shared();
        return true;
    }

    auto Gateway::validate_client_password(const nlohmann::json& json) -> bool {
        if (!json.contains("password")) {
            return false;
        }

        auto& self = *s_instance;
        const auto& password = static_cast<std::string>(json["password"]);

        self._session_password_mutex.lock_shared();
        const auto& session_password = self._session_password;

        if (password.empty() || session_password.empty() || password != session_password) {
            self._session_password_mutex.unlock_shared();
            return false;
        }

        self._session_password_mutex.unlock_shared();
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

    // Web endpoints

    auto Gateway::handle_status(const httplib::Request& req, httplib::Response& res) -> void {
        spdlog::debug("Received status request");
        auto& self = *s_instance;

        self._tasks_mutex.lock_shared();
        const auto task_count = self._tasks.size();
        self._tasks_mutex.unlock_shared();

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
                </body>
            </html>
        )*", task_count, total_task_count, total_processed_count), FOX_HTML_MIME_TYPE);
    }

    // Client endpoints

    auto Gateway::handle_authenticate(const httplib::Request& req, httplib::Response& res) -> void {
        spdlog::debug("Received authenticate request");

        auto req_body = nlohmann::json::parse(req.body);

        auto res_body = nlohmann::json::object();
        const auto result = validate_client_password(req_body);
        res_body["status"] = result;
        res_body["timestamp"] = static_cast<kstd::u64>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());

        res.status = 200;
        res.set_content(res_body.dump(), FOX_JSON_MIME_TYPE);
    }

    auto Gateway::handle_getstate(const httplib::Request& req, httplib::Response& res) -> void {
        spdlog::debug("Received getstate request");

        auto& self = *s_instance;
        const auto req_body = nlohmann::json::parse(req.body);

        if (!req_body.is_object()) {
            send_error(res, 500, "Invalid request body type");
            return;
        }

        if (!validate_client_password(req_body)) {
            send_error(res, 401, "Invalid password");
            return;
        }

        auto res_body = nlohmann::json::object();
        self._state_mutex.lock_shared();
        self._state.serialize(res_body);
        self._state_mutex.unlock_shared();

        res_body["is_online"] = static_cast<bool>(self._is_online);
        res_body["timestamp"] = static_cast<kstd::u64>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());

        res.status = 200;
        res.set_content(res_body.dump(), FOX_JSON_MIME_TYPE);
    }

    auto Gateway::handle_enqueue(const httplib::Request& req, httplib::Response& res) -> void {
        spdlog::debug("Received endpoint request");

        auto& self = *s_instance;
        const auto req_body = nlohmann::json::parse(req.body);

        if (!req_body.is_object()) {
            send_error(res, 500, "Invalid request body type");
            return;
        }

        if (!validate_client_password(req_body)) {
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
        res_body["timestamp"] = static_cast<kstd::u64>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());

        res.status = 200;
        res.set_content(res_body.dump(), FOX_JSON_MIME_TYPE);
    }

    // Server endpoints

    auto Gateway::handle_fetch(const httplib::Request& req, httplib::Response& res) -> void {
        using namespace nlohmann::literals;
        spdlog::debug("Received fetch request");

        auto& self = *s_instance;
        auto req_body = nlohmann::json::parse(req.body);

        if (!req_body.is_object()) {
            send_error(res, 500, "Invalid request body type");
            return;
        }

        if (!validate_server_password(req_body)) {
            send_error(res, 401, "Invalid password");
            return;
        }

        auto res_body = nlohmann::json::object();
        res_body["tasks"] = self.dequeue_and_compile();
        res_body["timestamp"] = static_cast<kstd::u64>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());

        res.status = 200;
        res.set_content(res_body.dump(), FOX_JSON_MIME_TYPE);
    }

    auto Gateway::handle_setonline(const httplib::Request& req, httplib::Response& res) -> void {
        auto req_body = nlohmann::json::parse(req.body);

        if (!req_body.is_object()) {
            send_error(res, 500, "Invalid request body type");
            return;
        }

        if (!validate_server_password(req_body)) {
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

        if (!new_state) { // Reset active session on disconnect
            self._session_password_mutex.lock();
            self._session_password = "";
            self._session_password_mutex.unlock();
        }

        auto res_body = nlohmann::json::object();
        res_body["status"] = new_state != previous_state;
        res_body["previous"] = previous_state;
        res_body["timestamp"] = static_cast<kstd::u64>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());

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

        if (!validate_server_password(req_body)) {
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

    auto Gateway::handle_newsession(const httplib::Request& req, httplib::Response& res) -> void {
        spdlog::debug("Received reset password request");

        auto& self = *s_instance;
        self._session_password_mutex.lock_shared();

        if (!self._session_password.empty()) {
            send_error(res, 401, "Session already in progress");
            return;
        }

        self._session_password_mutex.unlock_shared();

        const auto req_body = nlohmann::json::parse(req.body);

        if (!req_body.is_object()) {
            send_error(res, 500, "Invalid request body type");
            return;
        }

        if (!validate_server_password(req_body)) {
            send_error(res, 401, "Invalid password");
            return;
        }

        std::string session_password;

        if (req_body.contains("new_password")) {
            // Allow specifying a new password in the request body
            session_password = req_body["new_password"];
        }
        else {
            // Otherwise generate a random password
            kstd::usize length = 16;

            if (req_body.contains("length")) {
                length = req_body["length"];

                if (length < 10) {
                    send_error(res, 500, "Invalid password length, needs to be at least 10 characters");
                    return;
                }
            }

            session_password = generate_password(length);
        }

        auto res_body = nlohmann::json::object();
        res_body["password"] = session_password;
        res_body["timestamp"] = static_cast<kstd::u64>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());

        self._session_password_mutex.lock();
        self._session_password = session_password;
        self._session_password_mutex.unlock();

        res.status = 200;
        res.set_content(res_body.dump(), FOX_JSON_MIME_TYPE);
    }
}