// network_demo.c - TCP Socket Layer Demo
// Demonstrates real P2P networking with multiple nodes

#include "../include/physicscoin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Forward declarations from sockets.c
typedef struct {
    int fd;
    struct sockaddr_in addr;
    int connected;
    uint8_t peer_id[32];
} PCSocket;

typedef struct {
    PCSocket socket;
    uint64_t last_seen;
    uint32_t messages_sent;
    uint32_t messages_received;
    int banned;
} PCPeer;

typedef struct {
    PCSocket listen_socket;
    PCPeer peers[125];
    uint32_t num_peers;
    uint16_t port;
    uint8_t node_id[32];
} PCNetwork;

PCError pc_network_init(PCNetwork* network, uint16_t port, const uint8_t* node_id);
PCError pc_network_add_peer(PCNetwork* network, const char* ip, uint16_t port);
PCError pc_network_poll(PCNetwork* network, int timeout_ms);
PCError pc_network_broadcast(PCNetwork* network, const void* data, size_t len);
void pc_network_print_stats(const PCNetwork* network);
void pc_network_free(PCNetwork* network);

static volatile int running = 1;

void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <port> [peer_ip:peer_port] ...\n", argv[0]);
        printf("Example: %s 9000\n", argv[0]);
        printf("Example: %s 9001 127.0.0.1:9000\n", argv[0]);
        return 1;
    }
    
    uint16_t port = atoi(argv[1]);
    
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║              TCP SOCKET NETWORKING DEMO                       ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
    
    // Generate node ID
    uint8_t node_id[32];
    for (int i = 0; i < 32; i++) {
        node_id[i] = rand() & 0xFF;
    }
    
    // Initialize network
    PCNetwork network;
    PCError err = pc_network_init(&network, port, node_id);
    if (err != PC_OK) {
        fprintf(stderr, "Failed to initialize network: %s\n", pc_strerror(err));
        return 1;
    }
    
    printf("Node started on port %u\n", port);
    printf("Node ID: ");
    for (int i = 0; i < 8; i++) printf("%02x", node_id[i]);
    printf("...\n\n");
    
    // Connect to peers from command line
    for (int i = 2; i < argc; i++) {
        char ip[64];
        uint16_t peer_port;
        
        if (sscanf(argv[i], "%63[^:]:%hu", ip, &peer_port) == 2) {
            printf("Connecting to peer %s:%u...\n", ip, peer_port);
            err = pc_network_add_peer(&network, ip, peer_port);
            if (err == PC_OK) {
                printf("✓ Connected\n");
            } else {
                printf("✗ Failed: %s\n", pc_strerror(err));
            }
        }
    }
    
    printf("\n");
    pc_network_print_stats(&network);
    
    // Set up signal handler
    signal(SIGINT, signal_handler);
    
    printf("\nNetwork running. Press Ctrl+C to stop.\n");
    printf("Listening for connections and messages...\n\n");
    
    // Main loop
    int iteration = 0;
    while (running) {
        // Poll network
        pc_network_poll(&network, 1000);  // 1 second timeout
        
        // Every 5 seconds, broadcast a test message
        if (iteration % 5 == 0) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Hello from node %u at iteration %d", port, iteration);
            
            if (network.num_peers > 0) {
                printf("Broadcasting: %s\n", msg);
                pc_network_broadcast(&network, msg, strlen(msg) + 1);
            }
        }
        
        iteration++;
    }
    
    printf("\n\nShutting down...\n");
    pc_network_print_stats(&network);
    pc_network_free(&network);
    
    return 0;
}
