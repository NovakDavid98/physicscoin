// gossip.c - Gossip Protocol for Delta Sync
// Nodes broadcast only deltas (~100 bytes) instead of full states

#include "../include/physicscoin.h"
#include "../crypto/sha256.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

// Forward declaration from delta.c
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

PCError pc_delta_compute(const PCState* before, const PCState* after, PCStateDelta* delta);
size_t pc_delta_serialize(const PCStateDelta* delta, uint8_t* buffer, size_t max);
PCError pc_delta_deserialize(PCStateDelta* delta, const uint8_t* buffer, size_t size);
PCError pc_delta_apply(PCState* state, const PCStateDelta* delta);

// Gossip message structure
typedef struct {
    uint8_t message_id[16];       // Unique message ID
    uint8_t sender_node[32];      // Who sent this
    uint64_t timestamp;           // When sent
    PCStateDelta delta;           // The actual state delta
    uint8_t signature[64];        // Signature over delta
} PCGossipMessage;

// Peer node
typedef struct {
    uint8_t node_id[32];
    char ip_address[64];
    uint16_t port;
    uint64_t last_seen;
    uint8_t last_known_hash[32];
} PCPeer;

// Gossip network
typedef struct {
    PCPeer peers[100];
    uint32_t num_peers;
    uint8_t seen_messages[1000][16];  // Track which messages we've seen
    uint32_t num_seen_messages;
} PCGossipNetwork;

// Initialize gossip network
PCError pc_gossip_init(PCGossipNetwork* network) {
    if (!network) return PC_ERR_IO;
    
    memset(network, 0, sizeof(PCGossipNetwork));
    network->num_peers = 0;
    network->num_seen_messages = 0;
    
    return PC_OK;
}

// Add peer to network
PCError pc_gossip_add_peer(PCGossipNetwork* network, const uint8_t* node_id, 
                           const char* ip, uint16_t port) {
    if (!network || network->num_peers >= 100) return PC_ERR_MAX_WALLETS;
    
    PCPeer* peer = &network->peers[network->num_peers];
    memcpy(peer->node_id, node_id, 32);
    strncpy(peer->ip_address, ip, sizeof(peer->ip_address) - 1);
    peer->port = port;
    peer->last_seen = (uint64_t)time(NULL);
    
    network->num_peers++;
    
    return PC_OK;
}

// Check if message already seen (prevent loops)
static int is_message_seen(PCGossipNetwork* network, const uint8_t* message_id) {
    for (uint32_t i = 0; i < network->num_seen_messages; i++) {
        if (memcmp(network->seen_messages[i], message_id, 16) == 0) {
            return 1;
        }
    }
    return 0;
}

// Mark message as seen
static void mark_message_seen(PCGossipNetwork* network, const uint8_t* message_id) {
    if (network->num_seen_messages < 1000) {
        memcpy(network->seen_messages[network->num_seen_messages], message_id, 16);
        network->num_seen_messages++;
    }
}

// Create gossip message from state delta
PCError pc_gossip_create_message(const PCStateDelta* delta,
                                  const uint8_t* sender_node,
                                  PCGossipMessage* msg) {
    if (!delta || !sender_node || !msg) return PC_ERR_IO;
    
    // Generate random message ID
    for (int i = 0; i < 16; i++) {
        msg->message_id[i] = rand() & 0xFF;
    }
    
    memcpy(msg->sender_node, sender_node, 32);
    msg->timestamp = (uint64_t)time(NULL);
    memcpy(&msg->delta, delta, sizeof(PCStateDelta));
    
    // Sign message (simplified - hash of delta)
    uint8_t delta_buf[65536];
    size_t delta_size = pc_delta_serialize(delta, delta_buf, sizeof(delta_buf));
    
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, delta_buf, delta_size);
    sha256_update(&ctx, msg->message_id, 16);
    sha256_update(&ctx, sender_node, 32);
    uint8_t hash[32];
    sha256_final(&ctx, hash);
    
    memcpy(msg->signature, hash, 32);
    memcpy(msg->signature + 32, hash, 32);  // Simplified signature
    
    return PC_OK;
}

