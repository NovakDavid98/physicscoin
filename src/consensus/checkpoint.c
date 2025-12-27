// checkpoint.c - Checkpoint Voting for State Finalization
// Part of Conservation-Based Consensus (CBC)

#include "../include/physicscoin.h"
#include "../crypto/sha256.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#define MAX_VALIDATORS 100
#define FINALITY_THRESHOLD 67  // 2/3 = 67%

// Validator signature
typedef struct {
    uint8_t validator_pubkey[32];
    uint8_t signature[64];
    uint64_t signed_at;
} ValidatorSignature;

// Checkpoint structure
typedef struct {
    uint64_t checkpoint_id;
    uint64_t tx_count_since_last;
    uint8_t state_hash[32];
    uint8_t prev_checkpoint_hash[32];
    uint64_t timestamp;
    
    ValidatorSignature signatures[MAX_VALIDATORS];
    uint32_t num_signatures;
    
    int finalized;
} PCCheckpoint;

// Checkpoint chain (last N checkpoints)
typedef struct {
    PCCheckpoint checkpoints[100];
    uint32_t count;
    uint64_t next_checkpoint_id;
    uint32_t tx_since_last_checkpoint;
    uint32_t checkpoint_interval;  // Create checkpoint every N TXs
} PCCheckpointChain;

// Initialize checkpoint chain
void checkpoint_chain_init(PCCheckpointChain* chain, uint32_t interval) {
    if (!chain) return;
    
    memset(chain, 0, sizeof(PCCheckpointChain));
    chain->checkpoint_interval = interval;
    chain->next_checkpoint_id = 0;
}

// Create new checkpoint proposal
PCError checkpoint_create(PCCheckpointChain* chain, const PCState* state, PCCheckpoint* out) {
    if (!chain || !state || !out) return PC_ERR_IO;
    
    memset(out, 0, sizeof(PCCheckpoint));
    
    out->checkpoint_id = chain->next_checkpoint_id;
    out->tx_count_since_last = chain->tx_since_last_checkpoint;
    out->timestamp = (uint64_t)time(NULL);
    out->finalized = 0;
    
    // Copy state hash
    memcpy(out->state_hash, state->state_hash, 32);
    
    // Link to previous checkpoint
    if (chain->count > 0) {
        memcpy(out->prev_checkpoint_hash, 
               chain->checkpoints[chain->count - 1].state_hash, 32);
    }
    
    return PC_OK;
}

// Hash checkpoint (for signing)
static void checkpoint_hash(const PCCheckpoint* cp, uint8_t hash[32]) {
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, (uint8_t*)&cp->checkpoint_id, 8);
    sha256_update(&ctx, (uint8_t*)&cp->tx_count_since_last, 8);
    sha256_update(&ctx, cp->state_hash, 32);
    sha256_update(&ctx, cp->prev_checkpoint_hash, 32);
    sha256_update(&ctx, (uint8_t*)&cp->timestamp, 8);
    sha256_final(&ctx, hash);
}

// Sign checkpoint (validator)
PCError checkpoint_sign(PCCheckpoint* cp, const PCKeypair* validator) {
    if (!cp || !validator) return PC_ERR_IO;
    if (cp->num_signatures >= MAX_VALIDATORS) return PC_ERR_MAX_WALLETS;
    
    // Check if already signed by this validator
    for (uint32_t i = 0; i < cp->num_signatures; i++) {
        if (memcmp(cp->signatures[i].validator_pubkey, validator->public_key, 32) == 0) {
            return PC_ERR_WALLET_EXISTS;  // Already signed
        }
    }
    
    // Create signature
    uint8_t cp_hash[32];
    checkpoint_hash(cp, cp_hash);
    
    ValidatorSignature* sig = &cp->signatures[cp->num_signatures];
    memcpy(sig->validator_pubkey, validator->public_key, 32);
    sig->signed_at = (uint64_t)time(NULL);
    
    // Sign (simplified - in production use ECDSA)
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, cp_hash, 32);
    sha256_update(&ctx, validator->secret_key, 64);
    sha256_final(&ctx, sig->signature);
    memcpy(sig->signature + 32, sig->signature, 32);  // Duplicate for 64-byte sig
    
    cp->num_signatures++;
    
    return PC_OK;
}

// Check if checkpoint has reached finality
int checkpoint_check_finality(PCCheckpoint* cp, uint32_t total_validators) {
    if (!cp || total_validators == 0) return 0;
    
    uint32_t threshold = (total_validators * FINALITY_THRESHOLD) / 100;
    if (threshold == 0) threshold = 1;
    
    if (cp->num_signatures >= threshold) {
        cp->finalized = 1;
        return 1;
    }
    
    return 0;
}

