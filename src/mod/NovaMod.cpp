#include "mod/NovaMod.h"

#include <filesystem>

#include "core/FrameClock.h"
#include "core/InputState.h"
#include "core/ModuleManager.h"
#include "game/GameHooks.h"
#include "game/GameProvider.h"
#include "modules/Modules.h"

namespace nova {

NovaMod &NovaMod::instance() {
    static NovaMod instance;
    return instance;
}

NovaMod::NovaMod() : mSelf(*ll::mod::NativeMod::current()) {}

bool NovaMod::load() {
    auto &self = getSelf();
    self.getLogger().debug("Loading...");

    std::error_code ec;
    for (const auto &dir : {self.getDataDir(), self.getConfigDir()}) {
        std::filesystem::create_directories(dir, ec);
        if (ec) {
            self.getLogger().error("Failed to create directory {}: {}", dir.string(), ec.message());
            return false;
        }
    }

    mConfigFile.emplace();
    if (!mConfigFile->load()) {
        self.getLogger().warn("Failed to load typed config");
        return false;
    }
    mConfig = mConfigFile->value();

    auto &mgr = ModuleManager::get();
    mgr.init(self);
    registerAllModules(mgr);
    GameHooks::get().load(self.getResourceDir() / "signatures.ini");

    self.getLogger().info("Loaded {} v{} ({} signature slot)", self.getName(), self.getVersion(),
                          GameHooks::get().slotCount());
    return true;
}

bool NovaMod::enable() {
    auto &self = getSelf();
    self.getLogger().debug("Enabling...");
    if (!mConfig.enabled) {
        self.getLogger().info("Nova Client is disabled by config");
        return true;
    }

    InputState::get().install();
    InputState::get().setActive(true);

    if (mConfig.fpsHook) {
        if (FrameClock::install()) self.getLogger().info("FPS hook (eglSwapBuffers) OK");
        else self.getLogger().warn("Không hook được eglSwapBuffers – FPS sẽ hiển thị \"--\"");
    }

    // Phải chạy trước start(): module cần dữ liệu game chỉ được đăng ký khi đã có provider.
    if (installGameProvider(GameHooks::get(), self.getLogger()))
        self.getLogger().info("Game provider đã sẵn sàng");

    ModuleManager::get().start();
    return true;
}

bool NovaMod::disable() {
    getSelf().getLogger().debug("Disabling...");
    ModuleManager::get().stop();
    InputState::get().setActive(false);
    FrameClock::uninstall();
    GameHooks::get().removeAll();
    return true;
}

bool NovaMod::unload() {
    getSelf().getLogger().debug("Unloading...");
    mConfigFile.reset();
    return true;
}

} // namespace nova
