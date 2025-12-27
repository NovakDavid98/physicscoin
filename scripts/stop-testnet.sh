#!/bin/bash
# stop-testnet.sh - Stop all testnet nodes

echo "Stopping PhysicsCoin testnet..."

# Kill all physicscoin processes with testnet
pkill -f "physicscoin.*testnet"

# Also kill by PID files if they exist
for i in 1 2 3; do
    if [ -f "testnet_node_${i}/api.pid" ]; then
        PID=$(cat "testnet_node_${i}/api.pid")
        kill $PID 2>/dev/null
    fi
    if [ -f "testnet_node_${i}/node.pid" ]; then
        PID=$(cat "testnet_node_${i}/node.pid")
        kill $PID 2>/dev/null
    fi
done

sleep 1
echo "✓ Testnet stopped"

