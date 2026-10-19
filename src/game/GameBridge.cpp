#include "game/GameBridge.h"

namespace nova {

GameBridge &GameBridge::get() {
    static GameBridge inst;
    return inst;
}

void GameBridge::setProvider(Provider p) {
    std::lock_guard lock(mMutex);
    mProvider = std::move(p);
}

bool GameBridge::hasProvider() const {
    std::lock_guard lock(mMutex);
    return static_cast<bool>(mProvider);
}

bool GameBridge::snapshot(GameState &out) const {
    Provider p;
    {
        std::lock_guard lock(mMutex);
        p = mProvider;
    }
    return p ? p(out) : false;
}

} // namespace nova
