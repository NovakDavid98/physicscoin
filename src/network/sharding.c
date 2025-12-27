// sharding.c - Wallet-Based Sharding
// Horizontal scaling by partitioning wallets into independent shards

#include "../include/physicscoin.h"
#include "../crypto/sha256.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#define NUM_SHARDS 16

// Shard structure
typedef struct {
    uint8_t shard_id;                // 0x0 to 0xF
    PCState local_state;              // This shard's state
    uint8_t shard_hash[32];          // Hash of this shard
    uint64_t transaction_count;       // Metrics
} PCShard;

// Sharded network
typedef struct {
    PCShard shards[NUM_SHARDS];
    uint32_t num_shards;
    double total_supply;              // Sum across all shards
} PCShardedNetwork;

// Determine which shard a wallet belongs to
static uint8_t get_shard_for_wallet(const uint8_t* pubkey) {
    // Use first byte of pubkey to determine shard (0x0-0xF)
    return pubkey[0] >> 4;  // Top 4 bits = shard ID
}

// Initialize sharded network
PCError pc_sharding_init(PCShardedNetwork* network, double initial_supply) {
    if (!network) return PC_ERR_IO;
    
    memset(network, 0, sizeof(PCShardedNetwork));
    network->num_shards = NUM_SHARDS;
    network->total_supply = initial_supply;
    
    // Initialize each shard with empty state
    for (int i = 0; i < NUM_SHARDS; i++) {
        network->shards[i].shard_id = i;
        
        // Create empty state for this shard
        PCState* state = &network->shards[i].local_state;
        memset(state, 0, sizeof(PCState));  // Zero everything first
        state->version = 1;
        state->total_supply = 0;  // Each shard starts with 0
        state->num_wallets = 0;
        state->timestamp = (uint64_t)time(NULL);
        state->wallets = NULL;
        state->wallets_capacity = 0;  // Start with 0, will grow on first wallet
        
        // Initialize hashes to zero
        memset(state->state_hash, 0, 32);
        memset(state->prev_hash, 0, 32);
        
        // Compute initial hash
        pc_state_compute_hash(state);
        memcpy(network->shards[i].shard_hash, state->state_hash, 32);
        network->shards[i].transaction_count = 0;
    }
    
    return PC_OK;
}

// Get shard for wallet
PCShard* pc_sharding_get_shard(PCShardedNetwork* network, const uint8_t* pubkey) {
    uint8_t shard_id = get_shard_for_wallet(pubkey);
    return &network->shards[shard_id];
}

// Create wallet in appropriate shard
PCError pc_sharding_create_wallet(PCShardedNetwork* network, const uint8_t* pubkey, double balance) {
    if (!network || !pubkey) return PC_ERR_IO;
    
    PCShard* shard = pc_sharding_get_shard(network, pubkey);
    PCError err = pc_state_create_wallet(&shard->local_state, pubkey, balance);
    
    if (err == PC_OK) {
        pc_state_compute_hash(&shard->local_state);
        memcpy(shard->shard_hash, shard->local_state.state_hash, 32);
    }
    
    return err;
}

// Execute transaction within a shard (intra-shard)
PCError pc_sharding_execute_intra_tx(PCShardedNetwork* network, const PCTransaction* tx) {
    if (!network || !tx) return PC_ERR_IO;
    
    // Check both wallets in same shard
    uint8_t from_shard = get_shard_for_wallet(tx->from);
    uint8_t to_shard = get_shard_for_wallet(tx->to);
    
    if (from_shard != to_shard) {
        return PC_ERR_INVALID_SIGNATURE;  // Must be same shard
    }
    
    PCShard* shard = &network->shards[from_shard];
    PCError err = pc_state_execute_tx(&shard->local_state, tx);
    
    if (err == PC_OK) {
        shard->transaction_count++;
        pc_state_compute_hash(&shard->local_state);
        memcpy(shard->shard_hash, shard->local_state.state_hash, 32);
    }
    
    return err;
}

