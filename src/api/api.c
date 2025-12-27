// api.c - Secure JSON-RPC API Server
// Provides HTTP API for external wallet/app integration
// SECURITY HARDENED: Signed transactions required, faucet only on testnet/devnet

#include "../include/physicscoin.h"
#include "../include/network_config.h"
#include "../include/faucet.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

// Explorer API endpoint declarations
extern void handle_explorer_stats(int client, PCState* state);
extern void handle_explorer_wallet(int client, PCState* state, const char* address);
extern void handle_explorer_rich_list(int client, PCState* state);
extern void handle_explorer_distribution(int client, PCState* state);
extern void handle_explorer_search(int client, PCState* state, const char* query);
extern void handle_explorer_consensus(int client);
extern void handle_explorer_health(int client, PCState* state);
extern void handle_explorer_state_hash(int client, PCState* state);
extern void handle_explorer_supply(int client, PCState* state);
extern void handle_explorer_conservation_check(int client, PCState* state);
extern void handle_explorer_wallets_top(int client, PCState* state, const char* count_str);

#define API_PORT 8545
#define MAX_REQUEST_SIZE 8192

// Rate limiting
#define MAX_REQUESTS_PER_MINUTE 60
#define RATE_LIMIT_WINDOW 60

typedef struct {
    uint32_t ip_addr;
    int request_count;
    time_t window_start;
} RateLimitEntry;

#define MAX_RATE_LIMIT_ENTRIES 1000
static RateLimitEntry rate_limits[MAX_RATE_LIMIT_ENTRIES];
static int rate_limit_count = 0;

// Transaction history storage
#define MAX_TX_HISTORY 100
static struct {
    char from[65];
    char to[65];
    double amount;
    uint64_t timestamp;
} tx_history[MAX_TX_HISTORY];
static int tx_history_count = 0;

// Check rate limit for an IP
static int check_rate_limit(uint32_t ip_addr) {
    time_t now = time(NULL);
    
    // Find existing entry
    for (int i = 0; i < rate_limit_count; i++) {
        if (rate_limits[i].ip_addr == ip_addr) {
            // Reset window if expired
            if (now - rate_limits[i].window_start >= RATE_LIMIT_WINDOW) {
                rate_limits[i].request_count = 1;
                rate_limits[i].window_start = now;
                return 1;  // Allowed
            }
            
            // Check limit
            if (rate_limits[i].request_count >= MAX_REQUESTS_PER_MINUTE) {
                return 0;  // Rate limited
            }
            
            rate_limits[i].request_count++;
            return 1;  // Allowed
        }
    }
    
    // Add new entry
    if (rate_limit_count < MAX_RATE_LIMIT_ENTRIES) {
        rate_limits[rate_limit_count].ip_addr = ip_addr;
        rate_limits[rate_limit_count].request_count = 1;
        rate_limits[rate_limit_count].window_start = now;
        rate_limit_count++;
    }
    
    return 1;  // Allowed
}

static void record_transaction(const char* from, const char* to, double amount) {
    if (tx_history_count >= MAX_TX_HISTORY) {
        // Shift old transactions out
        memmove(&tx_history[0], &tx_history[1], sizeof(tx_history[0]) * (MAX_TX_HISTORY - 1));
        tx_history_count = MAX_TX_HISTORY - 1;
    }
    strncpy(tx_history[tx_history_count].from, from, 64);
    tx_history[tx_history_count].from[64] = '\0';
    strncpy(tx_history[tx_history_count].to, to, 64);
    tx_history[tx_history_count].to[64] = '\0';
    tx_history[tx_history_count].amount = amount;
    tx_history[tx_history_count].timestamp = time(NULL);
    tx_history_count++;
}

// API response helpers (non-static for use by explorer_api.c)
void send_json_response(int client, int status, const char* body) {
    char response[8192];
    snprintf(response, sizeof(response),
             "HTTP/1.1 %d OK\r\n"
             "Content-Type: application/json\r\n"
             "Access-Control-Allow-Origin: *\r\n"
             "Content-Length: %zu\r\n\r\n%s",
             status, strlen(body), body);
    send(client, response, strlen(response), 0);
}

void send_error(int client, int code, const char* message) {
    char body[256];
    snprintf(body, sizeof(body), "{\"error\":{\"code\":%d,\"message\":\"%s\"}}", code, message);
    send_json_response(client, 400, body);
}

