// main.c - PhysicsCoin CLI
// Command-line interface for all operations

#include "../include/physicscoin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define STATE_FILE "state.pcs"
#define WALLET_FILE "wallet.pcw"

void print_usage(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║               PHYSICSCOIN CLI v%s                     ║\n", PHYSICSCOIN_VERSION);
    printf("║        Physics-Based Cryptocurrency                       ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("Usage: physicscoin <command> [options]\n\n");
    printf("Commands:\n");
    printf("  init <supply>           Create genesis with initial supply\n");
    printf("  wallet create           Generate new wallet keypair\n");
    printf("  balance <address>       Check wallet balance\n");
    printf("  send <to> <amount>      Send coins (uses local wallet)\n");
    printf("  state                   Display universe state\n");
    printf("  verify                  Verify energy conservation\n");
    printf("  demo                    Run interactive demo\n");
    printf("\n");
}

void print_state(const PCState* state) {
    printf("\n┌──────────────────────────────────────────────────────────────┐\n");
    printf("│                    UNIVERSE STATE                            │\n");
    printf("├──────────────────────────────────────────────────────────────┤\n");
    printf("│ Version:        %-10lu                                   │\n", state->version);
    printf("│ Wallets:        %-10u                                   │\n", state->num_wallets);
    printf("│ Total Supply:   %-20.8f                   │\n", state->total_supply);
    printf("│ State Hash:     ");
    for (int i = 0; i < 8; i++) printf("%02x", state->state_hash[i]);
    printf("...                         │\n");
    printf("├──────────────────────────────────────────────────────────────┤\n");
    
    double actual_sum = 0.0;
    for (uint32_t i = 0; i < state->num_wallets; i++) {
        char addr[65];
        pc_pubkey_to_hex(state->wallets[i].public_key, addr);
        printf("│ %.8s... : %20.8f (nonce: %lu)     │\n", 
               addr, state->wallets[i].energy, state->wallets[i].nonce);
        actual_sum += state->wallets[i].energy;
    }
    
    printf("├──────────────────────────────────────────────────────────────┤\n");
    double error = state->total_supply - actual_sum;
    printf("│ Conservation Error: %.12e                          │\n", error);
    printf("└──────────────────────────────────────────────────────────────┘\n\n");
}

int cmd_init(double supply) {
    printf("Creating genesis state with %.2f coins...\n", supply);
    
    PCKeypair founder;
    if (pc_keypair_generate(&founder) != PC_OK) {
        printf("Error: Failed to generate keypair\n");
        return 1;
    }
    
    FILE* f = fopen(WALLET_FILE, "wb");
    if (f) {
        fwrite(&founder, sizeof(founder), 1, f);
        fclose(f);
    }
    
    char addr[65];
    pc_pubkey_to_hex(founder.public_key, addr);
    printf("Founder address: %s\n", addr);
    
    PCState state;
    PCError err = pc_state_genesis(&state, founder.public_key, supply);
    if (err != PC_OK) {
        printf("Error: %s\n", pc_strerror(err));
        return 1;
    }
    
    err = pc_state_save(&state, STATE_FILE);
    if (err != PC_OK) {
        printf("Error saving state: %s\n", pc_strerror(err));
        pc_state_free(&state);
        return 1;
    }
    
    printf("Genesis created! State saved to %s\n", STATE_FILE);
    printf("Wallet saved to %s\n", WALLET_FILE);
    
    print_state(&state);
    pc_state_free(&state);
    return 0;
}

int cmd_wallet_create(void) {
    PCKeypair kp;
    if (pc_keypair_generate(&kp) != PC_OK) {
        printf("Error: Failed to generate keypair\n");
        return 1;
    }
    
    FILE* f = fopen(WALLET_FILE, "wb");
    if (f) {
        fwrite(&kp, sizeof(kp), 1, f);
        fclose(f);
    }
    
    char addr[65];
    pc_pubkey_to_hex(kp.public_key, addr);
    printf("New wallet created!\n");
    printf("Address: %s\n", addr);
    printf("Saved to: %s\n", WALLET_FILE);
    
    return 0;
}

int cmd_balance(const char* address) {
    PCState state;
    if (pc_state_load(&state, STATE_FILE) != PC_OK) {
        printf("Error: Cannot load state. Run 'physicscoin init' first.\n");
        return 1;
    }
    
    uint8_t pubkey[PHYSICSCOIN_KEY_SIZE];
    if (pc_hex_to_pubkey(address, pubkey) != PC_OK) {
        printf("Error: Invalid address format\n");
        pc_state_free(&state);
        return 1;
    }
    
    PCWallet* w = pc_state_get_wallet(&state, pubkey);
    if (!w) {
        printf("Balance: 0.00000000 (wallet not found)\n");
    } else {
        printf("Balance: %.8f\n", w->energy);
        printf("Nonce:   %lu\n", w->nonce);
    }
    
    pc_state_free(&state);
    return 0;
}

int cmd_send(const char* to_addr, double amount) {
    PCKeypair kp;
    FILE* wf = fopen(WALLET_FILE, "rb");
    if (!wf) {
        printf("Error: No wallet found. Run 'physicscoin wallet create' first.\n");
        return 1;
    }
    fread(&kp, sizeof(kp), 1, wf);
    fclose(wf);
    
    PCState state;
    if (pc_state_load(&state, STATE_FILE) != PC_OK) {
        printf("Error: Cannot load state\n");
        return 1;
    }
    
    PCWallet* sender = pc_state_get_wallet(&state, kp.public_key);
    if (!sender) {
        printf("Error: Your wallet not found in state\n");
        pc_state_free(&state);
        return 1;
    }
    
    uint8_t to_pubkey[PHYSICSCOIN_KEY_SIZE];
    if (pc_hex_to_pubkey(to_addr, to_pubkey) != PC_OK) {
        printf("Error: Invalid recipient address\n");
        pc_state_free(&state);
        return 1;
    }
    
    PCTransaction tx;
    memcpy(tx.from, kp.public_key, PHYSICSCOIN_KEY_SIZE);
    memcpy(tx.to, to_pubkey, PHYSICSCOIN_KEY_SIZE);
    tx.amount = amount;
    tx.nonce = sender->nonce;
    tx.timestamp = (uint64_t)time(NULL);
    
    if (pc_transaction_sign(&tx, &kp) != PC_OK) {
        printf("Error: Failed to sign transaction\n");
        pc_state_free(&state);
        return 1;
    }
    
    char from_hex[65], to_hex[65];
    pc_pubkey_to_hex(tx.from, from_hex);
    pc_pubkey_to_hex(tx.to, to_hex);
    
    printf("Sending %.8f from %.8s... to %.8s...\n", amount, from_hex, to_hex);
    
    PCError err = pc_state_execute_tx(&state, &tx);
    if (err != PC_OK) {
        printf("Error: %s\n", pc_strerror(err));
        pc_state_free(&state);
        return 1;
    }
    
    if (pc_state_save(&state, STATE_FILE) != PC_OK) {
        printf("Error: Failed to save state\n");
        pc_state_free(&state);
        return 1;
    }
    
    printf("✓ Transaction confirmed!\n");
    print_state(&state);
    pc_state_free(&state);
    return 0;
}

int cmd_state(void) {
    PCState state;
    if (pc_state_load(&state, STATE_FILE) != PC_OK) {
        printf("Error: Cannot load state. Run 'physicscoin init' first.\n");
        return 1;
    }
    
    print_state(&state);
    pc_state_free(&state);
    return 0;
}

int cmd_verify(void) {
    PCState state;
    if (pc_state_load(&state, STATE_FILE) != PC_OK) {
        printf("Error: Cannot load state\n");
        return 1;
    }
    
    printf("Verifying energy conservation...\n");
    
    PCError err = pc_state_verify_conservation(&state);
    if (err == PC_OK) {
        printf("✓ Conservation law verified!\n");
        printf("  Total Supply: %.8f\n", state.total_supply);
        
        double sum = 0.0;
        for (uint32_t i = 0; i < state.num_wallets; i++) {
            sum += state.wallets[i].energy;
        }
        printf("  Actual Sum:   %.8f\n", sum);
        printf("  Error:        %.12e\n", state.total_supply - sum);
    } else {
        printf("✗ CONSERVATION VIOLATED!\n");
    }
    
    pc_state_free(&state);
    return (err == PC_OK) ? 0 : 1;
}

int cmd_demo(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║             PHYSICSCOIN INTERACTIVE DEMO                  ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
    
    printf("Creating 3 wallets...\n\n");
    
    PCKeypair alice, bob, charlie;
    pc_keypair_generate(&alice);
    pc_keypair_generate(&bob);
    pc_keypair_generate(&charlie);
    
    char alice_addr[65], bob_addr[65], charlie_addr[65];
    pc_pubkey_to_hex(alice.public_key, alice_addr);
    pc_pubkey_to_hex(bob.public_key, bob_addr);
    pc_pubkey_to_hex(charlie.public_key, charlie_addr);
    
    printf("Alice:   %.16s...\n", alice_addr);
    printf("Bob:     %.16s...\n", bob_addr);
    printf("Charlie: %.16s...\n", charlie_addr);
    
    printf("\n═══ GENESIS ═══\n");
    PCState state;
    pc_state_genesis(&state, alice.public_key, 1000.0);
    
    pc_state_create_wallet(&state, bob.public_key, 0);
    pc_state_create_wallet(&state, charlie.public_key, 0);
    
    print_state(&state);
    
    printf("═══ TRANSACTIONS ═══\n");
    
    PCTransaction tx1 = {0};
    memcpy(tx1.from, alice.public_key, PHYSICSCOIN_KEY_SIZE);
    memcpy(tx1.to, bob.public_key, PHYSICSCOIN_KEY_SIZE);
    tx1.amount = 100.0;
    tx1.nonce = 0;
    tx1.timestamp = time(NULL);
    pc_transaction_sign(&tx1, &alice);
    
    printf("TX1: Alice → Bob: 100 coins... ");
    PCError err = pc_state_execute_tx(&state, &tx1);
    printf("%s\n", err == PC_OK ? "✓" : pc_strerror(err));
    
    PCTransaction tx2 = {0};
    memcpy(tx2.from, alice.public_key, PHYSICSCOIN_KEY_SIZE);
    memcpy(tx2.to, charlie.public_key, PHYSICSCOIN_KEY_SIZE);
    tx2.amount = 50.0;
    tx2.nonce = 1;
    tx2.timestamp = time(NULL);
    pc_transaction_sign(&tx2, &alice);
    
    printf("TX2: Alice → Charlie: 50 coins... ");
    err = pc_state_execute_tx(&state, &tx2);
    printf("%s\n", err == PC_OK ? "✓" : pc_strerror(err));
    
    PCTransaction tx3 = {0};
    memcpy(tx3.from, bob.public_key, PHYSICSCOIN_KEY_SIZE);
    memcpy(tx3.to, charlie.public_key, PHYSICSCOIN_KEY_SIZE);
    tx3.amount = 25.0;
    tx3.nonce = 0;
    tx3.timestamp = time(NULL);
    pc_transaction_sign(&tx3, &bob);
    
    printf("TX3: Bob → Charlie: 25 coins... ");
    err = pc_state_execute_tx(&state, &tx3);
    printf("%s\n", err == PC_OK ? "✓" : pc_strerror(err));
    
    print_state(&state);
    
    printf("═══ DOUBLE-SPEND ATTEMPT ═══\n");
    PCTransaction tx_bad = {0};
    memcpy(tx_bad.from, charlie.public_key, PHYSICSCOIN_KEY_SIZE);
    memcpy(tx_bad.to, alice.public_key, PHYSICSCOIN_KEY_SIZE);
    tx_bad.amount = 200.0;
    tx_bad.nonce = 0;
    tx_bad.timestamp = time(NULL);
    pc_transaction_sign(&tx_bad, &charlie);
    
    printf("TX: Charlie → Alice: 200 coins (Charlie only has 75)... ");
    err = pc_state_execute_tx(&state, &tx_bad);
    printf("%s\n", err == PC_OK ? "✓" : pc_strerror(err));
    
    printf("\n═══ CONSERVATION VERIFICATION ═══\n");
    err = pc_state_verify_conservation(&state);
    printf("Energy Conservation: %s\n", err == PC_OK ? "✓ VERIFIED" : "✗ FAILED");
    
    uint8_t buffer[4096];
    size_t size = pc_state_serialize(&state, buffer, sizeof(buffer));
    printf("\n═══ STATE COMPRESSION ═══\n");
    printf("State size: %zu bytes\n", size);
    printf("Transactions processed: 3\n");
    printf("Bitcoin equivalent: ~500 GB\n");
    printf("Compression ratio: %.0f million : 1\n", 500.0 * 1024 * 1024 * 1024 / size / 1000000);
    
    pc_state_free(&state);
    printf("\n");
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return 0;
    }
    
    const char* cmd = argv[1];
    
    if (strcmp(cmd, "init") == 0) {
        if (argc < 3) {
            printf("Usage: physicscoin init <supply>\n");
            return 1;
        }
        return cmd_init(atof(argv[2]));
    }
    else if (strcmp(cmd, "wallet") == 0) {
        if (argc < 3 || strcmp(argv[2], "create") == 0) {
            return cmd_wallet_create();
        }
        printf("Unknown wallet command\n");
        return 1;
    }
    else if (strcmp(cmd, "balance") == 0) {
        if (argc < 3) {
            printf("Usage: physicscoin balance <address>\n");
            return 1;
        }
        return cmd_balance(argv[2]);
    }
    else if (strcmp(cmd, "send") == 0) {
        if (argc < 4) {
            printf("Usage: physicscoin send <to> <amount>\n");
            return 1;
        }
        return cmd_send(argv[2], atof(argv[3]));
    }
    else if (strcmp(cmd, "state") == 0) {
        return cmd_state();
    }
    else if (strcmp(cmd, "verify") == 0) {
        return cmd_verify();
    }
    else if (strcmp(cmd, "demo") == 0) {
        return cmd_demo();
    }
    else {
        printf("Unknown command: %s\n", cmd);
        print_usage();
        return 1;
    }
}
