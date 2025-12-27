// crypto.c - High-Performance Ed25519 with OpenMP Parallelism
// Uses libsodium Ed25519 + OpenMP for multi-core verification

#include "../include/physicscoin.h"
#include "sha256.h"
#include <sodium.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

static int sodium_initialized = 0;

static void ensure_sodium_init(void) {
    if (!sodium_initialized) {
        if (sodium_init() < 0) {
            fprintf(stderr, "Failed to initialize libsodium\n");
            exit(1);
        }
        sodium_initialized = 1;
    }
}

PCError pc_keypair_generate(PCKeypair* kp) {
    ensure_sodium_init();
    
    unsigned char seed[32];
    unsigned char pk[crypto_sign_PUBLICKEYBYTES];
    unsigned char sk[crypto_sign_SECRETKEYBYTES];
    
    randombytes_buf(seed, 32);
    crypto_sign_seed_keypair(pk, sk, seed);
    
    memcpy(kp->secret_key, sk, 64);
    memcpy(kp->public_key, pk, 32);
    
    return PC_OK;
}

static inline size_t create_message(const PCTransaction* tx, uint8_t* message) {
    memcpy(message, tx->from, 32);
    memcpy(message + 32, tx->to, 32);
    memcpy(message + 64, &tx->amount, 8);
    memcpy(message + 72, &tx->nonce, 8);
    memcpy(message + 80, &tx->timestamp, 8);
    return 88;
}

PCError pc_transaction_sign(PCTransaction* tx, const PCKeypair* kp) {
    ensure_sodium_init();
    
    if (memcmp(tx->from, kp->public_key, 32) != 0) {
        return PC_ERR_INVALID_SIGNATURE;
    }
    
    uint8_t message[128];
    size_t msg_len = create_message(tx, message);
    
    crypto_sign_detached(tx->signature, NULL, message, msg_len, kp->secret_key);
    
    return PC_OK;
}

PCError pc_transaction_verify(const PCTransaction* tx) {
    ensure_sodium_init();
    
    if (*(uint64_t*)tx->signature == 0 && *(uint64_t*)(tx->signature + 8) == 0) {
        return PC_ERR_INVALID_SIGNATURE;
    }
    
    uint8_t message[128];
    size_t msg_len = create_message(tx, message);
    
    if (crypto_sign_verify_detached(tx->signature, message, msg_len, tx->from) != 0) {
        return PC_ERR_INVALID_SIGNATURE;
    }
    
    return PC_OK;
}

// OpenMP parallel batch verification
int pc_transaction_verify_batch(const PCTransaction** txs, int count, int* results) {
    ensure_sodium_init();
    
    int success = 0;
    
    #pragma omp parallel for reduction(+:success) schedule(dynamic, 64)
    for (int i = 0; i < count; i++) {
        uint8_t message[128];
        size_t msg_len = create_message(txs[i], message);
        
        if (crypto_sign_verify_detached(txs[i]->signature, message, msg_len, txs[i]->from) == 0) {
            results[i] = 1;
            success++;
        } else {
            results[i] = 0;
        }
    }
    
    return success;
}

void pc_pubkey_to_hex(const uint8_t* pubkey, char* hex_out) {
    for (int i = 0; i < 32; i++) {
        sprintf(hex_out + (i * 2), "%02x", pubkey[i]);
    }
    hex_out[64] = '\0';
}

PCError pc_hex_to_pubkey(const char* hex, uint8_t* pubkey_out) {
    if (strlen(hex) != 64) return PC_ERR_IO;
    
    for (int i = 0; i < 32; i++) {
        unsigned int byte;
        if (sscanf(hex + (i * 2), "%02x", &byte) != 1) return PC_ERR_IO;
        pubkey_out[i] = (uint8_t)byte;
    }
    
    return PC_OK;
}

void pc_crypto_cleanup(void) {}
