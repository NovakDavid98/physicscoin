// network_config.c - Network Configuration Implementation
#include "../include/network_config.h"
#include <stdio.h>
#include <string.h>

// Current network (default to devnet for safety)
static PCNetworkType g_current_network = PC_NETWORK_DEVNET;

// Network configurations
static const PCNetworkConfig g_network_configs[] = {
    // Mainnet
    {
        .type = PC_NETWORK_MAINNET,
        .magic = PC_MAGIC_MAINNET,
        .default_port = 9333,
        .api_port = 8545,
        .network_name = "mainnet",
        .state_file = "state_mainnet.pcs",
        .genesis_supply = 21000000.0,  // 21M supply like Bitcoin
        .has_faucet = 0,
        .faucet_amount = 0.0,
        .faucet_cooldown = 0
    },
    // Testnet
    {
        .type = PC_NETWORK_TESTNET,
        .magic = PC_MAGIC_TESTNET,
        .default_port = 19333,
        .api_port = 18545,
        .network_name = "testnet",
        .state_file = "state_testnet.pcs",
        .genesis_supply = 10000000.0,  // 10M testnet supply
        .has_faucet = 1,
        .faucet_amount = 100.0,         // 100 coins per faucet request
        .faucet_cooldown = 3600         // 1 hour cooldown
    },
    // Devnet (local development)
    {
        .type = PC_NETWORK_DEVNET,
        .magic = PC_MAGIC_DEVNET,
        .default_port = 29333,
        .api_port = 28545,
        .network_name = "devnet",
        .state_file = "state_devnet.pcs",
        .genesis_supply = 1000000.0,   // 1M devnet supply
        .has_faucet = 1,
        .faucet_amount = 1000.0,       // 1000 coins per request
        .faucet_cooldown = 60          // 1 minute cooldown
    }
};

const PCNetworkConfig* pc_network_get_config(PCNetworkType type) {
    if (type >= 0 && type < 3) {
        return &g_network_configs[type];
    }
    return &g_network_configs[PC_NETWORK_DEVNET];  // Default to devnet
}

PCNetworkType pc_network_get_current(void) {
    return g_current_network;
}

void pc_network_set_current(PCNetworkType type) {
    if (type >= PC_NETWORK_MAINNET && type <= PC_NETWORK_DEVNET) {
        g_current_network = type;
        const PCNetworkConfig* config = pc_network_get_config(type);
        printf("Network set to: %s\n", config->network_name);
        printf("  P2P Port: %d\n", config->default_port);
        printf("  API Port: %d\n", config->api_port);
        printf("  State File: %s\n", config->state_file);
        printf("  Faucet: %s\n", config->has_faucet ? "enabled" : "disabled");
    }
}

int pc_network_faucet_enabled(void) {
    const PCNetworkConfig* config = pc_network_get_config(g_current_network);
    return config->has_faucet;
}

// Parse network type from string
PCNetworkType pc_network_parse(const char* network_str) {
    if (!network_str) return PC_NETWORK_DEVNET;
    
    if (strcmp(network_str, "mainnet") == 0) return PC_NETWORK_MAINNET;
    if (strcmp(network_str, "testnet") == 0) return PC_NETWORK_TESTNET;
    if (strcmp(network_str, "devnet") == 0) return PC_NETWORK_DEVNET;
    
    return PC_NETWORK_DEVNET;  // Default
}

// Print network info
void pc_network_print_info(void) {
    const PCNetworkConfig* config = pc_network_get_config(g_current_network);
    
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║           PHYSICSCOIN NETWORK CONFIGURATION                   ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("Network:        %s\n", config->network_name);
    printf("Magic Bytes:    0x%08X\n", config->magic);
    printf("P2P Port:       %d\n", config->default_port);
    printf("API Port:       %d\n", config->api_port);
    printf("State File:     %s\n", config->state_file);
    printf("Genesis Supply: %.2f\n", config->genesis_supply);
    printf("Faucet:         %s", config->has_faucet ? "✓ enabled" : "✗ disabled");
    if (config->has_faucet) {
        printf(" (%.2f coins, %d sec cooldown)", config->faucet_amount, config->faucet_cooldown);
    }
    printf("\n\n");
}

