#include "game/GameHooks.h"

#include <algorithm>
#include <cctype>
#include <fstream>

#include <pl/Logger.hpp>
#include <pl/memory/Signature.hpp>

namespace nova {
namespace {

std::string trim(std::string s) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

} // namespace

GameHooks &GameHooks::get() {
    static GameHooks inst;
    return inst;
}

void GameHooks::load(const std::filesystem::path &iniFile) {
    mSigs.clear();
    std::ifstream in(iniFile);
    if (!in) return;
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string slot = trim(line.substr(0, eq));
        std::string sig = trim(line.substr(eq + 1));
        if (!slot.empty() && !sig.empty()) mSigs[slot].push_back(std::move(sig));
    }
}

uintptr_t GameHooks::resolve(const std::string &slot) const {
    auto it = mSigs.find(slot);
    if (it == mSigs.end()) return 0;
    for (const auto &sig : it->second) {
        if (uintptr_t addr = pl::memory::resolveSignature(sig, "libminecraftpe.so")) return addr;
    }
    return 0;
}

bool GameHooks::hookSlot(const std::string &slot, pl::memory::FuncPtr detour,
                         pl::memory::FuncPtr *original) {
    uintptr_t addr = resolve(slot);
    if (!addr) {
        preloaderLogger.warn("[nova] signature chưa resolve được cho slot '{}'", slot);
        return false;
    }
    if (pl::memory::hook(reinterpret_cast<pl::memory::FuncPtr>(addr), detour, original) != 0) {
        preloaderLogger.warn("[nova] cài hook thất bại cho slot '{}'", slot);
        return false;
    }
    mInstalled.push_back({addr, detour});
    return true;
}

void GameHooks::removeAll() {
    for (auto it = mInstalled.rbegin(); it != mInstalled.rend(); ++it)
        pl::memory::unhook(reinterpret_cast<pl::memory::FuncPtr>(it->target), it->detour);
    mInstalled.clear();
}

} // namespace nova
