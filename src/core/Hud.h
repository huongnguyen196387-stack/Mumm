#pragma once

#include <bit>
#include <string>
#include <vector>

#include <pl/ModMenu.hpp>

#include "core/Common.h"

namespace nova {

using pl::modmenu::DrawCommand;
using pl::modmenu::DrawCommandType;

struct HudContext {
    float width = 2400.f;  // kích thước surface overlay (px)
    float height = 1080.f;
    float scale = 1.f;     // height / 1080
};

enum class Align { Left, Right, Center };

// Tọa độ đặc biệt của HudOverlay: x/y <= -19000 => tính từ TÂM màn hình (v + 20000),
// x/y <= -9000 => tính từ cạnh PHẢI/DƯỚI (v + 10000).
inline constexpr float kFromCenter = -20000.f;
inline constexpr float kFromEdge = -10000.f;

// Gom draw command cho một module. y của text là baseline; dùng textTop() để đặt theo đỉnh.
class Hud {
  public:
    void text(float x, float baselineY, std::string str, uint32_t color, float size,
              Align align = Align::Left, uint32_t bg = 0) {
        DrawCommand c;
        c.type = DrawCommandType::Text;
        c.x = x;
        c.y = baselineY;
        c.w = align == Align::Left ? 0.f : (align == Align::Right ? -1.f : -2.f);
        c.x3 = std::bit_cast<float>(bg); // launcher đọc x3 như int bits => màu nền text
        c.color = color;
        c.size = size;
        c.text = std::move(str);
        mCmds.push_back(std::move(c));
    }

    // Text đặt theo đỉnh hộp (khớp hộp nền mà launcher tự vẽ: cao = size + 4).
    void textTop(float x, float top, std::string str, uint32_t color, float size,
                 Align align = Align::Left, uint32_t bg = 0) {
        text(x, top + size * 0.9f, std::move(str), color, size, align, bg);
    }

    // Text canh giữa trong hộp (x,y,w,h).
    void textInBox(float x, float y, float w, float h, std::string str, uint32_t color, float size) {
        DrawCommand c;
        c.type = DrawCommandType::Text;
        c.x = x;
        c.y = y;
        c.w = w;
        c.h = h;
        c.color = color;
        c.size = size;
        c.text = std::move(str);
        mCmds.push_back(std::move(c));
    }

    void rect(float x, float y, float w, float h, uint32_t color, float radius = 0.f) {
        push(DrawCommandType::Rect, x, y, w, h, color, 0.f, radius);
    }
    void rectFilled(float x, float y, float w, float h, uint32_t color, float radius = 0.f) {
        push(DrawCommandType::RectFilled, x, y, w, h, color, 0.f, radius);
    }
    void line(float x1, float y1, float x2, float y2, uint32_t color, float width) {
        push(DrawCommandType::Line, x1, y1, x2 - x1, y2 - y1, color, width, 0.f);
    }
    void circleFilled(float cx, float cy, float radius, uint32_t color) {
        push(DrawCommandType::CircleFilled, cx, cy, 0.f, 0.f, color, radius, 0.f);
    }

    static float textWidth(std::string_view s, float size) {
        return static_cast<float>(utf8Length(s)) * size * 0.56f;
    }

    [[nodiscard]] const std::vector<DrawCommand> &commands() const { return mCmds; }
    [[nodiscard]] bool empty() const { return mCmds.empty(); }

  private:
    void push(DrawCommandType t, float x, float y, float w, float h, uint32_t color, float size,
              float x3) {
        DrawCommand c;
        c.type = t;
        c.x = x;
        c.y = y;
        c.w = w;
        c.h = h;
        c.x3 = x3;
        c.color = color;
        c.size = size;
        mCmds.push_back(std::move(c));
    }

    std::vector<DrawCommand> mCmds;
};

} // namespace nova
