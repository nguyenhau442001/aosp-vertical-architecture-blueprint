#!/usr/bin/env bash
set -e

# ==============================================================================
# Replace the emulator's VHAL with the Blueprint CAN VHAL (development only).
# Usage: ./scripts/deploy_vhal.sh path/to/android.hardware.automotive.vehicle@V3-blueprint-can-service
#
# Prerequisites: `make run` done (root + writable /vendor).
# Feed data afterwards from the host:
#   cd tools/cansim && python3 -m cansim serve THERMAL_STATUS --ramp OutsideTemp:-10:40:20
# ==============================================================================

BIN="${1:?usage: $0 <path to blueprint VHAL binary>}"
NAME="$(basename "$BIN")"
PORT="${CANSIM_PORT:-29536}"
VHAL="android.hardware.automotive.vehicle.IVehicle/default"

adb root > /dev/null
adb wait-for-device
adb remount > /dev/null

echo "📦 Pushing $NAME"
adb push "$BIN" "/vendor/bin/hw/$NAME" > /dev/null
adb shell chmod 755 "/vendor/bin/hw/$NAME"

echo "🔓 SELinux permissive (pushed binary has no vendor sepolicy yet)"
adb shell setenforce 0

echo "🔁 adb reverse tcp:$PORT -> host cansim"
adb reverse "tcp:$PORT" "tcp:$PORT"

# Stop whatever VHAL init started (emulator: *vehicle-hal*).
for svc in $(adb shell getprop | tr -d '\r' | sed -n 's/^\[init\.svc\.\(.*vehicle.*\)\]: \[running\]$/\1/p'); do
    echo "⏹  stop $svc"
    adb shell stop "$svc"
done

echo "▶️  Starting $NAME"
adb shell "nohup /vendor/bin/hw/$NAME > /dev/null 2>&1 &"
sleep 2

# CarService holds a binder to the old VHAL; it restarts itself when that dies.
# If properties look stale, restart the framework: adb shell stop && adb shell start
adb shell dumpsys "$VHAL" --can-status
