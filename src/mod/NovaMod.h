#pragma once

#include <optional>

#include "mod/Config.h"

#include <pl/Mod.hpp>

namespace nova {

class NovaMod {
  public:
    static NovaMod &instance();

    NovaMod();

    [[nodiscard]] ll::mod::NativeMod &getSelf() const { return mSelf; }

    bool load();
    bool enable();
    bool disable();
    bool unload();

  private:
    ll::mod::NativeMod &mSelf;
    ModConfig mConfig;
    std::optional<pl::config::ConfigFile<ModConfig>> mConfigFile;
};

} // namespace nova