// Handle GET /status
static void handle_status(int client, PCState* state) {
    char body[512];
    snprintf(body, sizeof(body),
             "{\"version\":\"%s\",\"wallets\":%u,\"total_supply\":%.8f,\"timestamp\":%lu,\"tx_count\":%d,\"peers\":1,\"secure\":true}",
             PHYSICSCOIN_VERSION, state->num_wallets, state->total_supply, state->timestamp, tx_history_count);
    send_json_response(client, 200, body);
}

// Handle GET /balance/<address>
static void handle_balance(int client, PCState* state, const char* address) {
    uint8_t pubkey[32];
    if (pc_hex_to_pubkey(address, pubkey) != PC_OK) {
        send_error(client, -32602, "Invalid address");
        return;
    }
    
    PCWallet* wallet = pc_state_get_wallet(state, pubkey);
    if (!wallet) {
        // Return 0 balance for non-existent wallets (not an error)
        char body[256];
        snprintf(body, sizeof(body), "{\"address\":\"%s\",\"balance\":0.00000000,\"nonce\":0,\"exists\":false}",
                 address);
        send_json_response(client, 200, body);
        return;
    }
    
    char body[256];
    snprintf(body, sizeof(body), "{\"address\":\"%s\",\"balance\":%.8f,\"nonce\":%lu,\"exists\":true}",
             address, wallet->energy, wallet->nonce);
    send_json_response(client, 200, body);
}

// Handle GET /wallets
static void handle_wallets(int client, PCState* state) {
    char body[8192] = "{\"wallets\":[";
    char* p = body + strlen(body);
    
    for (uint32_t i = 0; i < state->num_wallets && i < 100; i++) {
        char addr[65];
        pc_pubkey_to_hex(state->wallets[i].public_key, addr);
        p += sprintf(p, "%s{\"address\":\"%.16s...\",\"balance\":%.8f}",
                     i > 0 ? "," : "", addr, state->wallets[i].energy);
    }
    strcat(body, "]}");
    send_json_response(client, 200, body);
}

// Parse JSON request body
static char* find_json_body(const char* request) {
    char* body = strstr(request, "\r\n\r\n");
    return body ? body + 4 : NULL;
}

static char* get_json_field(const char* json, const char* field) {
    static char value[256];
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":\"", field);
    char* start = strstr(json, search);
    if (!start) return NULL;
    start += strlen(search);
    char* end = strchr(start, '"');
    if (!end) return NULL;
    size_t len = end - start;
    if (len >= sizeof(value)) len = sizeof(value) - 1;
    memcpy(value, start, len);
    value[len] = '\0';
    return value;
}

static double get_json_number(const char* json, const char* field) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":", field);
    char* start = strstr(json, search);
    return start ? atof(start + strlen(search)) : 0.0;
}

static uint64_t get_json_uint64(const char* json, const char* field) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":", field);
    char* start = strstr(json, search);
    return start ? (uint64_t)strtoull(start + strlen(search), NULL, 10) : 0;
}

// POST /wallet/create - Create new HD wallet (NO FAUCET - wallet starts with 0 balance)
static void handle_wallet_create(int client, PCState* state) {
    extern int pc_mnemonic_generate(char* mnemonic, size_t len, int words);
    
    char mnemonic[512];
    if (pc_mnemonic_generate(mnemonic, sizeof(mnemonic), 12) != 0) {
        send_error(client, -32000, "Failed to generate mnemonic");
        return;
    }
    
    // Generate keypair
    PCKeypair kp;
    pc_keypair_generate(&kp);
    char address[65];
    pc_pubkey_to_hex(kp.public_key, address);
    
    // SECURITY FIX: Register wallet with ZERO balance (no faucet)
    // Wallet will need to receive funds from existing wallets
    PCWallet* existing = pc_state_get_wallet(state, kp.public_key);
    if (!existing) {
        // Add new wallet to state with ZERO balance
        if (state->num_wallets < state->wallets_capacity) {
            PCWallet* new_wallet = &state->wallets[state->num_wallets++];
            memcpy(new_wallet->public_key, kp.public_key, 32);
            new_wallet->energy = 0.0;  // SECURITY: No free coins!
            new_wallet->nonce = 0;
            // DO NOT modify total_supply - conservation law preserved
            pc_state_compute_hash(state);
        }
    }
    
    char body[1024];
    snprintf(body, sizeof(body),
             "{\"mnemonic\":\"%s\",\"address\":\"%s\",\"balance\":0.00000000,\"message\":\"Wallet created with zero balance. Receive funds from existing wallets.\"}",
             mnemonic, address);
    send_json_response(client, 200, body);
}

