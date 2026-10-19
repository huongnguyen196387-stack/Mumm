#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <pl/Mod.hpp>

#include "core/Module.h"
#include "core/Settings.h"

namespace nova {

class ModuleManager {
  public:
    static ModuleManager &get();

    void init(ll::mod::NativeMod &self);
    void add(std::unique_ptr<Module> module);

    // Đăng ký tất cả module "available()" vào Mod Menu của launcher và chạy luồng cập nhật HUD.
    void start();
    void stop();

  private:
    struct Entry {
        std::unique_ptr<Module> module;
        bool registered = false;
        bool needsClear = false;   // cần xoá draw command (vừa bị tắt)
        uint64_t lastHash = 0;     // hash draw command lần submit trước
        uint64_t lastEditorHash = 0;
    };

    void registerModule(Entry &entry);
    void applyToggle(const std::string &moduleId, bool enabled);
    void applyConfig(const std::string &moduleId, const std::string &key, const std::string &value);
    void loop();

    std::string schemaJson(const Module &m) const;

    ll::mod::NativeMod *mSelf = nullptr;
    Settings mSettings;
    std::vector<Entry> mEntries;
    std::unordered_map<std::string, size_t> mIndex;
    std::mutex mMutex;
    std::thread mThread;
    std::atomic<bool> mRunning{false};
};

} // namespace nova
