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
#define MAX_REQUEST_SIZE 8192

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
             "{\"version\":\"%s\",\"wallets\":%u,\"total_supply\":%.8f,\"timestamp\":%lu}",
             PHYSICSCOIN_VERSION, state->num_wallets, state->total_supply, state->timestamp);
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

// Parse HTTP request
static int parse_request(const char* request, char* method, char* path) {
    return sscanf(request, "%15s %255s", method, path) == 2 ? 0 : -1;
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
    printf("  GET /status, GET /wallets, GET /balance/<addr>\n\n");
    
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
        
        if (strcmp(method, "GET") == 0) {
            if (strcmp(path, "/status") == 0) handle_status(client, state);
            else if (strcmp(path, "/wallets") == 0) handle_wallets(client, state);
            else if (strncmp(path, "/balance/", 9) == 0) handle_balance(client, state, path + 9);
            else send_error(client, -32601, "Not found");
        } else {
            send_error(client, -32600, "Method not allowed");
        }
        close(client);
    }
    return 0;
}
