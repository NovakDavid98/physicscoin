// faucet.c - Testnet Faucet Implementation
#include "../include/faucet.h"
#include "../include/network_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_FAUCET_RECORDS 10000
#define FAUCET_DATA_FILE "faucet_history.dat"

static PCFaucetRecord* g_faucet_records = NULL;
static uint32_t g_num_records = 0;
static int g_initialized = 0;

// Initialize faucet system
void pc_faucet_init(void) {
    if (g_initialized) return;
    
    g_faucet_records = calloc(MAX_FAUCET_RECORDS, sizeof(PCFaucetRecord));
    g_num_records = 0;
    
    // Try to load existing history
    FILE* f = fopen(FAUCET_DATA_FILE, "rb");
    if (f) {
        fread(&g_num_records, sizeof(uint32_t), 1, f);
        if (g_num_records > MAX_FAUCET_RECORDS) g_num_records = 0;
        fread(g_faucet_records, sizeof(PCFaucetRecord), g_num_records, f);
        fclose(f);
        printf("Loaded %u faucet records\n", g_num_records);
    }
    
    g_initialized = 1;
}

// Save faucet history
static void faucet_save(void) {
    FILE* f = fopen(FAUCET_DATA_FILE, "wb");
    if (f) {
        fwrite(&g_num_records, sizeof(uint32_t), 1, f);
        fwrite(g_faucet_records, sizeof(PCFaucetRecord), g_num_records, f);
        fclose(f);
    }
}

// Find faucet record for address
static PCFaucetRecord* faucet_find_record(const uint8_t* address) {
    for (uint32_t i = 0; i < g_num_records; i++) {
        if (memcmp(g_faucet_records[i].address, address, 32) == 0) {
            return &g_faucet_records[i];
        }
    }
    return NULL;
}

// Check if address can request faucet
int pc_faucet_can_request(const uint8_t* address) {
    if (!g_initialized) pc_faucet_init();
    
    const PCNetworkConfig* config = pc_network_get_config(pc_network_get_current());
    if (!config->has_faucet) return 0;
    
    PCFaucetRecord* record = faucet_find_record(address);
    if (!record) return 1;  // Never requested before
    
    uint64_t now = (uint64_t)time(NULL);
    uint64_t elapsed = now - record->last_request_time;
    
    return elapsed >= config->faucet_cooldown;
}

// Get time until next request
uint64_t pc_faucet_time_until_next(const uint8_t* address) {
    if (!g_initialized) pc_faucet_init();
    
    const PCNetworkConfig* config = pc_network_get_config(pc_network_get_current());
    PCFaucetRecord* record = faucet_find_record(address);
    
    if (!record) return 0;
    
    uint64_t now = (uint64_t)time(NULL);
    uint64_t elapsed = now - record->last_request_time;
    
    if (elapsed >= config->faucet_cooldown) return 0;
    return config->faucet_cooldown - elapsed;
}

// Request faucet funds
PCError pc_faucet_request(PCState* state, const uint8_t* address, double* amount_out) {
    if (!g_initialized) pc_faucet_init();
    
    const PCNetworkConfig* config = pc_network_get_config(pc_network_get_current());
    
    // Check if faucet is enabled for this network
    if (!config->has_faucet) {
        return PC_ERR_INVALID_STATE;
    }
    
    // Check rate limit
    if (!pc_faucet_can_request(address)) {
        return PC_ERR_RATE_LIMIT;
    }
    
    // Get or create wallet
    PCWallet* wallet = pc_state_get_wallet(state, address);
    if (!wallet) {
        PCError err = pc_state_create_wallet(state, address, 0.0);
        if (err != PC_OK) return err;
        wallet = pc_state_get_wallet(state, address);
    }
    
    // Add funds
    wallet->energy += config->faucet_amount;
    state->total_supply += config->faucet_amount;
    
    // Record request
    PCFaucetRecord* record = faucet_find_record(address);
    if (!record) {
        if (g_num_records >= MAX_FAUCET_RECORDS) {
            // Remove oldest record
            memmove(&g_faucet_records[0], &g_faucet_records[1],
                    (MAX_FAUCET_RECORDS - 1) * sizeof(PCFaucetRecord));
            g_num_records = MAX_FAUCET_RECORDS - 1;
        }
        record = &g_faucet_records[g_num_records++];
        memcpy(record->address, address, 32);
    }
    
    record->last_request_time = (uint64_t)time(NULL);
    faucet_save();
    
    if (amount_out) *amount_out = config->faucet_amount;
    
    return PC_OK;
}

// Clear faucet history
void pc_faucet_clear(void) {
    if (!g_initialized) return;
    g_num_records = 0;
    faucet_save();
}

// Free faucet resources
void pc_faucet_free(void) {
    if (!g_initialized) return;
    if (g_faucet_records) {
        free(g_faucet_records);
        g_faucet_records = NULL;
    }
    g_num_records = 0;
    g_initialized = 0;
}

