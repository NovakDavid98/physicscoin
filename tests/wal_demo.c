// wal_demo.c - Write-Ahead Log Demo
// Demonstrates crash recovery with WAL

#include "../include/physicscoin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Forward declarations from wal.c
typedef struct {
    FILE* file;
    struct { uint32_t magic; uint32_t version; uint64_t created_at; uint64_t entry_count; uint8_t state_hash[32]; } header;
    uint64_t current_sequence;
    int dirty;
} PCWAL;

PCError pc_wal_init(PCWAL* wal, const char* filename);
PCError pc_wal_log_tx(PCWAL* wal, const PCTransaction* tx);
PCError pc_wal_log_genesis(PCWAL* wal, const uint8_t* creator_pubkey, double supply);
PCError pc_wal_checkpoint(PCWAL* wal, const PCState* state);
PCError pc_wal_recover(PCWAL* wal, PCState* state);
PCError pc_wal_truncate(PCWAL* wal);
void pc_wal_close(PCWAL* wal);
void pc_wal_print(const PCWAL* wal);

int main(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║           WRITE-AHEAD LOG (WAL) DEMO                          ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
    
    // Clean up old files
    remove("test.wal");
    remove("physicscoin.checkpoint");
    
    // === PHASE 1: Create transactions with WAL ===
    printf("═══ Phase 1: Creating Transactions with WAL ═══\n\n");
    
    PCWAL wal;
    PCError err = pc_wal_init(&wal, "test.wal");
    if (err != PC_OK) {
        printf("Failed to init WAL: %s\n", pc_strerror(err));
        return 1;
    }
    
    // Create wallets
    PCKeypair alice, bob;
    pc_keypair_generate(&alice);
    pc_keypair_generate(&bob);
    
    // Log genesis
    printf("Logging genesis...\n");
    pc_wal_log_genesis(&wal, alice.public_key, 1000.0);
    
    // Create and log transactions
    PCState state;
    pc_state_genesis(&state, alice.public_key, 1000.0);
    pc_state_create_wallet(&state, bob.public_key, 0);
    
    printf("Creating 5 transactions...\n");
    for (int i = 0; i < 5; i++) {
        PCTransaction tx = {0};
        memcpy(tx.from, alice.public_key, 32);
        memcpy(tx.to, bob.public_key, 32);
        tx.amount = 100.0;
        tx.nonce = i;
        tx.timestamp = time(NULL);
        pc_transaction_sign(&tx, &alice);
        
        // Log BEFORE executing
        pc_wal_log_tx(&wal, &tx);
        
        // Execute
        pc_state_execute_tx(&state, &tx);
        
        printf("  TX %d: Alice → Bob (100 coins)\n", i + 1);
    }
    
    printf("\nState after 5 TXs:\n");
    printf("  Alice: %.2f\n", pc_state_get_wallet(&state, alice.public_key)->energy);
    printf("  Bob:   %.2f\n", pc_state_get_wallet(&state, bob.public_key)->energy);
    
    // Create checkpoint
    printf("\nCreating checkpoint...\n");
    pc_wal_checkpoint(&wal, &state);
    
    pc_wal_print(&wal);
    pc_wal_close(&wal);
    pc_state_free(&state);
    
    // === PHASE 2: Simulate crash and recovery ===
    printf("\n═══ Phase 2: Simulating Crash & Recovery ═══\n\n");
    
    printf("Simulating power failure...\n");
    printf("State lost! WAL still on disk.\n\n");
    
    // Reopen WAL and recover
    printf("Recovering from WAL...\n");
    
    PCWAL wal2;
    pc_wal_init(&wal2, "test.wal");
    
    PCState recovered;
    memset(&recovered, 0, sizeof(PCState));
    
    pc_wal_recover(&wal2, &recovered);
    
    printf("\nRecovered state:\n");
    printf("  Wallets: %u\n", recovered.num_wallets);
    printf("  Total supply: %.2f\n", recovered.total_supply);
    
    // Verify balances match
    printf("\n═══ Phase 3: Verification ═══\n\n");
    
    double alice_expected = 500.0;  // 1000 - 5*100
    double bob_expected = 500.0;    // 0 + 5*100
    
    double alice_balance = 0, bob_balance = 0;
    
    for (uint32_t i = 0; i < recovered.num_wallets; i++) {
        if (memcmp(recovered.wallets[i].public_key, alice.public_key, 32) == 0) {
            alice_balance = recovered.wallets[i].energy;
        }
        if (memcmp(recovered.wallets[i].public_key, bob.public_key, 32) == 0) {
            bob_balance = recovered.wallets[i].energy;
        }
    }
    
    printf("Expected: Alice=%.0f, Bob=%.0f\n", alice_expected, bob_expected);
    printf("Actual:   Alice=%.0f, Bob=%.0f\n", alice_balance, bob_balance);
    
    int success = (alice_balance == alice_expected && bob_balance == bob_expected);
    
    printf("\n╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  WAL RECOVERY: %s                                  ║\n", 
           success ? "✓ PERFECT" : "✗ FAILED ");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
    
    pc_wal_close(&wal2);
    pc_state_free(&recovered);
    
    // Cleanup
    remove("test.wal");
    remove("physicscoin.checkpoint");
    
    return success ? 0 : 1;
}
