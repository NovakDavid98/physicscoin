#!/bin/bash
# testnet-node.sh - Start a PhysicsCoin testnet node
# Usage: ./testnet-node.sh <node_number> [seed_address]

NODE_NUM=${1:-1}
SEED_NODE=${2:-""}

# Configuration
NETWORK="testnet"
BASE_DIR="testnet_node_${NODE_NUM}"
P2P_PORT=$((19333 + NODE_NUM - 1))
API_PORT=$((18545 + NODE_NUM - 1))

echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║     PHYSICSCOIN TESTNET NODE ${NODE_NUM}                         "
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""
echo "Configuration:"
echo "  Network:    ${NETWORK}"
echo "  Data Dir:   ${BASE_DIR}"
echo "  P2P Port:   ${P2P_PORT}"
echo "  API Port:   ${API_PORT}"
if [ -n "$SEED_NODE" ]; then
    echo "  Seed Node:  ${SEED_NODE}"
fi
echo ""

# Create node directory
mkdir -p "$BASE_DIR"
cd "$BASE_DIR"

# Copy binary if needed
if [ ! -f "physicscoin" ]; then
    cp ../physicscoin .
fi

# Start the node
if [ -n "$SEED_NODE" ]; then
    echo "Starting node with seed connection to ${SEED_NODE}..."
    ./physicscoin --network "$NETWORK" node start --port "$P2P_PORT" --connect "$SEED_NODE" &
else
    echo "Starting standalone node..."
    ./physicscoin --network "$NETWORK" node start --port "$P2P_PORT" &
fi
NODE_PID=$!

# Give it a moment to start
sleep 2

# Start API server
echo "Starting API server on port ${API_PORT}..."
./physicscoin --network "$NETWORK" api serve "$API_PORT" > api_${NODE_NUM}.log 2>&1 &
API_PID=$!

echo ""
echo "✓ Node started successfully!"
echo "  Node PID:   ${NODE_PID}"
echo "  API PID:    ${API_PID}"
echo "  API URL:    http://localhost:${API_PORT}"
echo ""
echo "View logs:"
echo "  tail -f ${BASE_DIR}/api_${NODE_NUM}.log"
echo ""
echo "Stop node:"
echo "  kill ${NODE_PID} ${API_PID}"
echo ""

# Save PIDs for later
echo "${NODE_PID}" > node.pid
echo "${API_PID}" > api.pid

# Keep script running to show logs
echo "Press Ctrl+C to stop tailing logs (node will keep running)"
sleep 2
tail -f api_${NODE_NUM}.log

