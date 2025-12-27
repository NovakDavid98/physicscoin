// api.c - Simple JSON-RPC API Server
// Provides HTTP API for external wallet/app integration

#include "../include/physicscoin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#define API_PORT 8545
#define API_PORT 8545
#define MAX_REQUEST_SIZE 8192

// Transaction history storage
#define MAX_TX_HISTORY 100
static struct {
    char from[65];
    char to[65];
    double amount;
    uint64_t timestamp;
} tx_history[MAX_TX_HISTORY];
static int tx_history_count = 0;

static void record_transaction(const char* from, const char* to, double amount) {
    if (tx_history_count >= MAX_TX_HISTORY) {
        // Shift old transactions out
        memmove(&tx_history[0], &tx_history[1], sizeof(tx_history[0]) * (MAX_TX_HISTORY - 1));
        tx_history_count = MAX_TX_HISTORY - 1;
    }
    strncpy(tx_history[tx_history_count].from, from, 64);
    strncpy(tx_history[tx_history_count].to, to, 64);
    tx_history[tx_history_count].amount = amount;
    tx_history[tx_history_count].timestamp = time(NULL);
    tx_history_count++;
}

// API response helpers
static void send_json_response(int client, int status, const char* body) {
    char response[4096];
    snprintf(response, sizeof(response),
             "HTTP/1.1 %d OK\r\n"
             "Content-Type: application/json\r\n"
             "Access-Control-Allow-Origin: *\r\n"
             "Content-Length: %zu\r\n\r\n%s",
             status, strlen(body), body);
    send(client, response, strlen(response), 0);
}

static void send_error(int client, int code, const char* message) {
    char body[256];
    snprintf(body, sizeof(body), "{\"error\":{\"code\":%d,\"message\":\"%s\"}}", code, message);
    send_json_response(client, 400, body);
}

// Handle GET /status
static void handle_status(int client, PCState* state) {
    char body[512];
    snprintf(body, sizeof(body),
             "{\"version\":\"%s\",\"wallets\":%u,\"total_supply\":%.8f,\"timestamp\":%lu,\"tx_count\":%d,\"peers\":1}",
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
        send_error(client, -32602, "Wallet not found");
        return;
    }
    
    char body[256];
    snprintf(body, sizeof(body), "{\"address\":\"%s\",\"balance\":%.8f,\"nonce\":%lu}",
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

// POST /wallet/create - Create new HD wallet and register in state
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
    
    // Register wallet in state with faucet funding (1000 coins)
    double faucet_amount = 1000.0;
    PCWallet* existing = pc_state_get_wallet(state, kp.public_key);
    if (!existing) {
        // Add new wallet to state
        if (state->num_wallets < 1000) {
            PCWallet* new_wallet = &state->wallets[state->num_wallets++];
            memcpy(new_wallet->public_key, kp.public_key, 32);
            new_wallet->energy = faucet_amount;
            new_wallet->nonce = 0;
            state->total_supply += faucet_amount;
            
            // Record faucet TX
            record_transaction("FAUCET", address, faucet_amount);
        }
    }
    
    char body[1024];
    snprintf(body, sizeof(body),
             "{\"mnemonic\":\"%s\",\"address\":\"%s\",\"balance\":%.8f}",
             mnemonic, address, faucet_amount);
    send_json_response(client, 200, body);
}

// POST /transaction/send - Send transaction
static void handle_transaction_send(int client, PCState* state, const char* json) {
    const char* from = get_json_field(json, "from");
    const char* to = get_json_field(json, "to");
    double amount = get_json_number(json, "amount");
    
    if (!from || !to || amount <= 0) {
        send_error(client, -32602, "Invalid parameters");
        return;
    }
    
    uint8_t from_key[32], to_key[32];
    if (pc_hex_to_pubkey(from, from_key) != PC_OK || pc_hex_to_pubkey(to, to_key) != PC_OK) {
        send_error(client, -32602, "Invalid address");
        return;
    }
    
    PCTransaction tx;
    memcpy(tx.from, from_key, 32);
    memcpy(tx.to, to_key, 32);
    tx.amount = amount;
    tx.timestamp = time(NULL);
    
    // Get nonce
    PCWallet* wallet = pc_state_get_wallet(state, from_key);
    if (!wallet) {
        send_error(client, -32602, "Sender wallet not found");
        return;
    }
    tx.nonce = wallet->nonce;
    
    PCError err = pc_state_execute_tx(state, &tx);
    if (err != PC_OK) {
        send_error(client, -32000, "Transaction failed");
        return;
    }
    
    // Record the transaction
    record_transaction(from, to, amount);
    
    char body[256];
    snprintf(body, sizeof(body), "{\"success\":true,\"amount\":%.8f}", amount);
    send_json_response(client, 200, body);
}

// POST /stream/open - Open payment stream
static void handle_stream_open(int client, const char* json) {
    const char* from = get_json_field(json, "from");
    const char* to = get_json_field(json, "to");
    double rate = get_json_number(json, "rate");
    
    if (!from || !to || rate <= 0) {
        send_error(client, -32602, "Invalid parameters");
        return;
    }
    
    // Generate stream ID (simplified)
    char stream_id[17];
    snprintf(stream_id, sizeof(stream_id), "%016lx", (unsigned long)time(NULL));
    
    char body[256];
    snprintf(body, sizeof(body),
             "{\"stream_id\":\"%s\",\"from\":\"%s\",\"to\":\"%s\",\"rate\":%.8f}",
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
    if (!wallet) {
        send_error(client, -32602, "Wallet not found");
        return;
    }
    
    // Generate proof hash
    pc_state_compute_hash(state);
    char state_hash_hex[65];
    for (int i = 0; i < 32; i++) sprintf(state_hash_hex + i*2, "%02x", state->state_hash[i]);
    
    char body[512];
    snprintf(body, sizeof(body),
             "{\"address\":\"%s\",\"balance\":%.8f,\"state_hash\":\"%s\",\"timestamp\":%lu}",
             address, wallet->energy, state_hash_hex, time(NULL));
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
    
    for (int i = tx_history_count - 1; i >= 0 && i >= tx_history_count - 20; i--) {
        p += sprintf(p, "%s{\"from\":\"%s\",\"to\":\"%s\",\"amount\":%.8f,\"timestamp\":%lu}",
                     i < tx_history_count - 1 ? "," : "",
                     tx_history[i].from, tx_history[i].to,
                     tx_history[i].amount, tx_history[i].timestamp);
    }
    strcat(body, "]}");
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
    
    printf("API Server: http://localhost:%d\n", port);
    printf("  GET  /status, /wallets, /balance/<addr>\n");
    printf("  POST /wallet/create, /transaction/send, /stream/open, /proof/generate\n\n");
    
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client < 0) continue;
        
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
            else if (strncmp(path, "/balance/", 9) == 0) handle_balance(client, state, path + 9);
            else send_error(client, -32601, "Not found");
        }
        else if (strcmp(method, "POST") == 0) {
            char* json_body = find_json_body(request);
            if (!json_body) json_body = "{}";
            
            if (strcmp(path, "/wallet/create") == 0) handle_wallet_create(client, state);
            else if (strcmp(path, "/transaction/send") == 0) handle_transaction_send(client, state, json_body);
            else if (strcmp(path, "/stream/open") == 0) handle_stream_open(client, json_body);
            else if (strcmp(path, "/proof/generate") == 0) handle_proof_generate(client, state, json_body);
            else send_error(client, -32601, "Not found");
        }
        else {
            send_error(client, -32600, "Method not allowed");
        }
        close(client);
    }
    return 0;
}
