// Toggle Sprint / Auto Sneak: hai nút on-screen kiểu "công tắc" – bấm một lần để giữ phím,
// bấm lần nữa để nhả. Launcher gửi KeyEvent thật vào game nên không cần hook.

#include <pl/ModMenu.hpp>

#include "core/InputState.h"
#include "modules/Modules.h"

namespace nova {
namespace {

class MovementModule final : public Module {
  public:
    explicit MovementModule(std::string modId)
        : Module("movement_keys", "Toggle Sprint / Auto Sneak",
                 "Hai nút công tắc trên màn hình: giữ chạy nhanh (Ctrl) và giữ ngồi xổm (Shift) mà không cần giữ tay."),
          mModId(std::move(modId)) {
        addInt("sprintKey", "Keycode chạy nhanh", key::CtrlLeft, 0, 400, 1).advanced = true;
        addInt("sneakKey", "Keycode ngồi xổm", key::ShiftLeft, 0, 400, 1).advanced = true;
        addInt("btnWidth", "Chiều rộng nút", 100, 60, 200, 5, "%").category = "appearance";
        addInt("btnHeight", "Chiều cao nút", 100, 60, 200, 5, "%").category = "appearance";
        setDefaultEnabled(true);
    }

    // Đổi keycode / kích thước nút cần khởi động lại game (nút được đăng ký một lần khi nạp).
    void registerExtras() override {
        const float w = number("btnWidth") / 100.f, h = number("btnHeight") / 100.f;

        pl::modmenu::ButtonBuilder sprint("nova.sprint", "Toggle Sprint");
        sprint.moduleId(id()).modId(mModId).label("SPRINT")
            .androidKeyCode(integer("sprintKey"))
            .behavior(pl::modmenu::ButtonBehavior::Toggle)
            .stylePreset(pl::modmenu::ButtonStylePreset::Accent)
            .sizeScale(w, h);
        if (!sprint.registerButton()) mFailed = true;

        pl::modmenu::ButtonBuilder sneak("nova.sneak", "Auto Sneak");
        sneak.moduleId(id()).modId(mModId).label("SNEAK")
            .androidKeyCode(integer("sneakKey"))
            .behavior(pl::modmenu::ButtonBehavior::Toggle)
            .stylePreset(pl::modmenu::ButtonStylePreset::Accent)
            .sizeScale(w, h);
        if (!sneak.registerButton()) mFailed = true;
    }

    void unregisterExtras() override {
        pl::modmenu::unregisterButton("nova.sprint");
        pl::modmenu::unregisterButton("nova.sneak");
    }

  private:
    std::string mModId;
    bool mFailed = false;
};

} // namespace

void registerMovementModules(ModuleManager &mgr) {
    mgr.add(std::make_unique<MovementModule>(ll::mod::NativeMod::current()->getId()));
}

} // namespace nova
