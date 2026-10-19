#include "core/InputState.h"

#include <chrono>
#include <cmath>

#include <pl/Input.hpp>

namespace nova {
namespace {

int64_t nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

constexpr int kActionMask = 0xFF;
constexpr int kDown = 0, kUp = 1, kCancel = 3, kPointerDown = 5, kPointerUp = 6;

} // namespace

InputState &InputState::get() {
    static InputState inst;
    return inst;
}

void InputState::install() {
    if (mInstalled) return;
    mInstalled = true;
    // Tất cả callback trả về false => không chặn input của game.
    pl::input::registerTouchCallback([this](const pl::input::TouchEvent &e) {
        return onTouch(e.action, e.pointerId, e.x, e.y);
    });
    pl::input::registerKeyCallback(
        [this](const pl::input::KeyEvent &e) { return onKey(e.keyCode, e.isKeyDown); });
    pl::input::registerMouseCallback(
        [this](const pl::input::MouseEvent &e) { return onMouse(e.button, e.isDown); });
}

void InputState::setSurface(float w, float h) {
    std::lock_guard lock(mMutex);
    mW = w;
    mH = h;
}

bool InputState::onKey(int keyCode, bool down) {
    if (!mActive.load() || keyCode < 0 || keyCode >= 512) return false;
    std::lock_guard lock(mMutex);
    mKeys[static_cast<size_t>(keyCode)] = down;
    return false;
}

bool InputState::onMouse(int button, bool down) {
    if (!mActive.load()) return false;
    // Android MotionEvent: BUTTON_PRIMARY = 1, BUTTON_SECONDARY = 2. Giá trị 0 coi như chuột trái.
    int idx = (button == 2) ? 1 : 0;
    if (button != 0 && button != 1 && button != 2) return false;
    std::lock_guard lock(mMutex);
    if (down && !mMouse[idx]) pushClick(idx);
    mMouse[idx] = down;
    return false;
}

bool InputState::onTouch(int action, int pointerId, float x, float y) {
    if (!mActive.load()) return false;
    const int a = action & kActionMask;
    std::lock_guard lock(mMutex);

    if (a == kDown || a == kPointerDown) {
        // Vùng joystick: góc dưới-trái. Vùng "đánh/đặt": giữa màn hình, tránh nút nhảy & hotbar.
        bool joy = x < mW * 0.40f && y > mH * 0.35f;
        mTouches[pointerId] = Touch{x, y, x, y, joy};
        bool inTapZone = x > mW * 0.20f && x < mW * 0.88f && y > mH * 0.10f && y < mH * 0.82f;
        if (!joy && inTapZone && mTouchClick.load()) pushClick(0);
    } else if (a == kUp || a == kPointerUp || a == kCancel) {
        mTouches.erase(pointerId);
    } else {
        auto it = mTouches.find(pointerId);
        if (it != mTouches.end()) {
            it->second.x = x;
            it->second.y = y;
        }
        // ACTION_MOVE mang toạ độ của mọi pointer nhưng SDK chỉ gửi từng pointer một – đủ dùng.
    }
    return false;
}

void InputState::pushClick(int button) { mClicks[button].push_back(nowMs()); }

bool InputState::keyDown(int keyCode) const {
    if (keyCode < 0 || keyCode >= 512) return false;
    std::lock_guard lock(mMutex);
    return mKeys[static_cast<size_t>(keyCode)];
}

bool InputState::mouseDown(int button) const {
    if (button < 0 || button > 1) return false;
    std::lock_guard lock(mMutex);
    if (mMouse[button]) return true;
    if (button == 0) {
        for (const auto &[id, t] : mTouches) {
            bool inTapZone = t.sx > mW * 0.20f && t.sx < mW * 0.88f && t.sy > mH * 0.10f &&
                             t.sy < mH * 0.82f;
            if (!t.joystick && inTapZone) return true;
        }
    }
    return false;
}

int InputState::cps(int button) {
    if (button < 0 || button > 1) return 0;
    std::lock_guard lock(mMutex);
    const int64_t cutoff = nowMs() - 1000;
    auto &q = mClicks[button];
    while (!q.empty() && q.front() < cutoff) q.pop_front();
    return static_cast<int>(q.size());
}

uint32_t InputState::joystickMask() const {
    std::lock_guard lock(mMutex);
    const float dead = 40.f * (mH / 1080.f);
    uint32_t mask = 0;
    for (const auto &[id, t] : mTouches) {
        if (!t.joystick) continue;
        float dx = t.x - t.sx, dy = t.y - t.sy;
        if (std::hypot(dx, dy) < dead) continue;
        // Chia 8 hướng: mỗi trục kích hoạt khi thành phần đủ lớn so với vector.
        float len = std::hypot(dx, dy);
        if (dy / len < -0.38f) mask |= JoyUp;
        if (dy / len > 0.38f) mask |= JoyDown;
        if (dx / len < -0.38f) mask |= JoyLeft;
        if (dx / len > 0.38f) mask |= JoyRight;
    }
    return mask;
}

} // namespace nova
