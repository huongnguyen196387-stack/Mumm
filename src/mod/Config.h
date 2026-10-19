#pragma once

#include <string>
#include <string_view>

#include <pl/Config.hpp>

namespace nova {

// Cấu hình cấp mod (config/config.json). Tuỳ chọn của từng tính năng nằm trong Mod Menu
// và được lưu ở config/settings.ini.
struct ModConfig {
    int version = 1;
    bool enabled = true;
    bool fpsHook = true;
};

nlohmann::json makeDefaultConfigJson();
nlohmann::json makeConfigSchemaJson();

} // namespace nova

template <> struct pl::config::Schema<nova::ModConfig> {
    static constexpr std::string_view title = "Nova Client Config";
    static constexpr std::string_view description = {};

    static constexpr FieldSchema field(std::string_view name) {
        if (name == "version")
            return {.title = "Version", .readOnly = true};
        if (name == "enabled")
            return {.title = "Enabled", .description = "Bật/tắt toàn bộ Nova Client."};
        if (name == "fpsHook")
            return {.title = "FPS hook",
                    .description = "Hook eglSwapBuffers để đếm FPS thật. Tắt nếu game bị crash khi khởi động."};
        return {};
    }
};
