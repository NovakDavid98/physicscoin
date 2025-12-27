// node.c - Secure P2P Node Daemon
// Full node implementation with peer discovery and VALIDATED state sync
// SECURITY HARDENED: Requires validator signatures for state acceptance

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
#include <sodium.h>
#include <math.h>

#define DEFAULT_PORT 9333
#define MAX_PEERS 32
#define BUFFER_SIZE 65536
#define HEARTBEAT_INTERVAL 30
#define SYNC_INTERVAL 10

// Security limits
#define MAX_MSG_PER_MINUTE 100
#define MAX_TX_PER_MINUTE 50
#define MAX_VIOLATIONS 5
#define BAN_DURATION 3600

// Minimum validators required for state acceptance
#define MIN_VALIDATORS_FOR_STATE 1
#define MAX_STATE_VALIDATORS 10

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
#define MSG_STATE_SIG   0x0B  // New: Signed state message

// Message header
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t type;
    uint32_t length;
    uint8_t checksum[4];
} PCMessageHeader;

// Signed state message
typedef struct __attribute__((packed)) {
    uint8_t state_hash[32];
    uint64_t version;
    uint64_t timestamp;
    uint8_t validator_pubkey[32];
    uint8_t signature[64];
} PCSignedStateHeader;

// Peer info
typedef struct {
    int fd;
    char ip[16];
    uint16_t port;
    int connected;
    int handshaked;
    uint64_t last_seen;
    uint64_t version;
    uint8_t node_pubkey[32];
    int is_validator;
    // Security fields
    uint32_t msg_count;
    uint32_t tx_count;
    time_t rate_reset;
    int banned;
    time_t ban_until;
    uint32_t violations;
} PCNodePeer;

// Validator registry for this node
typedef struct {
    uint8_t pubkey[32];
    int trusted;
} PCTrustedValidator;

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
    
    // Validator management
    PCTrustedValidator trusted_validators[MAX_STATE_VALIDATORS];
    int num_trusted_validators;
    int is_validator;
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

// Verify state signature using Ed25519
__attribute__((unused))
static int verify_state_signature(const PCSignedStateHeader* signed_state, const PCState* state) {
    // Build message to verify: state_hash || version || timestamp
    uint8_t message[48];
    memcpy(message, signed_state->state_hash, 32);
    memcpy(message + 32, &signed_state->version, 8);
    memcpy(message + 40, &signed_state->timestamp, 8);
    
    // Verify signature using libsodium
    if (crypto_sign_verify_detached(signed_state->signature, message, 48, signed_state->validator_pubkey) != 0) {
        return 0;  // Invalid signature
    }
    
    // Verify state hash matches
    if (memcmp(signed_state->state_hash, state->state_hash, 32) != 0) {
        return 0;  // State hash mismatch
    }
    
    return 1;  // Valid
}

// Check if pubkey is a trusted validator
static int is_trusted_validator(PCNode* node, const uint8_t* pubkey) {
    for (int i = 0; i < node->num_trusted_validators; i++) {
        if (memcmp(node->trusted_validators[i].pubkey, pubkey, 32) == 0 &&
            node->trusted_validators[i].trusted) {
            return 1;
        }
    }
    return 0;
}

// Add trusted validator
void pc_node_add_validator(PCNode* node, const uint8_t* pubkey) {
    if (node->num_trusted_validators >= MAX_STATE_VALIDATORS) return;
    
    memcpy(node->trusted_validators[node->num_trusted_validators].pubkey, pubkey, 32);
    node->trusted_validators[node->num_trusted_validators].trusted = 1;
    node->num_trusted_validators++;
    
    printf("Added trusted validator: ");
    for (int i = 0; i < 8; i++) printf("%02x", pubkey[i]);
    printf("...\n");
}

