#!/usr/bin/env bash
set -e

# ==============================================================================
# Android Emulator Launcher with -writable-system flag
# Designed for AOSP / Android Automotive Architecture Development
# ==============================================================================

# Locate ANDROID_HOME if not already exported
if [ -z "$ANDROID_HOME" ]; then
    if [ -d "$HOME/Library/Android/sdk" ]; then
        export ANDROID_HOME="$HOME/Library/Android/sdk"
    else
        echo "❌ Error: Android SDK not found. Please install Android SDK or set ANDROID_HOME."
        exit 1
    fi
fi

EMULATOR_BIN="$ANDROID_HOME/emulator/emulator"

if [ ! -x "$EMULATOR_BIN" ]; then
    echo "❌ Error: Emulator binary not found or not executable at: $EMULATOR_BIN"
    exit 1
fi

# Default AVD name or override via first argument ($1)
DEFAULT_AVD="Automotive_1408p_landscape"
AVD_NAME="${1:-$DEFAULT_AVD}"

# Verify whether the target AVD exists
AVAILABLE_AVDS=$("$EMULATOR_BIN" -list-avds)
if ! echo "$AVAILABLE_AVDS" | grep -q "^${AVD_NAME}$"; then
    echo "⚠️  Warning: AVD '$AVD_NAME' is not found in the list of available AVDs:"
    echo "$AVAILABLE_AVDS"
    echo ""
    echo "👉 Please verify your AVD name or create one via Android Studio / avdmanager."
    exit 1
fi

echo "======================================================================"
echo "🚀 Launching AVD: $AVD_NAME"
echo "🔧 Mode: -writable-system (Required for root, disable-verity & remount)"
echo "======================================================================"
echo "💡 Next steps once the emulator finishes booting to home screen:"
echo "   Option 1: Run ./scripts/unlock_partitions.sh in a new terminal"
echo "   Option 2: Manually run:"
echo "             adb root"
echo "             adb disable-verity"
echo "             adb reboot"
echo "             (after reboot) adb root && adb remount"
echo "======================================================================"
echo ""

# Launch emulator with -writable-system flag
exec "$EMULATOR_BIN" -avd "$AVD_NAME" -writable-system -gpu auto
