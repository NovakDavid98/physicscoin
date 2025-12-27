#!/bin/bash
# start-testnet.sh - Start a 3-node PhysicsCoin testnet
# This script starts 3 interconnected testnet nodes

echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║     PHYSICSCOIN 3-NODE TESTNET LAUNCHER                       ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""

# Clean up any existing nodes
echo "Cleaning up existing testnet nodes..."
pkill -f "physicscoin.*testnet" 2>/dev/null
rm -rf testnet_node_* 2>/dev/null
sleep 1

# Build if needed
if [ ! -f "physicscoin" ]; then
    echo "Building PhysicsCoin..."
    make clean && make all
    if [ $? -ne 0 ]; then
        echo "❌ Build failed!"
        exit 1
    fi
fi

echo ""
echo "Starting 3-node testnet..."
echo ""

# Start Node 1 (seed node)
echo "▶ Starting Node 1 (Seed Node)..."
mkdir -p testnet_node_1
cd testnet_node_1
../physicscoin --network testnet api serve 18545 > api.log 2>&1 &
NODE1_API_PID=$!
cd ..
sleep 2

# Start Node 2
echo "▶ Starting Node 2..."
mkdir -p testnet_node_2
cd testnet_node_2
../physicscoin --network testnet api serve 18546 > api.log 2>&1 &
NODE2_API_PID=$!
cd ..
sleep 2

# Start Node 3
echo "▶ Starting Node 3..."
mkdir -p testnet_node_3
cd testnet_node_3
../physicscoin --network testnet api serve 18547 > api.log 2>&1 &
NODE3_API_PID=$!
cd ..
sleep 2

echo ""
echo "✅ Testnet Started Successfully!"
echo ""
echo "Node 1: http://localhost:18545"
echo "Node 2: http://localhost:18546"
echo "Node 3: http://localhost:18547"
echo ""
echo "Quick Test:"
echo "  curl http://localhost:18545/status | jq ."
echo "  curl http://localhost:18545/faucet/info | jq ."
echo ""
echo "Stop testnet:"
echo "  ./scripts/stop-testnet.sh"
echo ""

# Save PIDs
echo "$NODE1_API_PID" > testnet_node_1/api.pid
echo "$NODE2_API_PID" > testnet_node_2/api.pid
echo "$NODE3_API_PID" > testnet_node_3/api.pid

echo "All nodes running. Check logs:"
echo "  tail -f testnet_node_1/api.log"
echo "  tail -f testnet_node_2/api.log"
echo "  tail -f testnet_node_3/api.log"
echo ""