// Execute cross-shard transaction (2-phase commit)
PCError pc_sharding_execute_cross_tx(PCShardedNetwork* network, const PCTransaction* tx) {
    if (!network || !tx) return PC_ERR_IO;
    
    uint8_t from_shard_id = get_shard_for_wallet(tx->from);
    uint8_t to_shard_id = get_shard_for_wallet(tx->to);
    
    if (from_shard_id == to_shard_id) {
        return PC_ERR_INVALID_SIGNATURE;  // Use intra-shard for this
    }
    
    PCShard* from_shard = &network->shards[from_shard_id];
    PCShard* to_shard = &network->shards[to_shard_id];
    
    printf("Cross-shard TX: Shard %u → Shard %u (%.2f coins)\n", 
           from_shard_id, to_shard_id, tx->amount);
    
    // PHASE 1: Prepare (lock funds in source shard)
    PCWallet* sender = pc_state_get_wallet(&from_shard->local_state, tx->from);
    if (!sender) return PC_ERR_WALLET_NOT_FOUND;
    
    if (sender->energy < tx->amount) {
        return PC_ERR_INSUFFICIENT_FUNDS;
    }
    
    // Deduct from sender
    sender->energy -= tx->amount;
    sender->nonce++;
    from_shard->local_state.total_supply -= tx->amount;
    
    printf("  Phase 1: Locked %.2f in shard %u\n", tx->amount, from_shard_id);
    
    // PHASE 2: Commit (add to destination shard)
    PCWallet* receiver = pc_state_get_wallet(&to_shard->local_state, tx->to);
    if (!receiver) {
        pc_state_create_wallet(&to_shard->local_state, tx->to, 0);
        receiver = pc_state_get_wallet(&to_shard->local_state, tx->to);
    }
    
    receiver->energy += tx->amount;
    to_shard->local_state.total_supply += tx->amount;
    
    printf("  Phase 2: Added %.2f to shard %u\n", tx->amount, to_shard_id);
    
    // Update both shards
    from_shard->transaction_count++;
    to_shard->transaction_count++;
    
    pc_state_compute_hash(&from_shard->local_state);
    pc_state_compute_hash(&to_shard->local_state);
    
    memcpy(from_shard->shard_hash, from_shard->local_state.state_hash, 32);
    memcpy(to_shard->shard_hash, to_shard->local_state.state_hash, 32);
    
    printf("  ✓ Cross-shard transaction complete\n");
    
    return PC_OK;
}

// Get balance from appropriate shard
PCError pc_sharding_get_balance(PCShardedNetwork* network, const uint8_t* pubkey, double* balance) {
    if (!network || !pubkey || !balance) return PC_ERR_IO;
    
    PCShard* shard = pc_sharding_get_shard(network, pubkey);
    PCWallet* wallet = pc_state_get_wallet(&shard->local_state, pubkey);
    
    if (!wallet) {
        *balance = 0.0;
        return PC_ERR_WALLET_NOT_FOUND;
    }
    
    *balance = wallet->energy;
    return PC_OK;
}

// Verify conservation across all shards
PCError pc_sharding_verify_conservation(const PCShardedNetwork* network) {
    if (!network) return PC_ERR_IO;
    
    double total = 0.0;
    
    for (int i = 0; i < NUM_SHARDS; i++) {
        total += network->shards[i].local_state.total_supply;
    }
    
    double error = fabs(total - network->total_supply);
    
    if (error > 1e-9) {
        return PC_ERR_CONSERVATION_VIOLATED;
    }
    
    return PC_OK;
}

// Print shard statistics
void pc_sharding_print_stats(const PCShardedNetwork* network) {
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║              SHARDED NETWORK STATISTICS                       ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
    
    printf("Shards: %u\n", network->num_shards);
    printf("Total Supply: %.8f\n\n", network->total_supply);
    
    printf("Per-Shard Breakdown:\n");
    printf("┌──────┬──────────┬───────────────┬──────────┬────────────────┐\n");
    printf("│ ID   │ Wallets  │ Supply        │ TX Count │ Hash           │\n");
    printf("├──────┼──────────┼───────────────┼──────────┼────────────────┤\n");
    
    double verified_total = 0.0;
    uint64_t total_tx = 0;
    
    for (int i = 0; i < NUM_SHARDS; i++) {
        const PCShard* shard = &network->shards[i];
        
        printf("│ 0x%X  │ %-8u │ %-13.2f │ %-8lu │ ", 
               shard->shard_id,
               shard->local_state.num_wallets,
               shard->local_state.total_supply,
               shard->transaction_count);
        
        for (int j = 0; j < 4; j++) printf("%02x", shard->shard_hash[j]);
        printf("...    │\n");
        
        verified_total += shard->local_state.total_supply;
        total_tx += shard->transaction_count;
    }
    
    printf("└──────┴──────────┴───────────────┴──────────┴────────────────┘\n\n");
    
    printf("Totals:\n");
    printf("  Transactions: %lu\n", total_tx);
    printf("  Sum of shards: %.8f\n", verified_total);
    printf("  Conservation error: %.2e\n", fabs(verified_total - network->total_supply));
    
    PCError cons = pc_sharding_verify_conservation(network);
    printf("  Conservation: %s\n", cons == PC_OK ? "✓ VERIFIED" : "✗ VIOLATED");
}

// Calculate theoretical throughput with parallel shards
double pc_sharding_theoretical_throughput(uint32_t num_shards, double per_shard_tps) {
    // Assumes perfect parallelization
    return num_shards * per_shard_tps;
}

// Free sharding network
void pc_sharding_free(PCShardedNetwork* network) {
    if (network) {
        for (int i = 0; i < NUM_SHARDS; i++) {
            pc_state_free(&network->shards[i].local_state);
        }
    }
}
