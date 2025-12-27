// crypto.c - Cryptographic operations with libsecp256k1
// Real ECDSA signatures compatible with Bitcoin/Ethereum

#include "../include/physicscoin.h"
#include "sha256.h"
#include <secp256k1.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Global secp256k1 context
static secp256k1_context* secp_ctx = NULL;

// Keypair storage for verification (maps pubkey hash -> full pubkey)
// In production, this would be in state or recovered from signature
#define MAX_KEYS 10000
static struct {
    uint8_t pubkey_hash[32];
    secp256k1_pubkey pubkey;
    uint8_t seckey[32];
} key_store[MAX_KEYS];
static int key_count = 0;

// Initialize secp256k1 context
static void ensure_secp_ctx(void) {
    if (!secp_ctx) {
        secp_ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    }
}

// Get random bytes from /dev/urandom
static int get_random_bytes(uint8_t* buf, size_t len) {
    FILE* f = fopen("/dev/urandom", "rb");
    if (!f) return 0;
    size_t read = fread(buf, 1, len, f);
    fclose(f);
    return read == len;
}

// Find stored pubkey by hash
static secp256k1_pubkey* find_pubkey(const uint8_t* pubkey_hash) {
    for (int i = 0; i < key_count; i++) {
        if (memcmp(key_store[i].pubkey_hash, pubkey_hash, 32) == 0) {
            return &key_store[i].pubkey;
        }
    }
    return NULL;
}

// Generate new keypair using secp256k1
PCError pc_keypair_generate(PCKeypair* kp) {
    ensure_secp_ctx();
    
    // Generate random 32-byte secret key
    uint8_t seckey[32];
    if (!get_random_bytes(seckey, 32)) {
        return PC_ERR_IO;
    }
    
    // Verify it's a valid secret key
    while (!secp256k1_ec_seckey_verify(secp_ctx, seckey)) {
        if (!get_random_bytes(seckey, 32)) {
            return PC_ERR_IO;
        }
    }
    
    // Store secret key (padded to 64 bytes for compatibility)
    memset(kp->secret_key, 0, 64);
    memcpy(kp->secret_key, seckey, 32);
    
    // Derive public key
    secp256k1_pubkey pubkey;
    if (!secp256k1_ec_pubkey_create(secp_ctx, &pubkey, seckey)) {
        return PC_ERR_IO;
    }
    
    // Serialize public key (compressed, 33 bytes)
    uint8_t pubkey_ser[33];
    size_t pubkey_len = 33;
    secp256k1_ec_pubkey_serialize(secp_ctx, pubkey_ser, &pubkey_len, &pubkey, SECP256K1_EC_COMPRESSED);
    
    // Use SHA256 of serialized pubkey as our 32-byte public key
    sha256(pubkey_ser, pubkey_len, kp->public_key);
    
    // Store for later verification
    if (key_count < MAX_KEYS) {
        memcpy(key_store[key_count].pubkey_hash, kp->public_key, 32);
        memcpy(&key_store[key_count].pubkey, &pubkey, sizeof(secp256k1_pubkey));
        memcpy(key_store[key_count].seckey, seckey, 32);
        key_count++;
    }
    
    return PC_OK;
}

// Create message hash for signing/verification
static void create_msg_hash(const PCTransaction* tx, uint8_t* msg_hash) {
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
    
    sha256(message, msg_len, msg_hash);
}

// Sign a transaction using ECDSA
PCError pc_transaction_sign(PCTransaction* tx, const PCKeypair* kp) {
    ensure_secp_ctx();
    
    // SECURITY CHECK: Signer must match tx->from
    if (memcmp(tx->from, kp->public_key, PHYSICSCOIN_KEY_SIZE) != 0) {
        return PC_ERR_INVALID_SIGNATURE;
    }
    
    // Create message hash
    uint8_t msg_hash[32];
    create_msg_hash(tx, msg_hash);
    
    // Sign with ECDSA
    secp256k1_ecdsa_signature sig;
    if (!secp256k1_ecdsa_sign(secp_ctx, &sig, msg_hash, kp->secret_key, NULL, NULL)) {
        return PC_ERR_INVALID_SIGNATURE;
    }
    
    // Serialize signature to 64 bytes
    secp256k1_ecdsa_signature_serialize_compact(secp_ctx, tx->signature, &sig);
    
    return PC_OK;
}

// Verify transaction signature
PCError pc_transaction_verify(const PCTransaction* tx) {
    ensure_secp_ctx();
    
    // Check signature is non-zero
    int all_zero = 1;
    for (int i = 0; i < PHYSICSCOIN_SIG_SIZE; i++) {
        if (tx->signature[i] != 0) {
            all_zero = 0;
            break;
        }
    }
    if (all_zero) return PC_ERR_INVALID_SIGNATURE;
    
    // Recreate message hash
    uint8_t msg_hash[32];
    create_msg_hash(tx, msg_hash);
    
    // Parse signature
    secp256k1_ecdsa_signature sig;
    if (!secp256k1_ecdsa_signature_parse_compact(secp_ctx, &sig, tx->signature)) {
        return PC_ERR_INVALID_SIGNATURE;
    }
    
    // Find the public key for this sender
    secp256k1_pubkey* pubkey = find_pubkey(tx->from);
    if (!pubkey) {
        // Key not in store - can't verify
        // In production, use recoverable signatures or include pubkey in TX
        return PC_ERR_INVALID_SIGNATURE;
    }
    
    // Verify ECDSA signature
    if (!secp256k1_ecdsa_verify(secp_ctx, &sig, msg_hash, pubkey)) {
        return PC_ERR_INVALID_SIGNATURE;
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

// Cleanup (call on exit)
void pc_crypto_cleanup(void) {
    if (secp_ctx) {
        secp256k1_context_destroy(secp_ctx);
        secp_ctx = NULL;
    }
    key_count = 0;
}
