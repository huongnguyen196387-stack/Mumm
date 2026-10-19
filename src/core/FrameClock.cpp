#include "core/FrameClock.h"

#include <dlfcn.h>
#include <pthread.h>

#include <atomic>
#include <cstring>

#include <pl/memory/Hook.hpp>

namespace nova {
namespace {

using SwapFn = unsigned int (*)(void *display, void *surface);

SwapFn gOrig = nullptr;
void *gTarget = nullptr;
bool gInstalled = false;

std::atomic<uint64_t> gGameFrames{0}; // thread không phải RenderThread
std::atomic<uint64_t> gUiFrames{0};   // RenderThread (HWUI)

uint64_t gLastGame = 0, gLastUi = 0;
int64_t gLastMs = 0;
std::atomic<float> gFps{0.f};

unsigned int hookedSwap(void *display, void *surface) {
    thread_local int cls = -1; // 0 = game, 1 = ui
    if (cls < 0) {
        char name[32] = {0};
        pthread_getname_np(pthread_self(), name, sizeof name);
        cls = std::strcmp(name, "RenderThread") == 0 ? 1 : 0;
    }
    (cls == 0 ? gGameFrames : gUiFrames).fetch_add(1, std::memory_order_relaxed);
    return gOrig ? gOrig(display, surface) : 1u;
}

} // namespace

bool FrameClock::install() {
    if (gInstalled) return true;
    gTarget = dlsym(RTLD_DEFAULT, "eglSwapBuffers");
    if (!gTarget) return false;
    int rc = pl::memory::hook(gTarget, reinterpret_cast<pl::memory::FuncPtr>(&hookedSwap),
                              reinterpret_cast<pl::memory::FuncPtr *>(&gOrig));
    gInstalled = (rc == 0);
    return gInstalled;
}

void FrameClock::uninstall() {
    if (!gInstalled) return;
    pl::memory::unhook(gTarget, reinterpret_cast<pl::memory::FuncPtr>(&hookedSwap));
    gInstalled = false;
}

bool FrameClock::installed() { return gInstalled; }

void FrameClock::update(int64_t nowMs) {
    if (!gInstalled) return;
    if (gLastMs == 0) {
        gLastMs = nowMs;
        gLastGame = gGameFrames.load();
        gLastUi = gUiFrames.load();
        return;
    }
    int64_t dt = nowMs - gLastMs;
    if (dt < 500) return;

    uint64_t g = gGameFrames.load(), u = gUiFrames.load();
    uint64_t frames = (g - gLastGame) > 0 ? (g - gLastGame) : (u - gLastUi); // ưu tiên thread game
    float inst = static_cast<float>(frames) * 1000.f / static_cast<float>(dt);
    float prev = gFps.load();
    gFps.store(prev <= 0.f ? inst : prev * 0.4f + inst * 0.6f);

    gLastMs = nowMs;
    gLastGame = g;
    gLastUi = u;
}

float FrameClock::fps() { return gFps.load(); }

float FrameClock::frameTimeMs() {
    float f = gFps.load();
    return f > 0.5f ? 1000.f / f : 0.f;
}

} // namespace nova
