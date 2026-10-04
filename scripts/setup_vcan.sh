#!/usr/bin/env bash
set -e

# ==============================================================================
# Create a virtual CAN interface on Linux (UTM VM / any Linux host).
# Usage: ./scripts/setup_vcan.sh [IFACE]
# ==============================================================================

IFACE="${1:-vcan0}"

if [ "$(uname -s)" != "Linux" ]; then
    echo "❌ SocketCAN needs Linux. On macOS use: python3 -m cansim serve ... (TCP)"
    exit 1
fi

sudo modprobe vcan
if ! ip link show "$IFACE" > /dev/null 2>&1; then
    sudo ip link add dev "$IFACE" type vcan
fi
sudo ip link set up "$IFACE"

echo "✅ $IFACE is up. Try: candump $IFACE"
