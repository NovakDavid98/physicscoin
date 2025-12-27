// proofs.c - Audit Proof System
// Generate and verify balance proofs for any state

#include "../include/physicscoin.h"
#include "../crypto/sha256.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

// Balance proof structure
typedef struct {
    uint8_t state_hash[32];      // The state being proven
    uint8_t wallet_pubkey[32];   // Wallet in question
    double balance;               // Claimed balance
    uint64_t nonce;               // Wallet nonce at that time
    uint64_t timestamp;           // When proof was generated
    uint8_t proof_hash[32];       // Hash binding all fields
} PCBalanceProof;

// Generate a balance proof for a wallet at current state
PCError pc_proof_generate(const PCState* state, const uint8_t* pubkey, PCBalanceProof* proof) {
    if (!state || !pubkey || !proof) return PC_ERR_IO;
    
    // Find wallet
    PCWallet* wallet = NULL;
    for (uint32_t i = 0; i < state->num_wallets; i++) {
        if (memcmp(state->wallets[i].public_key, pubkey, PHYSICSCOIN_KEY_SIZE) == 0) {
            wallet = &state->wallets[i];
            break;
        }
    }
    
    if (!wallet) return PC_ERR_WALLET_NOT_FOUND;
    
    // Fill proof
    memcpy(proof->state_hash, state->state_hash, 32);
    memcpy(proof->wallet_pubkey, pubkey, 32);
    proof->balance = wallet->energy;
    proof->nonce = wallet->nonce;
    proof->timestamp = (uint64_t)time(NULL);
    
    // Compute proof hash: H(state_hash || pubkey || balance || nonce)
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, proof->state_hash, 32);
    sha256_update(&ctx, proof->wallet_pubkey, 32);
    sha256_update(&ctx, (uint8_t*)&proof->balance, sizeof(double));
    sha256_update(&ctx, (uint8_t*)&proof->nonce, sizeof(uint64_t));
    sha256_update(&ctx, (uint8_t*)&proof->timestamp, sizeof(uint64_t));
    sha256_final(&ctx, proof->proof_hash);
    
    return PC_OK;
}

// Verify a balance proof against a state
PCError pc_proof_verify(const PCState* state, const PCBalanceProof* proof) {
    if (!state || !proof) return PC_ERR_IO;
    
    // Check state hash matches
    if (memcmp(state->state_hash, proof->state_hash, 32) != 0) {
        return PC_ERR_INVALID_SIGNATURE;  // State mismatch
    }
    
    // Find wallet and verify balance
    PCWallet* wallet = NULL;
    for (uint32_t i = 0; i < state->num_wallets; i++) {
        if (memcmp(state->wallets[i].public_key, proof->wallet_pubkey, 32) == 0) {
            wallet = &state->wallets[i];
            break;
        }
    }
    
    if (!wallet) return PC_ERR_WALLET_NOT_FOUND;
    
    // Verify balance and nonce match
    if (wallet->energy != proof->balance || wallet->nonce != proof->nonce) {
        return PC_ERR_INVALID_SIGNATURE;
    }
    
    // Verify proof hash
    uint8_t computed_hash[32];
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, proof->state_hash, 32);
    sha256_update(&ctx, proof->wallet_pubkey, 32);
    sha256_update(&ctx, (uint8_t*)&proof->balance, sizeof(double));
    sha256_update(&ctx, (uint8_t*)&proof->nonce, sizeof(uint64_t));
    sha256_update(&ctx, (uint8_t*)&proof->timestamp, sizeof(uint64_t));
    sha256_final(&ctx, computed_hash);
    
    if (memcmp(computed_hash, proof->proof_hash, 32) != 0) {
        return PC_ERR_INVALID_SIGNATURE;
    }
    
    return PC_OK;
}

// Serialize proof to buffer
size_t pc_proof_serialize(const PCBalanceProof* proof, uint8_t* buffer, size_t max) {
    if (max < sizeof(PCBalanceProof)) return 0;
    memcpy(buffer, proof, sizeof(PCBalanceProof));
    return sizeof(PCBalanceProof);
}

// Deserialize proof from buffer
PCError pc_proof_deserialize(PCBalanceProof* proof, const uint8_t* buffer, size_t size) {
    if (size < sizeof(PCBalanceProof)) return PC_ERR_IO;
    memcpy(proof, buffer, sizeof(PCBalanceProof));
    return PC_OK;
}

// Save proof to file
PCError pc_proof_save(const PCBalanceProof* proof, const char* filename) {
    FILE* f = fopen(filename, "wb");
    if (!f) return PC_ERR_IO;
    
    size_t written = fwrite(proof, sizeof(PCBalanceProof), 1, f);
    fclose(f);
    
    return (written == 1) ? PC_OK : PC_ERR_IO;
}

// Load proof from file
PCError pc_proof_load(PCBalanceProof* proof, const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return PC_ERR_IO;
    
    size_t read = fread(proof, sizeof(PCBalanceProof), 1, f);
    fclose(f);
    
    return (read == 1) ? PC_OK : PC_ERR_IO;
}

// Print proof to stdout
void pc_proof_print(const PCBalanceProof* proof) {
    printf("Balance Proof:\n");
    printf("  State Hash: ");
    for (int i = 0; i < 16; i++) printf("%02x", proof->state_hash[i]);
    printf("...\n");
    
    printf("  Wallet:     ");
    for (int i = 0; i < 16; i++) printf("%02x", proof->wallet_pubkey[i]);
    printf("...\n");
    
    printf("  Balance:    %.8f\n", proof->balance);
    printf("  Nonce:      %lu\n", proof->nonce);
    printf("  Timestamp:  %lu\n", proof->timestamp);
    
    printf("  Proof Hash: ");
    for (int i = 0; i < 16; i++) printf("%02x", proof->proof_hash[i]);
    printf("...\n");
}
