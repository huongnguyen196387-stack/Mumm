#pragma once

#include "core/ModuleManager.h"

namespace nova {

// Tạo và đăng ký toàn bộ module của client vào ModuleManager.
void registerAllModules(ModuleManager &mgr);

} // namespace nova
