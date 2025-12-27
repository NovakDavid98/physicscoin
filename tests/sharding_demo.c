// sharding_demo.c - Demonstration of wallet-based sharding

#include "../include/physicscoin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NUM_SHARDS 16

// Forward declarations from sharding.c
typedef struct {
    uint8_t shard_id;
    PCState local_state;
    uint8_t shard_hash[32];
    uint64_t transaction_count;
} PCShard;

typedef struct {
    PCShard shards[NUM_SHARDS];
    uint32_t num_shards;
    double total_supply;
} PCShardedNetwork;

PCError pc_sharding_init(PCShardedNetwork* network, double initial_supply);
PCError pc_sharding_create_wallet(PCShardedNetwork* network, const uint8_t* pubkey, double balance);
PCError pc_sharding_execute_intra_tx(PCShardedNetwork* network, const PCTransaction* tx);
PCError pc_sharding_execute_cross_tx(PCShardedNetwork* network, const PCTransaction* tx);
PCError pc_sharding_get_balance(PCShardedNetwork* network, const uint8_t* pubkey, double* balance);
void pc_sharding_print_stats(const PCShardedNetwork* network);
void pc_sharding_free(PCShardedNetwork* network);
PCShard* pc_sharding_get_shard(PCShardedNetwork* network, const uint8_t* pubkey);

int main(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║           WALLET-BASED SHARDING DEMO                         ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
    
    // Initialize sharded network
    PCShardedNetwork network;
    pc_sharding_init(&network, 10000.0);
    
    printf("═══ Creating Wallets Across Shards ═══\n\n");
    
    // Create wallets that will land in different shards
    PCKeypair wallets[8];
    for (int i = 0; i < 8; i++) {
        pc_keypair_generate(&wallets[i]);
        
        // Manually set first byte to control sharding
        wallets[i].public_key[0] = (i << 4);  // 0x00, 0x10, 0x20, ..., 0x70
        
        double balance = 1000.0 + i * 100;
        pc_sharding_create_wallet(&network, wallets[i].public_key, balance);
        
        PCShard* shard = pc_sharding_get_shard(&network, wallets[i].public_key);
        
        char addr[65];
        pc_pubkey_to_hex(wallets[i].public_key, addr);
        printf("Wallet %d: %.8s... → Shard 0x%X (balance: %.2f)\n",
               i, addr, shard->shard_id, balance);
    }
    
    printf("\n");
    pc_sharding_print_stats(&network);
    
    // INTRA-SHARD TRANSACTIONS
    printf("\n═══ Intra-Shard Transactions ═══\n\n");
    
    // Both wallets in same shard (0x0)
    PCTransaction intra_tx = {0};
    memcpy(intra_tx.from, wallets[0].public_key, 32);
    memcpy(intra_tx.to, wallets[0].public_key, 32);
    intra_tx.to[0] = 0x01;  // Still in shard 0
    intra_tx.amount = 50.0;
    intra_tx.nonce = 0;
    intra_tx.timestamp = time(NULL);
    pc_transaction_sign(&intra_tx, &wallets[0]);
    
    // Create receiver first
    pc_sharding_create_wallet(&network, intra_tx.to, 0);
    
    printf("Executing intra-shard TX (Shard 0x0 → Shard 0x0)...\n");
    PCError err = pc_sharding_execute_intra_tx(&network, &intra_tx);
    printf("Result: %s\n\n", err == PC_OK ? "✓ Success" : pc_strerror(err));
    
    // CROSS-SHARD TRANSACTIONS
    printf("═══ Cross-Shard Transactions (2-Phase Commit) ═══\n\n");
    
    // Wallet 0 (shard 0x0) → Wallet 1 (shard 0x1)
    PCTransaction cross_tx = {0};
    memcpy(cross_tx.from, wallets[0].public_key, 32);
    memcpy(cross_tx.to, wallets[1].public_key, 32);
    cross_tx.amount = 200.0;
    cross_tx.nonce = 1;
    cross_tx.timestamp = time(NULL);
    pc_transaction_sign(&cross_tx, &wallets[0]);
    
    err = pc_sharding_execute_cross_tx(&network, &cross_tx);
    printf("\n");
    
    // Another cross-shard
    PCTransaction cross_tx2 = {0};
    memcpy(cross_tx2.from, wallets[2].public_key, 32);
    memcpy(cross_tx2.to, wallets[5].public_key, 32);
    cross_tx2.amount = 150.0;
    cross_tx2.nonce = 0;
    cross_tx2.timestamp = time(NULL);
    pc_transaction_sign(&cross_tx2, &wallets[2]);
    
    pc_sharding_execute_cross_tx(&network, &cross_tx2);
    
    printf("\n");
    pc_sharding_print_stats(&network);
    
    // Theoretical throughput
    printf("\n═══ Throughput Analysis ═══\n\n");
    double per_shard_tps = 216000;  // Current CPU performance
    double theoretical = network.num_shards * per_shard_tps;
    
    printf("Per-shard throughput: %.0f tx/sec\n", per_shard_tps);
    printf("Number of shards: %u\n", network.num_shards);
    printf("Theoretical total: %.2f M tx/sec\n", theoretical / 1000000.0);
    printf("(Assuming perfect parallelization)\n\n");
    
    printf("✓ Sharding demo complete!\n");
    printf("\nKey Insights:\n");
    printf("  • Wallets distributed across %u shards\n", network.num_shards);
    printf("  • Intra-shard TX: Single-shard execution\n");
    printf("  • Cross-shard TX: 2-phase commit protocol\n");
    printf("  • Horizontal scaling: %.1fM tx/sec potential\n\n", theoretical / 1000000.0);
    
    pc_sharding_free(&network);
    return 0;
}
