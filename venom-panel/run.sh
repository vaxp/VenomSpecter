#!/bin/bash

cd "$(dirname "$0")"

echo "Starting venom-style panel..."
./venom-panel &

echo "Panel started! Press Ctrl+C to stop."
echo ""
echo "Features:"
echo "  - System icons (WiFi, Bluetooth, Battery)"
echo "  - Clock with date"
echo "  - Control Center (click grid icon)"
echo ""
echo "To stop: pkill -f venom-panel"
