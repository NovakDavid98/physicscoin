// explorer_api.c - Block Explorer REST API Endpoints
// Query blockchain state, wallets, transactions, consensus

#include "../include/physicscoin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

// External functions from API
extern void send_json_response(int client, int status, const char* body);
extern void send_error(int client, int code, const char* message);
extern char* get_json_field(const char* json, const char* field);
extern double get_json_number(const char* json, const char* field);

// GET /explorer/stats - Network statistics
void handle_explorer_stats(int client, PCState* state) {
    extern uint64_t poa_get_height(void);
    extern uint32_t poa_active_validator_count(void);
    
    uint64_t block_height = poa_get_height();
    uint32_t validators = poa_active_validator_count();
    
    // Calculate richest wallet
    double max_balance = 0;
    for (uint32_t i = 0; i < state->num_wallets; i++) {
        if (state->wallets[i].energy > max_balance) {
            max_balance = state->wallets[i].energy;
        }
    }
    
    // Calculate average balance
    double avg_balance = state->num_wallets > 0 ? 
                         state->total_supply / state->num_wallets : 0.0;
    
    char body[1024];
    snprintf(body, sizeof(body),
             "{"
             "\"block_height\":%lu,"
             "\"total_supply\":%.8f,"
             "\"total_wallets\":%u,"
             "\"avg_balance\":%.8f,"
             "\"max_balance\":%.8f,"
             "\"validators\":%u,"
             "\"state_version\":%lu,"
             "\"timestamp\":%lu"
             "}",
             block_height,
             state->total_supply,
             state->num_wallets,
             avg_balance,
             max_balance,
             validators,
             state->version,
             state->timestamp);
    
    send_json_response(client, 200, body);
}

// GET /explorer/wallet/<address> - Detailed wallet info
void handle_explorer_wallet(int client, PCState* state, const char* address) {
    uint8_t pubkey[32];
    if (pc_hex_to_pubkey(address, pubkey) != PC_OK) {
        send_error(client, -32602, "Invalid address");
        return;
    }
    
    PCWallet* wallet = pc_state_get_wallet(state, pubkey);
    if (!wallet) {
        send_error(client, -32602, "Wallet not found");
        return;
    }
    
    // Calculate wallet rank by balance
    uint32_t rank = 1;
    for (uint32_t i = 0; i < state->num_wallets; i++) {
        if (state->wallets[i].energy > wallet->energy) {
            rank++;
        }
    }
    
    // Calculate percentage of total supply
    double percent = (wallet->energy / state->total_supply) * 100.0;
    
    char body[1024];
    snprintf(body, sizeof(body),
             "{"
             "\"address\":\"%s\","
             "\"balance\":%.8f,"
             "\"nonce\":%lu,"
             "\"rank\":%u,"
             "\"percent_of_supply\":%.4f,"
             "\"exists\":true"
             "}",
             address,
             wallet->energy,
             wallet->nonce,
             rank,
             percent);
    
    send_json_response(client, 200, body);
}

// GET /explorer/rich - Top wallets by balance
void handle_explorer_rich_list(int client, PCState* state) {
    // Create sorted list of wallets
    typedef struct {
        uint32_t index;
        double balance;
    } WalletSort;
    
    WalletSort* sorted = malloc(state->num_wallets * sizeof(WalletSort));
    if (!sorted) {
        send_error(client, -32000, "Memory allocation failed");
        return;
    }
    
    for (uint32_t i = 0; i < state->num_wallets; i++) {
        sorted[i].index = i;
        sorted[i].balance = state->wallets[i].energy;
    }
    
    // Bubble sort (good enough for small lists)
    for (uint32_t i = 0; i < state->num_wallets; i++) {
        for (uint32_t j = i + 1; j < state->num_wallets; j++) {
            if (sorted[j].balance > sorted[i].balance) {
                WalletSort temp = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = temp;
            }
        }
    }
    
    // Build JSON response (top 20)
    char body[8192] = "{\"rich_list\":[";
    char* p = body + strlen(body);
    size_t remaining = sizeof(body) - strlen(body) - 10;
    
    uint32_t limit = state->num_wallets < 20 ? state->num_wallets : 20;
    for (uint32_t i = 0; i < limit && remaining > 200; i++) {
        PCWallet* w = &state->wallets[sorted[i].index];
        char addr[65];
        pc_pubkey_to_hex(w->public_key, addr);
        
        double percent = (w->energy / state->total_supply) * 100.0;
        
        int written = snprintf(p, remaining,
                              "%s{\"rank\":%u,\"address\":\"%.16s...\",\"balance\":%.8f,\"percent\":%.4f}",
                              i > 0 ? "," : "",
                              i + 1,
                              addr,
                              w->energy,
                              percent);
        p += written;
        remaining -= written;
    }
    
    strcat(body, "]}");
    
    free(sorted);
    send_json_response(client, 200, body);
}

// GET /explorer/distribution - Balance distribution statistics
void handle_explorer_distribution(int client, PCState* state) {
    uint32_t tiny = 0;     // < 1
    uint32_t small = 0;    // 1-100
    uint32_t medium = 0;   // 100-1000
    uint32_t large = 0;    // 1000-10000
    uint32_t whale = 0;    // > 10000
    
    for (uint32_t i = 0; i < state->num_wallets; i++) {
        double bal = state->wallets[i].energy;
        if (bal < 1.0) tiny++;
        else if (bal < 100.0) small++;
        else if (bal < 1000.0) medium++;
        else if (bal < 10000.0) large++;
        else whale++;
    }
    
    char body[512];
    snprintf(body, sizeof(body),
             "{"
             "\"total_wallets\":%u,"
             "\"tiny\":{\"count\":%u,\"threshold\":\"< 1\"},"
             "\"small\":{\"count\":%u,\"threshold\":\"1-100\"},"
             "\"medium\":{\"count\":%u,\"threshold\":\"100-1k\"},"
             "\"large\":{\"count\":%u,\"threshold\":\"1k-10k\"},"
             "\"whale\":{\"count\":%u,\"threshold\":\"> 10k\"}"
             "}",
             state->num_wallets,
             tiny, small, medium, large, whale);
    
    send_json_response(client, 200, body);
}

