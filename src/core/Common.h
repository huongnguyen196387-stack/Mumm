#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

namespace nova {

// Màu trong toàn bộ SDK là ARGB (0xAARRGGBB). Config Color được lưu dạng "#AARRGGBB".
constexpr uint32_t makeArgb(int a, int r, int g, int b) {
    return (uint32_t(a & 255) << 24) | (uint32_t(r & 255) << 16) | (uint32_t(g & 255) << 8) |
           uint32_t(b & 255);
}

inline uint32_t withAlpha(uint32_t c, int a) {
    return (c & 0x00FFFFFFu) | (uint32_t(std::clamp(a, 0, 255)) << 24);
}

inline uint32_t parseColor(std::string_view s, uint32_t fallback) {
    if (s.empty()) return fallback;
    if (s.front() == '#') {
        s.remove_prefix(1);
    } else if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s.remove_prefix(2);
    } else {
        return fallback;
    }
    std::string tmp(s);
    char *end = nullptr;
    unsigned long v = std::strtoul(tmp.c_str(), &end, 16);
    if (end == tmp.c_str() || *end != '\0') return fallback;
    if (tmp.size() <= 6) v |= 0xFF000000ul;
    return static_cast<uint32_t>(v);
}

inline std::string colorToString(uint32_t c) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "#%08X", c);
    return buf;
}

inline bool parseBool(std::string_view s, bool def) {
    if (s == "true" || s == "1") return true;
    if (s == "false" || s == "0") return false;
    return def;
}

inline int parseInt(std::string_view s, int def) {
    std::string t(s);
    char *end = nullptr;
    long v = std::strtol(t.c_str(), &end, 10);
    return end == t.c_str() ? def : static_cast<int>(v);
}

inline float parseFloat(std::string_view s, float def) {
    std::string t(s);
    char *end = nullptr;
    float v = std::strtof(t.c_str(), &end);
    return end == t.c_str() ? def : v;
}

inline std::string numToString(float v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%g", v);
    return buf;
}

// Số ký tự UTF-8 (bỏ byte continuation) – dùng để ước lượng bề rộng text.
inline size_t utf8Length(std::string_view s) {
    size_t n = 0;
    for (unsigned char c : s)
        if ((c & 0xC0) != 0x80) ++n;
    return n;
}

} // namespace nova
