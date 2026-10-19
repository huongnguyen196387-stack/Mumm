#pragma once

#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>

namespace nova {

// Lưu cấu hình dạng "group.key=value" (mỗi dòng một mục) trong config/settings.ini.
class Settings {
  public:
    void load(const std::filesystem::path &file);
    void flushIfDirty();

    std::optional<std::string> get(const std::string &group, const std::string &key) const;
    void set(const std::string &group, const std::string &key, const std::string &value);

  private:
    mutable std::mutex mMutex;
    std::filesystem::path mPath;
    std::map<std::string, std::string> mValues;
    bool mDirty = false;
};

} // namespace nova
