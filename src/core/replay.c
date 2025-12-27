// replay.c - State Replay Engine
// Deterministically verify state by replaying transaction history

#include "../include/physicscoin.h"
#include "../crypto/sha256.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#define MAX_REPLAY_TRANSACTIONS 100000

// Replay log structure
typedef struct {
    PCState genesis;                     // Initial state
    PCTransaction* transactions;         // All transactions in order
    uint32_t num_transactions;
    uint8_t expected_final_hash[32];    // What we expect at the end
} PCReplayLog;

// Create a new replay log from genesis
PCError pc_replay_init(PCReplayLog* log, const PCState* genesis) {
    if (!log || !genesis) return PC_ERR_IO;
    
    memset(log, 0, sizeof(PCReplayLog));
    
    // Deep copy genesis state
    log->genesis.version = genesis->version;
    log->genesis.total_supply = genesis->total_supply;
    log->genesis.num_wallets = genesis->num_wallets;
    log->genesis.timestamp = genesis->timestamp;
    memcpy(log->genesis.state_hash, genesis->state_hash, 32);
    memcpy(log->genesis.prev_hash, genesis->prev_hash, 32);
    
    log->genesis.wallets = malloc(genesis->num_wallets * sizeof(PCWallet));
    if (!log->genesis.wallets) return PC_ERR_IO;
    
    memcpy(log->genesis.wallets, genesis->wallets, 
           genesis->num_wallets * sizeof(PCWallet));
    
    log->transactions = calloc(MAX_REPLAY_TRANSACTIONS, sizeof(PCTransaction));
    if (!log->transactions) {
        free(log->genesis.wallets);
        return PC_ERR_IO;
    }
    
    log->num_transactions = 0;
    
    return PC_OK;
}

// Add transaction to replay log
PCError pc_replay_add_tx(PCReplayLog* log, const PCTransaction* tx) {
    if (!log || !tx) return PC_ERR_IO;
    if (log->num_transactions >= MAX_REPLAY_TRANSACTIONS) {
        return PC_ERR_MAX_WALLETS;
    }
    
    memcpy(&log->transactions[log->num_transactions], tx, sizeof(PCTransaction));
    log->num_transactions++;
    
    return PC_OK;
}

// Replay all transactions and verify final state
PCError pc_replay_verify(const PCReplayLog* log, const uint8_t* expected_hash) {
    if (!log) return PC_ERR_IO;
    
    printf("═══ REPLAY VERIFICATION ═══\n");
    printf("Genesis hash: ");
    for (int i = 0; i < 8; i++) printf("%02x", log->genesis.state_hash[i]);
    printf("...\n");
    printf("Replaying %u transactions...\n", log->num_transactions);
    
    // Create working state from genesis
    PCState state;
    state.version = log->genesis.version;
    state.total_supply = log->genesis.total_supply;
    state.num_wallets = log->genesis.num_wallets;
    state.timestamp = log->genesis.timestamp;
    memcpy(state.state_hash, log->genesis.state_hash, 32);
    memcpy(state.prev_hash, log->genesis.prev_hash, 32);
    
    state.wallets = malloc(log->genesis.num_wallets * sizeof(PCWallet));
    if (!state.wallets) return PC_ERR_IO;
    memcpy(state.wallets, log->genesis.wallets, 
           log->genesis.num_wallets * sizeof(PCWallet));
    
    // Replay each transaction
    uint32_t successful = 0;
    uint32_t failed = 0;
    
    for (uint32_t i = 0; i < log->num_transactions; i++) {
        PCError err = pc_state_execute_tx(&state, &log->transactions[i]);
        
        if (err == PC_OK) {
            successful++;
        } else {
            failed++;
            printf("  TX %u failed: %s\n", i, pc_strerror(err));
        }
        
        // Progress indicator every 1000 tx
        if ((i + 1) % 1000 == 0) {
            printf("  Processed %u/%u...\n", i + 1, log->num_transactions);
        }
    }
    
    printf("\nReplay complete:\n");
    printf("  Successful: %u\n", successful);
    printf("  Failed: %u\n", failed);
    printf("  Final hash: ");
    for (int i = 0; i < 8; i++) printf("%02x", state.state_hash[i]);
    printf("...\n");
    
    if (expected_hash) {
        printf("  Expected:   ");
        for (int i = 0; i < 8; i++) printf("%02x", expected_hash[i]);
        printf("...\n");
        
        if (memcmp(state.state_hash, expected_hash, 32) == 0) {
            printf("\n✓ VERIFICATION SUCCESSFUL!\n");
            printf("  State hash matches expected value.\n");
            printf("  History is deterministically proven.\n");
        } else {
            printf("\n✗ VERIFICATION FAILED!\n");
            printf("  State hash does NOT match.\n");
            pc_state_free(&state);
            return PC_ERR_INVALID_SIGNATURE;
        }
    }
    
    // Verify conservation
    PCError cons = pc_state_verify_conservation(&state);
    printf("  Conservation: %s\n", cons == PC_OK ? "✓ Verified" : "✗ VIOLATED");
    
    pc_state_free(&state);
    return PC_OK;
}

