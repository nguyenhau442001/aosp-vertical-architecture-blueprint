#!/usr/bin/env bash
set -e

# ==============================================================================
# Push NDK-built native/ binaries to a device/emulator and run them.
# /data/local/tmp needs no root and no remount.
#
#   ./scripts/android_native.sh push      copy binaries to the device
#   ./scripts/android_native.sh test      push + run every gtest on the device
#   ./scripts/android_native.sh bridge    push + adb reverse + run bp-canbridge on the device
#   ./scripts/android_native.sh dump      push + adb reverse + run bp-candump on the device
# ==============================================================================

CMD="${1:-push}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/native/build-android"
DEST=/data/local/tmp/blueprint
PORT="${CANSIM_PORT:-29536}"
BINS="bp-candump bp-canbridge cansignal_test cantransport_test canbridge_test"

push() {
    for b in $BINS; do
        [ -f "$OUT/$b" ] || { echo "❌ $OUT/$b missing, run ./scripts/build_android.sh"; exit 1; }
    done
    adb wait-for-device
    adb shell mkdir -p "$DEST"
    for b in $BINS; do
        adb push "$OUT/$b" "$DEST/$b" > /dev/null 2>&1 || { echo "❌ push $b failed"; exit 1; }
    done
    adb shell chmod 755 "$DEST"/*
    echo "📦 pushed to $DEST"
}

reverse() {
    # Device 127.0.0.1:$PORT -> host 127.0.0.1:$PORT (where cansim serve listens)
    adb reverse "tcp:$PORT" "tcp:$PORT" > /dev/null
    echo "🔁 adb reverse tcp:$PORT -> host (start: cd tools/cansim && python3 -m cansim serve THERMAL_STATUS ...)"
}

case "$CMD" in
    push)
        push
        ;;
    test)
        push
        status=0
        for t in cansignal_test cantransport_test canbridge_test; do
            echo "🧪 $t"
            adb shell "cd $DEST && ./$t --gtest_brief=1" || status=1
        done
        exit $status
        ;;
    bridge)
        push
        reverse
        adb shell "$DEST/bp-canbridge tcp:127.0.0.1:$PORT"
        ;;
    dump)
        push
        reverse
        adb shell "$DEST/bp-candump tcp:127.0.0.1:$PORT"
        ;;
    *)
        echo "usage: $0 push|test|bridge|dump"
        exit 2
        ;;
esac