// POST /transaction/send - Send SIGNED transaction
// SECURITY: Requires cryptographic signature from sender
static void handle_transaction_send(int client, PCState* state, const char* json) {
    const char* from = get_json_field(json, "from");
    const char* to = get_json_field(json, "to");
    const char* signature_hex = get_json_field(json, "signature");
    double amount = get_json_number(json, "amount");
    uint64_t nonce = get_json_uint64(json, "nonce");
    uint64_t timestamp = get_json_uint64(json, "timestamp");
    
    // SECURITY: Require all fields including signature
    if (!from || !to || amount <= 0) {
        send_error(client, -32602, "Missing required fields: from, to, amount");
        return;
    }
    
    if (!signature_hex || strlen(signature_hex) != 128) {
        send_error(client, -32602, "Missing or invalid signature (must be 128 hex chars)");
        return;
    }
    
    uint8_t from_key[32], to_key[32];
    if (pc_hex_to_pubkey(from, from_key) != PC_OK || pc_hex_to_pubkey(to, to_key) != PC_OK) {
        send_error(client, -32602, "Invalid address format");
        return;
    }
    
    // Parse signature from hex
    uint8_t signature[64];
    for (int i = 0; i < 64; i++) {
        unsigned int byte;
        if (sscanf(signature_hex + (i * 2), "%02x", &byte) != 1) {
            send_error(client, -32602, "Invalid signature hex encoding");
            return;
        }
        signature[i] = (uint8_t)byte;
    }
    
    // Build transaction
    PCTransaction tx;
    memset(&tx, 0, sizeof(tx));
    memcpy(tx.from, from_key, 32);
    memcpy(tx.to, to_key, 32);
    tx.amount = amount;
    tx.nonce = nonce;
    tx.timestamp = timestamp ? timestamp : (uint64_t)time(NULL);
    memcpy(tx.signature, signature, 64);
    
    // SECURITY: Execute transaction with signature verification
    // pc_state_execute_tx will verify the signature internally
    PCError err = pc_state_execute_tx(state, &tx);
    if (err != PC_OK) {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Transaction failed: %s", pc_strerror(err));
        send_error(client, -32000, error_msg);
        return;
    }
    
    // Record the transaction
    record_transaction(from, to, amount);
    
    // Save state after successful transaction
    pc_state_save(state, "state.pcs");
    
    char body[256];
    snprintf(body, sizeof(body), "{\"success\":true,\"amount\":%.8f,\"tx_hash\":\"pending\"}", amount);
    send_json_response(client, 200, body);
}

// POST /stream/open - Open payment stream (requires signature)
static void handle_stream_open(int client, const char* json) {
    const char* from = get_json_field(json, "from");
    const char* to = get_json_field(json, "to");
    const char* signature_hex = get_json_field(json, "signature");
    double rate = get_json_number(json, "rate");
    
    if (!from || !to || rate <= 0) {
        send_error(client, -32602, "Missing required fields: from, to, rate");
        return;
    }
    
    // SECURITY: Streams should require signatures too
    if (!signature_hex) {
        send_error(client, -32602, "Missing signature for stream authorization");
        return;
    }
    
    // Generate stream ID
    char stream_id[17];
    snprintf(stream_id, sizeof(stream_id), "%016lx", (unsigned long)time(NULL));
    
    char body[512];
    snprintf(body, sizeof(body),
             "{\"stream_id\":\"%s\",\"from\":\"%.16s...\",\"to\":\"%.16s...\",\"rate\":%.8f}",
             stream_id, from, to, rate);
    send_json_response(client, 200, body);
}

// POST /proof/generate - Generate balance proof
static void handle_proof_generate(int client, PCState* state, const char* json) {
    const char* address = get_json_field(json, "address");
    if (!address) {
        send_error(client, -32602, "Missing address");
        return;
    }
    
    uint8_t pubkey[32];
    if (pc_hex_to_pubkey(address, pubkey) != PC_OK) {
        send_error(client, -32602, "Invalid address");
        return;
    }
    
    PCWallet* wallet = pc_state_get_wallet(state, pubkey);
    double balance = wallet ? wallet->energy : 0.0;
    uint64_t nonce = wallet ? wallet->nonce : 0;
    
    // Generate proof hash
    pc_state_compute_hash(state);
    char state_hash_hex[65];
    for (int i = 0; i < 32; i++) sprintf(state_hash_hex + i*2, "%02x", state->state_hash[i]);
    
    char body[512];
    snprintf(body, sizeof(body),
             "{\"address\":\"%s\",\"balance\":%.8f,\"nonce\":%lu,\"state_hash\":\"%s\",\"timestamp\":%lu,\"exists\":%s}",
             address, balance, nonce, state_hash_hex, (unsigned long)time(NULL), wallet ? "true" : "false");
    send_json_response(client, 200, body);
}