// Replay and return final state (for inspection)
PCError pc_replay_execute(const PCReplayLog* log, PCState* final_state) {
    if (!log || !final_state) return PC_ERR_IO;
    
    // Initialize from genesis
    final_state->version = log->genesis.version;
    final_state->total_supply = log->genesis.total_supply;
    final_state->num_wallets = log->genesis.num_wallets;
    final_state->timestamp = log->genesis.timestamp;
    memcpy(final_state->state_hash, log->genesis.state_hash, 32);
    memcpy(final_state->prev_hash, log->genesis.prev_hash, 32);
    
    final_state->wallets = malloc(log->genesis.num_wallets * sizeof(PCWallet));
    if (!final_state->wallets) return PC_ERR_IO;
    memcpy(final_state->wallets, log->genesis.wallets,
           log->genesis.num_wallets * sizeof(PCWallet));
    
    // Execute all transactions
    for (uint32_t i = 0; i < log->num_transactions; i++) {
        pc_state_execute_tx(final_state, &log->transactions[i]);
    }
    
    return PC_OK;
}

// Free replay log
void pc_replay_free(PCReplayLog* log) {
    if (log) {
        if (log->genesis.wallets) {
            free(log->genesis.wallets);
        }
        if (log->transactions) {
            free(log->transactions);
        }
        memset(log, 0, sizeof(PCReplayLog));
    }
}

// Save replay log to file (for audit trails)
PCError pc_replay_save(const PCReplayLog* log, const char* filename) {
    FILE* f = fopen(filename, "wb");
    if (!f) return PC_ERR_IO;
    
    // Write genesis state
    uint8_t genesis_buf[65536];
    size_t genesis_size = pc_state_serialize(&log->genesis, genesis_buf, sizeof(genesis_buf));
    fwrite(&genesis_size, sizeof(size_t), 1, f);
    fwrite(genesis_buf, genesis_size, 1, f);
    
    // Write number of transactions
    fwrite(&log->num_transactions, sizeof(uint32_t), 1, f);
    
    // Write all transactions
    fwrite(log->transactions, sizeof(PCTransaction), log->num_transactions, f);
    
    // Write expected final hash
    fwrite(log->expected_final_hash, 32, 1, f);
    
    fclose(f);
    return PC_OK;
}

// Load replay log from file
PCError pc_replay_load(PCReplayLog* log, const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return PC_ERR_IO;
    
    // Read genesis state
    size_t genesis_size;
    if (fread(&genesis_size, sizeof(size_t), 1, f) != 1) {
        fclose(f);
        return PC_ERR_IO;
    }
    
    uint8_t genesis_buf[65536];
    if (fread(genesis_buf, genesis_size, 1, f) != 1) {
        fclose(f);
        return PC_ERR_IO;
    }
    
    pc_state_deserialize(&log->genesis, genesis_buf, genesis_size);
    
    // Read transactions
    if (fread(&log->num_transactions, sizeof(uint32_t), 1, f) != 1) {
        fclose(f);
        return PC_ERR_IO;
    }
    
    log->transactions = calloc(log->num_transactions, sizeof(PCTransaction));
    if (!log->transactions) {
        fclose(f);
        return PC_ERR_IO;
    }
    
    if (fread(log->transactions, sizeof(PCTransaction), log->num_transactions, f) != log->num_transactions) {
        free(log->transactions);
        fclose(f);
        return PC_ERR_IO;
    }
    
    // Read expected hash
    fread(log->expected_final_hash, 32, 1, f);
    
    fclose(f);
    return PC_OK;
}

// Print replay log summary
void pc_replay_print(const PCReplayLog* log) {
    printf("Replay Log Summary:\n");
    printf("  Genesis Supply: %.8f\n", log->genesis.total_supply);
    printf("  Genesis Wallets: %u\n", log->genesis.num_wallets);
    printf("  Transactions: %u\n", log->num_transactions);
    printf("  Expected Hash: ");
    for (int i = 0; i < 8; i++) printf("%02x", log->expected_final_hash[i]);
    printf("...\n");
}
