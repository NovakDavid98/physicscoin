// gossip_demo.c - Demonstration of gossip protocol
// Shows how nodes sync with ~100 bytes instead of full state

#include "../include/physicscoin.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations from gossip.c
typedef struct {
    uint8_t pubkey[32];
    double old_balance;
    double new_balance;
    uint64_t old_nonce;
    uint64_t new_nonce;
} PCWalletDelta;

typedef struct {
    uint8_t prev_hash[32];
    uint8_t new_hash[32];
    uint64_t prev_timestamp;
    uint64_t new_timestamp;
    uint32_t num_changes;
    PCWalletDelta changes[1000];
} PCStateDelta;

typedef struct {
    uint8_t message_id[16];
    uint8_t sender_node[32];
    uint64_t timestamp;
    PCStateDelta delta;
    uint8_t signature[64];
} PCGossipMessage;

typedef struct {
    uint8_t node_id[32];
    char ip_address[64];
    uint16_t port;
    uint64_t last_seen;
    uint8_t last_known_hash[32];
} PCPeer;

typedef struct {
    PCPeer peers[100];
    uint32_t num_peers;
    uint8_t seen_messages[1000][16];
    uint32_t num_seen_messages;
} PCGossipNetwork;

// Function declarations
PCError pc_gossip_init(PCGossipNetwork* network);
PCError pc_gossip_add_peer(PCGossipNetwork* network, const uint8_t* node_id, const char* ip, uint16_t port);
PCError pc_gossip_create_message(const PCStateDelta* delta, const uint8_t* sender_node, PCGossipMessage* msg);
PCError pc_gossip_broadcast(PCGossipNetwork* network, const PCGossipMessage* msg);
PCError pc_gossip_receive(PCGossipNetwork* network, PCState* state, const PCGossipMessage* msg);
void pc_gossip_print_stats(const PCGossipNetwork* network);
size_t pc_gossip_bandwidth(const PCGossipMessage* msg);

PCError pc_delta_compute(const PCState* before, const PCState* after, PCStateDelta* delta);

