// Các module chạy hoàn toàn bằng overlay + input + hook EGL: không cần signature của Minecraft.

#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>

#include "core/FrameClock.h"
#include "core/InputState.h"
#include "modules/Modules.h"
#include "modules/TextHudModule.h"

namespace nova {
namespace {

// ───────────────────────────── FPS ─────────────────────────────
class FpsModule final : public TextHudModule {
  public:
    FpsModule() : TextHudModule("fps", "FPS Counter", "Hiển thị số khung hình/giây thực của game.", 16, 16) {
        addToggle("showLabel", "Hiện nhãn \"FPS\"", true);
        addToggle("showFrameTime", "Hiện frame time (ms)", false);
        setDefaultEnabled(true);
    }

  protected:
    std::vector<std::string> lines(const HudContext &) override {
        std::string s;
        if (!FrameClock::installed() || FrameClock::fps() <= 0.f) {
            s = "--";
        } else {
            s = std::to_string(int(std::lround(FrameClock::fps())));
        }
        if (flag("showLabel")) s += " FPS";
        if (flag("showFrameTime") && FrameClock::frameTimeMs() > 0.f) {
            char buf[24];
            std::snprintf(buf, sizeof buf, "  %.1f ms", FrameClock::frameTimeMs());
            s += buf;
        }
        return {s};
    }
};

// ───────────────────────────── CPS ─────────────────────────────
class CpsModule final : public TextHudModule {
  public:
    CpsModule() : TextHudModule("cps", "CPS Counter", "Đếm số click mỗi giây, tách chuột trái/phải.", 16, 90) {
        addToggle("separate", "Tách trái / phải", true, "Hiện \"L | R\" thay vì tổng.");
        addToggle("showLabel", "Hiện nhãn \"CPS\"", true);
        addToggle("touchClick", "Tính chạm màn hình là click trái", true,
                  "Chạm vùng giữa màn hình (không phải joystick/nút) được tính là click trái.");
        setDefaultEnabled(true);
    }
    void onEnable() override { InputState::get().setTouchCountsAsClick(flag("touchClick")); }
    void onConfigChanged(const std::string &k) override {
        if (k == "touchClick") InputState::get().setTouchCountsAsClick(flag("touchClick"));
    }

  protected:
    std::vector<std::string> lines(const HudContext &) override {
        auto &in = InputState::get();
        int l = in.cps(0), r = in.cps(1);
        std::string s = flag("separate") ? std::to_string(l) + " | " + std::to_string(r)
                                         : std::to_string(l + r);
        if (flag("showLabel")) s += " CPS";
        return {s};
    }
};

// ───────────────────────────── Clock ─────────────────────────────
class ClockModule final : public TextHudModule {
  public:
    ClockModule() : TextHudModule("clock", "Clock", "Hiển thị giờ hệ thống.", 16, 160) {
        addToggle("use24h", "Định dạng 24 giờ", true);
        addToggle("showSeconds", "Hiện giây", false);
    }

  protected:
    std::vector<std::string> lines(const HudContext &) override {
        std::time_t t = std::time(nullptr);
        std::tm tm{};
        localtime_r(&t, &tm);
        char buf[32];
        const char *fmt = flag("use24h") ? (flag("showSeconds") ? "%H:%M:%S" : "%H:%M")
                                         : (flag("showSeconds") ? "%I:%M:%S %p" : "%I:%M %p");
        std::strftime(buf, sizeof buf, fmt, &tm);
        return {buf};
    }
};

// ───────────────────────────── Debug (kiểu F3) ─────────────────────────────
class DebugModule final : public TextHudModule {
  public:
    DebugModule()
        : TextHudModule("debug", "Debug Menu", "Bảng thông tin kỹ thuật kiểu F3: FPS, RAM, luồng, màn hình.", 16, 240) {
        addToggle("showMemory", "Hiện RAM / số luồng", true);
        addToggle("showSurface", "Hiện kích thước overlay", true);
    }
    void onTick(float dt) override {
        mAccum += dt;
        if (mAccum < 1.f) return;
        mAccum = 0.f;
        mRssMb = readRssMb();
        mThreads = readThreads();
    }

  protected:
    std::vector<std::string> lines(const HudContext &ctx) override {
        std::vector<std::string> out;
        char buf[96];
        out.push_back("Nova Client – Debug");
        if (FrameClock::installed() && FrameClock::fps() > 0.f) {
            std::snprintf(buf, sizeof buf, "FPS: %d (%.1f ms)", int(std::lround(FrameClock::fps())),
                          FrameClock::frameTimeMs());
        } else {
            std::snprintf(buf, sizeof buf, "FPS: --");
        }
        out.push_back(buf);
        auto &in = InputState::get();
        std::snprintf(buf, sizeof buf, "CPS: %d | %d", in.cps(0), in.cps(1));
        out.push_back(buf);
        if (flag("showMemory")) {
            std::snprintf(buf, sizeof buf, "RAM: %.0f MB   Threads: %d", mRssMb, mThreads);
            out.push_back(buf);
        }
        if (flag("showSurface")) {
            std::snprintf(buf, sizeof buf, "Overlay: %dx%d (x%.2f)", int(ctx.width), int(ctx.height), ctx.scale);
            out.push_back(buf);
        }
        return out;
    }

