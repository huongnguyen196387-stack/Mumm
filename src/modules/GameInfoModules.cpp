// Các module cần dữ liệu từ bên trong game (toạ độ, giáp, ping, reach...).
// Chúng chỉ được đăng ký vào menu khi đã có GameBridge::Provider (hook theo phiên bản Minecraft).

#include <chrono>
#include <cmath>

#include "game/GameBridge.h"
#include "modules/Modules.h"
#include "modules/TextHudModule.h"

namespace nova {
namespace {

class GameInfoBase : public TextHudModule {
  public:
    using TextHudModule::TextHudModule;
    [[nodiscard]] bool available() const override { return GameBridge::get().hasProvider(); }
};

class CoordsModule final : public GameInfoBase {
  public:
    CoordsModule() : GameInfoBase("coords", "Coordinates", "Toạ độ XYZ và hướng nhìn.", 16, 330) {
        addToggle("showFacing", "Hiện hướng nhìn", true);
        addInt("decimals", "Số chữ số thập phân", 1, 0, 3);
        setDefaultEnabled(true);
    }

  protected:
    std::vector<std::string> lines(const HudContext &) override {
        GameState st;
        if (!GameBridge::get().snapshot(st) || !st.inWorld) return {};
        char fmt[24], buf[96];
        std::snprintf(fmt, sizeof fmt, "XYZ: %%.%df %%.%df %%.%df", integer("decimals"), integer("decimals"),
                      integer("decimals"));
        std::snprintf(buf, sizeof buf, fmt, st.x, st.y, st.z);
        std::vector<std::string> out{buf};
        if (flag("showFacing")) {
            static const char *dirs[] = {"S", "SW", "W", "NW", "N", "NE", "E", "SE"};
            float yaw = std::fmod(std::fmod(st.yaw, 360.f) + 360.f, 360.f);
            out.push_back(std::string("Facing: ") + dirs[int((yaw + 22.5f) / 45.f) % 8]);
        }
        return out;
    }
};

class SpeedModule final : public GameInfoBase {
  public:
    SpeedModule() : GameInfoBase("speed", "Speed Display", "Tốc độ di chuyển thực tế (block/giây).", 16, 400) {}

    void onTick(float dt) override {
        GameState st;
        if (!GameBridge::get().snapshot(st) || !st.inWorld) {
            mHave = false;
            return;
        }
        if (mHave && dt > 0.f) {
            float dx = float(st.x - mLastX), dz = float(st.z - mLastZ);
            float inst = std::sqrt(dx * dx + dz * dz) / dt;
            mSpeed = mSpeed * 0.7f + inst * 0.3f;
        }
        mLastX = st.x;
        mLastZ = st.z;
        mHave = true;
    }

  protected:
    std::vector<std::string> lines(const HudContext &) override {
        if (!mHave) return {};
        char buf[40];
        std::snprintf(buf, sizeof buf, "Speed: %.2f b/s", mSpeed);
        return {buf};
    }

  private:
    bool mHave = false;
    double mLastX = 0, mLastZ = 0;
    float mSpeed = 0.f;
};

class ReachModule final : public GameInfoBase {
  public:
    ReachModule() : GameInfoBase("reach", "Reach Display", "Khoảng cách của đòn đánh gần nhất.", 16, 470) {}

    void onTick(float dt) override {
        GameState st;
        if (!GameBridge::get().snapshot(st)) return;
        if (st.reach >= 0.f && std::fabs(st.reach - mLast) > 1e-4f) {
            mLast = st.reach;
            mAge = 0.f;
        } else {
            mAge += dt;
        }
    }

  protected:
    std::vector<std::string> lines(const HudContext &) override {
        char buf[40];
        if (mLast < 0.f || mAge > 3.f) return {"Reach: --"};
        std::snprintf(buf, sizeof buf, "Reach: %.2f", mLast);
        return {buf};
    }

  private:
    float mLast = -1.f, mAge = 99.f;
};

class PingModule final : public GameInfoBase {
  public:
    PingModule() : GameInfoBase("ping", "Ping Counter", "Độ trễ tới server (ms).", 16, 540) {}

  protected:
    std::vector<std::string> lines(const HudContext &) override {
        GameState st;
        if (!GameBridge::get().snapshot(st) || st.pingMs < 0) return {"Ping: --"};
        return {"Ping: " + std::to_string(st.pingMs) + " ms"};
    }
};

class ArmorModule final : public GameInfoBase {
  public:
    ArmorModule() : GameInfoBase("armor", "Armor Status", "Độ bền giáp đang mặc.", 16, 610) {
        addToggle("percent", "Hiện phần trăm", false);
    }

  protected:
    std::vector<std::string> lines(const HudContext &) override {
        GameState st;
        if (!GameBridge::get().snapshot(st)) return {};
        static const char *slots[] = {"Helmet", "Chest", "Legs", "Boots"};
        std::vector<std::string> out;
        for (int i = 0; i < 4; ++i) {
            const auto &a = st.armor[i];
            if (!a.valid || a.maxDurability <= 0) continue;
            std::string s = std::string(slots[i]) + ": ";
            if (flag("percent")) s += std::to_string(a.durability * 100 / a.maxDurability) + "%";
            else s += std::to_string(a.durability) + "/" + std::to_string(a.maxDurability);
            out.push_back(std::move(s));
        }
        return out;
    }
};

class ItemCounterModule final : public GameInfoBase {
  public:
    ItemCounterModule()
        : GameInfoBase("items", "Item Counter", "Số lượng vật phẩm đang cầm và tổng trong hành trang.", 16, 700) {}

  protected:
    std::vector<std::string> lines(const HudContext &) override {
        GameState st;
        if (!GameBridge::get().snapshot(st) || st.heldName.empty()) return {};
        return {st.heldName + " x" + std::to_string(st.heldCount) + "  (" + std::to_string(st.heldTotal) + ")"};
    }
};

} // namespace

void registerGameInfoModules(ModuleManager &mgr) {
    mgr.add(std::make_unique<CoordsModule>());
    mgr.add(std::make_unique<SpeedModule>());
    mgr.add(std::make_unique<ReachModule>());
    mgr.add(std::make_unique<PingModule>());
    mgr.add(std::make_unique<ArmorModule>());
    mgr.add(std::make_unique<ItemCounterModule>());
}

} // namespace nova