int main(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║              GOSSIP PROTOCOL DEMO                             ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
    
    // Create 3 simulated nodes
    PCGossipNetwork node1, node2, node3;
    pc_gossip_init(&node1);
    pc_gossip_init(&node2);
    pc_gossip_init(&node3);
    
    uint8_t node1_id[32] = {0x01};
    uint8_t node2_id[32] = {0x02};
    uint8_t node3_id[32] = {0x03};
    
    // Set up peer relationships
    pc_gossip_add_peer(&node1, node2_id, "192.168.1.2", 9000);
    pc_gossip_add_peer(&node1, node3_id, "192.168.1.3", 9000);
    
    pc_gossip_add_peer(&node2, node1_id, "192.168.1.1", 9000);
    pc_gossip_add_peer(&node2, node3_id, "192.168.1.3", 9000);
    
    pc_gossip_add_peer(&node3, node1_id, "192.168.1.1", 9000);
    pc_gossip_add_peer(&node3, node2_id, "192.168.1.2", 9000);
    
    printf("═══ Network Topology ═══\n\n");
    printf("Node 1 (192.168.1.1)\n");
    pc_gossip_print_stats(&node1);
    printf("\n");
    
    // Create identical genesis states on all nodes
    PCKeypair alice, bob;
    pc_keypair_generate(&alice);
    pc_keypair_generate(&bob);
    
    PCState state1, state2, state3;
    pc_state_genesis(&state1, alice.public_key, 1000.0);
    pc_state_create_wallet(&state1, bob.public_key, 0);
    
    // Clone to other nodes
    memcpy(&state2, &state1, sizeof(PCState));
    state2.wallets = malloc(state1.num_wallets * sizeof(PCWallet));
    memcpy(state2.wallets, state1.wallets, state1.num_wallets * sizeof(PCWallet));
    
    memcpy(&state3, &state1, sizeof(PCState));
    state3.wallets = malloc(state1.num_wallets * sizeof(PCWallet));
    memcpy(state3.wallets, state1.wallets, state1.num_wallets * sizeof(PCWallet));
    
    printf("═══ Initial State (All Nodes) ═══\n");
    printf("Alice: %.2f\n", pc_state_get_wallet(&state1, alice.public_key)->energy);
    printf("Bob: %.2f\n", pc_state_get_wallet(&state1, bob.public_key)->energy);
    printf("Hash: ");
    for (int i = 0; i < 8; i++) printf("%02x", state1.state_hash[i]);
    printf("...\n\n");
    
    // Node 1 executes a transaction
    printf("═══ Node 1: Executing Transaction ═══\n");
    PCState state_before = state1;
    state_before.wallets = malloc(state1.num_wallets * sizeof(PCWallet));
    memcpy(state_before.wallets, state1.wallets, state1.num_wallets * sizeof(PCWallet));
    
    PCTransaction tx = {0};
    memcpy(tx.from, alice.public_key, 32);
    memcpy(tx.to, bob.public_key, 32);
    tx.amount = 100.0;
    tx.nonce = 0;
    tx.timestamp = time(NULL);
    pc_transaction_sign(&tx, &alice);
    
    pc_state_execute_tx(&state1, &tx);
    
    printf("Transaction: Alice → Bob : 100.0\n");
    printf("New state hash: ");
    for (int i = 0; i < 8; i++) printf("%02x", state1.state_hash[i]);
    printf("...\n\n");
    
    // Compute delta
    PCStateDelta delta;
    pc_delta_compute(&state_before, &state1, &delta);
    
    printf("═══ Creating Gossip Message ═══\n");
    PCGossipMessage msg;
    pc_gossip_create_message(&delta, node1_id, &msg);
    
    size_t bandwidth = pc_gossip_bandwidth(&msg);
    printf("Message size: %zu bytes\n", bandwidth);
    printf("Changes: %u wallets\n\n", msg.delta.num_changes);
    
    // Broadcast
    printf("═══ Broadcasting to Network ═══\n");
    pc_gossip_broadcast(&node1, &msg);
    
    printf("\n═══ Nodes Receiving Update ═══\n\n");
    
    // Node 2 receives
    printf("Node 2 receiving...\n");
    pc_gossip_receive(&node2, &state2, &msg);
    printf("  New hash: ");
    for (int i = 0; i < 8; i++) printf("%02x", state2.state_hash[i]);
    printf("...\n\n");
    
    // Node 3 receives
    printf("Node 3 receiving...\n");
    pc_gossip_receive(&node3, &state3, &msg);
    printf("  New hash: ");
    for (int i = 0; i < 8; i++) printf("%02x", state3.state_hash[i]);
    printf("...\n\n");
    
    // Verify all nodes in sync
    printf("═══ Verification ═══\n");
    int in_sync = 1;
    in_sync &= (memcmp(state1.state_hash, state2.state_hash, 32) == 0);
    in_sync &= (memcmp(state1.state_hash, state3.state_hash, 32) == 0);
    
    if (in_sync) {
        printf("✓ All nodes in perfect sync!\n");
    } else {
        printf("✗ Nodes out of sync\n");
    }
    
    printf("\nBandwidth Comparison:\n");
    printf("  Full state: %zu bytes\n", sizeof(PCState) + state1.num_wallets * sizeof(PCWallet));
    printf("  Gossip delta: %zu bytes\n", bandwidth);
    printf("  Savings: %.1f%%\n\n", 
           100.0 * (1.0 - (double)bandwidth / (sizeof(PCState) + state1.num_wallets * sizeof(PCWallet))));
    
    // Cleanup
    free(state_before.wallets);
    pc_state_free(&state1);
    pc_state_free(&state2);
    pc_state_free(&state3);
    
    return 0;
}