  private:
    static float readRssMb() {
        std::ifstream f("/proc/self/statm");
        long total = 0, rss = 0;
        if (!(f >> total >> rss)) return 0.f;
        return float(rss) * float(sysconf(_SC_PAGESIZE)) / (1024.f * 1024.f);
    }
    static int readThreads() {
        std::ifstream f("/proc/self/status");
        std::string line;
        while (std::getline(f, line))
            if (line.rfind("Threads:", 0) == 0) return parseInt(line.substr(8), 0);
        return 0;
    }
    float mAccum = 1.f;
    float mRssMb = 0.f;
    int mThreads = 0;
};

// ───────────────────────────── Keystrokes ─────────────────────────────
class KeystrokesModule final : public Module {
  public:
    KeystrokesModule()
        : Module("keystrokes", "Keystrokes", "Hiển thị phím W-A-S-D, Space và chuột trái/phải (kèm CPS).") {
        addHudOptions(16, 600, false);
        addInt("keySize", "Kích thước phím", 64, 36, 120, 2, "px").category = "appearance";
        addInt("gap", "Khoảng cách", 6, 0, 20, 1, "px").category = "appearance";
        addToggle("showMouse", "Hiện chuột trái/phải", true);
        addToggle("showSpace", "Hiện phím Space", true);
        addToggle("showCps", "Hiện CPS trong nút chuột", true);
        addToggle("touchJoystick", "Ước lượng từ joystick cảm ứng", true,
                  "Suy ra W/A/S/D từ hướng kéo joystick ảo (không cần bàn phím).");
        addColor("idleColor", "Màu nền phím", 0x80000000u);
        addColor("pressedColor", "Màu phím đang bấm", 0xCCFFFFFFu);
        addColor("textColor", "Màu chữ", 0xFFFFFFFFu);
        addColor("pressedTextColor", "Màu chữ khi bấm", 0xFF000000u);
        setDefaultEnabled(true);
    }

    void onRender(Hud &hud, const HudContext &ctx) override {
        auto &in = InputState::get();
        const float s = ctx.scale * std::max(0.5f, number("scale") / 100.f);
        const float k = float(integer("keySize")) * s, g = float(integer("gap")) * s;
        const float x0 = posX(), y0 = posY();
        const uint32_t mask = flag("touchJoystick") ? in.joystickMask() : 0u;

        auto keyBox = [&](float x, float y, float w, float h, const std::string &label, bool down,
                          float textSize) {
            hud.rectFilled(x, y, w, h, down ? color("pressedColor") : color("idleColor"), k * 0.14f);
            hud.textInBox(x, y, w, h, label, down ? color("pressedTextColor") : color("textColor"), textSize);
        };

        const float ts = k * 0.42f;
        keyBox(x0 + k + g, y0, k, k, "W", in.keyDown(key::W) || (mask & JoyUp), ts);
        keyBox(x0, y0 + k + g, k, k, "A", in.keyDown(key::A) || (mask & JoyLeft), ts);
        keyBox(x0 + k + g, y0 + k + g, k, k, "S", in.keyDown(key::S) || (mask & JoyDown), ts);
        keyBox(x0 + 2 * (k + g), y0 + k + g, k, k, "D", in.keyDown(key::D) || (mask & JoyRight), ts);

        float bottom = y0 + 2 * k + g;
        const float fullW = 3 * k + 2 * g;
        if (flag("showMouse")) {
            const float y = y0 + 2 * (k + g);
            const float w = (fullW - g) / 2.f;
            // Hộp text giữa không hỗ trợ xuống dòng: vẽ tên và CPS thành hai text riêng.
            hud.rectFilled(x0, y, w, k, in.mouseDown(0) ? color("pressedColor") : color("idleColor"), k * 0.14f);
            hud.rectFilled(x0 + w + g, y, w, k, in.mouseDown(1) ? color("pressedColor") : color("idleColor"), k * 0.14f);
            const uint32_t tL = in.mouseDown(0) ? color("pressedTextColor") : color("textColor");
            const uint32_t tR = in.mouseDown(1) ? color("pressedTextColor") : color("textColor");
            if (flag("showCps")) {
                hud.textInBox(x0, y, w, k * 0.58f, "LMB", tL, ts * 0.9f);
                hud.textInBox(x0, y + k * 0.42f, w, k * 0.5f, std::to_string(in.cps(0)), tL, ts * 0.8f);
                hud.textInBox(x0 + w + g, y, w, k * 0.58f, "RMB", tR, ts * 0.9f);
                hud.textInBox(x0 + w + g, y + k * 0.42f, w, k * 0.5f, std::to_string(in.cps(1)), tR, ts * 0.8f);
            } else {
                hud.textInBox(x0, y, w, k, "LMB", tL, ts * 0.9f);
                hud.textInBox(x0 + w + g, y, w, k, "RMB", tR, ts * 0.9f);
            }
            bottom = y + k;
        }
        if (flag("showSpace")) {
            const float y = bottom + g;
            const float h = k * 0.55f;
            const bool down = in.keyDown(key::Space);
            hud.rectFilled(x0, y, fullW, h, down ? color("pressedColor") : color("idleColor"), k * 0.14f);
            const float lw = fullW * 0.35f;
            hud.rectFilled(x0 + (fullW - lw) / 2.f, y + h / 2.f - 1.5f * s, lw, 3.f * s,
                           down ? color("pressedTextColor") : color("textColor"), 1.5f * s);
            bottom = y + h;
        }
        reportBounds(fullW, bottom - y0);
    }
};

// ───────────────────────────── Crosshair ─────────────────────────────
class CrosshairModule final : public Module {
  public:
    CrosshairModule() : Module("crosshair", "Crosshair", "Tâm ngắm tuỳ chỉnh: kiểu dáng, màu sắc, kích thước.") {
        addChoice("style", "Kiểu", "cross",
                  {{"cross", "Chữ thập"}, {"dot", "Chấm"}, {"circle", "Vòng tròn"},
                   {"square", "Vuông"}, {"x", "Chữ X"}});
        addInt("size", "Kích thước", 12, 2, 60, 1, "px");
        addInt("thickness", "Độ dày", 2, 1, 10, 1, "px");
        addInt("gap", "Khoảng hở tâm", 4, 0, 30, 1, "px");
        addColor("color", "Màu tâm ngắm", 0xFFFFFFFFu);
        addToggle("outline", "Viền", true).category = "appearance";
        addColor("outlineColor", "Màu viền", 0xFF000000u);
        setDefaultEnabled(false);
    }

