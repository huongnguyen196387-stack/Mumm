#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <pl/ModMenu.hpp>

#include "core/Common.h"
#include "core/Hud.h"

namespace nova {

using pl::modmenu::ConfigChoiceStyleV2;
using pl::modmenu::ConfigControlTypeV2;

struct Choice {
    std::string value;
    std::string label;
};

// Mô tả một tuỳ chọn trong menu cấu hình của module.
struct Opt {
    std::string key;
    std::string title;
    std::string desc;
    std::string category = "general"; // general | appearance | position
    ConfigControlTypeV2 type = ConfigControlTypeV2::Toggle;
    ConfigChoiceStyleV2 style = ConfigChoiceStyleV2::Auto;
    std::string def;
    std::string min, max, step, unit;
    std::vector<Choice> choices;
    bool advanced = false;
    bool colorAlpha = true;
    std::string visibleKey;   // chỉ hiện khi visibleKey == visibleValue (tuỳ chọn)
    std::string visibleValue;
};

// Lớp cơ sở của mọi tính năng. Mọi hàm virtual được gọi khi ModuleManager đang giữ lock.
class Module {
  public:
    Module(std::string id, std::string name, std::string description)
        : mId(std::move(id)), mName(std::move(name)), mDescription(std::move(description)) {}
    virtual ~Module() = default;

    [[nodiscard]] const std::string &id() const { return mId; }
    [[nodiscard]] const std::string &name() const { return mName; }
    [[nodiscard]] const std::string &description() const { return mDescription; }
    [[nodiscard]] bool enabled() const { return mEnabled; }
    [[nodiscard]] bool hasHud() const { return mHasHud; }
    [[nodiscard]] const std::vector<Opt> &options() const { return mOpts; }
    [[nodiscard]] bool defaultEnabled() const { return mDefaultEnabled; }

    // false => module không được đăng ký vào menu (thiếu backend, ví dụ chưa có hook game).
    [[nodiscard]] virtual bool available() const { return true; }

    virtual void onEnable() {}
    virtual void onDisable() {}
    virtual void onTick(float /*dtSeconds*/) {}
    virtual void onRender(Hud & /*hud*/, const HudContext & /*ctx*/) {}
    virtual void onConfigChanged(const std::string & /*key*/) {}
    // Module tự đăng ký nút on-screen... (gọi một lần sau khi menu đã đăng ký)
    virtual void registerExtras() {}
    virtual void unregisterExtras() {}

    // --- config ---
    [[nodiscard]] std::string str(const std::string &key) const {
        auto it = mValues.find(key);
        if (it != mValues.end()) return it->second;
        for (const auto &o : mOpts)
            if (o.key == key) return o.def;
        return {};
    }
    [[nodiscard]] bool flag(const std::string &key) const { return parseBool(str(key), false); }
    [[nodiscard]] int integer(const std::string &key) const { return parseInt(str(key), 0); }
    [[nodiscard]] float number(const std::string &key) const { return parseFloat(str(key), 0.f); }
    [[nodiscard]] uint32_t color(const std::string &key) const {
        return parseColor(str(key), 0xFFFFFFFFu);
    }
    void setValue(const std::string &key, std::string value) { mValues[key] = std::move(value); }
    void setEnabledInternal(bool v) { mEnabled = v; }

    // Vị trí/kích thước HUD cuối cùng module báo cho HUD editor.
    [[nodiscard]] float boundsW() const { return mBoundsW; }
    [[nodiscard]] float boundsH() const { return mBoundsH; }
    [[nodiscard]] float posX() const { return number("hudPosX"); }
    [[nodiscard]] float posY() const { return number("hudPosY"); }

  protected:
    Opt &add(Opt o) {
        mOpts.push_back(std::move(o));
        return mOpts.back();
    }

    Opt &addToggle(std::string key, std::string title, bool def, std::string desc = {}) {
        Opt o;
        o.key = std::move(key);
        o.title = std::move(title);
        o.desc = std::move(desc);
        o.def = def ? "true" : "false";
        return add(std::move(o));
    }

    Opt &addInt(std::string key, std::string title, int def, int lo, int hi, int step = 1,
                std::string unit = {}) {
        Opt o;
        o.key = std::move(key);
        o.title = std::move(title);
        o.type = ConfigControlTypeV2::SliderInt;
        o.def = std::to_string(def);
        o.min = std::to_string(lo);
        o.max = std::to_string(hi);
        o.step = std::to_string(step);
        o.unit = std::move(unit);
        return add(std::move(o));
    }

    Opt &addColor(std::string key, std::string title, uint32_t def, bool alpha = true) {
        Opt o;
        o.key = std::move(key);
        o.title = std::move(title);
        o.type = ConfigControlTypeV2::Color;
        o.def = colorToString(def);
        o.colorAlpha = alpha;
        o.category = "appearance";
        return add(std::move(o));
    }

    Opt &addChoice(std::string key, std::string title, std::string def, std::vector<Choice> choices) {
        Opt o;
        o.key = std::move(key);
        o.title = std::move(title);
        o.type = ConfigControlTypeV2::Choice;
        o.style = ConfigChoiceStyleV2::Dropdown;
        o.def = std::move(def);
        o.choices = std::move(choices);
        return add(std::move(o));
    }

    // Tuỳ chọn chung cho module có HUD: scale, màu chữ, màu nền, vị trí (kéo trong HUD editor).
    void addHudOptions(float defX, float defY, bool withColors = true) {
        mHasHud = true;
        addInt("scale", "Kích thước", 100, 50, 250, 5, "%").category = "appearance";
        if (withColors) {
            addColor("textColor", "Màu chữ", 0xFFFFFFFFu);
            addColor("bgColor", "Màu nền", 0x66000000u);
        }
        auto &x = addInt("hudPosX", "Vị trí X", int(defX), 0, 6000, 1, "px");
        x.category = "position";
        x.advanced = true;
        auto &y = addInt("hudPosY", "Vị trí Y", int(defY), 0, 3000, 1, "px");
        y.category = "position";
        y.advanced = true;
    }

    // Kích thước chữ cơ sở theo scale màn hình * tuỳ chọn "scale".
    [[nodiscard]] float fontSize(const HudContext &ctx, float base = 30.f) const {
        return base * ctx.scale * std::max(0.5f, number("scale") / 100.f);
    }

    void reportBounds(float w, float h) {
        mBoundsW = w;
        mBoundsH = h;
    }

    void setDefaultEnabled(bool v) { mDefaultEnabled = v; }

  private:
    std::string mId, mName, mDescription;
    std::vector<Opt> mOpts;
    std::map<std::string, std::string> mValues;
    bool mEnabled = false;
    bool mDefaultEnabled = false;
    bool mHasHud = false;
    float mBoundsW = 0.f, mBoundsH = 0.f;
};

} // namespace nova
