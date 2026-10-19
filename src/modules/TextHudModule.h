#pragma once

#include <string>
#include <vector>

#include "core/Module.h"

namespace nova {

// Module hiển thị một hộp chữ (một hoặc nhiều dòng) tại hudPosX/hudPosY, kéo được trong HUD editor.
class TextHudModule : public Module {
  public:
    TextHudModule(std::string id, std::string name, std::string desc, float defX, float defY)
        : Module(std::move(id), std::move(name), std::move(desc)) {
        addHudOptions(defX, defY);
    }

  protected:
    // Trả về các dòng cần vẽ; rỗng => không vẽ gì.
    virtual std::vector<std::string> lines(const HudContext &ctx) = 0;

    void onRender(Hud &hud, const HudContext &ctx) override {
        auto ls = lines(ctx);
        if (ls.empty()) {
            reportBounds(0.f, 0.f);
            return;
        }
        const float size = fontSize(ctx);
        const float padX = size * 0.45f, padY = size * 0.30f, lineH = size * 1.18f;
        float w = 0.f;
        for (const auto &l : ls) w = std::max(w, Hud::textWidth(l, size));
        const float boxW = w + padX * 2.f;
        const float boxH = padY * 2.f + lineH * float(ls.size() - 1) + size;
        const float x = posX(), y = posY();

        if ((color("bgColor") >> 24) != 0) hud.rectFilled(x, y, boxW, boxH, color("bgColor"), size * 0.25f);
        for (size_t i = 0; i < ls.size(); ++i)
            hud.textTop(x + padX, y + padY + lineH * float(i), ls[i], color("textColor"), size);
        reportBounds(boxW, boxH);
    }
};

} // namespace nova
