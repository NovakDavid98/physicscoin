// delta.c - Secure Light Client Protocol
// Efficient state synchronization via deltas
// SECURITY HARDENED: Conservation verification on delta application

#include "../include/physicscoin.h"
#include "../crypto/sha256.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

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
    double total_supply;  // SECURITY: Include total supply for verification
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
    delta->total_supply = after->total_supply;
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

// SECURITY: Verify delta maintains conservation before applying
static PCError verify_delta_conservation(const PCState* state, const PCStateDelta* delta) {
    // Calculate what the new total would be after applying delta
    double current_sum = 0.0;
    for (uint32_t i = 0; i < state->num_wallets; i++) {
        current_sum += state->wallets[i].energy;
    }
    
    // Calculate delta effect
    double delta_effect = 0.0;
    for (uint32_t i = 0; i < delta->num_changes; i++) {
        const PCWalletDelta* wd = &delta->changes[i];
        
        // Find if wallet exists in current state
        int found = 0;
        for (uint32_t j = 0; j < state->num_wallets; j++) {
            if (memcmp(state->wallets[j].public_key, wd->pubkey, 32) == 0) {
                // Existing wallet: calculate difference
                delta_effect += (wd->new_balance - state->wallets[j].energy);
                found = 1;
                break;
            }
        }
        
        if (!found) {
            // New wallet: add its balance
            delta_effect += wd->new_balance;
        }
    }
    
    double new_sum = current_sum + delta_effect;
    
    // SECURITY: Verify the new sum matches the claimed total supply
    if (fabs(new_sum - delta->total_supply) > 1e-9) {
        printf("SECURITY: Delta conservation check failed!\n");
        printf("  Current sum: %.8f\n", current_sum);
        printf("  Delta effect: %.8f\n", delta_effect);
        printf("  Expected new sum: %.8f\n", new_sum);
        printf("  Claimed total supply: %.8f\n", delta->total_supply);
        return PC_ERR_CONSERVATION_VIOLATED;
    }
    
    // SECURITY: Verify no individual balance becomes negative
    for (uint32_t i = 0; i < delta->num_changes; i++) {
        if (delta->changes[i].new_balance < 0) {
            printf("SECURITY: Delta contains negative balance!\n");
            return PC_ERR_INVALID_AMOUNT;
        }
    }
    
    return PC_OK;
}

// Apply delta to a state (for light client sync) - SECURITY HARDENED
PCError pc_delta_apply(PCState* state, const PCStateDelta* delta) {
    if (!state || !delta) return PC_ERR_IO;
    
    // SECURITY CHECK 1: Verify state hash matches delta's prev_hash
    if (memcmp(state->state_hash, delta->prev_hash, 32) != 0) {
        printf("SECURITY: State hash mismatch - delta does not chain from current state\n");
        return PC_ERR_INVALID_SIGNATURE;
    }
    
    // SECURITY CHECK 2: Verify conservation BEFORE applying
    PCError cons_check = verify_delta_conservation(state, delta);
    if (cons_check != PC_OK) {
        return cons_check;
    }
    
    // SECURITY CHECK 3: Verify total supply hasn't changed
    if (state->total_supply > 0 && fabs(delta->total_supply - state->total_supply) > 1e-9) {
        printf("SECURITY: Delta attempts to change total supply from %.8f to %.8f\n",
               state->total_supply, delta->total_supply);
        return PC_ERR_CONSERVATION_VIOLATED;
    }
    
    // All security checks passed - apply changes
    for (uint32_t i = 0; i < delta->num_changes; i++) {
        const PCWalletDelta* wd = &delta->changes[i];
        
        PCWallet* wallet = pc_state_get_wallet(state, wd->pubkey);
        if (!wallet) {
            // Create new wallet (with 0 initial balance, delta will set it)
            PCError err = pc_state_create_wallet(state, wd->pubkey, 0);
            if (err != PC_OK && err != PC_ERR_WALLET_EXISTS) {
                return err;
            }
            wallet = pc_state_get_wallet(state, wd->pubkey);
            if (!wallet) return PC_ERR_IO;
        }
        
        wallet->energy = wd->new_balance;
        wallet->nonce = wd->new_nonce;
    }
    
    // Update timestamps and hashes
    state->timestamp = delta->new_timestamp;
    memcpy(state->prev_hash, delta->prev_hash, 32);
    
    // Recompute and verify hash
    pc_state_compute_hash(state);
    
    // SECURITY CHECK 4: Verify computed hash matches expected hash
    if (memcmp(state->state_hash, delta->new_hash, 32) != 0) {
        printf("SECURITY: State hash after delta application doesn't match expected hash\n");
        printf("  Expected: ");
        for (int i = 0; i < 8; i++) printf("%02x", delta->new_hash[i]);
        printf("...\n");
        printf("  Got:      ");
        for (int i = 0; i < 8; i++) printf("%02x", state->state_hash[i]);
        printf("...\n");
        return PC_ERR_INVALID_SIGNATURE;
    }
    
    // SECURITY CHECK 5: Final conservation verification
    PCError final_check = pc_state_verify_conservation(state);
    if (final_check != PC_OK) {
        printf("SECURITY: Conservation violated after delta application\n");
        return PC_ERR_CONSERVATION_VIOLATED;
    }
    
    return PC_OK;
}

