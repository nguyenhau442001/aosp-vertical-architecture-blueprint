#!/usr/bin/env bash
set -e

# ==============================================================================
# One-shot runner: Starts Android 15 Automotive & unlocks partitions
# Usage: ./scripts/run.sh [AVD_NAME]
# ==============================================================================

AVD_NAME="${1:-Automotive_15_ARM64}"
SDK="${ANDROID_HOME:-$HOME/Library/Android/sdk}"
EMULATOR="$SDK/emulator/emulator"

# 1. Start emulator in background if not already running
if ! pgrep -f "emulator.*$AVD_NAME" > /dev/null 2>&1; then
    echo "🚗 Launching $AVD_NAME with -writable-system..."
    "$EMULATOR" -avd "$AVD_NAME" -writable-system -gpu auto > /dev/null 2>&1 &
else
    echo "ℹ️  $AVD_NAME is already running."
fi

# 2. Wait for boot completion
echo "⏳ Waiting for Android to complete boot..."
adb wait-for-device
while [ "$(adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" != "1" ]; do
    sleep 2
done

# 3. Disable verity & reboot (one-time integrity unlock)
echo "🔓 Disabling verity & rebooting..."
adb root > /dev/null 2>&1 || true
sleep 1
adb disable-verity
adb reboot

# 4. Wait for reboot completion
echo "⏳ Waiting for reboot..."
adb wait-for-device
while [ "$(adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" != "1" ]; do
    sleep 2
done

# 5. Remount partitions as writable
adb root > /dev/null 2>&1 || true
sleep 1
adb remount

echo ""
echo "======================================================================"
echo "✅ SUCCESS: $AVD_NAME is ready and partitions (/system, /vendor) are writable!"
echo "======================================================================"
