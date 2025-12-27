// delta.c - Light Client Protocol
// Efficient state synchronization via deltas

#include "../include/physicscoin.h"
#include "../crypto/sha256.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Maximum wallet changes per delta
#define MAX_DELTA_CHANGES 1000

// Single wallet change
typedef struct {
    uint8_t pubkey[32];
    double old_balance;
    double new_balance;
    uint64_t old_nonce;
    uint64_t new_nonce;
} PCWalletDelta;

// State delta (changes between two states)
typedef struct {
    uint8_t prev_hash[32];
    uint8_t new_hash[32];
    uint64_t prev_timestamp;
    uint64_t new_timestamp;
    uint32_t num_changes;
    PCWalletDelta changes[MAX_DELTA_CHANGES];
} PCStateDelta;

// Compute delta between two states
PCError pc_delta_compute(const PCState* before, const PCState* after, PCStateDelta* delta) {
    if (!before || !after || !delta) return PC_ERR_IO;
    
    memset(delta, 0, sizeof(PCStateDelta));
    memcpy(delta->prev_hash, before->state_hash, 32);
    memcpy(delta->new_hash, after->state_hash, 32);
    delta->prev_timestamp = before->timestamp;
    delta->new_timestamp = after->timestamp;
    delta->num_changes = 0;
    
    // Find all wallets that changed
    for (uint32_t i = 0; i < after->num_wallets; i++) {
        const PCWallet* new_wallet = &after->wallets[i];
        
        // Find corresponding wallet in before state
        const PCWallet* old_wallet = NULL;
        for (uint32_t j = 0; j < before->num_wallets; j++) {
            if (memcmp(before->wallets[j].public_key, new_wallet->public_key, 32) == 0) {
                old_wallet = &before->wallets[j];
                break;
            }
        }
        
        // Check if changed
        int changed = 0;
        double old_balance = 0;
        uint64_t old_nonce = 0;
        
        if (!old_wallet) {
            // New wallet
            changed = 1;
        } else if (old_wallet->energy != new_wallet->energy ||
                   old_wallet->nonce != new_wallet->nonce) {
            changed = 1;
            old_balance = old_wallet->energy;
            old_nonce = old_wallet->nonce;
        }
        
        if (changed && delta->num_changes < MAX_DELTA_CHANGES) {
            PCWalletDelta* wd = &delta->changes[delta->num_changes];
            memcpy(wd->pubkey, new_wallet->public_key, 32);
            wd->old_balance = old_balance;
            wd->new_balance = new_wallet->energy;
            wd->old_nonce = old_nonce;
            wd->new_nonce = new_wallet->nonce;
            delta->num_changes++;
        }
    }
    
    return PC_OK;
}

// Apply delta to a state (for light client sync)
PCError pc_delta_apply(PCState* state, const PCStateDelta* delta) {
    if (!state || !delta) return PC_ERR_IO;
    
    // Verify state hash matches delta's prev_hash
    if (memcmp(state->state_hash, delta->prev_hash, 32) != 0) {
        return PC_ERR_INVALID_SIGNATURE;  // State mismatch
    }
    
    // Apply each change
    for (uint32_t i = 0; i < delta->num_changes; i++) {
        const PCWalletDelta* wd = &delta->changes[i];
        
        PCWallet* wallet = pc_state_get_wallet(state, wd->pubkey);
        if (!wallet) {
            // Create new wallet
            pc_state_create_wallet(state, wd->pubkey, 0);
            wallet = pc_state_get_wallet(state, wd->pubkey);
        }
        
        if (wallet) {
            wallet->energy = wd->new_balance;
            wallet->nonce = wd->new_nonce;
        }
    }
    
    // Update timestamps and hashes
    state->timestamp = delta->new_timestamp;
    memcpy(state->prev_hash, delta->prev_hash, 32);
    memcpy(state->state_hash, delta->new_hash, 32);
    
    return PC_OK;
}

