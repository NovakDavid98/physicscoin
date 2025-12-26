// physicscoin.h - Public API for PhysicsCoin
// A physics-based cryptocurrency using energy conservation

#ifndef PHYSICSCOIN_H
#define PHYSICSCOIN_H

#include <stdint.h>
#include <stddef.h>

#define PHYSICSCOIN_VERSION "1.0.0"
#define PHYSICSCOIN_MAX_WALLETS 10000
#define PHYSICSCOIN_KEY_SIZE 32
#define PHYSICSCOIN_SIG_SIZE 64
#define PHYSICSCOIN_HASH_SIZE 32

// Error codes
typedef enum {
    PC_OK = 0,
    PC_ERR_INSUFFICIENT_FUNDS = -1,
    PC_ERR_INVALID_SIGNATURE = -2,
    PC_ERR_WALLET_NOT_FOUND = -3,
    PC_ERR_WALLET_EXISTS = -4,
    PC_ERR_MAX_WALLETS = -5,
    PC_ERR_INVALID_AMOUNT = -6,
    PC_ERR_CONSERVATION_VIOLATED = -7,
    PC_ERR_IO = -8,
    PC_ERR_CRYPTO = -9
} PCError;

// Wallet structure
typedef struct {
    uint8_t public_key[PHYSICSCOIN_KEY_SIZE];
    double energy;       // Balance as Hamiltonian energy
    uint64_t nonce;      // Transaction counter (replay protection)
} PCWallet;

// Transaction structure
typedef struct {
    uint8_t from[PHYSICSCOIN_KEY_SIZE];
    uint8_t to[PHYSICSCOIN_KEY_SIZE];
    double amount;
    uint64_t nonce;
    uint64_t timestamp;
    uint8_t signature[PHYSICSCOIN_SIG_SIZE];
} PCTransaction;

// Universe state - the entire ledger
typedef struct {
    uint64_t version;
    uint64_t timestamp;
    uint32_t num_wallets;
    double total_supply;
    uint8_t state_hash[PHYSICSCOIN_HASH_SIZE];
    uint8_t prev_hash[PHYSICSCOIN_HASH_SIZE];
    PCWallet* wallets;
    size_t wallets_capacity;
} PCState;

// Keypair for signing
typedef struct {
    uint8_t public_key[PHYSICSCOIN_KEY_SIZE];
    uint8_t secret_key[64];  // Ed25519 secret key
} PCKeypair;

// ============ Core API ============

// Initialize a new universe state
PCError pc_state_init(PCState* state);

// Free state resources
void pc_state_free(PCState* state);

// Create genesis state with initial supply
PCError pc_state_genesis(PCState* state, const uint8_t* founder_pubkey, double initial_supply);

// Get wallet by public key (returns NULL if not found)
PCWallet* pc_state_get_wallet(PCState* state, const uint8_t* pubkey);

// Create new wallet in state
PCError pc_state_create_wallet(PCState* state, const uint8_t* pubkey, double initial_balance);

// Execute a transaction
PCError pc_state_execute_tx(PCState* state, const PCTransaction* tx);

// Verify conservation law
PCError pc_state_verify_conservation(const PCState* state);

// Compute state hash
void pc_state_compute_hash(PCState* state);

// ============ Crypto API ============

// Generate new keypair
PCError pc_keypair_generate(PCKeypair* kp);

// Sign a transaction
PCError pc_transaction_sign(PCTransaction* tx, const PCKeypair* kp);

// Verify transaction signature
PCError pc_transaction_verify(const PCTransaction* tx);

// ============ Serialization API ============

// Save state to file
PCError pc_state_save(const PCState* state, const char* filename);

// Load state from file
PCError pc_state_load(PCState* state, const char* filename);

// Serialize state to buffer (returns bytes written)
size_t pc_state_serialize(const PCState* state, uint8_t* buffer, size_t max_size);

// Deserialize state from buffer
PCError pc_state_deserialize(PCState* state, const uint8_t* buffer, size_t size);

// ============ Utility ============

// Convert public key to hex string
void pc_pubkey_to_hex(const uint8_t* pubkey, char* hex_out);

// Parse hex string to public key
PCError pc_hex_to_pubkey(const char* hex, uint8_t* pubkey_out);

// Get error message
const char* pc_strerror(PCError err);

#endif // PHYSICSCOIN_H
