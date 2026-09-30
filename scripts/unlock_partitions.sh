#!/usr/bin/env bash
set -e

# ==============================================================================
# Script tự động hóa Bước 2: Bẻ khóa phân vùng (Root, Disable-verity, Remount)
# Yêu cầu: AVD đang chạy với cờ -writable-system
# ==============================================================================

echo "🔍 Đang kiểm tra thiết bị qua ADB..."
adb wait-for-device

echo "⏳ Đang đợi Android boot hoàn tất (sys.boot_completed)..."
while [ "$(adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" != "1" ]; do
    sleep 2
done

echo "🔓 1. Chuyển sang adb root..."
adb root
sleep 2

echo "🛡️  2. Tắt bảo vệ vẹn toàn (disable-verity)..."
adb disable-verity

echo "🔄 3. Đang reboot lại máy ảo..."
adb reboot

echo "⏳ Đang chờ máy ảo khởi động lại..."
adb wait-for-device
while [ "$(adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" != "1" ]; do
    sleep 2
done

echo "🔓 4. Cấp quyền root sau khi reboot..."
adb root
sleep 2

echo "📂 5. Mở khóa quyền ghi đè phân vùng (remount)..."
adb remount

echo ""
echo "======================================================================"
echo "🎉 CHÚC MỪNG: Phân vùng hệ thống (/system, /vendor) đã được mở khóa ghi đè thành công!"
echo "👉 Bạn đã sẵn sàng cho Bước 3 & Bước 4 (Bơm binary & service vào máy ảo)."
echo "======================================================================"