// Serialize delta to buffer
size_t pc_delta_serialize(const PCStateDelta* delta, uint8_t* buffer, size_t max) {
    // Header size
    size_t header_size = 32 + 32 + 8 + 8 + 4;  // hashes + timestamps + num_changes
    size_t change_size = delta->num_changes * sizeof(PCWalletDelta);
    size_t total = header_size + change_size;
    
    if (total > max) return 0;
    
    size_t offset = 0;
    
    memcpy(buffer + offset, delta->prev_hash, 32); offset += 32;
    memcpy(buffer + offset, delta->new_hash, 32); offset += 32;
    memcpy(buffer + offset, &delta->prev_timestamp, 8); offset += 8;
    memcpy(buffer + offset, &delta->new_timestamp, 8); offset += 8;
    memcpy(buffer + offset, &delta->num_changes, 4); offset += 4;
    memcpy(buffer + offset, delta->changes, change_size);
    
    return total;
}

// Deserialize delta from buffer
PCError pc_delta_deserialize(PCStateDelta* delta, const uint8_t* buffer, size_t size) {
    size_t header_size = 32 + 32 + 8 + 8 + 4;
    if (size < header_size) return PC_ERR_IO;
    
    size_t offset = 0;
    
    memcpy(delta->prev_hash, buffer + offset, 32); offset += 32;
    memcpy(delta->new_hash, buffer + offset, 32); offset += 32;
    memcpy(&delta->prev_timestamp, buffer + offset, 8); offset += 8;
    memcpy(&delta->new_timestamp, buffer + offset, 8); offset += 8;
    memcpy(&delta->num_changes, buffer + offset, 4); offset += 4;
    
    if (delta->num_changes > MAX_DELTA_CHANGES) return PC_ERR_IO;
    
    size_t change_size = delta->num_changes * sizeof(PCWalletDelta);
    if (size < header_size + change_size) return PC_ERR_IO;
    
    memcpy(delta->changes, buffer + offset, change_size);
    
    return PC_OK;
}

// Print delta info
void pc_delta_print(const PCStateDelta* delta) {
    printf("State Delta:\n");
    printf("  From: ");
    for (int i = 0; i < 8; i++) printf("%02x", delta->prev_hash[i]);
    printf("...\n");
    
    printf("  To:   ");
    for (int i = 0; i < 8; i++) printf("%02x", delta->new_hash[i]);
    printf("...\n");
    
    printf("  Changes: %u wallets\n", delta->num_changes);
    
    for (uint32_t i = 0; i < delta->num_changes; i++) {
        const PCWalletDelta* wd = &delta->changes[i];
        printf("    [%d] ", i);
        for (int j = 0; j < 4; j++) printf("%02x", wd->pubkey[j]);
        printf("...: %.8f → %.8f\n", wd->old_balance, wd->new_balance);
    }
}

// Get delta size (for bandwidth estimation)
size_t pc_delta_size(const PCStateDelta* delta) {
    return 32 + 32 + 8 + 8 + 4 + (delta->num_changes * sizeof(PCWalletDelta));
}

// Check if a specific wallet is affected by delta
int pc_delta_affects_wallet(const PCStateDelta* delta, const uint8_t* pubkey) {
    for (uint32_t i = 0; i < delta->num_changes; i++) {
        if (memcmp(delta->changes[i].pubkey, pubkey, 32) == 0) {
            return 1;
        }
    }
    return 0;
}

// Filter delta to only include specific wallets (for light clients)
PCError pc_delta_filter(const PCStateDelta* full, const uint8_t pubkeys[][32], 
                        int num_pubkeys, PCStateDelta* filtered) {
    memcpy(filtered->prev_hash, full->prev_hash, 32);
    memcpy(filtered->new_hash, full->new_hash, 32);
    filtered->prev_timestamp = full->prev_timestamp;
    filtered->new_timestamp = full->new_timestamp;
    filtered->num_changes = 0;
    
    for (uint32_t i = 0; i < full->num_changes; i++) {
        for (int j = 0; j < num_pubkeys; j++) {
            if (memcmp(full->changes[i].pubkey, pubkeys[j], 32) == 0) {
                memcpy(&filtered->changes[filtered->num_changes], 
                       &full->changes[i], sizeof(PCWalletDelta));
                filtered->num_changes++;
                break;
            }
        }
    }
    
    return PC_OK;
}
