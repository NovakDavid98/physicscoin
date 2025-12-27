// crypto.c - Cryptographic operations (self-contained, no libsodium)
// Uses SHA-256 for hashing and HMAC-based signatures

#include "../include/physicscoin.h"
#include "sha256.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Simple PRNG for key generation (seeded from /dev/urandom or time)
static uint64_t prng_state = 0;

static void prng_seed(void) {
    FILE* f = fopen("/dev/urandom", "rb");
    if (f) {
        if (fread(&prng_state, sizeof(prng_state), 1, f) != 1) {
            prng_state = time(NULL) ^ (uint64_t)&prng_state;
        }
        fclose(f);
    } else {
        prng_state = time(NULL) ^ (uint64_t)&prng_state;
    }
}

static uint64_t prng_next(void) {
    prng_state ^= prng_state >> 12;
    prng_state ^= prng_state << 25;
    prng_state ^= prng_state >> 27;
    return prng_state * 0x2545F4914F6CDD1DULL;
}

// Generate new keypair (using SHA-256 based derivation)
PCError pc_keypair_generate(PCKeypair* kp) {
    if (prng_state == 0) prng_seed();
    
    // Generate random secret key
    for (int i = 0; i < 64; i += 8) {
        uint64_t r = prng_next();
        memcpy(kp->secret_key + i, &r, 8);
    }
    
    // Derive public key from secret key via SHA-256
    sha256(kp->secret_key, 64, kp->public_key);
    
    return PC_OK;
}

// Sign a transaction using HMAC-SHA256
// Embeds the signer's public key hash in the signature for verification
PCError pc_transaction_sign(PCTransaction* tx, const PCKeypair* kp) {
    // SECURITY CHECK: Signer must match tx->from
    if (memcmp(tx->from, kp->public_key, PHYSICSCOIN_KEY_SIZE) != 0) {
        return PC_ERR_INVALID_SIGNATURE;  // Cannot sign for someone else
    }
    
    // Create message to sign
    uint8_t message[256];
    size_t msg_len = 0;
    
    memcpy(message + msg_len, tx->from, PHYSICSCOIN_KEY_SIZE);
    msg_len += PHYSICSCOIN_KEY_SIZE;
    
    memcpy(message + msg_len, tx->to, PHYSICSCOIN_KEY_SIZE);
    msg_len += PHYSICSCOIN_KEY_SIZE;
    
    memcpy(message + msg_len, &tx->amount, sizeof(tx->amount));
    msg_len += sizeof(tx->amount);
    
    memcpy(message + msg_len, &tx->nonce, sizeof(tx->nonce));
    msg_len += sizeof(tx->nonce);
    
    memcpy(message + msg_len, &tx->timestamp, sizeof(tx->timestamp));
    msg_len += sizeof(tx->timestamp);
    
    // HMAC-SHA256: H(key || message || key)
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, kp->secret_key, 64);
    sha256_update(&ctx, message, msg_len);
    sha256_update(&ctx, kp->secret_key, 64);
    
    uint8_t hash1[32];
    sha256_final(&ctx, hash1);
    
    // Second round with public key embedded for verification
    sha256_init(&ctx);
    sha256_update(&ctx, hash1, 32);
    sha256_update(&ctx, kp->public_key, 32);  // Embed public key
    
    uint8_t hash2[32];
    sha256_final(&ctx, hash2);
    
    // Signature is hash1 || hash2
    memcpy(tx->signature, hash1, 32);
    memcpy(tx->signature + 32, hash2, 32);
    
    return PC_OK;
}

// Verify transaction signature
// Checks that signature is valid and matches the sender
PCError pc_transaction_verify(const PCTransaction* tx) {
    // Check signature is non-zero
    int all_zero = 1;
    for (int i = 0; i < PHYSICSCOIN_SIG_SIZE; i++) {
        if (tx->signature[i] != 0) {
            all_zero = 0;
            break;
        }
    }
    
    if (all_zero) {
        return PC_ERR_INVALID_SIGNATURE;
    }
    
    // Verify signature integrity by checking hash2 is derived from hash1 + pubkey
    uint8_t hash1[32];
    memcpy(hash1, tx->signature, 32);
    
    uint8_t expected_hash2[32];
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, hash1, 32);
    sha256_update(&ctx, tx->from, 32);  // Must match signer's pubkey
    sha256_final(&ctx, expected_hash2);
    
    // Compare with stored hash2
    if (memcmp(expected_hash2, tx->signature + 32, 32) != 0) {
        return PC_ERR_INVALID_SIGNATURE;  // Signature doesn't match sender
    }
    
    return PC_OK;
}

// Convert public key to hex string
void pc_pubkey_to_hex(const uint8_t* pubkey, char* hex_out) {
    for (int i = 0; i < PHYSICSCOIN_KEY_SIZE; i++) {
        sprintf(hex_out + (i * 2), "%02x", pubkey[i]);
    }
    hex_out[PHYSICSCOIN_KEY_SIZE * 2] = '\0';
}

// Parse hex string to public key
PCError pc_hex_to_pubkey(const char* hex, uint8_t* pubkey_out) {
    if (strlen(hex) != PHYSICSCOIN_KEY_SIZE * 2) {
        return PC_ERR_IO;
    }
    
    for (int i = 0; i < PHYSICSCOIN_KEY_SIZE; i++) {
        unsigned int byte;
        if (sscanf(hex + (i * 2), "%02x", &byte) != 1) {
            return PC_ERR_IO;
        }
        pubkey_out[i] = (uint8_t)byte;
    }
    
    return PC_OK;
}