// Add finalized checkpoint to chain
PCError checkpoint_commit(PCCheckpointChain* chain, const PCCheckpoint* cp) {
    if (!chain || !cp) return PC_ERR_IO;
    if (!cp->finalized) return PC_ERR_INVALID_SIGNATURE;  // Must be finalized
    if (chain->count >= 100) {
        // Shift out oldest
        memmove(&chain->checkpoints[0], &chain->checkpoints[1], 
                99 * sizeof(PCCheckpoint));
        chain->count = 99;
    }
    
    memcpy(&chain->checkpoints[chain->count], cp, sizeof(PCCheckpoint));
    chain->count++;
    chain->next_checkpoint_id++;
    chain->tx_since_last_checkpoint = 0;
    
    return PC_OK;
}

// Check if we should create a checkpoint
int checkpoint_should_create(const PCCheckpointChain* chain) {
    if (!chain) return 0;
    return chain->tx_since_last_checkpoint >= chain->checkpoint_interval;
}

// Record transaction (for checkpoint timing)
void checkpoint_record_tx(PCCheckpointChain* chain) {
    if (chain) chain->tx_since_last_checkpoint++;
}

// Get latest finalized checkpoint
PCCheckpoint* checkpoint_get_latest(PCCheckpointChain* chain) {
    if (!chain || chain->count == 0) return NULL;
    return &chain->checkpoints[chain->count - 1];
}

// Verify checkpoint signature
int checkpoint_verify_signature(const PCCheckpoint* cp, uint32_t sig_idx,
                                 const uint8_t* validator_pubkey) {
    if (!cp || sig_idx >= cp->num_signatures) return 0;
    
    const ValidatorSignature* sig = &cp->signatures[sig_idx];
    
    // Check pubkey matches
    if (memcmp(sig->validator_pubkey, validator_pubkey, 32) != 0) {
        return 0;
    }
    
    // Verify signature (simplified)
    uint8_t cp_hash[32];
    checkpoint_hash(cp, cp_hash);
    
    // In production, verify ECDSA signature here
    // For now, just check signature is non-zero
    uint8_t zero[64] = {0};
    if (memcmp(sig->signature, zero, 64) == 0) {
        return 0;
    }
    
    return 1;
}

// Serialize checkpoint
size_t checkpoint_serialize(const PCCheckpoint* cp, uint8_t* buffer, size_t max) {
    if (!cp || !buffer) return 0;
    
    size_t needed = 8 + 8 + 32 + 32 + 8 + 4 + (cp->num_signatures * (32 + 64 + 8)) + 4;
    if (max < needed) return 0;
    
    size_t offset = 0;
    
    memcpy(buffer + offset, &cp->checkpoint_id, 8); offset += 8;
    memcpy(buffer + offset, &cp->tx_count_since_last, 8); offset += 8;
    memcpy(buffer + offset, cp->state_hash, 32); offset += 32;
    memcpy(buffer + offset, cp->prev_checkpoint_hash, 32); offset += 32;
    memcpy(buffer + offset, &cp->timestamp, 8); offset += 8;
    memcpy(buffer + offset, &cp->num_signatures, 4); offset += 4;
    
    for (uint32_t i = 0; i < cp->num_signatures; i++) {
        memcpy(buffer + offset, cp->signatures[i].validator_pubkey, 32); offset += 32;
        memcpy(buffer + offset, cp->signatures[i].signature, 64); offset += 64;
        memcpy(buffer + offset, &cp->signatures[i].signed_at, 8); offset += 8;
    }
    
    memcpy(buffer + offset, &cp->finalized, 4); offset += 4;
    
    return offset;
}

// Print checkpoint for debugging
void checkpoint_print(const PCCheckpoint* cp) {
    if (!cp) return;
    
    printf("Checkpoint #%lu:\n", cp->checkpoint_id);
    printf("  TXs since last: %lu\n", cp->tx_count_since_last);
    printf("  Timestamp: %lu\n", cp->timestamp);
    printf("  State hash: ");
    for (int i = 0; i < 8; i++) printf("%02x", cp->state_hash[i]);
    printf("...\n");
    printf("  Signatures: %u\n", cp->num_signatures);
    printf("  Finalized: %s\n", cp->finalized ? "YES" : "NO");
}

// Print chain statistics
void checkpoint_chain_print(const PCCheckpointChain* chain) {
    if (!chain) return;
    
    printf("Checkpoint Chain:\n");
    printf("  Total checkpoints: %u\n", chain->count);
    printf("  Next ID: %lu\n", chain->next_checkpoint_id);
    printf("  TXs since last: %u / %u\n", 
           chain->tx_since_last_checkpoint, chain->checkpoint_interval);
    
    if (chain->count > 0) {
        printf("  Latest: #%lu (finalized: %s)\n",
               chain->checkpoints[chain->count - 1].checkpoint_id,
               chain->checkpoints[chain->count - 1].finalized ? "yes" : "no");
    }
}
