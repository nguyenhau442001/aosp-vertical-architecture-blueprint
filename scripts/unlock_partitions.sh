#!/usr/bin/env bash
set -e

# ==============================================================================
# Automated Step 2: Unlock System Partitions (Root, Disable-verity, Remount)
# Requirement: AVD must be launched with the -writable-system flag
# ==============================================================================

echo "🔍 Waiting for device via ADB..."
adb wait-for-device

echo "⏳ Waiting for Android to complete boot (sys.boot_completed)..."
while [ "$(adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" != "1" ]; do
    sleep 2
done

echo "🔓 1. Restarting adbd with root permissions..."
adb root
sleep 2

echo "🛡️  2. Disabling dm-verity integrity checks..."
adb disable-verity

echo "🔄 3. Rebooting emulator..."
adb reboot

echo "⏳ Waiting for emulator to reboot..."
adb wait-for-device
while [ "$(adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" != "1" ]; do
    sleep 2
done

echo "🔓 4. Re-granting root permissions post-reboot..."
adb root
sleep 2

echo "📂 5. Remounting system partitions with write permissions..."
adb remount

echo ""
echo "======================================================================"
echo "🎉 SUCCESS: System and vendor partitions have been successfully remounted as writable!"
echo "👉 You are now ready for Step 3 & Step 4 (Build & deploy your native service/binaries)."
echo "======================================================================"
