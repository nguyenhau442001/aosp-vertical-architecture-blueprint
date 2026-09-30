#!/usr/bin/env bash
set -e

# ==============================================================================
# Script khởi chạy Android Emulator với cờ -writable-system
# Dành cho AOSP / Android Automotive Architecture Development
# ==============================================================================

# Xác định ANDROID_HOME nếu chưa được set trong môi trường
if [ -z "$ANDROID_HOME" ]; then
    if [ -d "$HOME/Library/Android/sdk" ]; then
        export ANDROID_HOME="$HOME/Library/Android/sdk"
    else
        echo "❌ Lỗi: Không tìm thấy Android SDK. Vui lòng cài đặt Android SDK hoặc export ANDROID_HOME."
        exit 1
    fi
fi

EMULATOR_BIN="$ANDROID_HOME/emulator/emulator"

if [ ! -x "$EMULATOR_BIN" ]; then
    echo "❌ Lỗi: Không tìm thấy file thực thi emulator tại: $EMULATOR_BIN"
    exit 1
fi

# Tên AVD mặc định hoặc lấy từ tham số truyền vào $1
DEFAULT_AVD="Automotive_1408p_landscape"
AVD_NAME="${1:-$DEFAULT_AVD}"

# Kiểm tra xem AVD có tồn tại không
AVAILABLE_AVDS=$("$EMULATOR_BIN" -list-avds)
if ! echo "$AVAILABLE_AVDS" | grep -q "^${AVD_NAME}$"; then
    echo "⚠️  Cảnh báo: AVD '$AVD_NAME' không nằm trong danh sách AVDs khả dụng:"
    echo "$AVAILABLE_AVDS"
    echo ""
    echo "👉 Hãy kiểm tra lại tên AVD hoặc tạo mới bằng Android Studio / avdmanager."
    exit 1
fi

echo "======================================================================"
echo "🚀 Đang khởi chạy AVD: $AVD_NAME"
echo "🔧 Chế độ: -writable-system (Hỗ trợ root, disable-verity và adb remount)"
echo "======================================================================"
echo "💡 Các lệnh tiếp theo khi máy ảo boot xong màn hình chính:"
echo "   Bước 2.1: adb root"
echo "   Bước 2.2: adb disable-verity"
echo "   Bước 2.3: adb reboot"
echo "   Bước 2.4: adb root && adb remount"
echo "======================================================================"
echo ""

# Chạy emulator với cờ -writable-system
exec "$EMULATOR_BIN" -avd "$AVD_NAME" -writable-system -gpu auto
