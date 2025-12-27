// replay_demo.c - Demonstration of deterministic replay
// Shows how we can prove entire history with minimal storage

#include "../include/physicscoin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Forward declarations from replay.c
typedef struct {
    PCState genesis;
    PCTransaction* transactions;
    uint32_t num_transactions;
    uint8_t expected_final_hash[32];
} PCReplayLog;

PCError pc_replay_init(PCReplayLog* log, const PCState* genesis);
PCError pc_replay_add_tx(PCReplayLog* log, const PCTransaction* tx);
PCError pc_replay_verify(const PCReplayLog* log, const uint8_t* expected_hash);
void pc_replay_free(PCReplayLog* log);
void pc_replay_print(const PCReplayLog* log);

// Forward declarations from timetravel.c
typedef struct {
    uint8_t state_hash[32];
    uint64_t timestamp;
    uint32_t transaction_index;
    PCState state;
} PCStateCheckpoint;

typedef struct {
    PCStateCheckpoint* checkpoints;
    uint32_t num_checkpoints;
    uint32_t checkpoint_interval;
} PCCheckpointHistory;

PCError pc_checkpoints_init(PCCheckpointHistory* history, uint32_t interval);
PCError pc_checkpoints_add(PCCheckpointHistory* history, const PCState* state, uint32_t tx_index);
PCError pc_query_balance_at(PCCheckpointHistory* history, const uint8_t* pubkey, uint64_t timestamp, double* balance);
void pc_checkpoints_print(const PCCheckpointHistory* history);
void pc_checkpoints_free(PCCheckpointHistory* history);
size_t pc_checkpoints_storage(const PCCheckpointHistory* history);