// GET /explorer/search/<query> - Search for address or transaction
void handle_explorer_search(int client, PCState* state, const char* query) {
    // Try as address
    uint8_t pubkey[32];
    if (pc_hex_to_pubkey(query, pubkey) == PC_OK) {
        PCWallet* wallet = pc_state_get_wallet(state, pubkey);
        if (wallet) {
            char body[512];
            snprintf(body, sizeof(body),
                     "{"
                     "\"type\":\"wallet\","
                     "\"address\":\"%s\","
                     "\"balance\":%.8f,"
                     "\"nonce\":%lu"
                     "}",
                     query,
                     wallet->energy,
                     wallet->nonce);
            send_json_response(client, 200, body);
            return;
        }
    }
    
    // Not found
    send_error(client, -32602, "Not found");
}

// GET /explorer/consensus - Consensus state
void handle_explorer_consensus(int client) {
    extern uint64_t poa_get_height(void);
    extern uint32_t poa_active_validator_count(void);
    
    uint64_t height = poa_get_height();
    uint32_t validators = poa_active_validator_count();
    
    char body[512];
    snprintf(body, sizeof(body),
             "{"
             "\"type\":\"proof_of_authority\","
             "\"block_height\":%lu,"
             "\"active_validators\":%u,"
             "\"block_time\":5,"
             "\"quorum_threshold\":67"
             "}",
             height,
             validators);
    
    send_json_response(client, 200, body);
}

// GET /explorer/health - System health check
void handle_explorer_health(int client, PCState* state) {
    PCError cons = pc_state_verify_conservation(state);
    
    double sum = 0.0;
    for (uint32_t i = 0; i < state->num_wallets; i++) {
        sum += state->wallets[i].energy;
    }
    
    double error = fabs(state->total_supply - sum);
    int healthy = (cons == PC_OK && error < 1e-9);
    
    char body[512];
    snprintf(body, sizeof(body),
             "{"
             "\"status\":\"%s\","
             "\"conservation_verified\":%s,"
             "\"conservation_error\":%.12e,"
             "\"total_supply\":%.8f,"
             "\"wallet_sum\":%.8f,"
             "\"wallets\":%u,"
             "\"state_version\":%lu"
             "}",
             healthy ? "healthy" : "unhealthy",
             cons == PC_OK ? "true" : "false",
             error,
             state->total_supply,
             sum,
             state->num_wallets,
             state->version);
    
    send_json_response(client, 200, body);
}

// GET /explorer/state/hash - Current state hash
void handle_explorer_state_hash(int client, PCState* state) {
    char hash_hex[65];
    for (int i = 0; i < 32; i++) {
        sprintf(hash_hex + (i * 2), "%02x", state->state_hash[i]);
    }
    
    char prev_hash_hex[65];
    for (int i = 0; i < 32; i++) {
        sprintf(prev_hash_hex + (i * 2), "%02x", state->prev_hash[i]);
    }
    
    char body[512];
    snprintf(body, sizeof(body),
             "{"
             "\"current_hash\":\"%s\","
             "\"prev_hash\":\"%s\","
             "\"version\":%lu,"
             "\"timestamp\":%lu"
             "}",
             hash_hex,
             prev_hash_hex,
             state->version,
             state->timestamp);
    
    send_json_response(client, 200, body);
}

// GET /explorer/supply - Supply analytics
void handle_explorer_supply(int client, PCState* state) {
    double circulating = 0.0;
    uint32_t active_wallets = 0;
    
    for (uint32_t i = 0; i < state->num_wallets; i++) {
        if (state->wallets[i].energy > 0) {
            circulating += state->wallets[i].energy;
            active_wallets++;
        }
    }
    
    double velocity = active_wallets > 0 ? circulating / active_wallets : 0.0;
    
    char body[512];
    snprintf(body, sizeof(body),
             "{"
             "\"total_supply\":%.8f,"
             "\"circulating_supply\":%.8f,"
             "\"active_wallets\":%u,"
             "\"inactive_wallets\":%u,"
             "\"velocity\":%.8f"
             "}",
             state->total_supply,
             circulating,
             active_wallets,
             state->num_wallets - active_wallets,
             velocity);
    
    send_json_response(client, 200, body);
}

// Integrate explorer endpoints into main API server
void register_explorer_endpoints(void) {
    printf("Explorer API endpoints registered:\n");
    printf("  GET  /explorer/stats          - Network statistics\n");
    printf("  GET  /explorer/wallet/<addr>  - Wallet details\n");
    printf("  GET  /explorer/rich            - Rich list (top 20)\n");
    printf("  GET  /explorer/distribution    - Balance distribution\n");
    printf("  GET  /explorer/search/<query>  - Search address\n");
    printf("  GET  /explorer/consensus       - Consensus state\n");
    printf("  GET  /explorer/health          - System health\n");
    printf("  GET  /explorer/state/hash      - State hash info\n");
    printf("  GET  /explorer/supply          - Supply analytics\n");
}

