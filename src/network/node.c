// node.c - P2P Node Daemon
// Full node implementation with peer discovery and state sync

#include "../include/physicscoin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>

#define DEFAULT_PORT 9333
#define MAX_PEERS 32
#define BUFFER_SIZE 65536
#define HEARTBEAT_INTERVAL 30
#define SYNC_INTERVAL 10

// Security limits
#define MAX_MSG_PER_MINUTE 100    // Max messages per minute per peer
#define MAX_TX_PER_MINUTE 50      // Max TXs per minute per peer
#define MAX_VIOLATIONS 5          // Ban after this many violations
#define BAN_DURATION 3600         // 1 hour ban

// Message types
#define MSG_VERSION     0x01
#define MSG_VERACK      0x02
#define MSG_GETSTATE    0x03
#define MSG_STATE       0x04
#define MSG_TX          0x05
#define MSG_DELTA       0x06
#define MSG_PING        0x07
#define MSG_PONG        0x08
#define MSG_PEERS       0x09
#define MSG_GETPEERS    0x0A

// Message header
typedef struct __attribute__((packed)) {
    uint32_t magic;         // 0x50435343 "PCSC"
    uint8_t type;
    uint32_t length;
    uint8_t checksum[4];
} PCMessageHeader;

// Peer info
typedef struct {
    int fd;
    char ip[16];
    uint16_t port;
    int connected;
    int handshaked;
    uint64_t last_seen;
    uint64_t version;
    // Security fields
    uint32_t msg_count;         // Messages this minute
    uint32_t tx_count;          // TXs this minute
    time_t rate_reset;          // When to reset counters
    int banned;                 // Banned flag
    time_t ban_until;           // Ban expiry (0 = permanent)
    uint32_t violations;        // Protocol violations
} PCNodePeer;

// Node state
typedef struct {
    int listen_fd;
    uint16_t port;
    uint8_t node_id[32];
    PCNodePeer peers[MAX_PEERS];
    uint32_t num_peers;
    PCState state;
    PCKeypair wallet;
    volatile int running;
    pthread_mutex_t state_lock;
} PCNode;

static PCNode* g_node = NULL;

// Signal handler
void node_signal_handler(int sig) {
    (void)sig;
    if (g_node) {
        printf("\nShutting down node...\n");
        g_node->running = 0;
    }
}

// Calculate checksum
void calc_checksum(const uint8_t* data, size_t len, uint8_t* out) {
    uint32_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum = (sum + data[i]) * 31;
    }
    memcpy(out, &sum, 4);
}

// Send message
int node_send_message(PCNodePeer* peer, uint8_t type, const void* data, size_t len) {
    if (!peer->connected) return -1;
    
    PCMessageHeader header = {0};
    header.magic = 0x50435343;  // "PCSC"
    header.type = type;
    header.length = len;
    if (data && len > 0) {
        calc_checksum(data, len, header.checksum);
    }
    
    // Send header
    if (send(peer->fd, &header, sizeof(header), 0) != sizeof(header)) {
        return -1;
    }
    
    // Send payload
    if (data && len > 0) {
        if (send(peer->fd, data, len, MSG_NOSIGNAL) != (ssize_t)len) {
            return -1;
        }
    }
    
    return 0;
}

// Receive message
int node_recv_message(PCNodePeer* peer, PCMessageHeader* header, uint8_t* buffer, size_t buflen) {
    if (!peer->connected) return -1;
    
    // Receive header
    ssize_t n = recv(peer->fd, header, sizeof(*header), MSG_WAITALL);
    if (n != sizeof(*header)) {
        return -1;
    }
    
    // Verify magic
    if (header->magic != 0x50435343) {
        return -1;
    }
    
    // Receive payload
    if (header->length > 0) {
        if (header->length > buflen) return -1;
        n = recv(peer->fd, buffer, header->length, MSG_WAITALL);
        if (n != (ssize_t)header->length) {
            return -1;
        }
    }
    
    return 0;
}

// Handle version message
void handle_version(PCNode* node, PCNodePeer* peer, const uint8_t* data, size_t len) {
    if (len >= 8) {
        memcpy(&peer->version, data, 8);
        printf("[%s:%d] Version: %lu\n", peer->ip, peer->port, peer->version);
    }
    
    // Send verack
    node_send_message(peer, MSG_VERACK, NULL, 0);
    peer->handshaked = 1;
    peer->last_seen = time(NULL);
    
    // Request state if we're behind
    node_send_message(peer, MSG_GETSTATE, NULL, 0);
}