int main(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║         DETERMINISTIC REPLAY & TIME-TRAVEL DEMO              ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
    
    // Create wallets
    PCKeypair alice, bob, charlie;
    pc_keypair_generate(&alice);
    pc_keypair_generate(&bob);
    pc_keypair_generate(&charlie);
    
    char alice_addr[65], bob_addr[65], charlie_addr[65];
    pc_pubkey_to_hex(alice.public_key, alice_addr);
    pc_pubkey_to_hex(bob.public_key, bob_addr);
    pc_pubkey_to_hex(charlie.public_key, charlie_addr);
    
    printf("═══ PART 1: Building Transaction History ═══\n\n");
    printf("Alice:   %.16s...\n", alice_addr);
    printf("Bob:     %.16s...\n", bob_addr);
    printf("Charlie: %.16s...\n\n", charlie_addr);
    
    // Create genesis
    PCState state;
    pc_state_genesis(&state, alice.public_key, 1000.0);
    pc_state_create_wallet(&state, bob.public_key, 0);
    pc_state_create_wallet(&state, charlie.public_key, 0);
    
    // Initialize replay log and checkpoints
    PCReplayLog replay_log;
    pc_replay_init(&replay_log, &state);
    
    PCCheckpointHistory checkpoints;
    pc_checkpoints_init(&checkpoints, 5);  // Checkpoint every 5 transactions
    pc_checkpoints_add(&checkpoints, &state, 0);  // Genesis checkpoint
    
    printf("Genesis state:\n");
    printf("  Alice: %.2f\n", pc_state_get_wallet(&state, alice.public_key)->energy);
    printf("  Bob: %.2f\n", pc_state_get_wallet(&state, bob.public_key)->energy);
    printf("  Charlie: %.2f\n\n", pc_state_get_wallet(&state, charlie.public_key)->energy);
    
    // Execute 20 transactions
    printf("Executing 20 random transactions...\n\n");
    
    for (int i = 0; i < 20; i++) {
        PCTransaction tx = {0};
        
        // Random sender and receiver
        int sender_idx = rand() % 3;
        int receiver_idx = rand() % 3;
        
        PCKeypair* sender_kp;
        uint8_t* sender_key;
        uint8_t* receiver_key;
        
        if (sender_idx == 0) {
            sender_kp = &alice;
            sender_key = alice.public_key;
        } else if (sender_idx == 1) {
            sender_kp = &bob;
            sender_key = bob.public_key;
        } else {
            sender_kp = &charlie;
            sender_key = charlie.public_key;
        }
        
        if (receiver_idx == 0) receiver_key = alice.public_key;
        else if (receiver_idx == 1) receiver_key = bob.public_key;
        else receiver_key = charlie.public_key;
        
        if (sender_key == receiver_key) continue;  // Skip self-transfers
        
        PCWallet* sender_wallet = pc_state_get_wallet(&state, sender_key);
        if (!sender_wallet || sender_wallet->energy < 10.0) continue;
        
        memcpy(tx.from, sender_key, 32);
        memcpy(tx.to, receiver_key, 32);
        tx.amount = 10.0 + (rand() % 50);
        if (tx.amount > sender_wallet->energy) tx.amount = sender_wallet->energy * 0.5;
        tx.nonce = sender_wallet->nonce;
        tx.timestamp = (uint64_t)time(NULL) + i;
        pc_transaction_sign(&tx, sender_kp);
        
        PCError err = pc_state_execute_tx(&state, &tx);
        if (err == PC_OK) {
            printf("  TX %d: %.8s → %.8s : %.2f ✓\n", 
                   i, alice_addr + (sender_idx * 8), bob_addr + (receiver_idx * 8), tx.amount);
            
            // Add to replay log
            pc_replay_add_tx(&replay_log, &tx);
            
            // Checkpoint every 5 transactions
            if ((i + 1) % 5 == 0) {
                pc_checkpoints_add(&checkpoints, &state, i + 1);
                printf("    [Checkpoint created at TX %d]\n", i + 1);
            }
        }
    }
    
    // Save final hash
    memcpy(replay_log.expected_final_hash, state.state_hash, 32);
    
    printf("\n═══ Final State ═══\n");
    printf("Alice: %.2f\n", pc_state_get_wallet(&state, alice.public_key)->energy);
    printf("Bob: %.2f\n", pc_state_get_wallet(&state, bob.public_key)->energy);
    printf("Charlie: %.2f\n\n", pc_state_get_wallet(&state, charlie.public_key)->energy);
    
    printf("Final hash: ");
    for (int i = 0; i < 16; i++) printf("%02x", state.state_hash[i]);
    printf("...\n\n");
    
    // PART 2: Deterministic Replay
    printf("═══ PART 2: Deterministic Replay Verification ═══\n\n");
    pc_replay_print(&replay_log);
    printf("\n");
    
    PCError err = pc_replay_verify(&replay_log, replay_log.expected_final_hash);
    if (err != PC_OK) {
        printf("Replay verification failed!\n");
        return 1;
    }
    
    // PART 3: Time-Travel Queries
    printf("\n═══ PART 3: Time-Travel Balance Queries ═══\n\n");
    pc_checkpoints_print(&checkpoints);
    
    printf("\nStorage comparison:\n");
    printf("  Traditional blockchain: ~%zu bytes\n", 
           replay_log.num_transactions * 200);  // ~200 bytes per TX
    printf("  Our checkpoints: %zu bytes\n", pc_checkpoints_storage(&checkpoints));
    printf("  Savings: %.1f%%\n", 
           100.0 * (1.0 - (double)pc_checkpoints_storage(&checkpoints) / 
                         (replay_log.num_transactions * 200)));
    
    printf("\n═══ Querying Historical Balances ═══\n\n");
    
    // Query balance at different times
    uint64_t genesis_time = checkpoints.checkpoints[0].timestamp;
    
    for (uint32_t i = 0; i < checkpoints.num_checkpoints; i++) {
        double balance;
        uint64_t query_time = checkpoints.checkpoints[i].timestamp;
        
        pc_query_balance_at(&checkpoints, alice.public_key, query_time, &balance);
        printf("Alice's balance at TX %u (time %lu): %.2f\n", 
               checkpoints.checkpoints[i].transaction_index, query_time, balance);
    }
    
    printf("\n✓ All demonstrations complete!\n");
    printf("\nKey Insights:\n");
    printf("  • History is 100%% deterministically verifiable\n");
    printf("  • Can query any past balance using checkpoints\n");
    printf("  • Storage: %zu bytes for %u checkpoints (vs blockchain)\n",
           pc_checkpoints_storage(&checkpoints), checkpoints.num_checkpoints);
    printf("  • No trust required—anyone can replay and verify\n\n");
    
    // Cleanup
    pc_replay_free(&replay_log);
    pc_checkpoints_free(&checkpoints);
    pc_state_free(&state);
    
    return 0;
}
