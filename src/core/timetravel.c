// timetravel.c - Time-Travel Balance Queries
// Query wallet balance at any point in history using checkpoints

#include "../include/physicscoin.h"
#include "../crypto/sha256.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#define MAX_CHECKPOINTS 10000

// State checkpoint structure
typedef struct {
    uint8_t state_hash[32];
    uint64_t timestamp;
    uint32_t transaction_index;  // Which transaction created this state
    PCState state;                // Full state at this point
} PCStateCheckpoint;

// Checkpoint history
typedef struct {
    PCStateCheckpoint* checkpoints;
    uint32_t num_checkpoints;
    uint32_t checkpoint_interval;  // Checkpoint every N transactions
} PCCheckpointHistory;

// Initialize checkpoint history
PCError pc_checkpoints_init(PCCheckpointHistory* history, uint32_t interval) {
    if (!history) return PC_ERR_IO;
    
    history->checkpoints = calloc(MAX_CHECKPOINTS, sizeof(PCStateCheckpoint));
    if (!history->checkpoints) return PC_ERR_IO;
    
    history->num_checkpoints = 0;
    history->checkpoint_interval = interval;
    
    return PC_OK;
}

// Add checkpoint
PCError pc_checkpoints_add(PCCheckpointHistory* history, const PCState* state, uint32_t tx_index) {
    if (!history || !state) return PC_ERR_IO;
    if (history->num_checkpoints >= MAX_CHECKPOINTS) {
        return PC_ERR_MAX_WALLETS;
    }
    
    PCStateCheckpoint* cp = &history->checkpoints[history->num_checkpoints];
    
    // Copy metadata
    memcpy(cp->state_hash, state->state_hash, 32);
    cp->timestamp = state->timestamp;
    cp->transaction_index = tx_index;
    
    // Deep copy state
    cp->state.version = state->version;
    cp->state.total_supply = state->total_supply;
    cp->state.num_wallets = state->num_wallets;
    cp->state.timestamp = state->timestamp;
    memcpy(cp->state.state_hash, state->state_hash, 32);
    memcpy(cp->state.prev_hash, state->prev_hash, 32);
    
    cp->state.wallets = malloc(state->num_wallets * sizeof(PCWallet));
    if (!cp->state.wallets) return PC_ERR_IO;
    
    memcpy(cp->state.wallets, state->wallets, 
           state->num_wallets * sizeof(PCWallet));
    
    history->num_checkpoints++;
    
    return PC_OK;
}

