#include "core/Settings.h"

#include <algorithm>
#include <fstream>

namespace nova {

void Settings::load(const std::filesystem::path &file) {
    std::lock_guard lock(mMutex);
    mPath = file;
    mValues.clear();
    mDirty = false;

    std::ifstream in(file);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto eq = line.find('=');
        if (line.empty() || line[0] == '#' || eq == std::string::npos || eq == 0) continue;
        mValues[line.substr(0, eq)] = line.substr(eq + 1);
    }
}

void Settings::flushIfDirty() {
    std::lock_guard lock(mMutex);
    if (!mDirty || mPath.empty()) return;

    std::error_code ec;
    std::filesystem::create_directories(mPath.parent_path(), ec);

    // Ghi file tạm rồi đổi tên để không bị hỏng file nếu game bị kill giữa chừng.
    auto tmp = mPath;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) return;
        for (const auto &[k, v] : mValues) out << k << '=' << v << '\n';
        if (!out.good()) return;
    }
    std::filesystem::rename(tmp, mPath, ec);
    if (!ec) mDirty = false;
}

std::optional<std::string> Settings::get(const std::string &group, const std::string &key) const {
    std::lock_guard lock(mMutex);
    auto it = mValues.find(group + "." + key);
    if (it == mValues.end()) return std::nullopt;
    return it->second;
}

void Settings::set(const std::string &group, const std::string &key, const std::string &value) {
    std::lock_guard lock(mMutex);
    std::string clean = value;
    std::replace(clean.begin(), clean.end(), '\n', ' ');
    std::replace(clean.begin(), clean.end(), '\r', ' ');
    auto [it, inserted] = mValues.try_emplace(group + "." + key, clean);
    if (inserted || it->second != clean) {
        it->second = clean;
        mDirty = true;
    }
}

} // namespace nova