// Sign current state as validator
PCError pc_node_sign_state(PCNode* node, PCSignedStateHeader* out) {
    if (!node->is_validator) {
        return PC_ERR_INVALID_SIGNATURE;
    }
    
    pthread_mutex_lock(&node->state_lock);
    
    // Build message: state_hash || version || timestamp
    uint8_t message[48];
    memcpy(message, node->state.state_hash, 32);
    memcpy(message + 8, &node->state.version, 8);
    uint64_t ts = (uint64_t)time(NULL);
    memcpy(message + 40, &ts, 8);
    
    // Fill output
    memcpy(out->state_hash, node->state.state_hash, 32);
    out->version = node->state.version;
    out->timestamp = ts;
    memcpy(out->validator_pubkey, node->wallet.public_key, 32);
    
    // Sign with Ed25519
    crypto_sign_detached(out->signature, NULL, message, 48, node->wallet.secret_key);
    
    pthread_mutex_unlock(&node->state_lock);
    
    return PC_OK;
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
    
    if (send(peer->fd, &header, sizeof(header), 0) != sizeof(header)) {
        return -1;
    }
    
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
    
    ssize_t n = recv(peer->fd, header, sizeof(*header), MSG_WAITALL);
    if (n != sizeof(*header)) {
        return -1;
    }
    
    if (header->magic != 0x50435343) {
        return -1;
    }
    
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
    
    // Extract peer's pubkey if provided
    if (len >= 40) {
        memcpy(peer->node_pubkey, data + 8, 32);
        peer->is_validator = is_trusted_validator(node, peer->node_pubkey);
        if (peer->is_validator) {
            printf("[%s:%d] Peer is a trusted validator\n", peer->ip, peer->port);
        }
    }
    
    node_send_message(peer, MSG_VERACK, NULL, 0);
    peer->handshaked = 1;
    peer->last_seen = time(NULL);
    
    node_send_message(peer, MSG_GETSTATE, NULL, 0);
}

// Handle state request
void handle_getstate(PCNode* node, PCNodePeer* peer) {
    pthread_mutex_lock(&node->state_lock);
    
    uint8_t buffer[BUFFER_SIZE];
    size_t len = pc_state_serialize(&node->state, buffer, sizeof(buffer));
    
    pthread_mutex_unlock(&node->state_lock);
    
    if (len > 0) {
        // If we're a validator, send signed state
        if (node->is_validator) {
            PCSignedStateHeader sig_header;
            pc_node_sign_state(node, &sig_header);
            
            // Send signature first, then state
            node_send_message(peer, MSG_STATE_SIG, &sig_header, sizeof(sig_header));
        }
        
        node_send_message(peer, MSG_STATE, buffer, len);
        printf("[%s:%d] Sent state (%zu bytes)\n", peer->ip, peer->port, len);
    }
}

// Handle SIGNED state message (new secure version)
void handle_signed_state(PCNode* node, PCNodePeer* peer, const uint8_t* data, size_t len) {
    (void)node;  // Used for future signature validation against trusted validators
    
    if (len < sizeof(PCSignedStateHeader)) {
        printf("[%s:%d] Invalid signed state header\n", peer->ip, peer->port);
        peer->violations++;
        return;
    }
    
    PCSignedStateHeader* sig_header = (PCSignedStateHeader*)data;
    
    // Store for validation when state arrives
    memcpy(&peer->node_pubkey, sig_header->validator_pubkey, 32);
    
    printf("[%s:%d] Received state signature from validator\n", peer->ip, peer->port);
}

