#pragma once

#include <atomic>
#include <bitset>
#include <cstdint>
#include <deque>
#include <mutex>
#include <unordered_map>

namespace nova {

// Android keycode dùng trong module.
namespace key {
inline constexpr int A = 29, D = 32, S = 47, W = 51, Space = 62, ShiftLeft = 59, CtrlLeft = 113;
} // namespace key

enum JoyDir : uint32_t { JoyUp = 1, JoyDown = 2, JoyLeft = 4, JoyRight = 8 };

// Trạng thái input toàn process (bàn phím, chuột, touch) – phục vụ CPS và Keystrokes.
class InputState {
  public:
    static InputState &get();

    void install(); // đăng ký callback một lần (SDK không có API huỷ đăng ký)
    void setActive(bool v) { mActive.store(v); }
    void setSurface(float w, float h);
    void setTouchCountsAsClick(bool v) { mTouchClick.store(v); }

    [[nodiscard]] bool keyDown(int keyCode) const;
    [[nodiscard]] bool mouseDown(int button) const; // 0 = trái, 1 = phải
    [[nodiscard]] int cps(int button);              // số click trong 1 giây gần nhất
    [[nodiscard]] uint32_t joystickMask() const;    // ước lượng hướng từ joystick cảm ứng

  private:
    bool onTouch(int action, int pointerId, float x, float y);
    bool onKey(int keyCode, bool down);
    bool onMouse(int button, bool down);
    void pushClick(int button);

    struct Touch {
        float sx, sy, x, y;
        bool joystick;
    };

    mutable std::mutex mMutex;
    std::bitset<512> mKeys;
    bool mMouse[2] = {false, false};
    std::deque<int64_t> mClicks[2];
    std::unordered_map<int, Touch> mTouches;
    float mW = 2400.f, mH = 1080.f;
    std::atomic<bool> mActive{false};
    std::atomic<bool> mTouchClick{true};
    bool mInstalled = false;
};

} // namespace nova
