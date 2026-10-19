#include "game/GameProvider.h"

#include "game/GameBridge.h"

namespace nova {

// ─────────────────────────────────────────────────────────────────────────────
// TRẠNG THÁI HIỆN TẠI: chưa có provider.
//
// Mọi dữ liệu "bên trong game" (toạ độ, giáp, ping, reach, vật phẩm, FOV, độ sáng...) đều nằm
// trong libminecraftpe.so và thay đổi theo từng bản Minecraft, nên cần signature/offset đúng bản
// mà bạn đang chơi. Repo này không kèm số liệu đó vì không thể suy ra từ file đã gửi.
//
// Cách bổ sung (xem README mục "Thêm hook game"):
//   1. Tìm signature của hàm cần hook (ví dụ LocalPlayer::tick) cho đúng phiên bản MC,
//      ghi vào resources/signatures.ini:      localPlayerTick=A9 ? ? ? 91 ...
//   2. Ở đây:   hooks.hookSlot("localPlayerTick", (FuncPtr)detour, (FuncPtr*)&orig);
//      Trong detour, đọc dữ liệu từ `this` (offset theo phiên bản) và ghi vào một GameState
//      dùng chung (có mutex).
//   3. Gọi GameBridge::get().setProvider([](GameState &out) { ...copy state...; return true; });
//      Sau đó các module Coordinates / Speed / Reach / Ping / Armor / Item Counter tự xuất hiện
//      trong menu.
// ─────────────────────────────────────────────────────────────────────────────
bool installGameProvider(GameHooks &hooks, pl::log::Logger &log) {
    if (hooks.slotCount() == 0) {
        log.info("signatures.ini trống – các module cần dữ liệu game (Coordinates, Armor, Ping...) bị ẩn");
    } else {
        log.info("Đã nạp {} slot signature nhưng chưa có provider triển khai", hooks.slotCount());
    }
    return false;
}

} // namespace nova