// Find checkpoint closest to (but before) timestamp
static PCStateCheckpoint* find_checkpoint_before(PCCheckpointHistory* history, uint64_t timestamp) {
    if (history->num_checkpoints == 0) return NULL;
    
    // Binary search for closest checkpoint
    int left = 0;
    int right = history->num_checkpoints - 1;
    PCStateCheckpoint* best = NULL;
    
    while (left <= right) {
        int mid = (left + right) / 2;
        PCStateCheckpoint* cp = &history->checkpoints[mid];
        
        if (cp->timestamp <= timestamp) {
            best = cp;
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    
    return best;
}

// Query balance at specific timestamp
PCError pc_query_balance_at(PCCheckpointHistory* history,
                             const uint8_t* pubkey,
                             uint64_t timestamp,
                             double* balance) {
    if (!history || !pubkey || !balance) return PC_ERR_IO;
    
    // Find checkpoint before timestamp
    PCStateCheckpoint* cp = find_checkpoint_before(history, timestamp);
    
    if (!cp) {
        printf("No checkpoint found before timestamp %lu\n", timestamp);
        return PC_ERR_WALLET_NOT_FOUND;
    }
    
    printf("Using checkpoint at timestamp %lu (TX %u)\n", 
           cp->timestamp, cp->transaction_index);
    
    // Find wallet in this checkpoint
    PCWallet* wallet = pc_state_get_wallet(&cp->state, pubkey);
    
    if (!wallet) {
        *balance = 0.0;
        printf("Wallet not found at that time (balance: 0)\n");
    } else {
        *balance = wallet->energy;
    }
    
    return PC_OK;
}

// Query state hash at specific timestamp
PCError pc_query_state_hash_at(PCCheckpointHistory* history,
                                uint64_t timestamp,
                                uint8_t* hash_out) {
    if (!history || !hash_out) return PC_ERR_IO;
    
    PCStateCheckpoint* cp = find_checkpoint_before(history, timestamp);
    if (!cp) return PC_ERR_WALLET_NOT_FOUND;
    
    memcpy(hash_out, cp->state_hash, 32);
    return PC_OK;
}

// Print all checkpoints
void pc_checkpoints_print(const PCCheckpointHistory* history) {
    printf("Checkpoint History (%u checkpoints):\n", history->num_checkpoints);
    printf("Interval: Every %u transactions\n\n", history->checkpoint_interval);
    
    for (uint32_t i = 0; i < history->num_checkpoints; i++) {
        const PCStateCheckpoint* cp = &history->checkpoints[i];
        
        printf("[%u] TX %u | Time %lu | Hash ", 
               i, cp->transaction_index, cp->timestamp);
        for (int j = 0; j < 8; j++) printf("%02x", cp->state_hash[j]);
        printf("...\n");
    }
}

// Free checkpoint history
void pc_checkpoints_free(PCCheckpointHistory* history) {
    if (history && history->checkpoints) {
        for (uint32_t i = 0; i < history->num_checkpoints; i++) {
            if (history->checkpoints[i].state.wallets) {
                free(history->checkpoints[i].state.wallets);
            }
        }
        free(history->checkpoints);
        history->checkpoints = NULL;
        history->num_checkpoints = 0;
    }
}

// Calculate storage used by checkpoints
size_t pc_checkpoints_storage(const PCCheckpointHistory* history) {
    if (!history) return 0;
    
    size_t total = 0;
    
    for (uint32_t i = 0; i < history->num_checkpoints; i++) {
        const PCStateCheckpoint* cp = &history->checkpoints[i];
        
        // Hash + timestamp + index
        total += 32 + 8 + 4;
        
        // State
        total += sizeof(PCWallet) * cp->state.num_wallets;
        total += 100;  // Overhead
    }
    
    return total;
}

// Save checkpoints to file
PCError pc_checkpoints_save(const PCCheckpointHistory* history, const char* filename) {
    FILE* f = fopen(filename, "wb");
    if (!f) return PC_ERR_IO;
    
    // Write header
    fwrite(&history->num_checkpoints, sizeof(uint32_t), 1, f);
    fwrite(&history->checkpoint_interval, sizeof(uint32_t), 1, f);
    
    // Write each checkpoint
    for (uint32_t i = 0; i < history->num_checkpoints; i++) {
        const PCStateCheckpoint* cp = &history->checkpoints[i];
        
        fwrite(cp->state_hash, 32, 1, f);
        fwrite(&cp->timestamp, sizeof(uint64_t), 1, f);
        fwrite(&cp->transaction_index, sizeof(uint32_t), 1, f);
        
        // Serialize state
        uint8_t state_buf[65536];
        size_t state_size = pc_state_serialize(&cp->state, state_buf, sizeof(state_buf));
        fwrite(&state_size, sizeof(size_t), 1, f);
        fwrite(state_buf, state_size, 1, f);
    }
    
    fclose(f);
    return PC_OK;
}

// Load checkpoints from file
PCError pc_checkpoints_load(PCCheckpointHistory* history, const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return PC_ERR_IO;
    
    // Read header
    if (fread(&history->num_checkpoints, sizeof(uint32_t), 1, f) != 1) {
        fclose(f);
        return PC_ERR_IO;
    }
    
    if (fread(&history->checkpoint_interval, sizeof(uint32_t), 1, f) != 1) {
        fclose(f);
        return PC_ERR_IO;
    }
    
    history->checkpoints = calloc(history->num_checkpoints, sizeof(PCStateCheckpoint));
    if (!history->checkpoints) {
        fclose(f);
        return PC_ERR_IO;
    }
    
    // Read each checkpoint
    for (uint32_t i = 0; i < history->num_checkpoints; i++) {
        PCStateCheckpoint* cp = &history->checkpoints[i];
        
        if (fread(cp->state_hash, 32, 1, f) != 1) goto error;
        if (fread(&cp->timestamp, sizeof(uint64_t), 1, f) != 1) goto error;
        if (fread(&cp->transaction_index, sizeof(uint32_t), 1, f) != 1) goto error;
        
        size_t state_size;
        if (fread(&state_size, sizeof(size_t), 1, f) != 1) goto error;
        
        uint8_t state_buf[65536];
        if (fread(state_buf, state_size, 1, f) != 1) goto error;
        
        if (pc_state_deserialize(&cp->state, state_buf, state_size) != PC_OK) {
            goto error;
        }
    }
    
    fclose(f);
    return PC_OK;
    
error:
    pc_checkpoints_free(history);
    fclose(f);
    return PC_ERR_IO;
}