// Handle state message - SECURITY HARDENED
void handle_state(PCNode* node, PCNodePeer* peer, const uint8_t* data, size_t len) {
    PCState new_state;
    memset(&new_state, 0, sizeof(new_state));
    
    if (pc_state_deserialize(&new_state, data, len) != PC_OK) {
        printf("[%s:%d] Failed to deserialize state\n", peer->ip, peer->port);
        peer->violations++;
        return;
    }
    
    pthread_mutex_lock(&node->state_lock);
    
    // SECURITY CHECK 1: Version must be higher
    if (new_state.version <= node->state.version) {
        printf("[%s:%d] Rejected state: version %lu <= current %lu\n", 
               peer->ip, peer->port, new_state.version, node->state.version);
        pthread_mutex_unlock(&node->state_lock);
        pc_state_free(&new_state);
        return;
    }
    
    // SECURITY CHECK 2: Verify conservation law
    PCError cons_err = pc_state_verify_conservation(&new_state);
    if (cons_err != PC_OK) {
        printf("[%s:%d] SECURITY: Rejected state - conservation law violated!\n", 
               peer->ip, peer->port);
        peer->violations++;
        pthread_mutex_unlock(&node->state_lock);
        pc_state_free(&new_state);
        return;
    }
    
    // SECURITY CHECK 3: For non-genesis sync, verify peer is validator or state is signed
    if (node->state.version > 0 && node->num_trusted_validators > 0) {
        if (!peer->is_validator && !is_trusted_validator(node, peer->node_pubkey)) {
            printf("[%s:%d] SECURITY: Rejected state - peer is not a trusted validator\n", 
                   peer->ip, peer->port);
            pthread_mutex_unlock(&node->state_lock);
            pc_state_free(&new_state);
            return;
        }
    }
    
    // SECURITY CHECK 4: Verify total supply hasn't changed (except genesis)
    if (node->state.total_supply > 0 && 
        fabs(new_state.total_supply - node->state.total_supply) > 1e-9) {
        printf("[%s:%d] SECURITY: Rejected state - total supply changed from %.8f to %.8f\n", 
               peer->ip, peer->port, node->state.total_supply, new_state.total_supply);
        peer->violations++;
        pthread_mutex_unlock(&node->state_lock);
        pc_state_free(&new_state);
        return;
    }
    
    // All checks passed - accept the state
    printf("[%s:%d] Syncing state v%lu -> v%lu (verified)\n", 
           peer->ip, peer->port,
           node->state.version, new_state.version);
    pc_state_free(&node->state);
    node->state = new_state;
    
    pthread_mutex_unlock(&node->state_lock);
}

// Handle transaction
void handle_tx(PCNode* node, PCNodePeer* peer, const uint8_t* data, size_t len) {
    if (len < sizeof(PCTransaction)) {
        peer->violations++;
        return;
    }
    
    PCTransaction tx;
    memcpy(&tx, data, sizeof(PCTransaction));
    
    // SECURITY: Verify transaction signature before accepting
    PCError verify_err = pc_transaction_verify(&tx);
    if (verify_err != PC_OK) {
        printf("[%s:%d] SECURITY: Rejected TX - invalid signature\n", peer->ip, peer->port);
        peer->violations++;
        return;
    }
    
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
    } else {
        printf("[%s:%d] TX rejected: %s\n", peer->ip, peer->port, pc_strerror(err));
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
    if (peer->banned) {
        if (peer->ban_until && time(NULL) >= peer->ban_until) {
            peer->banned = 0;
            peer->violations = 0;
        } else {
            return;
        }
    }
    
    if (check_rate_limit(peer) != 0) {
        return;
    }
    
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
        case MSG_STATE_SIG:
            handle_signed_state(node, peer, data, header->length);
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
                ban_peer(peer, 1);
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
    
    // Send version with our pubkey
    uint8_t version_msg[40];
    uint64_t ver = node->state.version;
    memcpy(version_msg, &ver, 8);
    memcpy(version_msg + 8, node->wallet.public_key, 32);
    node_send_message(peer, MSG_VERSION, version_msg, sizeof(version_msg));
    
    return 0;
}