// Handle state request
void handle_getstate(PCNode* node, PCNodePeer* peer) {
    pthread_mutex_lock(&node->state_lock);
    
    uint8_t buffer[BUFFER_SIZE];
    size_t len = pc_state_serialize(&node->state, buffer, sizeof(buffer));
    
    pthread_mutex_unlock(&node->state_lock);
    
    if (len > 0) {
        node_send_message(peer, MSG_STATE, buffer, len);
        printf("[%s:%d] Sent state (%zu bytes)\n", peer->ip, peer->port, len);
    }
}

// Handle state message
void handle_state(PCNode* node, PCNodePeer* peer, const uint8_t* data, size_t len) {
    PCState new_state;
    if (pc_state_deserialize(&new_state, data, len) == PC_OK) {
        pthread_mutex_lock(&node->state_lock);
        
        // Accept if version is higher
        if (new_state.version > node->state.version) {
            printf("[%s:%d] Syncing state v%lu -> v%lu\n", 
                   peer->ip, peer->port,
                   node->state.version, new_state.version);
            pc_state_free(&node->state);
            node->state = new_state;
        } else {
            pc_state_free(&new_state);
        }
        
        pthread_mutex_unlock(&node->state_lock);
    }
}

// Handle transaction
void handle_tx(PCNode* node, PCNodePeer* peer, const uint8_t* data, size_t len) {
    if (len < sizeof(PCTransaction)) return;
    
    PCTransaction tx;
    memcpy(&tx, data, sizeof(PCTransaction));
    
    pthread_mutex_lock(&node->state_lock);
    PCError err = pc_state_execute_tx(&node->state, &tx);
    pthread_mutex_unlock(&node->state_lock);
    
    if (err == PC_OK) {
        printf("[%s:%d] TX accepted (%.2f coins)\n", peer->ip, peer->port, tx.amount);
        
        // Broadcast to other peers
        for (uint32_t i = 0; i < node->num_peers; i++) {
            if (&node->peers[i] != peer && node->peers[i].handshaked) {
                node_send_message(&node->peers[i], MSG_TX, &tx, sizeof(tx));
            }
        }
    }
}

// Handle ping
void handle_ping(PCNode* node, PCNodePeer* peer, const uint8_t* data, size_t len) {
    (void)node;
    node_send_message(peer, MSG_PONG, data, len);
    peer->last_seen = time(NULL);
}

// Ban a peer
void ban_peer(PCNodePeer* peer, int permanent) {
    peer->banned = 1;
    peer->ban_until = permanent ? 0 : time(NULL) + BAN_DURATION;
    printf("[%s:%d] BANNED for %s\n", peer->ip, peer->port, 
           permanent ? "permanently" : "1 hour");
    if (peer->connected) {
        close(peer->fd);
        peer->connected = 0;
    }
}

// Check rate limits
int check_rate_limit(PCNodePeer* peer) {
    time_t now = time(NULL);
    
    // Reset counters every minute
    if (now >= peer->rate_reset) {
        peer->msg_count = 0;
        peer->tx_count = 0;
        peer->rate_reset = now + 60;
    }
    
    peer->msg_count++;
    
    if (peer->msg_count > MAX_MSG_PER_MINUTE) {
        peer->violations++;
        printf("[%s:%d] Rate limit exceeded (%u msgs/min)\n", 
               peer->ip, peer->port, peer->msg_count);
        if (peer->violations >= MAX_VIOLATIONS) {
            ban_peer(peer, 0);
        }
        return -1;
    }
    
    return 0;
}

