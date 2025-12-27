// faucet.h - Testnet Faucet System
#ifndef PHYSICSCOIN_FAUCET_H
#define PHYSICSCOIN_FAUCET_H

#include "../include/physicscoin.h"
#include <stdint.h>

// Faucet request record
typedef struct {
    uint8_t address[32];
    uint64_t last_request_time;
} PCFaucetRecord;

// Initialize faucet system
void pc_faucet_init(void);

// Request faucet funds
// Returns PC_OK on success, PC_ERR_RATE_LIMIT if cooldown not expired
PCError pc_faucet_request(PCState* state, const uint8_t* address, double* amount_out);

// Check if address can request faucet
int pc_faucet_can_request(const uint8_t* address);

// Get time until next request is allowed
uint64_t pc_faucet_time_until_next(const uint8_t* address);

// Clear faucet history (for testing)
void pc_faucet_clear(void);

// Free faucet resources
void pc_faucet_free(void);

#endif // PHYSICSCOIN_FAUCET_H