// Broadcast message to all peers (simulation)
PCError pc_gossip_broadcast(PCGossipNetwork* network, const PCGossipMessage* msg) {
    if (!network || !msg) return PC_ERR_IO;
    
    // Check if already seen
    if (is_message_seen(network, msg->message_id)) {
        return PC_OK;  // Don't rebroadcast
    }
    
    mark_message_seen(network, msg->message_id);
    
    printf("Broadcasting delta to %u peers:\n", network->num_peers);
    printf("  Message ID: ");
    for (int i = 0; i < 8; i++) printf("%02x", msg->message_id[i]);
    printf("...\n");
    printf("  Delta size: %u changes\n", msg->delta.num_changes);
    
    // Simulate sending to each peer
    for (uint32_t i = 0; i < network->num_peers; i++) {
        PCPeer* peer = &network->peers[i];
        printf("  → Peer %.8s... (%s:%u)\n", 
               peer->node_id, peer->ip_address, peer->port);
    }
    
    return PC_OK;
}

// Receive and process message
PCError pc_gossip_receive(PCGossipNetwork* network, PCState* state, const PCGossipMessage* msg) {
    if (!network || !state || !msg) return PC_ERR_IO;
    
    // Check if already seen
    if (is_message_seen(network, msg->message_id)) {
        return PC_OK;  // Ignore duplicate
    }
    
    mark_message_seen(network, msg->message_id);
    
    printf("Received gossip message:\n");
    printf("  From: %.8s...\n", msg->sender_node);
    printf("  Changes: %u wallets\n", msg->delta.num_changes);
    
    // Verify signature (simplified)
    // In production, verify ECDSA signature here
    
    // Apply delta to state
    PCError err = pc_delta_apply(state, &msg->delta);
    if (err != PC_OK) {
        printf("  ✗ Failed to apply delta: %s\n", pc_strerror(err));
        return err;
    }
    
    printf("  ✓ Delta applied successfully\n");
    
    // Rebroadcast to other peers (gossip propagation)
    for (uint32_t i = 0; i < network->num_peers; i++) {
        // Don't send back to sender
        if (memcmp(network->peers[i].node_id, msg->sender_node, 32) != 0) {
            printf("  → Forwarding to %.8s...\n", network->peers[i].node_id);
        }
    }
    
    return PC_OK;
}

// Calculate bandwidth usage
size_t pc_gossip_bandwidth(const PCGossipMessage* msg) {
    size_t total = 0;
    
    total += 16;  // message_id
    total += 32;  // sender_node
    total += 8;   // timestamp
    total += 64;  // signature
    
    // Delta size
    uint8_t delta_buf[65536];
    size_t delta_size = pc_delta_serialize(&msg->delta, delta_buf, sizeof(delta_buf));
    total += delta_size;
    
    return total;
}

// Sync with peer (request their state if we're behind)
PCError pc_gossip_sync_with_peer(PCGossipNetwork* network, PCState* local_state, 
                                  uint32_t peer_index) {
    if (!network || !local_state || peer_index >= network->num_peers) {
        return PC_ERR_IO;
    }
    
    PCPeer* peer = &network->peers[peer_index];
    
    printf("Syncing with peer %.8s... (%s:%u)\n", 
           peer->node_id, peer->ip_address, peer->port);
    
    // Compare state hashes
    if (memcmp(local_state->state_hash, peer->last_known_hash, 32) == 0) {
        printf("  ✓ Already in sync\n");
        return PC_OK;
    }
    
    printf("  State mismatch - requesting delta\n");
    printf("    Local:  ");
    for (int i = 0; i < 8; i++) printf("%02x", local_state->state_hash[i]);
    printf("...\n");
    printf("    Remote: ");
    for (int i = 0; i < 8; i++) printf("%02x", peer->last_known_hash[i]);
    printf("...\n");
    
    // In production, this would:
    // 1. Request delta from peer
    // 2. Receive serialized delta
    // 3. Apply to local state
    
    return PC_OK;
}

// Print network stats
void pc_gossip_print_stats(const PCGossipNetwork* network) {
    printf("Gossip Network Statistics:\n");
    printf("  Peers: %u\n", network->num_peers);
    printf("  Messages seen: %u\n", network->num_seen_messages);
    
    printf("\nPeers:\n");
    for (uint32_t i = 0; i < network->num_peers; i++) {
        const PCPeer* peer = &network->peers[i];
        printf("  [%u] %.8s... @ %s:%u (last seen: %lu)\n",
               i, peer->node_id, peer->ip_address, peer->port, peer->last_seen);
    }
}
