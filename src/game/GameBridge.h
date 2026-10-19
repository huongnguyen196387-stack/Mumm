#pragma once

#include <functional>
#include <mutex>
#include <string>

namespace nova {

struct ArmorPiece {
    bool valid = false;
    int durability = 0;    // độ bền còn lại
    int maxDurability = 0;
    std::string name;
};

// Trạng thái game mà các module "GameInfo" đọc. Do một Provider (hook vào libminecraftpe.so) điền.
struct GameState {
    bool inWorld = false;
    double x = 0, y = 0, z = 0;
    float yaw = 0, pitch = 0;
    float reach = -1.f;        // khoảng cách đòn đánh gần nhất; < 0 = chưa có
    int pingMs = -1;           // < 0 = không có
    ArmorPiece armor[4];       // mũ, giáp, quần, giày
    std::string heldName;
    int heldCount = 0;         // số vật phẩm đang cầm
    int heldTotal = 0;         // tổng số cùng loại trong hành trang
};

// Cầu nối giữa module (hiển thị) và lớp hook game (phụ thuộc phiên bản Minecraft).
// Khi chưa có provider, các module cần dữ liệu game sẽ không được đăng ký vào menu.
class GameBridge {
  public:
    using Provider = std::function<bool(GameState &)>; // trả true nếu đọc được dữ liệu hợp lệ

    static GameBridge &get();
    void setProvider(Provider p);
    [[nodiscard]] bool hasProvider() const;
    [[nodiscard]] bool snapshot(GameState &out) const;

  private:
    mutable std::mutex mMutex;
    Provider mProvider;
};

} // namespace nova
