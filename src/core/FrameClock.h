#pragma once

#include <cstdint>

namespace nova {

// Đếm FPS thật của game bằng cách hook eglSwapBuffers (không cần signature của libminecraftpe).
// Swap của RenderThread (HWUI vẽ overlay launcher) bị loại ra bằng tên thread.
class FrameClock {
  public:
    static bool install();
    static void uninstall();
    static bool installed();

    // Gọi định kỳ từ luồng HUD (~30Hz). nowMs: đồng hồ steady, đơn vị ms.
    static void update(int64_t nowMs);
    static float fps();
    static float frameTimeMs();
};

} // namespace nova