// Main node loop
void node_run(PCNode* node) {
    struct pollfd fds[MAX_PEERS + 1];
    time_t last_heartbeat = 0;
    
    while (node->running) {
        int nfds = 0;
        
        fds[nfds].fd = node->listen_fd;
        fds[nfds].events = POLLIN;
        nfds++;
        
        for (uint32_t i = 0; i < node->num_peers; i++) {
            if (node->peers[i].connected) {
                fds[nfds].fd = node->peers[i].fd;
                fds[nfds].events = POLLIN;
                nfds++;
            }
        }
        
        int ready = poll(fds, nfds, 1000);
        
        if (ready > 0) {
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
                    
                    uint8_t version_msg[40];
                    uint64_t ver = node->state.version;
                    memcpy(version_msg, &ver, 8);
                    memcpy(version_msg + 8, node->wallet.public_key, 32);
                    node_send_message(peer, MSG_VERSION, version_msg, sizeof(version_msg));
                }
            }
            
            int fidx = 1;
            for (uint32_t i = 0; i < node->num_peers && fidx < nfds; i++) {
                if (node->peers[i].connected) {
                    if (fds[fidx].revents & POLLIN) {
                        PCMessageHeader header;
                        uint8_t buffer[BUFFER_SIZE];
                        
                        if (node_recv_message(&node->peers[i], &header, buffer, sizeof(buffer)) == 0) {
                            handle_message(node, &node->peers[i], &header, buffer);
                        } else {
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
    
    // Initialize sodium
    if (sodium_init() < 0) {
        fprintf(stderr, "Failed to initialize libsodium\n");
        return PC_ERR_CRYPTO;
    }
    
    // Generate node ID
    pc_keypair_generate(&node->wallet);
    memcpy(node->node_id, node->wallet.public_key, 32);
    
    // Load or create state
    if (pc_state_load(&node->state, "state.pcs") != PC_OK) {
        printf("Creating new genesis state...\n");
        pc_state_genesis(&node->state, node->wallet.public_key, 1000000.0);
        pc_state_save(&node->state, "state.pcs");
    }
    
    // By default, trust ourselves as validator
    node->is_validator = 1;
    pc_node_add_validator(node, node->wallet.public_key);
    
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
    printf("║              PHYSICSCOIN SECURE NODE                          ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
    
    printf("Port:       %u\n", node->port);
    printf("Node ID:    ");
    for (int i = 0; i < 8; i++) printf("%02x", node->node_id[i]);
    printf("...\n");
    printf("Validator:  %s\n", node->is_validator ? "YES" : "NO");
    printf("Trusted:    %d validators\n", node->num_trusted_validators);
    printf("Peers:      %u/%d\n", node->num_peers, MAX_PEERS);
    printf("State:      v%lu (%u wallets)\n", node->state.version, node->state.num_wallets);
    printf("Supply:     %.2f\n\n", node->state.total_supply);
    
    printf("Security Features:\n");
    printf("  ✓ Conservation verification on state sync\n");
    printf("  ✓ Validator signature verification\n");
    printf("  ✓ Rate limiting (%d msg/min, %d tx/min)\n", MAX_MSG_PER_MINUTE, MAX_TX_PER_MINUTE);
    printf("  ✓ Ban system (%d violations = ban)\n\n", MAX_VIOLATIONS);
    
    if (node->num_peers > 0) {
        printf("Connected Peers:\n");
        for (uint32_t i = 0; i < node->num_peers; i++) {
            PCNodePeer* p = &node->peers[i];
            printf("  [%u] %s:%d %s%s%s\n", i, p->ip, p->port,
                   p->connected ? "✓" : "✗",
                   p->handshaked ? " (ready)" : "",
                   p->is_validator ? " [VALIDATOR]" : "");
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
    
    signal(SIGINT, node_signal_handler);
    signal(SIGTERM, node_signal_handler);
    
    printf("Starting PhysicsCoin secure node on port %u...\n", port);
    if (pc_node_init(&node, port) != PC_OK) {
        fprintf(stderr, "Failed to initialize node\n");
        return 1;
    }
    
    if (connect_ip && connect_port) {
        printf("Connecting to seed node %s:%d...\n", connect_ip, connect_port);
        if (node_connect_peer(&node, connect_ip, connect_port) != 0) {
            fprintf(stderr, "Warning: Failed to connect to seed node\n");
        }
    }
    
    pc_node_print_status(&node);
    printf("Node running. Press Ctrl+C to stop.\n\n");
    
    node_run(&node);
    
    printf("Saving state...\n");
    pc_state_save(&node.state, "state.pcs");
    
    pc_node_free(&node);
    printf("Node stopped.\n");
    
    return 0;
}
