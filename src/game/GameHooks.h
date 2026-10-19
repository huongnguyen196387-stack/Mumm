#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <pl/memory/Hook.hpp>

namespace nova {

// Bảng signature của libminecraftpe.so, đọc từ resources/signatures.ini.
// Định dạng mỗi dòng:  tênSlot=<byte pattern, '?' là wildcard>
// Một slot có thể xuất hiện nhiều lần (nhiều ứng viên cho các phiên bản MC khác nhau);
// ứng viên đầu tiên resolve được sẽ được dùng.
class GameHooks {
  public:
    static GameHooks &get();

    void load(const std::filesystem::path &iniFile);

    // Trả 0 nếu không resolve được signature nào của slot.
    [[nodiscard]] uintptr_t resolve(const std::string &slot) const;

    // Resolve + cài hook. Trả true nếu thành công. Hook được gỡ trong removeAll().
    bool hookSlot(const std::string &slot, pl::memory::FuncPtr detour, pl::memory::FuncPtr *original);
    void removeAll();

    [[nodiscard]] size_t slotCount() const { return mSigs.size(); }

  private:
    struct Installed {
        uintptr_t target;
        pl::memory::FuncPtr detour;
    };
    std::map<std::string, std::vector<std::string>> mSigs;
    std::vector<Installed> mInstalled;
};

} // namespace nova
