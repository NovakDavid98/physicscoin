// checkpoint.c - Secure Checkpoint Voting for State Finalization
// Part of Conservation-Based Consensus (CBC)
// SECURITY HARDENED: Real Ed25519 signatures

#include "../include/physicscoin.h"
#include "../crypto/sha256.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sodium.h>

#define MAX_VALIDATORS 100
#define FINALITY_THRESHOLD 67  // 2/3 = 67%

// Validator signature - with REAL Ed25519
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
    double total_supply;  // SECURITY: Include for conservation check
    
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
    uint32_t checkpoint_interval;
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
    out->total_supply = state->total_supply;
    
    // Copy state hash
    memcpy(out->state_hash, state->state_hash, 32);
    
    // Link to previous checkpoint
    if (chain->count > 0) {
        memcpy(out->prev_checkpoint_hash, 
               chain->checkpoints[chain->count - 1].state_hash, 32);
    }
    
    return PC_OK;
}

// Hash checkpoint (for signing) - canonical format
static void checkpoint_hash(const PCCheckpoint* cp, uint8_t hash[32]) {
    SHA256_CTX ctx;
    sha256_init(&ctx);
    
    // Create canonical message format for signing
    sha256_update(&ctx, (uint8_t*)&cp->checkpoint_id, 8);
    sha256_update(&ctx, (uint8_t*)&cp->tx_count_since_last, 8);
    sha256_update(&ctx, cp->state_hash, 32);
    sha256_update(&ctx, cp->prev_checkpoint_hash, 32);
    sha256_update(&ctx, (uint8_t*)&cp->timestamp, 8);
    sha256_update(&ctx, (uint8_t*)&cp->total_supply, 8);
    
    sha256_final(&ctx, hash);
}

// Sign checkpoint (validator) - REAL Ed25519 signature
PCError checkpoint_sign(PCCheckpoint* cp, const PCKeypair* validator) {
    if (!cp || !validator) return PC_ERR_IO;
    if (cp->num_signatures >= MAX_VALIDATORS) return PC_ERR_MAX_WALLETS;
    
    // Check if already signed by this validator
    for (uint32_t i = 0; i < cp->num_signatures; i++) {
        if (memcmp(cp->signatures[i].validator_pubkey, validator->public_key, 32) == 0) {
            return PC_ERR_WALLET_EXISTS;  // Already signed
        }
    }
    
    // Create canonical checkpoint hash
    uint8_t cp_hash[32];
    checkpoint_hash(cp, cp_hash);
    
    ValidatorSignature* sig = &cp->signatures[cp->num_signatures];
    memcpy(sig->validator_pubkey, validator->public_key, 32);
    sig->signed_at = (uint64_t)time(NULL);
    
    // SECURITY: Use REAL Ed25519 signature via libsodium
    if (crypto_sign_detached(sig->signature, NULL, cp_hash, 32, validator->secret_key) != 0) {
        return PC_ERR_CRYPTO;
    }
    
    cp->num_signatures++;
    
    printf("Checkpoint #%lu signed by validator ", cp->checkpoint_id);
    for (int i = 0; i < 8; i++) printf("%02x", validator->public_key[i]);
    printf("...\n");
    
    return PC_OK;
}

// Verify a single checkpoint signature - REAL Ed25519 verification
int checkpoint_verify_signature(const PCCheckpoint* cp, uint32_t sig_idx,
                                 const uint8_t* validator_pubkey) {
    if (!cp || sig_idx >= cp->num_signatures) return 0;
    
    const ValidatorSignature* sig = &cp->signatures[sig_idx];
    
    // Check pubkey matches
    if (memcmp(sig->validator_pubkey, validator_pubkey, 32) != 0) {
        return 0;
    }
    
    // Create canonical checkpoint hash
    uint8_t cp_hash[32];
    checkpoint_hash(cp, cp_hash);
    
    // SECURITY: Use REAL Ed25519 verification via libsodium
    if (crypto_sign_verify_detached(sig->signature, cp_hash, 32, sig->validator_pubkey) != 0) {
        printf("SECURITY: Invalid checkpoint signature from validator ");
        for (int i = 0; i < 8; i++) printf("%02x", validator_pubkey[i]);
        printf("...\n");
        return 0;
    }
    
    return 1;
}

// Verify ALL signatures on a checkpoint
int checkpoint_verify_all_signatures(const PCCheckpoint* cp) {
    if (!cp) return 0;
    
    int valid_count = 0;
    
    for (uint32_t i = 0; i < cp->num_signatures; i++) {
        // Create canonical checkpoint hash
        uint8_t cp_hash[32];
        checkpoint_hash(cp, cp_hash);
        
        // Verify signature
        if (crypto_sign_verify_detached(
                cp->signatures[i].signature, 
                cp_hash, 32, 
                cp->signatures[i].validator_pubkey) == 0) {
            valid_count++;
        } else {
            printf("SECURITY: Invalid signature at index %u\n", i);
        }
    }
    
    return valid_count;
}

