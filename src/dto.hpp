/**
 * @author TheF0x0
 * @since 06/04/2023
 */

#pragma once

#include <kstd/types.hpp>
#include <nlohmann/json.hpp>

#define FOX_JSON_SET(j, x) j[#x] = x
#define FOX_JSON_GET(j, x) x = j[#x]

namespace fox::dto {
    enum class TaskType : kstd::u8 {
        POWER,
        SPEED,
        MODE
    };

    enum class Mode : kstd::u8 {
        DEFAULT
    };

    struct PowerTask final {
        TaskType type;
        bool is_on;

        inline auto serialize(nlohmann::json& json) noexcept -> void {
            FOX_JSON_SET(json, type);
            FOX_JSON_SET(json, is_on);
        }

        inline auto deserialize(const nlohmann::json& json) noexcept -> void {
            FOX_JSON_GET(json, type);
            FOX_JSON_GET(json, is_on);
        }
    };

    struct SpeedTask final {
        TaskType type;
        kstd::i32 speed;

        inline auto serialize(nlohmann::json& json) noexcept -> void {
            FOX_JSON_SET(json, type);
            FOX_JSON_SET(json, speed);
        }

        inline auto deserialize(const nlohmann::json& json) noexcept -> void {
            FOX_JSON_GET(json, type);
            FOX_JSON_GET(json, speed);
        }
    };

    struct ModeTask final {
        TaskType type;
        Mode mode;

        inline auto serialize(nlohmann::json& json) noexcept -> void {
            FOX_JSON_SET(json, type);
            FOX_JSON_SET(json, mode);
        }

        inline auto deserialize(const nlohmann::json& json) noexcept -> void {
            FOX_JSON_GET(json, type);
            FOX_JSON_GET(json, mode);
        }
    };

    union Task {
        TaskType type;
        PowerTask power;
        SpeedTask speed;
        ModeTask mode;

        inline auto serialize(nlohmann::json& json) noexcept -> void {
            switch (type) {
                case TaskType::POWER:
                    power.serialize(json);
                    break;
                case TaskType::SPEED:
                    speed.serialize(json);
                    break;
                case TaskType::MODE:
                    mode.serialize(json);
                    break;
            }
        }

        inline auto deserialize(const nlohmann::json& json) noexcept -> void {
            switch (static_cast<TaskType>(json["type"])) {
                case TaskType::POWER:
                    power.deserialize(json);
                    break;
                case TaskType::SPEED:
                    speed.deserialize(json);
                    break;
                case TaskType::MODE:
                    mode.deserialize(json);
                    break;
            }
        }
    };

    struct DeviceState final {
        bool accepts_commands;
        bool is_on;
        kstd::u32 target_speed;
        kstd::u32 actual_speed;
        Mode mode;

        inline auto serialize(nlohmann::json& json) noexcept -> void {
            FOX_JSON_SET(json, accepts_commands);
            FOX_JSON_SET(json, is_on);
            FOX_JSON_SET(json, target_speed);
            FOX_JSON_SET(json, actual_speed);
            FOX_JSON_SET(json, mode);
        }

        inline auto deserialize(const nlohmann::json& json) noexcept -> void {
            FOX_JSON_GET(json, accepts_commands);
            FOX_JSON_GET(json, is_on);
            FOX_JSON_GET(json, target_speed);
            FOX_JSON_GET(json, actual_speed);
            FOX_JSON_GET(json, mode);
        }
    };
}