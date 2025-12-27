// sockets.c - TCP Socket Layer for P2P Networking
// Real socket implementation replacing simulated gossip

#include "../include/physicscoin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>

#define MAX_PEERS 125
#define BUFFER_SIZE 65536
#define MAGIC_NUMBER 0xFEFEFEFE

// Socket structure
typedef struct {
    int fd;                    // File descriptor
    struct sockaddr_in addr;   // Address
    int connected;             // Connection status
    uint8_t peer_id[32];       // Peer identifier
} PCSocket;

// Network message
typedef struct {
    uint32_t magic;            // 0xFEFEFEFE
    uint8_t command[12];       // "version", "delta", "tx", etc.
    uint32_t payload_length;
    uint8_t checksum[4];
    uint8_t* payload;
} PCNetworkMessage;

// Peer connection
typedef struct {
    PCSocket socket;
    uint64_t last_seen;
    uint32_t messages_sent;
    uint32_t messages_received;
    int banned;
} PCPeer;

// P2P Network
typedef struct {
    PCSocket listen_socket;     // Server socket
    PCPeer peers[MAX_PEERS];
    uint32_t num_peers;
    uint16_t port;
    uint8_t node_id[32];
} PCNetwork;

// Create socket
PCError pc_socket_create(PCSocket* sock, uint16_t port) {
    if (!sock) return PC_ERR_IO;
    
    memset(sock, 0, sizeof(PCSocket));
    
    // Create TCP socket
    sock->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock->fd < 0) {
        perror("socket");
        return PC_ERR_IO;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(sock->fd);
        return PC_ERR_IO;
    }
    
    // Bind to port
    sock->addr.sin_family = AF_INET;
    sock->addr.sin_addr.s_addr = INADDR_ANY;
    sock->addr.sin_port = htons(port);
    
    if (bind(sock->fd, (struct sockaddr*)&sock->addr, sizeof(sock->addr)) < 0) {
        perror("bind");
        close(sock->fd);
        return PC_ERR_IO;
    }
    
    sock->connected = 0;
    return PC_OK;
}

// Listen for connections
PCError pc_socket_listen(PCSocket* sock) {
    if (!sock) return PC_ERR_IO;
    
    if (listen(sock->fd, 10) < 0) {
        perror("listen");
        return PC_ERR_IO;
    }
    
    return PC_OK;
}

// Accept connection
PCError pc_socket_accept(PCSocket* listen_sock, PCSocket* client_sock) {
    if (!listen_sock || !client_sock) return PC_ERR_IO;
    
    socklen_t addr_len = sizeof(client_sock->addr);
    client_sock->fd = accept(listen_sock->fd, 
                             (struct sockaddr*)&client_sock->addr, 
                             &addr_len);
    
    if (client_sock->fd < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return PC_ERR_IO;  // No connection pending
        }
        perror("accept");
        return PC_ERR_IO;
    }
    
    client_sock->connected = 1;
    
    printf("Accepted connection from %s:%d\n",
           inet_ntoa(client_sock->addr.sin_addr),
           ntohs(client_sock->addr.sin_port));
    
    return PC_OK;
}

// Connect to remote peer
PCError pc_socket_connect(PCSocket* sock, const char* ip, uint16_t port) {
    if (!sock || !ip) return PC_ERR_IO;
    
    // Create socket
    sock->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock->fd < 0) {
        perror("socket");
        return PC_ERR_IO;
    }
    
    // Set destination address
    sock->addr.sin_family = AF_INET;
    sock->addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, ip, &sock->addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock->fd);
        return PC_ERR_IO;
    }
    
    printf("Connecting to %s:%d...\n", ip, port);
    
    // Connect
    if (connect(sock->fd, (struct sockaddr*)&sock->addr, sizeof(sock->addr)) < 0) {
        perror("connect");
        close(sock->fd);
        return PC_ERR_IO;
    }
    
    sock->connected = 1;
    printf("Connected to %s:%d\n", ip, port);
    
    return PC_OK;
}

// Send data
PCError pc_socket_send(PCSocket* sock, const void* data, size_t len) {
    if (!sock || !data || !sock->connected) return PC_ERR_IO;
    
    ssize_t sent = send(sock->fd, data, len, 0);
    if (sent < 0) {
        perror("send");
        return PC_ERR_IO;
    }
    
    if ((size_t)sent != len) {
        fprintf(stderr, "Partial send: %zd/%zu bytes\n", sent, len);
        return PC_ERR_IO;
    }
    
    return PC_OK;
}

// Receive data
PCError pc_socket_receive(PCSocket* sock, void* buffer, size_t* len) {
    if (!sock || !buffer || !len || !sock->connected) return PC_ERR_IO;
    
    ssize_t received = recv(sock->fd, buffer, *len, 0);
    if (received < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            *len = 0;
            return PC_OK;  // No data available (non-blocking)
        }
        perror("recv");
        return PC_ERR_IO;
    }
    
    if (received == 0) {
        // Connection closed
        sock->connected = 0;
        *len = 0;
        return PC_ERR_IO;
    }
    
    *len = received;
    return PC_OK;
}

