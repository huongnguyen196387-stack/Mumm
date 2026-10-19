#pragma once

#include <pl/Logger.hpp>

#include "game/GameHooks.h"

namespace nova {

// Điểm mở rộng: cài hook vào libminecraftpe.so và đăng ký GameBridge::Provider.
// Trả true nếu provider đã được đăng ký.
bool installGameProvider(GameHooks &hooks, pl::log::Logger &log);

} // namespace nova
