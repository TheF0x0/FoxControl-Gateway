/**
 * @author F0x0
 * @since 05/04/2023
 */

#pragma once

#include <thread>
#include <string>
#include <kstd/types.hpp>
#include <atomic_queue/atomic_queue.h>

namespace fox {
    enum class Task : kstd::u8 {
        POWER_ON,
        POWER_OFF,
        SPEED_UP,
        SPEED_DOWN,
        CHANGE_MODE
    };

    class Gateway final {
        static atomic_queue::AtomicQueueB2<Task> _tasks;
        static std::atomic_size_t _total_task_count;

        std::string _address;
        std::string _endpoint;
        kstd::u32 _port;

        static auto handle_error(const httplib::Request& req, httplib::Response& res) noexcept -> void;

        static auto handle_status(const httplib::Request& req, httplib::Response& res) noexcept -> void;

        static auto handle_fetch(const httplib::Request& req, httplib::Response& res) noexcept -> void;

        static auto handle_endpoint(const httplib::Request& req, httplib::Response& res) noexcept -> void;

        public:

        Gateway(std::string address, std::string endpoint, kstd::u32 port) noexcept;

        static inline auto enqueue_task(Task task) noexcept -> void {
            _tasks.push(task);
            ++_total_task_count;
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
    };
}