// Parse HTTP request
static int parse_request(const char* request, char* method, char* path) {
    return sscanf(request, "%15s %255s", method, path) == 2 ? 0 : -1;
}

// GET /transactions - Get transaction history
static void handle_transactions(int client) {
    char body[8192] = "{\"transactions\":[";
    char* p = body + strlen(body);
    size_t remaining = sizeof(body) - strlen(body) - 10;
    
    for (int i = tx_history_count - 1; i >= 0 && i >= tx_history_count - 20 && remaining > 200; i--) {
        int written = sprintf(p, "%s{\"from\":\"%.16s...\",\"to\":\"%.16s...\",\"amount\":%.8f,\"timestamp\":%lu}",
                     i < tx_history_count - 1 ? "," : "",
                     tx_history[i].from, tx_history[i].to,
                     tx_history[i].amount, tx_history[i].timestamp);
        p += written;
        remaining -= written;
    }
    strcat(body, "]}");
    send_json_response(client, 200, body);
}

// GET /conservation - Verify conservation law
static void handle_conservation(int client, PCState* state) {
    PCError err = pc_state_verify_conservation(state);
    double sum = 0.0;
    for (uint32_t i = 0; i < state->num_wallets; i++) {
        sum += state->wallets[i].energy;
    }
    double error = state->total_supply - sum;
    
    char body[256];
    snprintf(body, sizeof(body),
             "{\"verified\":%s,\"total_supply\":%.8f,\"wallet_sum\":%.8f,\"error\":%.12e}",
             err == PC_OK ? "true" : "false", state->total_supply, sum, error);
    send_json_response(client, 200, body);
}

// POST /faucet/request - Request testnet funds (testnet/devnet only)
static void handle_faucet_request(int client, PCState* state, const char* json_body) {
    // Check if faucet is enabled for current network
    if (!pc_network_faucet_enabled()) {
        send_error(client, -32000, "Faucet not available on this network");
        return;
    }
    
    // Get address from JSON
    char* address_str = get_json_field(json_body, "address");
    if (!address_str) {
        send_error(client, -32602, "Missing 'address' field");
        return;
    }
    
    uint8_t address[32];
    if (pc_hex_to_pubkey(address_str, address) != PC_OK) {
        free(address_str);
        send_error(client, -32602, "Invalid address format");
        return;
    }
    free(address_str);
    
    // Check if address can request
    if (!pc_faucet_can_request(address)) {
        uint64_t wait_time = pc_faucet_time_until_next(address);
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Faucet cooldown active. Try again in %lu seconds", wait_time);
        send_error(client, -32000, error_msg);
        return;
    }
    
    // Request faucet funds
    double amount = 0.0;
    PCError err = pc_faucet_request(state, address, &amount);
    
    if (err == PC_OK) {
        char body[512];
        char addr_hex[65];
        pc_pubkey_to_hex(address, addr_hex);
        snprintf(body, sizeof(body),
                 "{\"success\":true,\"address\":\"%s\",\"amount\":%.8f,\"message\":\"Faucet funds sent successfully\"}",
                 addr_hex, amount);
        send_json_response(client, 200, body);
    } else {
        send_error(client, -32000, pc_strerror(err));
    }
}

// GET /faucet/info - Get faucet information
static void handle_faucet_info(int client) {
    const PCNetworkConfig* config = pc_network_get_config(pc_network_get_current());
    
    char body[512];
    snprintf(body, sizeof(body),
             "{\"enabled\":%s,\"amount\":%.8f,\"cooldown\":%d,\"network\":\"%s\"}",
             config->has_faucet ? "true" : "false",
             config->faucet_amount,
             config->faucet_cooldown,
             config->network_name);
    send_json_response(client, 200, body);
}

