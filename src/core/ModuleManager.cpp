#include "core/ModuleManager.h"

#include <chrono>
#include <cstring>

#include <pl/ModMenu.hpp>

#include "core/FrameClock.h"
#include "core/InputState.h"

namespace nova {
namespace {

uint64_t fnv(uint64_t h, const void *data, size_t n) {
    const auto *p = static_cast<const unsigned char *>(data);
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    return h;
}

uint64_t hashCommands(const std::vector<pl::modmenu::DrawCommand> &cmds) {
    uint64_t h = 1469598103934665603ull;
    for (const auto &c : cmds) {
        int t = static_cast<int>(c.type);
        h = fnv(h, &t, sizeof t);
        float f[7] = {c.x, c.y, c.w, c.h, c.x3, c.y3, c.size};
        h = fnv(h, f, sizeof f);
        h = fnv(h, &c.color, sizeof c.color);
        h = fnv(h, c.text.data(), c.text.size());
    }
    return h;
}

const char *categoryTitle(const std::string &id) {
    if (id == "appearance") return "Giao diện";
    if (id == "position") return "Vị trí";
    return "Chung";
}

} // namespace

ModuleManager &ModuleManager::get() {
    static ModuleManager inst;
    return inst;
}

void ModuleManager::init(ll::mod::NativeMod &self) {
    mSelf = &self;
    mSettings.load(self.getConfigDir() / "settings.ini");
}

void ModuleManager::add(std::unique_ptr<Module> module) {
    std::lock_guard lock(mMutex);
    mIndex[module->id()] = mEntries.size();
    mEntries.push_back(Entry{std::move(module)});
}

std::string ModuleManager::schemaJson(const Module &m) const {
    using namespace pl::modmenu;
    ConfigSchemaBuilder b;
    b.defaultCategory("general");

    std::vector<std::string> seen;
    for (const auto &o : m.options()) {
        if (std::find(seen.begin(), seen.end(), o.category) == seen.end()) seen.push_back(o.category);
    }
    // "general" luôn đứng đầu nếu có.
    std::stable_partition(seen.begin(), seen.end(), [](const std::string &c) { return c == "general"; });
    for (const auto &c : seen) b.category(c, categoryTitle(c));

    for (const auto &o : m.options()) {
        ConfigNodeV2 n;
        n.id = m.id() + "." + o.key;
        n.key = o.key;
        n.category = o.category;
        n.title = o.title;
        n.description = o.desc;
        n.type = o.type;
        n.choiceStyle = o.style;
        n.defaultValue = o.def;
        n.currentValue = m.str(o.key);
        n.hasCurrentValue = true;
        n.minValue = o.min;
        n.maxValue = o.max;
        n.step = o.step;
        n.unit = o.unit;
        n.advanced = o.advanced;
        n.colorAlpha = o.colorAlpha;
        for (const auto &c : o.choices) {
            ConfigOptionV2 opt;
            opt.value = c.value;
            opt.label = c.label;
            n.options.push_back(std::move(opt));
        }
        if (!o.visibleKey.empty()) {
            n.visibleWhen.push_back({o.visibleKey, ConfigConditionOpV2::Equals, o.visibleValue});
        }
        b.node(std::move(n));
    }
    return b.toJson();
}

void ModuleManager::registerModule(Entry &entry) {
    Module &m = *entry.module;

    // Nạp giá trị đã lưu (nếu có) trước khi dựng schema để menu hiển thị đúng.
    for (const auto &o : m.options()) {
        if (auto v = mSettings.get(m.id(), o.key)) m.setValue(o.key, *v);
    }
    bool enabled = parseBool(mSettings.get(m.id(), "@enabled").value_or(m.defaultEnabled() ? "true" : "false"),
                             m.defaultEnabled());
    m.setEnabledInternal(enabled);

    const std::string id = m.id();
    pl::modmenu::ModuleBuilder builder(id, m.name());
    builder.description(m.description())
        .modId(mSelf->getId())
        .defaultEnabled(enabled)
        .hideInHudEditor(!m.hasHud())
        .onToggle([this](std::string_view mid, bool on) { applyToggle(std::string(mid), on); })
        .onConfigChanged([this](std::string_view mid, std::string_view key, std::string_view value) {
            applyConfig(std::string(mid), std::string(key), std::string(value));
        });

    if (!builder.registerModule()) {
        mSelf->getLogger().warn("Không đăng ký được module {}", id);
        return;
    }
    pl::modmenu::setConfigSchemaJson(id, schemaJson(m));
    entry.registered = true;
    m.registerExtras();
    if (enabled) m.onEnable();
}

void ModuleManager::applyToggle(const std::string &moduleId, bool enabled) {
    std::lock_guard lock(mMutex);
    auto it = mIndex.find(moduleId);
    if (it == mIndex.end()) return;
    Entry &e = mEntries[it->second];
    if (e.module->enabled() == enabled) return;
    e.module->setEnabledInternal(enabled);
    mSettings.set(moduleId, "@enabled", enabled ? "true" : "false");
    if (enabled) {
        e.module->onEnable();
    } else {
        e.module->onDisable();
        e.needsClear = true; // luồng HUD sẽ xoá draw command (không gọi API launcher trong callback)
    }
}

void ModuleManager::applyConfig(const std::string &moduleId, const std::string &key,
                                const std::string &value) {
    std::lock_guard lock(mMutex);
    auto it = mIndex.find(moduleId);
    if (it == mIndex.end()) return;
    Module &m = *mEntries[it->second].module;
    m.setValue(key, value);
    mSettings.set(moduleId, key, value);
    m.onConfigChanged(key);
}

void ModuleManager::start() {
    {
        std::lock_guard lock(mMutex);
        for (auto &e : mEntries) {
            if (!e.module->available()) {
                mSelf->getLogger().info("Bỏ qua module '{}' (backend chưa sẵn sàng)", e.module->id());
                continue;
            }
            registerModule(e);
        }
    }
    if (mRunning.exchange(true)) return;
    mThread = std::thread([this] { loop(); });
}

void ModuleManager::stop() {
    if (mRunning.exchange(false) && mThread.joinable()) mThread.join();

    std::vector<std::string> ids;
    {
        std::lock_guard lock(mMutex);
        for (auto &e : mEntries) {
            if (!e.registered) continue;
            if (e.module->enabled()) e.module->onDisable();
            e.module->unregisterExtras();
            ids.push_back(e.module->id());
            e.registered = false;
        }
    }
    for (const auto &id : ids) pl::modmenu::unregisterModule(id);
    mSettings.flushIfDirty();
}

void ModuleManager::loop() {
    using clock = std::chrono::steady_clock;
    auto last = clock::now();
    auto lastFlush = last;

    while (mRunning.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
        auto now = clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;

        auto surf = pl::modmenu::getHudSurfaceSize();
        HudContext ctx;
        if (surf.width > 100.f && surf.height > 100.f) {
            ctx.width = surf.width;
            ctx.height = surf.height;
        }
        ctx.scale = ctx.height / 1080.f;
        InputState::get().setSurface(ctx.width, ctx.height);
        FrameClock::update(std::chrono::duration_cast<std::chrono::milliseconds>(
                               now.time_since_epoch()).count());

        struct Out {
            std::string id;
            std::string name;
            std::vector<pl::modmenu::DrawCommand> cmds;
            bool submitCmds = false;
            bool hasEditor = false;
            bool submitEditor = false;
            pl::modmenu::HudEditorElement editor;
        };
        std::vector<Out> outs;

        {
            std::lock_guard lock(mMutex);
            for (auto &e : mEntries) {
                if (!e.registered) continue;
                Module &m = *e.module;
                Out o;
                o.id = m.id();
                o.name = m.name();

                if (e.needsClear) {
                    e.needsClear = false;
                    e.lastHash = 0;
                    o.submitCmds = true; // cmds rỗng => xoá overlay
                    outs.push_back(std::move(o));
                    continue;
                }
                if (!m.enabled()) continue;

                m.onTick(dt);
                Hud hud;
                m.onRender(hud, ctx);

                uint64_t h = hashCommands(hud.commands());
                if (h != e.lastHash) {
                    e.lastHash = h;
                    o.cmds = hud.commands();
                    o.submitCmds = true;
                }
                if (m.hasHud() && m.boundsW() > 0.f) {
                    o.hasEditor = true;
                    o.editor.elementId = m.id();
                    o.editor.displayName = m.name();
                    o.editor.x = m.posX();
                    o.editor.y = m.posY();
                    o.editor.width = m.boundsW();
                    o.editor.height = m.boundsH();
                    o.editor.snapThreshold = 12.f;
                    o.editor.gridGap = 4.f;
                    o.editor.snapFlags = pl::modmenu::HudSnapGrid | pl::modmenu::HudSnapElements |
                                         pl::modmenu::HudSnapScreenCenter;
                    float key[4] = {o.editor.x, o.editor.y, o.editor.width, o.editor.height};
                    uint64_t eh = fnv(1469598103934665603ull, key, sizeof key);
                    if (eh != e.lastEditorHash) {
                        e.lastEditorHash = eh;
                        o.submitEditor = true;
                    }
                }
                if (o.submitCmds || o.submitEditor) outs.push_back(std::move(o));
            }
        }

        // Gọi API launcher NGOÀI lock để tránh deadlock với callback onToggle/onConfigChanged.
        for (auto &o : outs) {
            if (o.submitCmds) pl::modmenu::submitDrawCommands(o.id, o.cmds);
            if (o.submitEditor) {
                pl::modmenu::HudEditorElement el = o.editor;
                pl::modmenu::submitHudEditorElements(o.id, std::span(&el, 1));
            }
        }

        if (now - lastFlush > std::chrono::seconds(2)) {
            lastFlush = now;
            mSettings.flushIfDirty();
        }
    }
}

} // namespace nova