// Set non-blocking mode
PCError pc_socket_set_nonblocking(PCSocket* sock) {
    if (!sock) return PC_ERR_IO;
    
    int flags = fcntl(sock->fd, F_GETFL, 0);
    if (flags < 0) {
        perror("fcntl F_GETFL");
        return PC_ERR_IO;
    }
    
    if (fcntl(sock->fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("fcntl F_SETFL");
        return PC_ERR_IO;
    }
    
    return PC_OK;
}

// Close socket
void pc_socket_close(PCSocket* sock) {
    if (sock && sock->fd >= 0) {
        close(sock->fd);
        sock->fd = -1;
        sock->connected = 0;
    }
}

// Initialize network
PCError pc_network_init(PCNetwork* network, uint16_t port, const uint8_t* node_id) {
    if (!network || !node_id) return PC_ERR_IO;
    
    memset(network, 0, sizeof(PCNetwork));
    network->port = port;
    memcpy(network->node_id, node_id, 32);
    network->num_peers = 0;
    
    // Create listen socket
    PCError err = pc_socket_create(&network->listen_socket, port);
    if (err != PC_OK) return err;
    
    err = pc_socket_listen(&network->listen_socket);
    if (err != PC_OK) {
        pc_socket_close(&network->listen_socket);
        return err;
    }
    
    // Set non-blocking
    pc_socket_set_nonblocking(&network->listen_socket);
    
    printf("Network initialized on port %u\n", port);
    
    return PC_OK;
}

// Add peer
PCError pc_network_add_peer(PCNetwork* network, const char* ip, uint16_t port) {
    if (!network || network->num_peers >= MAX_PEERS) return PC_ERR_MAX_WALLETS;
    
    PCPeer* peer = &network->peers[network->num_peers];
    memset(peer, 0, sizeof(PCPeer));
    
    PCError err = pc_socket_connect(&peer->socket, ip, port);
    if (err != PC_OK) return err;
    
    peer->last_seen = time(NULL);
    peer->messages_sent = 0;
    peer->messages_received = 0;
    peer->banned = 0;
    
    network->num_peers++;
    
    return PC_OK;
}

// Network loop (accepts connections and receives messages)
PCError pc_network_poll(PCNetwork* network, int timeout_ms) {
    if (!network) return PC_ERR_IO;
    
    struct pollfd fds[MAX_PEERS + 1];
    int nfds = 0;
    
    // Add listen socket
    fds[nfds].fd = network->listen_socket.fd;
    fds[nfds].events = POLLIN;
    nfds++;
    
    // Add peer sockets
    for (uint32_t i = 0; i < network->num_peers; i++) {
        if (network->peers[i].socket.connected && !network->peers[i].banned) {
            fds[nfds].fd = network->peers[i].socket.fd;
            fds[nfds].events = POLLIN;
            nfds++;
        }
    }
    
    // Poll
    int ready = poll(fds, nfds, timeout_ms);
    if (ready < 0) {
        perror("poll");
        return PC_ERR_IO;
    }
    
    // Check listen socket for new connections
    if (fds[0].revents & POLLIN) {
        PCSocket client_sock;
        if (pc_socket_accept(&network->listen_socket, &client_sock) == PC_OK) {
            if (network->num_peers < MAX_PEERS) {
                network->peers[network->num_peers].socket = client_sock;
                network->peers[network->num_peers].last_seen = time(NULL);
                network->num_peers++;
            } else {
                pc_socket_close(&client_sock);
            }
        }
    }
    
    // Check peer sockets for data
    for (uint32_t i = 0; i < network->num_peers; i++) {
        PCPeer* peer = &network->peers[i];
        if (peer->socket.connected && !peer->banned) {
            uint8_t buffer[BUFFER_SIZE];
            size_t len = sizeof(buffer);
            
            if (pc_socket_receive(&peer->socket, buffer, &len) == PC_OK && len > 0) {
                peer->last_seen = time(NULL);
                peer->messages_received++;
                
                // TODO: Parse and handle message
                printf("Received %zu bytes from peer %u\n", len, i);
            }
        }
    }
    
    return PC_OK;
}

// Broadcast to all peers
PCError pc_network_broadcast(PCNetwork* network, const void* data, size_t len) {
    if (!network || !data) return PC_ERR_IO;
    
    for (uint32_t i = 0; i < network->num_peers; i++) {
        PCPeer* peer = &network->peers[i];
        if (peer->socket.connected && !peer->banned) {
            pc_socket_send(&peer->socket, data, len);
            peer->messages_sent++;
        }
    }
    
    return PC_OK;
}

// Print network stats
void pc_network_print_stats(const PCNetwork* network) {
    printf("\n╔═══════════════════════════════════════════════╗\n");
    printf("║         NETWORK STATISTICS                    ║\n");
    printf("╚═══════════════════════════════════════════════╝\n\n");
    
    printf("Port: %u\n", network->port);
    printf("Node ID: ");
    for (int i = 0; i < 8; i++) printf("%02x", network->node_id[i]);
    printf("...\n");
    printf("Peers: %u/%d\n\n", network->num_peers, MAX_PEERS);
    
    for (uint32_t i = 0; i < network->num_peers; i++) {
        const PCPeer* peer = &network->peers[i];
        printf("[%u] %s:%d %s\n",
               i,
               inet_ntoa(peer->socket.addr.sin_addr),
               ntohs(peer->socket.addr.sin_port),
               peer->socket.connected ? "CONNECTED" : "disconnected");
        printf("     Sent: %u | Received: %u | Last seen: %lu\n",
               peer->messages_sent, peer->messages_received, peer->last_seen);
    }
}

// Cleanup
void pc_network_free(PCNetwork* network) {
    if (network) {
        pc_socket_close(&network->listen_socket);
        for (uint32_t i = 0; i < network->num_peers; i++) {
            pc_socket_close(&network->peers[i].socket);
        }
    }
}
