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
#include <kstd/types.hpp>
#include <nlohmann/json.hpp>

namespace fox {
    enum class Task : kstd::u8 {
        POWER_ON,
        POWER_OFF,
        SPEED_UP,
        SPEED_DOWN,
        CHANGE_MODE
    };

    class Gateway final {
        static Gateway* s_instance;

        std::string _address;
        std::string _endpoint;
        kstd::u32 _port;
        kstd::u32 _backlog;
        std::string _password;

        std::vector<Task> _tasks;
        std::shared_mutex _mutex;
        std::atomic_size_t _total_task_count;
        std::atomic_size_t _total_processed_count;

        static auto handle_exception(const httplib::Request& req, httplib::Response& res, std::exception_ptr error_ptr) -> void;

        static auto handle_error(const httplib::Request& req, httplib::Response& res) -> void;

        static auto handle_status(const httplib::Request& req, httplib::Response& res) -> void;

        static auto handle_fetch(const httplib::Request& req, httplib::Response& res) -> void;

        static auto handle_endpoint(const httplib::Request& req, httplib::Response& res) -> void;

        [[nodiscard]] auto dequeue_and_compile() noexcept -> nlohmann::json;

        public:

        Gateway(std::string address, std::string endpoint, kstd::u32 port, kstd::u32 backlog, std::string password) noexcept;

        inline auto enqueue_task(Task task) noexcept -> bool {
            _mutex.lock();

            if (_tasks.size() >= _backlog) {
                _mutex.unlock();
                return false;
            }

            _tasks.push_back(task);
            _mutex.unlock();

            ++_total_task_count;
            return true;
        }

        inline auto dequeue_task() noexcept -> std::optional<Task> {
            _mutex.lock();
            const auto front = _tasks.begin();

            if (front == _tasks.end()) {
                _mutex.unlock();
                return std::nullopt;
            }

            const auto result = *front;
            _tasks.erase(front);
            _mutex.unlock();

            ++_total_processed_count;

            return {result};
        }

        [[nodiscard]] inline auto get_address() const noexcept -> const std::string& {
            return _address;
        }

        [[nodiscard]] inline auto get_endpoint() const noexcept -> const std::string& {
            return _endpoint;
        }

        [[nodiscard]] inline auto get_port() const noexcept -> kstd::u32 {
            return _port;
        }

        [[nodiscard]] inline auto get_backlog() const noexcept -> kstd::u32 {
            return _backlog;
        }
    };
}