// Handle incoming message
void handle_message(PCNode* node, PCNodePeer* peer, PCMessageHeader* header, uint8_t* data) {
    // Skip if banned
    if (peer->banned) {
        if (peer->ban_until && time(NULL) >= peer->ban_until) {
            peer->banned = 0;  // Unban
            peer->violations = 0;
        } else {
            return;
        }
    }
    
    // Rate limiting
    if (check_rate_limit(peer) != 0) {
        return;
    }
    
    // TX-specific rate limit
    if (header->type == MSG_TX) {
        peer->tx_count++;
        if (peer->tx_count > MAX_TX_PER_MINUTE) {
            peer->violations++;
            printf("[%s:%d] TX rate limit (%u tx/min)\n", 
                   peer->ip, peer->port, peer->tx_count);
            return;
        }
    }
    
    switch (header->type) {
        case MSG_VERSION:
            handle_version(node, peer, data, header->length);
            break;
        case MSG_VERACK:
            peer->handshaked = 1;
            peer->last_seen = time(NULL);
            break;
        case MSG_GETSTATE:
            handle_getstate(node, peer);
            break;
        case MSG_STATE:
            handle_state(node, peer, data, header->length);
            break;
        case MSG_TX:
            handle_tx(node, peer, data, header->length);
            break;
        case MSG_PING:
            handle_ping(node, peer, data, header->length);
            break;
        case MSG_PONG:
            peer->last_seen = time(NULL);
            break;
        default:
            peer->violations++;
            printf("[%s:%d] Unknown message type: 0x%02x (violation %u)\n", 
                   peer->ip, peer->port, header->type, peer->violations);
            if (peer->violations >= MAX_VIOLATIONS) {
                ban_peer(peer, 1);  // Permanent ban for protocol violation
            }
    }
}