// Serialize delta to buffer
size_t pc_delta_serialize(const PCStateDelta* delta, uint8_t* buffer, size_t max) {
    // Header size: hashes + timestamps + num_changes + total_supply
    size_t header_size = 32 + 32 + 8 + 8 + 4 + 8;
    size_t change_size = delta->num_changes * sizeof(PCWalletDelta);
    size_t total = header_size + change_size;
    
    if (total > max) return 0;
    
    size_t offset = 0;
    
    memcpy(buffer + offset, delta->prev_hash, 32); offset += 32;
    memcpy(buffer + offset, delta->new_hash, 32); offset += 32;
    memcpy(buffer + offset, &delta->prev_timestamp, 8); offset += 8;
    memcpy(buffer + offset, &delta->new_timestamp, 8); offset += 8;
    memcpy(buffer + offset, &delta->num_changes, 4); offset += 4;
    memcpy(buffer + offset, &delta->total_supply, 8); offset += 8;
    memcpy(buffer + offset, delta->changes, change_size);
    
    return total;
}

// Deserialize delta from buffer
PCError pc_delta_deserialize(PCStateDelta* delta, const uint8_t* buffer, size_t size) {
    size_t header_size = 32 + 32 + 8 + 8 + 4 + 8;
    if (size < header_size) return PC_ERR_IO;
    
    size_t offset = 0;
    
    memcpy(delta->prev_hash, buffer + offset, 32); offset += 32;
    memcpy(delta->new_hash, buffer + offset, 32); offset += 32;
    memcpy(&delta->prev_timestamp, buffer + offset, 8); offset += 8;
    memcpy(&delta->new_timestamp, buffer + offset, 8); offset += 8;
    memcpy(&delta->num_changes, buffer + offset, 4); offset += 4;
    memcpy(&delta->total_supply, buffer + offset, 8); offset += 8;
    
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
    
    printf("  Total Supply: %.8f\n", delta->total_supply);
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
    return 32 + 32 + 8 + 8 + 4 + 8 + (delta->num_changes * sizeof(PCWalletDelta));
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
    filtered->total_supply = full->total_supply;
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

// SECURITY: Verify delta is internally consistent
PCError pc_delta_verify(const PCStateDelta* delta) {
    if (!delta) return PC_ERR_IO;
    
    // Check for duplicate pubkeys (would indicate malformed delta)
    for (uint32_t i = 0; i < delta->num_changes; i++) {
        for (uint32_t j = i + 1; j < delta->num_changes; j++) {
            if (memcmp(delta->changes[i].pubkey, delta->changes[j].pubkey, 32) == 0) {
                printf("SECURITY: Delta contains duplicate pubkey entries\n");
                return PC_ERR_INVALID_SIGNATURE;
            }
        }
    }
    
    // Check for negative balances
    for (uint32_t i = 0; i < delta->num_changes; i++) {
        if (delta->changes[i].new_balance < 0) {
            printf("SECURITY: Delta contains negative balance\n");
            return PC_ERR_INVALID_AMOUNT;
        }
    }
    
    // Check total supply is non-negative
    if (delta->total_supply < 0) {
        printf("SECURITY: Delta has negative total supply\n");
        return PC_ERR_INVALID_AMOUNT;
    }
    
    return PC_OK;
}
