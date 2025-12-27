// network.h - Network Configuration and Types
#ifndef PHYSICSCOIN_NETWORK_H
#define PHYSICSCOIN_NETWORK_H

#include <stdint.h>

// Network types
typedef enum {
    PC_NETWORK_MAINNET = 0,
    PC_NETWORK_TESTNET = 1,
    PC_NETWORK_DEVNET = 2
} PCNetworkType;

// Network configuration
typedef struct {
    PCNetworkType type;
    uint32_t magic;              // Network magic bytes for packet identification
    uint16_t default_port;       // Default P2P port
    uint16_t api_port;           // Default API port
    const char* network_name;
    const char* state_file;
    double genesis_supply;
    int has_faucet;              // 1 if faucet is enabled
    double faucet_amount;        // Amount to give per faucet request
    int faucet_cooldown;         // Seconds between faucet requests per address
} PCNetworkConfig;

// Get network configuration
const PCNetworkConfig* pc_network_get_config(PCNetworkType type);

// Get current network
PCNetworkType pc_network_get_current(void);

// Set current network
void pc_network_set_current(PCNetworkType type);

// Check if faucet is enabled for current network
int pc_network_faucet_enabled(void);

// Parse network type from string
PCNetworkType pc_network_parse(const char* network_str);

// Print network information
void pc_network_print_info(void);

// Network magic bytes
#define PC_MAGIC_MAINNET 0xD903A34E
#define PC_MAGIC_TESTNET 0x0709110B
#define PC_MAGIC_DEVNET  0xDAB5BFFA

#endif // PHYSICSCOIN_NETWORK_H