    void onRender(Hud &hud, const HudContext &ctx) override {
        const std::string style = str("style");
        const float sc = ctx.scale;
        const float size = float(integer("size")) * sc;
        const float th = std::max(1.f, float(integer("thickness")) * sc);
        const float gap = float(integer("gap")) * sc;
        const uint32_t col = color("color");
        const bool outline = flag("outline");
        const uint32_t ocol = color("outlineColor");
        const float ow = 1.5f * sc * 2.f; // phần viền thêm (cộng vào độ dày)

        // Toạ độ tương đối so với tâm màn hình; kFromCenter làm launcher tự quy đổi theo kích thước thật.
        auto L = [&](float x1, float y1, float x2, float y2, float t, uint32_t c) {
            hud.line(kFromCenter + x1, kFromCenter + y1, kFromCenter + x2, kFromCenter + y2, c, t);
        };
        auto both = [&](float x1, float y1, float x2, float y2) {
            if (outline) L(x1, y1, x2, y2, th + ow, ocol);
            L(x1, y1, x2, y2, th, col);
        };

        if (style == "dot") {
            if (outline) hud.circleFilled(kFromCenter, kFromCenter, th * 1.5f + ow / 2.f, ocol);
            hud.circleFilled(kFromCenter, kFromCenter, th * 1.5f, col);
        } else if (style == "circle") {
            constexpr int N = 28;
            for (int pass = (outline ? 0 : 1); pass < 2; ++pass) {
                for (int i = 0; i < N; ++i) {
                    float a1 = 6.2831853f * float(i) / N, a2 = 6.2831853f * float(i + 1) / N;
                    L(size * std::cos(a1), size * std::sin(a1), size * std::cos(a2), size * std::sin(a2),
                      pass == 0 ? th + ow : th, pass == 0 ? ocol : col);
                }
            }
        } else if (style == "square") {
            const float h = size;
            both(-h, -h, h, -h);
            both(h, -h, h, h);
            both(h, h, -h, h);
            both(-h, h, -h, -h);
        } else if (style == "x") {
            const float a = gap, b = gap + size;
            both(-b, -b, -a, -a);
            both(a, -b, b, -a);
            both(-b, b, -a, a);
            both(a, a, b, b);
        } else { // cross
            const float a = gap, b = gap + size;
            both(-b, 0, -a, 0);
            both(a, 0, b, 0);
            both(0, -b, 0, -a);
            both(0, a, 0, b);
        }
    }
};

} // namespace

// Khai báo ở các file khác.
void registerGameInfoModules(ModuleManager &mgr);
void registerMovementModules(ModuleManager &mgr);

void registerAllModules(ModuleManager &mgr) {
    mgr.add(std::make_unique<FpsModule>());
    mgr.add(std::make_unique<CpsModule>());
    mgr.add(std::make_unique<KeystrokesModule>());
    mgr.add(std::make_unique<CrosshairModule>());
    mgr.add(std::make_unique<ClockModule>());
    mgr.add(std::make_unique<DebugModule>());
    registerMovementModules(mgr);
    registerGameInfoModules(mgr);
}

} // namespace nova
