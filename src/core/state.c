// state.c - Universe state management
// Core of PhysicsCoin: energy-conserving ledger

#include "../include/physicscoin.h"
#include "../crypto/sha256.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

// Initialize empty state
PCError pc_state_init(PCState* state) {
    if (!state) return PC_ERR_IO;
    
    memset(state, 0, sizeof(PCState));
    state->version = 1;
    state->timestamp = (uint64_t)time(NULL);
    state->num_wallets = 0;
    state->total_supply = 0.0;
    
    state->wallets_capacity = 100;
    state->wallets = calloc(state->wallets_capacity, sizeof(PCWallet));
    if (!state->wallets) return PC_ERR_IO;
    
    memset(state->state_hash, 0, PHYSICSCOIN_HASH_SIZE);
    memset(state->prev_hash, 0, PHYSICSCOIN_HASH_SIZE);
    
    return PC_OK;
}

// Free state resources
void pc_state_free(PCState* state) {
    if (state && state->wallets) {
        free(state->wallets);
        state->wallets = NULL;
    }
}

// Create genesis state
PCError pc_state_genesis(PCState* state, const uint8_t* founder_pubkey, double initial_supply) {
    if (initial_supply <= 0) return PC_ERR_INVALID_AMOUNT;
    
    PCError err = pc_state_init(state);
    if (err != PC_OK) return err;
    
    // Create founder wallet with all supply
    err = pc_state_create_wallet(state, founder_pubkey, initial_supply);
    if (err != PC_OK) return err;
    
    state->total_supply = initial_supply;
    pc_state_compute_hash(state);
    
    return PC_OK;
}

// Find wallet by public key
PCWallet* pc_state_get_wallet(PCState* state, const uint8_t* pubkey) {
    for (uint32_t i = 0; i < state->num_wallets; i++) {
        if (memcmp(state->wallets[i].public_key, pubkey, PHYSICSCOIN_KEY_SIZE) == 0) {
            return &state->wallets[i];
        }
    }
    return NULL;
}

// Create new wallet
PCError pc_state_create_wallet(PCState* state, const uint8_t* pubkey, double initial_balance) {
    // Check if already exists
    if (pc_state_get_wallet(state, pubkey)) {
        return PC_ERR_WALLET_EXISTS;
    }
    
    // Expand capacity if needed
    if (state->num_wallets >= state->wallets_capacity) {
        size_t new_cap = state->wallets_capacity == 0 ? 8 : state->wallets_capacity * 2;
        if (new_cap > PHYSICSCOIN_MAX_WALLETS) {
            return PC_ERR_MAX_WALLETS;
        }
        PCWallet* new_wallets = realloc(state->wallets, new_cap * sizeof(PCWallet));
        if (!new_wallets) return PC_ERR_IO;
        state->wallets = new_wallets;
        state->wallets_capacity = new_cap;
    }
    
    // Add wallet
    PCWallet* w = &state->wallets[state->num_wallets];
    memcpy(w->public_key, pubkey, PHYSICSCOIN_KEY_SIZE);
    w->energy = initial_balance;
    w->nonce = 0;
    
    state->num_wallets++;
    
    // Update total supply only for genesis
    if (initial_balance > 0) {
        state->total_supply += initial_balance;
    }
    
    return PC_OK;
}

// Execute transaction (atomic energy transfer)
PCError pc_state_execute_tx(PCState* state, const PCTransaction* tx) {
    // Validate signature first
    PCError err = pc_transaction_verify(tx);
    if (err != PC_OK) return err;
    
    // Find wallets
    PCWallet* from = pc_state_get_wallet(state, tx->from);
    PCWallet* to = pc_state_get_wallet(state, tx->to);
    
    if (!from) return PC_ERR_WALLET_NOT_FOUND;
    
    // Create recipient wallet if it doesn't exist
    if (!to) {
        err = pc_state_create_wallet(state, tx->to, 0);
        if (err != PC_OK) return err;
        to = pc_state_get_wallet(state, tx->to);
    }
    
    // Check nonce (replay protection)
    if (tx->nonce != from->nonce) {
        return PC_ERR_INVALID_SIGNATURE;
    }
    
    // Check sufficient funds
    if (from->energy < tx->amount) {
        return PC_ERR_INSUFFICIENT_FUNDS;
    }
    
    if (tx->amount <= 0) {
        return PC_ERR_INVALID_AMOUNT;
    }
    
    // ===== ATOMIC ENERGY TRANSFER =====
    double before_sum = from->energy + to->energy;
    
    from->energy -= tx->amount;
    to->energy += tx->amount;
    from->nonce++;
    
    double after_sum = from->energy + to->energy;
    
    // Verify conservation
    if (fabs(before_sum - after_sum) > 1e-12) {
        from->energy += tx->amount;
        to->energy -= tx->amount;
        from->nonce--;
        return PC_ERR_CONSERVATION_VIOLATED;
    }
    
    // Update state
    state->timestamp = (uint64_t)time(NULL);
    memcpy(state->prev_hash, state->state_hash, PHYSICSCOIN_HASH_SIZE);
    pc_state_compute_hash(state);
    
    return PC_OK;
}

// Verify total energy conservation
PCError pc_state_verify_conservation(const PCState* state) {
    double actual_sum = 0.0;
    for (uint32_t i = 0; i < state->num_wallets; i++) {
        actual_sum += state->wallets[i].energy;
    }
    
    if (fabs(actual_sum - state->total_supply) > 1e-9) {
        return PC_ERR_CONSERVATION_VIOLATED;
    }
    
    return PC_OK;
}

// Compute SHA-256 hash of state
void pc_state_compute_hash(PCState* state) {
    SHA256_CTX ctx;
    sha256_init(&ctx);
    
    sha256_update(&ctx, (uint8_t*)&state->version, sizeof(state->version));
    sha256_update(&ctx, (uint8_t*)&state->timestamp, sizeof(state->timestamp));
    sha256_update(&ctx, (uint8_t*)&state->num_wallets, sizeof(state->num_wallets));
    sha256_update(&ctx, (uint8_t*)&state->total_supply, sizeof(state->total_supply));
    sha256_update(&ctx, state->prev_hash, PHYSICSCOIN_HASH_SIZE);
    
    for (uint32_t i = 0; i < state->num_wallets; i++) {
        PCWallet* w = &state->wallets[i];
        sha256_update(&ctx, w->public_key, PHYSICSCOIN_KEY_SIZE);
        sha256_update(&ctx, (uint8_t*)&w->energy, sizeof(w->energy));
        sha256_update(&ctx, (uint8_t*)&w->nonce, sizeof(w->nonce));
    }
    
    sha256_final(&ctx, state->state_hash);
}

// Error messages
const char* pc_strerror(PCError err) {
    switch (err) {
        case PC_OK: return "Success";
        case PC_ERR_INSUFFICIENT_FUNDS: return "Insufficient funds";
        case PC_ERR_INVALID_SIGNATURE: return "Invalid signature";
        case PC_ERR_WALLET_NOT_FOUND: return "Wallet not found";
        case PC_ERR_WALLET_EXISTS: return "Wallet already exists";
        case PC_ERR_MAX_WALLETS: return "Maximum wallets reached";
        case PC_ERR_INVALID_AMOUNT: return "Invalid amount";
        case PC_ERR_CONSERVATION_VIOLATED: return "Energy conservation violated";
        case PC_ERR_IO: return "I/O error";
        case PC_ERR_CRYPTO: return "Cryptographic error";
        default: return "Unknown error";
    }
}