// Connect to peer
int node_connect_peer(PCNode* node, const char* ip, uint16_t port) {
    if (node->num_peers >= MAX_PEERS) return -1;
    
    PCNodePeer* peer = &node->peers[node->num_peers];
    memset(peer, 0, sizeof(PCNodePeer));
    
    peer->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (peer->fd < 0) return -1;
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    
    if (connect(peer->fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(peer->fd);
        return -1;
    }
    
    strncpy(peer->ip, ip, 15);
    peer->port = port;
    peer->connected = 1;
    peer->last_seen = time(NULL);
    
    node->num_peers++;
    printf("Connected to %s:%d\n", ip, port);
    
    // Send version
    uint64_t version = node->state.version;
    node_send_message(peer, MSG_VERSION, &version, sizeof(version));
    
    return 0;
}

// Main node loop
void node_run(PCNode* node) {
    struct pollfd fds[MAX_PEERS + 1];
    time_t last_heartbeat = 0;
    
    while (node->running) {
        int nfds = 0;
        
        // Listen socket
        fds[nfds].fd = node->listen_fd;
        fds[nfds].events = POLLIN;
        nfds++;
        
        // Peer sockets
        for (uint32_t i = 0; i < node->num_peers; i++) {
            if (node->peers[i].connected) {
                fds[nfds].fd = node->peers[i].fd;
                fds[nfds].events = POLLIN;
                nfds++;
            }
        }
        
        // Poll with 1 second timeout
        int ready = poll(fds, nfds, 1000);
        
        if (ready > 0) {
            // Check for new connections
            if (fds[0].revents & POLLIN) {
                struct sockaddr_in client_addr;
                socklen_t addr_len = sizeof(client_addr);
                int client_fd = accept(node->listen_fd, 
                                       (struct sockaddr*)&client_addr, 
                                       &addr_len);
                                       
                if (client_fd >= 0 && node->num_peers < MAX_PEERS) {
                    PCNodePeer* peer = &node->peers[node->num_peers];
                    memset(peer, 0, sizeof(PCNodePeer));
                    peer->fd = client_fd;
                    inet_ntop(AF_INET, &client_addr.sin_addr, peer->ip, 16);
                    peer->port = ntohs(client_addr.sin_port);
                    peer->connected = 1;
                    peer->last_seen = time(NULL);
                    node->num_peers++;
                    
                    printf("Accepted connection from %s:%d\n", peer->ip, peer->port);
                    
                    // Send version
                    uint64_t version = node->state.version;
                    node_send_message(peer, MSG_VERSION, &version, sizeof(version));
                }
            }
            
            // Check peer messages
            int fidx = 1;
            for (uint32_t i = 0; i < node->num_peers && fidx < nfds; i++) {
                if (node->peers[i].connected) {
                    if (fds[fidx].revents & POLLIN) {
                        PCMessageHeader header;
                        uint8_t buffer[BUFFER_SIZE];
                        
                        if (node_recv_message(&node->peers[i], &header, buffer, sizeof(buffer)) == 0) {
                            handle_message(node, &node->peers[i], &header, buffer);
                        } else {
                            // Disconnect
                            printf("Peer %s:%d disconnected\n", 
                                   node->peers[i].ip, node->peers[i].port);
                            close(node->peers[i].fd);
                            node->peers[i].connected = 0;
                        }
                    }
                    fidx++;
                }
            }
        }
        
        // Periodic heartbeat
        time_t now = time(NULL);
        if (now - last_heartbeat >= HEARTBEAT_INTERVAL) {
            last_heartbeat = now;
            
            uint64_t ts = now;
            for (uint32_t i = 0; i < node->num_peers; i++) {
                if (node->peers[i].connected && node->peers[i].handshaked) {
                    node_send_message(&node->peers[i], MSG_PING, &ts, sizeof(ts));
                }
            }
        }
    }
}

// Initialize node
PCError pc_node_init(PCNode* node, uint16_t port) {
    memset(node, 0, sizeof(PCNode));
    node->port = port;
    node->running = 1;
    pthread_mutex_init(&node->state_lock, NULL);
    
    // Generate node ID
    pc_keypair_generate(&node->wallet);
    memcpy(node->node_id, node->wallet.public_key, 32);
    
    // Load or create state
    if (pc_state_load(&node->state, "state.pcs") != PC_OK) {
        printf("Creating new genesis state...\n");
        pc_state_genesis(&node->state, node->wallet.public_key, 1000000.0);
        pc_state_save(&node->state, "state.pcs");
    }
    
    // Create listen socket
    node->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (node->listen_fd < 0) {
        perror("socket");
        return PC_ERR_IO;
    }
    
    int opt = 1;
    setsockopt(node->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(node->listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(node->listen_fd);
        return PC_ERR_IO;
    }
    
    if (listen(node->listen_fd, 10) < 0) {
        perror("listen");
        close(node->listen_fd);
        return PC_ERR_IO;
    }
    
    return PC_OK;
}

// Print node status
void pc_node_print_status(PCNode* node) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║                    PHYSICSCOIN NODE                           ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
    
    printf("Port:     %u\n", node->port);
    printf("Node ID:  ");
    for (int i = 0; i < 8; i++) printf("%02x", node->node_id[i]);
    printf("...\n");
    printf("Peers:    %u/%d\n", node->num_peers, MAX_PEERS);
    printf("State:    v%lu (%u wallets)\n", node->state.version, node->state.num_wallets);
    printf("Supply:   %.2f\n\n", node->state.total_supply);
    
    if (node->num_peers > 0) {
        printf("Connected Peers:\n");
        for (uint32_t i = 0; i < node->num_peers; i++) {
            PCNodePeer* p = &node->peers[i];
            printf("  [%u] %s:%d %s%s\n", i, p->ip, p->port,
                   p->connected ? "✓" : "✗",
                   p->handshaked ? " (ready)" : "");
        }
    }
    printf("\n");
}

// Node cleanup
void pc_node_free(PCNode* node) {
    close(node->listen_fd);
    for (uint32_t i = 0; i < node->num_peers; i++) {
        if (node->peers[i].connected) {
            close(node->peers[i].fd);
        }
    }
    pc_state_free(&node->state);
    pthread_mutex_destroy(&node->state_lock);
}

// Main entry point for node command
int pc_node_main(int argc, char** argv) {
    uint16_t port = DEFAULT_PORT;
    char* connect_ip = NULL;
    uint16_t connect_port = 0;
    
    // Parse arguments
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--connect") == 0 && i + 1 < argc) {
            char* arg = argv[++i];
            char* colon = strchr(arg, ':');
            if (colon) {
                *colon = '\0';
                connect_ip = arg;
                connect_port = atoi(colon + 1);
            }
        }
    }
    
    PCNode node;
    g_node = &node;
    
    // Setup signal handler
    signal(SIGINT, node_signal_handler);
    signal(SIGTERM, node_signal_handler);
    
    // Initialize
    printf("Starting PhysicsCoin node on port %u...\n", port);
    if (pc_node_init(&node, port) != PC_OK) {
        fprintf(stderr, "Failed to initialize node\n");
        return 1;
    }
    
    // Connect to seed if specified
    if (connect_ip && connect_port) {
        printf("Connecting to seed node %s:%d...\n", connect_ip, connect_port);
        if (node_connect_peer(&node, connect_ip, connect_port) != 0) {
            fprintf(stderr, "Warning: Failed to connect to seed node\n");
        }
    }
    
    pc_node_print_status(&node);
    printf("Node running. Press Ctrl+C to stop.\n\n");
    
    // Run
    node_run(&node);
    
    // Save state
    printf("Saving state...\n");
    pc_state_save(&node.state, "state.pcs");
    
    // Cleanup
    pc_node_free(&node);
    printf("Node stopped.\n");
    
    return 0;
}
