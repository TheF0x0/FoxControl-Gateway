/**
 * @author F0x0
 * @since 05/04/2023
 */

#pragma once

#include <thread>
#include <string>
#include <shared_mutex>
#include <vector>
#include <optional>
#include <functional>
#include <exception>
#include <kstd/types.hpp>
#include <nlohmann/json.hpp>
#include <parallel_hashmap/phmap.h>

#include "dto.hpp"

namespace fox {
    struct AuthenticationError final : public std::runtime_error {
        explicit AuthenticationError(const char* message) noexcept:
                std::runtime_error(message) {
        }
    };

    class Gateway final {
        static Gateway* s_instance;

        httplib::Server _server;

        std::string _address;
        kstd::u32 _port;
        kstd::u32 _backlog;

        std::string _password;
        std::shared_mutex _password_mutex;
        std::string _session_password;
        std::shared_mutex _session_password_mutex;

        std::atomic_bool _is_running;
        std::thread _command_thread;
        phmap::flat_hash_map<std::string, std::function<void()>> _commands;

        std::atomic_bool _is_online;
        std::vector<dto::Task> _tasks;
        std::shared_mutex _tasks_mutex;
        dto::DeviceState _state;
        std::shared_mutex _state_mutex;

        std::atomic_size_t _total_task_count;
        std::atomic_size_t _total_processed_count;

        static auto generate_password(kstd::usize length = 16) noexcept -> std::string;

        static auto send_error(httplib::Response& res, kstd::i32 status, const std::string_view& message) noexcept -> void;

        static auto validate_server_password(const nlohmann::json& json) -> bool;

        static auto validate_client_password(const nlohmann::json& json) -> bool;

        static auto command_loop(Gateway* self) noexcept -> void;

        static auto handle_error(const httplib::Request& req, httplib::Response& res) -> void;

        // Web endpoints

        static auto handle_status(const httplib::Request& req, httplib::Response& res) -> void;

        // Client endpoints

        static auto handle_authenticate(const httplib::Request& req, httplib::Response& res) -> void;

        static auto handle_getstate(const httplib::Request& req, httplib::Response& res) -> void;

        static auto handle_enqueue(const httplib::Request& req, httplib::Response& res) -> void;

        // Server endpoints

        static auto handle_fetch(const httplib::Request& req, httplib::Response& res) -> void;

        static auto handle_setonline(const httplib::Request& req, httplib::Response& res) -> void;

        static auto handle_setstate(const httplib::Request& req, httplib::Response& res) -> void;

        static auto handle_newsession(const httplib::Request& req, httplib::Response& res) -> void;

        [[nodiscard]] auto dequeue_and_compile() noexcept -> nlohmann::json;

        auto register_commands() noexcept -> void;

        auto run_server() noexcept -> void;

        public:

        Gateway(std::string address, kstd::u32 port, kstd::u32 backlog, std::string password) noexcept;

        ~Gateway() noexcept;

        inline auto enqueue_task(dto::Task task) noexcept -> bool {
            _tasks_mutex.lock();

            if (_tasks.size() >= _backlog) {
                _tasks_mutex.unlock();
                return false;
            }

            _tasks.push_back(task);
            _tasks_mutex.unlock();

            ++_total_task_count;
            return true;
        }

        inline auto dequeue_task() noexcept -> std::optional<dto::Task> {
            _tasks_mutex.lock();
            const auto front = _tasks.begin();

            if (front == _tasks.end()) {
                _tasks_mutex.unlock();
                return std::nullopt;
            }

            const auto result = *front;
            _tasks.erase(front);
            _tasks_mutex.unlock();

            ++_total_processed_count;

            return {result};
        }

        [[nodiscard]] inline auto get_address() const noexcept -> const std::string& {
            return _address;
        }

        [[nodiscard]] inline auto get_port() const noexcept -> kstd::u32 {
            return _port;
        }

        [[nodiscard]] inline auto get_backlog() const noexcept -> kstd::u32 {
            return _backlog;
        }
    };
}