// Check if checkpoint has reached finality
int checkpoint_check_finality(PCCheckpoint* cp, uint32_t total_validators) {
    if (!cp || total_validators == 0) return 0;
    
    // First verify all signatures are valid
    int valid_sigs = checkpoint_verify_all_signatures(cp);
    
    uint32_t threshold = (total_validators * FINALITY_THRESHOLD) / 100;
    if (threshold == 0) threshold = 1;
    
    if ((uint32_t)valid_sigs >= threshold) {
        cp->finalized = 1;
        printf("Checkpoint #%lu finalized with %d/%u valid signatures (threshold: %u)\n",
               cp->checkpoint_id, valid_sigs, cp->num_signatures, threshold);
        return 1;
    }
    
    return 0;
}

// Add finalized checkpoint to chain
PCError checkpoint_commit(PCCheckpointChain* chain, const PCCheckpoint* cp) {
    if (!chain || !cp) return PC_ERR_IO;
    if (!cp->finalized) return PC_ERR_INVALID_SIGNATURE;
    
    // SECURITY: Verify all signatures before committing
    int valid = checkpoint_verify_all_signatures(cp);
    if (valid == 0) {
        printf("SECURITY: Cannot commit checkpoint - no valid signatures\n");
        return PC_ERR_INVALID_SIGNATURE;
    }
    
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

// Serialize checkpoint
size_t checkpoint_serialize(const PCCheckpoint* cp, uint8_t* buffer, size_t max) {
    if (!cp || !buffer) return 0;
    
    size_t needed = 8 + 8 + 32 + 32 + 8 + 8 + 4 + (cp->num_signatures * (32 + 64 + 8)) + 4;
    if (max < needed) return 0;
    
    size_t offset = 0;
    
    memcpy(buffer + offset, &cp->checkpoint_id, 8); offset += 8;
    memcpy(buffer + offset, &cp->tx_count_since_last, 8); offset += 8;
    memcpy(buffer + offset, cp->state_hash, 32); offset += 32;
    memcpy(buffer + offset, cp->prev_checkpoint_hash, 32); offset += 32;
    memcpy(buffer + offset, &cp->timestamp, 8); offset += 8;
    memcpy(buffer + offset, &cp->total_supply, 8); offset += 8;
    memcpy(buffer + offset, &cp->num_signatures, 4); offset += 4;
    
    for (uint32_t i = 0; i < cp->num_signatures; i++) {
        memcpy(buffer + offset, cp->signatures[i].validator_pubkey, 32); offset += 32;
        memcpy(buffer + offset, cp->signatures[i].signature, 64); offset += 64;
        memcpy(buffer + offset, &cp->signatures[i].signed_at, 8); offset += 8;
    }
    
    memcpy(buffer + offset, &cp->finalized, 4); offset += 4;
    
    return offset;
}

// Deserialize checkpoint
PCError checkpoint_deserialize(PCCheckpoint* cp, const uint8_t* buffer, size_t size) {
    if (!cp || !buffer) return PC_ERR_IO;
    
    size_t min_size = 8 + 8 + 32 + 32 + 8 + 8 + 4 + 4;
    if (size < min_size) return PC_ERR_IO;
    
    size_t offset = 0;
    
    memcpy(&cp->checkpoint_id, buffer + offset, 8); offset += 8;
    memcpy(&cp->tx_count_since_last, buffer + offset, 8); offset += 8;
    memcpy(cp->state_hash, buffer + offset, 32); offset += 32;
    memcpy(cp->prev_checkpoint_hash, buffer + offset, 32); offset += 32;
    memcpy(&cp->timestamp, buffer + offset, 8); offset += 8;
    memcpy(&cp->total_supply, buffer + offset, 8); offset += 8;
    memcpy(&cp->num_signatures, buffer + offset, 4); offset += 4;
    
    if (cp->num_signatures > MAX_VALIDATORS) return PC_ERR_IO;
    
    size_t sig_size = cp->num_signatures * (32 + 64 + 8);
    if (size < min_size + sig_size) return PC_ERR_IO;
    
    for (uint32_t i = 0; i < cp->num_signatures; i++) {
        memcpy(cp->signatures[i].validator_pubkey, buffer + offset, 32); offset += 32;
        memcpy(cp->signatures[i].signature, buffer + offset, 64); offset += 64;
        memcpy(&cp->signatures[i].signed_at, buffer + offset, 8); offset += 8;
    }
    
    memcpy(&cp->finalized, buffer + offset, 4);
    
    return PC_OK;
}

// Print checkpoint for debugging
void checkpoint_print(const PCCheckpoint* cp) {
    if (!cp) return;
    
    printf("Checkpoint #%lu:\n", cp->checkpoint_id);
    printf("  TXs since last: %lu\n", cp->tx_count_since_last);
    printf("  Timestamp: %lu\n", cp->timestamp);
    printf("  Total Supply: %.8f\n", cp->total_supply);
    printf("  State hash: ");
    for (int i = 0; i < 8; i++) printf("%02x", cp->state_hash[i]);
    printf("...\n");
    printf("  Signatures: %u\n", cp->num_signatures);
    
    for (uint32_t i = 0; i < cp->num_signatures; i++) {
        printf("    [%u] Validator: ", i);
        for (int j = 0; j < 8; j++) printf("%02x", cp->signatures[i].validator_pubkey[j]);
        printf("... at %lu\n", cp->signatures[i].signed_at);
    }
    
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
