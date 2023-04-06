/**
 * @author TheF0x0
 * @since 06/04/2023
 */

#pragma once

#include <kstd/types.hpp>
#include <nlohmann/json.hpp>

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
            json["type"] = type;
            json["is_on"] = is_on;
        }

        inline auto deserialize(const nlohmann::json& json) noexcept -> void {
            type = static_cast<TaskType>(json["type"]);
            is_on = static_cast<bool>(json["is_on"]);
        }
    };

    struct SpeedTask final {
        TaskType type;
        kstd::i32 speed;

        inline auto serialize(nlohmann::json& json) noexcept -> void {
            json["type"] = type;
            json["speed"] = speed;
        }

        inline auto deserialize(const nlohmann::json& json) noexcept -> void {
            type = static_cast<TaskType>(json["type"]);
            speed = static_cast<kstd::i32>(json["speed"]);
        }
    };

    struct ModeTask final {
        TaskType type;
        Mode mode;

        inline auto serialize(nlohmann::json& json) noexcept -> void {
            json["type"] = type;
            json["mode"] = mode;
        }

        inline auto deserialize(const nlohmann::json& json) noexcept -> void {
            type = static_cast<TaskType>(json["type"]);
            mode = static_cast<Mode>(json["mode"]);
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
}