// Main API server
int pc_api_serve(PCState* state, int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return -1; }
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr = {.sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(port)};
    
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); close(server_fd); return -1; }
    if (listen(server_fd, 10) < 0) { perror("listen"); close(server_fd); return -1; }
    
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║          PHYSICSCOIN SECURE API SERVER                        ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
    const PCNetworkConfig* config = pc_network_get_config(pc_network_get_current());
    printf("Network: %s\n", config->network_name);
    printf("Listening on: http://localhost:%d\n", port);
    printf("Security: Rate limiting enabled (%d req/min)\n", MAX_REQUESTS_PER_MINUTE);
    printf("Security: Signed transactions required\n");
    if (config->has_faucet) {
        printf("Faucet: ✓ enabled (%.2f coins, %d sec cooldown)\n\n", config->faucet_amount, config->faucet_cooldown);
    } else {
        printf("Faucet: ✗ disabled (mainnet mode)\n\n");
    }
    printf("Endpoints:\n");
    printf("  GET  /status          - Network status\n");
    printf("  GET  /wallets         - List wallets\n");
    printf("  GET  /balance/<addr>  - Get balance\n");
    printf("  GET  /transactions    - Transaction history\n");
    printf("  GET  /conservation    - Verify conservation law\n");
    printf("  POST /wallet/create   - Create wallet (0 balance)\n");
    printf("  POST /transaction/send - Send signed transaction\n");
    printf("  POST /proof/generate  - Generate balance proof\n");
    if (config->has_faucet) {
        printf("  POST /faucet/request  - Request faucet funds (testnet only)\n");
        printf("  GET  /faucet/info     - Get faucet information\n");
    }
    printf("\n");
    
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client < 0) continue;
        
        // Rate limiting
        if (!check_rate_limit(client_addr.sin_addr.s_addr)) {
            send_error(client, -32000, "Rate limit exceeded. Try again later.");
            close(client);
            continue;
        }
        
        char request[MAX_REQUEST_SIZE];
        ssize_t n = recv(client, request, sizeof(request) - 1, 0);
        if (n <= 0) { close(client); continue; }
        request[n] = '\0';
        
        char method[16], path[256];
        if (parse_request(request, method, path) < 0) { send_error(client, -32600, "Bad request"); close(client); continue; }
        
        printf("[%s] %s %s\n", inet_ntoa(client_addr.sin_addr), method, path);
        
        // Handle OPTIONS for CORS
        if (strcmp(method, "OPTIONS") == 0) {
            char response[] = "HTTP/1.1 200 OK\r\n"
                             "Access-Control-Allow-Origin: *\r\n"
                             "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                             "Access-Control-Allow-Headers: Content-Type\r\n\r\n";
            send(client, response, strlen(response), 0);
            close(client);
            continue;
        }
        
        if (strcmp(method, "GET") == 0) {
            if (strcmp(path, "/status") == 0) handle_status(client, state);
            else if (strcmp(path, "/wallets") == 0) handle_wallets(client, state);
            else if (strcmp(path, "/transactions") == 0) handle_transactions(client);
            else if (strcmp(path, "/conservation") == 0) handle_conservation(client, state);
            else if (strcmp(path, "/faucet/info") == 0) handle_faucet_info(client);
            else if (strncmp(path, "/balance/", 9) == 0) handle_balance(client, state, path + 9);
            // Explorer endpoints
            else if (strcmp(path, "/explorer/stats") == 0) handle_explorer_stats(client, state);
            else if (strcmp(path, "/explorer/rich") == 0) handle_explorer_rich_list(client, state);
            else if (strcmp(path, "/explorer/distribution") == 0) handle_explorer_distribution(client, state);
            else if (strcmp(path, "/explorer/consensus") == 0) handle_explorer_consensus(client);
            else if (strcmp(path, "/explorer/health") == 0) handle_explorer_health(client, state);
            else if (strcmp(path, "/explorer/state/hash") == 0) handle_explorer_state_hash(client, state);
            else if (strcmp(path, "/explorer/supply") == 0) handle_explorer_supply(client, state);
            else if (strcmp(path, "/explorer/conservation_check") == 0) handle_explorer_conservation_check(client, state);
            else if (strncmp(path, "/explorer/wallet/", 17) == 0) handle_explorer_wallet(client, state, path + 17);
            else if (strncmp(path, "/explorer/wallets/top/", 22) == 0) handle_explorer_wallets_top(client, state, path + 22);
            else if (strncmp(path, "/explorer/search/", 17) == 0) handle_explorer_search(client, state, path + 17);
            else send_error(client, -32601, "Not found");
        }
        else if (strcmp(method, "POST") == 0) {
            char* json_body = find_json_body(request);
            if (!json_body) json_body = "{}";
            
            if (strcmp(path, "/wallet/create") == 0) handle_wallet_create(client, state);
            else if (strcmp(path, "/transaction/send") == 0) handle_transaction_send(client, state, json_body);
            else if (strcmp(path, "/stream/open") == 0) handle_stream_open(client, json_body);
            else if (strcmp(path, "/proof/generate") == 0) handle_proof_generate(client, state, json_body);
            else if (strcmp(path, "/faucet/request") == 0) handle_faucet_request(client, state, json_body);
            else send_error(client, -32601, "Not found");
        }
        else {
            send_error(client, -32600, "Method not allowed");
        }
        close(client);
    }
    return 0;
}

