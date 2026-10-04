#!/usr/bin/env bash
set -e

# ==============================================================================
# Cross-compile native/ with the Android NDK (no AOSP tree needed).
# Output: native/build-android/{bp-candump,bp-canbridge,*_test}
# Usage: ./scripts/build_android.sh [ABI]      ABI: arm64-v8a (default) | x86_64
# ==============================================================================

ABI="${1:-arm64-v8a}"
SDK="${ANDROID_HOME:-$HOME/Library/Android/sdk}"
NDK="${ANDROID_NDK_HOME:-$(ls -d "$SDK"/ndk/* 2>/dev/null | sort -V | tail -1)}"
API=35
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/native/build-android"

if [ ! -f "$NDK/build/cmake/android.toolchain.cmake" ]; then
    echo "❌ NDK not found. Install one: sdkmanager 'ndk;29.0.14206865' or set ANDROID_NDK_HOME"
    exit 1
fi

echo "🔧 NDK $(basename "$NDK"), ABI $ABI, API $API"
# c++_static: binaries carry their own libc++, so a plain `adb push` is enough.
cmake -S "$ROOT/native" -B "$OUT" \
    -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI="$ABI" \
    -DANDROID_PLATFORM="android-$API" \
    -DANDROID_STL=c++_static \
    -DCMAKE_BUILD_TYPE=Release > /dev/null
cmake --build "$OUT" -j8 --target bp-candump bp-canbridge cansignal_test cantransport_test canbridge_test

echo "✅ $OUT"
