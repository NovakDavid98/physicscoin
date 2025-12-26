// serialize.c - State serialization and persistence
// Binary format for compact state storage

#include "../include/physicscoin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAGIC_NUMBER 0x50485953  // "PHYS"
#define FORMAT_VERSION 1

// Binary header
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t format_version;
    uint64_t state_version;
    uint64_t timestamp;
    uint32_t num_wallets;
    double total_supply;
    uint8_t state_hash[PHYSICSCOIN_HASH_SIZE];
    uint8_t prev_hash[PHYSICSCOIN_HASH_SIZE];
} StateHeader;

// Serialize state to buffer
size_t pc_state_serialize(const PCState* state, uint8_t* buffer, size_t max_size) {
    size_t header_size = sizeof(StateHeader);
    size_t wallet_size = state->num_wallets * sizeof(PCWallet);
    size_t total_size = header_size + wallet_size;
    
    if (total_size > max_size) return 0;
    
    // Write header
    StateHeader* hdr = (StateHeader*)buffer;
    hdr->magic = MAGIC_NUMBER;
    hdr->format_version = FORMAT_VERSION;
    hdr->state_version = state->version;
    hdr->timestamp = state->timestamp;
    hdr->num_wallets = state->num_wallets;
    hdr->total_supply = state->total_supply;
    memcpy(hdr->state_hash, state->state_hash, PHYSICSCOIN_HASH_SIZE);
    memcpy(hdr->prev_hash, state->prev_hash, PHYSICSCOIN_HASH_SIZE);
    
    // Write wallets
    memcpy(buffer + header_size, state->wallets, wallet_size);
    
    return total_size;
}

// Deserialize state from buffer
PCError pc_state_deserialize(PCState* state, const uint8_t* buffer, size_t size) {
    if (size < sizeof(StateHeader)) return PC_ERR_IO;
    
    const StateHeader* hdr = (const StateHeader*)buffer;
    
    // Validate magic
    if (hdr->magic != MAGIC_NUMBER) return PC_ERR_IO;
    if (hdr->format_version != FORMAT_VERSION) return PC_ERR_IO;
    
    // Initialize state
    pc_state_free(state);
    
    state->version = hdr->state_version;
    state->timestamp = hdr->timestamp;
    state->num_wallets = hdr->num_wallets;
    state->total_supply = hdr->total_supply;
    memcpy(state->state_hash, hdr->state_hash, PHYSICSCOIN_HASH_SIZE);
    memcpy(state->prev_hash, hdr->prev_hash, PHYSICSCOIN_HASH_SIZE);
    
    // Allocate wallets
    state->wallets_capacity = state->num_wallets + 100;
    state->wallets = calloc(state->wallets_capacity, sizeof(PCWallet));
    if (!state->wallets) return PC_ERR_IO;
    
    // Read wallets
    size_t wallet_size = state->num_wallets * sizeof(PCWallet);
    if (size < sizeof(StateHeader) + wallet_size) return PC_ERR_IO;
    
    memcpy(state->wallets, buffer + sizeof(StateHeader), wallet_size);
    
    return PC_OK;
}

// Save state to file
PCError pc_state_save(const PCState* state, const char* filename) {
    size_t buffer_size = sizeof(StateHeader) + state->num_wallets * sizeof(PCWallet);
    uint8_t* buffer = malloc(buffer_size);
    if (!buffer) return PC_ERR_IO;
    
    size_t written = pc_state_serialize(state, buffer, buffer_size);
    if (written == 0) {
        free(buffer);
        return PC_ERR_IO;
    }
    
    FILE* f = fopen(filename, "wb");
    if (!f) {
        free(buffer);
        return PC_ERR_IO;
    }
    
    size_t result = fwrite(buffer, 1, written, f);
    fclose(f);
    free(buffer);
    
    return (result == written) ? PC_OK : PC_ERR_IO;
}

// Load state from file
PCError pc_state_load(PCState* state, const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return PC_ERR_IO;
    
    // Get file size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size <= 0) {
        fclose(f);
        return PC_ERR_IO;
    }
    
    uint8_t* buffer = malloc(size);
    if (!buffer) {
        fclose(f);
        return PC_ERR_IO;
    }
    
    size_t read = fread(buffer, 1, size, f);
    fclose(f);
    
    if ((long)read != size) {
        free(buffer);
        return PC_ERR_IO;
    }
    
    PCError err = pc_state_deserialize(state, buffer, size);
    free(buffer);
    
    return err;
}
