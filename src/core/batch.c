// batch.c - Transaction Batching
// Process multiple non-conflicting transactions efficiently

#include "../include/physicscoin.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_BATCH_SIZE 1000

// Batch processing result
typedef struct {
    PCTransaction* transactions;
    uint32_t count;
    uint32_t successful;
    uint32_t failed;
    int* results;  // PC_OK or error code for each
} PCTransactionBatch;

// Analyze batch for conflicts (without executing)
// Two transactions conflict if they share a sender (nonce dependency)
PCError pc_batch_analyze(const PCState* state, PCTransactionBatch* batch) {
    if (!state || !batch || !batch->transactions) return PC_ERR_IO;
    
    // Reset results
    batch->successful = 0;
    batch->failed = 0;
    
    if (!batch->results) {
        batch->results = calloc(batch->count, sizeof(int));
        if (!batch->results) return PC_ERR_IO;
    }
    
    // Track which senders we've seen and their expected nonces
    uint8_t seen_senders[MAX_BATCH_SIZE][32];
    uint64_t expected_nonces[MAX_BATCH_SIZE];
    int num_seen = 0;
    
    for (uint32_t i = 0; i < batch->count; i++) {
        PCTransaction* tx = &batch->transactions[i];
        
        // Find sender's current nonce
        PCWallet* sender = NULL;
        for (uint32_t j = 0; j < state->num_wallets; j++) {
            if (memcmp(state->wallets[j].public_key, tx->from, 32) == 0) {
                sender = &state->wallets[j];
                break;
            }
        }
        
        if (!sender) {
            batch->results[i] = PC_ERR_WALLET_NOT_FOUND;
            batch->failed++;
            continue;
        }
        
        // Check if we've seen this sender before in batch
        uint64_t expected_nonce = sender->nonce;
        for (int j = 0; j < num_seen; j++) {
            if (memcmp(seen_senders[j], tx->from, 32) == 0) {
                expected_nonce = expected_nonces[j];
                break;
            }
        }
        
        // Validate nonce
        if (tx->nonce != expected_nonce) {
            batch->results[i] = PC_ERR_INVALID_SIGNATURE;
            batch->failed++;
            continue;
        }
        
        // Check balance (simplified - doesn't account for previous batch txs)
        if (sender->energy < tx->amount) {
            batch->results[i] = PC_ERR_INSUFFICIENT_FUNDS;
            batch->failed++;
            continue;
        }
        
        // Mark as valid
        batch->results[i] = PC_OK;
        batch->successful++;
        
        // Track this sender for nonce ordering
        int found = 0;
        for (int j = 0; j < num_seen; j++) {
            if (memcmp(seen_senders[j], tx->from, 32) == 0) {
                expected_nonces[j]++;
                found = 1;
                break;
            }
        }
        if (!found && num_seen < MAX_BATCH_SIZE) {
            memcpy(seen_senders[num_seen], tx->from, 32);
            expected_nonces[num_seen] = expected_nonce + 1;
            num_seen++;
        }
    }
    
    return PC_OK;
}

// Execute all transactions in batch
PCError pc_batch_execute(PCState* state, PCTransactionBatch* batch) {
    if (!state || !batch || !batch->transactions) return PC_ERR_IO;
    
    batch->successful = 0;
    batch->failed = 0;
    
    if (!batch->results) {
        batch->results = calloc(batch->count, sizeof(int));
        if (!batch->results) return PC_ERR_IO;
    }
    
    for (uint32_t i = 0; i < batch->count; i++) {
        PCError err = pc_state_execute_tx(state, &batch->transactions[i]);
        batch->results[i] = err;
        
        if (err == PC_OK) {
            batch->successful++;
        } else {
            batch->failed++;
        }
    }
    
    return PC_OK;
}

// Get conflict report
void pc_batch_report(const PCTransactionBatch* batch, char* report, size_t max) {
    size_t written = 0;
    
    written += snprintf(report + written, max - written,
                       "Batch Report: %u total, %u success, %u failed\n",
                       batch->count, batch->successful, batch->failed);
    
    if (batch->failed > 0) {
        written += snprintf(report + written, max - written, "Failures:\n");
        
        for (uint32_t i = 0; i < batch->count && written < max - 100; i++) {
            if (batch->results[i] != PC_OK) {
                written += snprintf(report + written, max - written,
                                   "  [%u] %s\n", i, pc_strerror(batch->results[i]));
            }
        }
    }
}

// Free batch resources
void pc_batch_free(PCTransactionBatch* batch) {
    if (batch->results) {
        free(batch->results);
        batch->results = NULL;
    }
}

// Count independent transaction groups (for parallelism estimation)
int pc_batch_count_independent_groups(const PCTransactionBatch* batch) {
    if (!batch || batch->count == 0) return 0;
    
    // Each unique sender forms a dependency chain
    uint8_t senders[MAX_BATCH_SIZE][32];
    int num_senders = 0;
    
    for (uint32_t i = 0; i < batch->count; i++) {
        int found = 0;
        for (int j = 0; j < num_senders; j++) {
            if (memcmp(senders[j], batch->transactions[i].from, 32) == 0) {
                found = 1;
                break;
            }
        }
        if (!found && num_senders < MAX_BATCH_SIZE) {
            memcpy(senders[num_senders], batch->transactions[i].from, 32);
            num_senders++;
        }
    }
    
    return num_senders;
}
