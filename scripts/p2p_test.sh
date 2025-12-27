#!/bin/bash
# p2p_test.sh - Test two nodes connecting

echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║           P2P TWO-NODE CONNECTION TEST                        ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""

# Cleanup
rm -f node1_state.pcs node2_state.pcs

# Start Node 1
echo "Starting Node 1 on port 9333..."
./physicscoin node start --port 9333 &
NODE1_PID=$!
sleep 2

# Start Node 2 and connect to Node 1
echo ""
echo "Starting Node 2 on port 9334, connecting to Node 1..."
./physicscoin node start --port 9334 --connect 127.0.0.1:9333 &
NODE2_PID=$!
sleep 3

echo ""
echo "Nodes running for 5 seconds..."
sleep 5

# Stop nodes
echo ""
echo "Stopping nodes..."
kill $NODE1_PID 2>/dev/null
kill $NODE2_PID 2>/dev/null
wait

echo ""
echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║  TEST COMPLETE                                                ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
