// main.c - PhysicsCoin CLI v2.0
// Full command-line interface with all features

#include "../include/physicscoin.h"
#include "../crypto/sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define STATE_FILE "state.pcs"
#define WALLET_FILE "wallet.pcw"

// ============ Forward declarations for new features ============

// From proofs.c
typedef struct {
    uint8_t state_hash[32];
    uint8_t wallet_pubkey[32];
    double balance;
    uint64_t nonce;
    uint64_t timestamp;
    uint8_t proof_hash[32];
} PCBalanceProof;

PCError pc_proof_generate(const PCState* state, const uint8_t* pubkey, PCBalanceProof* proof);
PCError pc_proof_verify(const PCState* state, const PCBalanceProof* proof);
PCError pc_proof_save(const PCBalanceProof* proof, const char* filename);
PCError pc_proof_load(PCBalanceProof* proof, const char* filename);
void pc_proof_print(const PCBalanceProof* proof);

// From streams.c
PCError pc_stream_open(PCState* state, const PCKeypair* payer_kp,
                       const uint8_t* receiver, double rate_per_second,
                       uint8_t* stream_id_out);
double pc_stream_accumulated(const uint8_t* stream_id);
PCError pc_stream_settle(PCState* state, const uint8_t* stream_id, const PCKeypair* payer_kp);
PCError pc_stream_close(PCState* state, const uint8_t* stream_id, const PCKeypair* payer_kp);
PCError pc_stream_info(const uint8_t* stream_id, double* rate, double* total, int* active);
void pc_stream_print(const uint8_t* stream_id);

// From delta.c
typedef struct {
    uint8_t pubkey[32];
    double old_balance;
    double new_balance;
    uint64_t old_nonce;
    uint64_t new_nonce;
} PCWalletDelta;

typedef struct {
    uint8_t prev_hash[32];
    uint8_t new_hash[32];
    uint64_t prev_timestamp;
    uint64_t new_timestamp;
    uint32_t num_changes;
    PCWalletDelta changes[1000];
} PCStateDelta;

PCError pc_delta_compute(const PCState* before, const PCState* after, PCStateDelta* delta);
void pc_delta_print(const PCStateDelta* delta);
size_t pc_delta_size(const PCStateDelta* delta);

// ============ Print functions ============

void print_usage(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║             PHYSICSCOIN CLI v%s                           ║\n", PHYSICSCOIN_VERSION);
    printf("║          Physics-Based Cryptocurrency                         ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("Usage: physicscoin <command> [options]\n\n");
    
    printf("BASIC COMMANDS:\n");
    printf("  init <supply>              Create genesis with initial supply\n");
    printf("  wallet create              Generate new wallet keypair\n");
    printf("  balance <address>          Check wallet balance\n");
    printf("  send <to> <amount>         Send coins\n");
    printf("  state                      Display universe state\n");
    printf("  verify                     Verify energy conservation\n");
    printf("  demo                       Run interactive demo\n");
    printf("\n");
    
    printf("PROOF COMMANDS:\n");
    printf("  prove <address>            Generate balance proof\n");
    printf("  verify-proof <file>        Verify a balance proof\n");
    printf("\n");
    
    printf("STREAMING COMMANDS:\n");
    printf("  stream open <to> <rate>    Open payment stream (rate/sec)\n");
    printf("  stream info <id>           Show stream info\n");
    printf("  stream settle <id>         Settle accumulated payments\n");
    printf("  stream close <id>          Close stream permanently\n");
    printf("  stream demo                Run streaming payment demo\n");
    printf("\n");
    
    printf("DELTA COMMANDS:\n");
    printf("  delta <file1> <file2>      Compute delta between states\n");
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

// ============ Basic commands ============

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
    if (fread(&kp, sizeof(kp), 1, wf) != 1) {
        fclose(wf);
        printf("Error: Failed to read wallet\n");
        return 1;
    }
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
    
    char from_hex[65];
    pc_pubkey_to_hex(tx.from, from_hex);
    printf("Sending %.8f from %.8s... to %.8s...\n", amount, from_hex, to_addr);
    
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

// ============ Proof commands ============

int cmd_prove(const char* address) {
    PCState state;
    if (pc_state_load(&state, STATE_FILE) != PC_OK) {
        printf("Error: Cannot load state\n");
        return 1;
    }
    
    uint8_t pubkey[PHYSICSCOIN_KEY_SIZE];
    if (pc_hex_to_pubkey(address, pubkey) != PC_OK) {
        printf("Error: Invalid address format\n");
        pc_state_free(&state);
        return 1;
    }
    
    PCBalanceProof proof;
    PCError err = pc_proof_generate(&state, pubkey, &proof);
    if (err != PC_OK) {
        printf("Error: %s\n", pc_strerror(err));
        pc_state_free(&state);
        return 1;
    }
    
    char filename[128];
    snprintf(filename, sizeof(filename), "proof_%.8s.pcp", address);
    
    err = pc_proof_save(&proof, filename);
    if (err != PC_OK) {
        printf("Error saving proof\n");
        pc_state_free(&state);
        return 1;
    }
    
    printf("✓ Balance proof generated!\n\n");
    pc_proof_print(&proof);
    printf("\nSaved to: %s\n", filename);
    
    pc_state_free(&state);
    return 0;
}

int cmd_verify_proof(const char* filename) {
    PCBalanceProof proof;
    if (pc_proof_load(&proof, filename) != PC_OK) {
        printf("Error: Cannot load proof file\n");
        return 1;
    }
    
    PCState state;
    if (pc_state_load(&state, STATE_FILE) != PC_OK) {
        printf("Error: Cannot load state\n");
        return 1;
    }
    
    printf("Verifying proof...\n");
    pc_proof_print(&proof);
    printf("\n");
    
    PCError err = pc_proof_verify(&state, &proof);
    if (err == PC_OK) {
        printf("✓ Proof VALID!\n");
        printf("  The wallet had %.8f at the claimed state.\n", proof.balance);
    } else {
        printf("✗ Proof INVALID: %s\n", pc_strerror(err));
    }
    
    pc_state_free(&state);
    return (err == PC_OK) ? 0 : 1;
}

// ============ Stream commands ============

int cmd_stream_open(const char* to_addr, double rate) {
    PCKeypair kp;
    FILE* wf = fopen(WALLET_FILE, "rb");
    if (!wf) {
        printf("Error: No wallet found\n");
        return 1;
    }
    if (fread(&kp, sizeof(kp), 1, wf) != 1) {
        fclose(wf);
        return 1;
    }
    fclose(wf);
    
    PCState state;
    if (pc_state_load(&state, STATE_FILE) != PC_OK) {
        printf("Error: Cannot load state\n");
        return 1;
    }
    
    uint8_t to_pubkey[PHYSICSCOIN_KEY_SIZE];
    if (pc_hex_to_pubkey(to_addr, to_pubkey) != PC_OK) {
        printf("Error: Invalid recipient address\n");
        pc_state_free(&state);
        return 1;
    }
    
    uint8_t stream_id[16];
    PCError err = pc_stream_open(&state, &kp, to_pubkey, rate, stream_id);
    if (err != PC_OK) {
        printf("Error: %s\n", pc_strerror(err));
        pc_state_free(&state);
        return 1;
    }
    
    printf("✓ Payment stream opened!\n");
    printf("Stream ID: ");
    for (int i = 0; i < 16; i++) printf("%02x", stream_id[i]);
    printf("\n");
    printf("Rate: %.12f /sec\n", rate);
    printf("Recipient: %.16s...\n", to_addr);
    
    pc_state_save(&state, STATE_FILE);
    pc_state_free(&state);
    return 0;
}

int cmd_stream_demo(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║           STREAMING PAYMENTS DEMO                             ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
    
    PCKeypair alice, bob;
    pc_keypair_generate(&alice);
    pc_keypair_generate(&bob);
    
    char alice_addr[65], bob_addr[65];
    pc_pubkey_to_hex(alice.public_key, alice_addr);
    pc_pubkey_to_hex(bob.public_key, bob_addr);
    
    printf("Alice: %.16s...\n", alice_addr);
    printf("Bob:   %.16s...\n", bob_addr);
    
    PCState state;
    pc_state_genesis(&state, alice.public_key, 1000.0);
    pc_state_create_wallet(&state, bob.public_key, 0);
    
    printf("\n═══ Initial State ═══\n");
    printf("Alice: %.8f\n", pc_state_get_wallet(&state, alice.public_key)->energy);
    printf("Bob:   %.8f\n", pc_state_get_wallet(&state, bob.public_key)->energy);
    
    // Open stream: 1 coin per second
    uint8_t stream_id[16];
    pc_stream_open(&state, &alice, bob.public_key, 1.0, stream_id);
    
    printf("\n═══ Stream Opened: 1.0 coin/sec ═══\n");
    printf("Stream ID: ");
    for (int i = 0; i < 8; i++) printf("%02x", stream_id[i]);
    printf("...\n");
    
    // Simulate time passing
    printf("\n═══ Simulating 5 seconds ═══\n");
    for (int i = 1; i <= 5; i++) {
        sleep(1);
        double acc = pc_stream_accumulated(stream_id);
        printf("t=%d: Accumulated: %.8f\n", i, acc);
    }
    
    // Settle
    printf("\n═══ Settlement ═══\n");
    PCError err = pc_stream_settle(&state, stream_id, &alice);
    printf("Settlement: %s\n", err == PC_OK ? "✓ Success" : pc_strerror(err));
    
    printf("\n═══ Final Balances ═══\n");
    printf("Alice: %.8f\n", pc_state_get_wallet(&state, alice.public_key)->energy);
    printf("Bob:   %.8f\n", pc_state_get_wallet(&state, bob.public_key)->energy);
    
    // Close stream
    pc_stream_close(&state, stream_id, &alice);
    printf("\n✓ Stream closed\n");
    
    // Verify conservation
    err = pc_state_verify_conservation(&state);
    printf("Conservation: %s\n\n", err == PC_OK ? "✓ VERIFIED" : "✗ FAILED");
    
    pc_state_free(&state);
    return 0;
}

// ============ Delta commands ============

int cmd_delta(const char* file1, const char* file2) {
    PCState state1, state2;
    
    if (pc_state_load(&state1, file1) != PC_OK) {
        printf("Error: Cannot load %s\n", file1);
        return 1;
    }
    
    if (pc_state_load(&state2, file2) != PC_OK) {
        printf("Error: Cannot load %s\n", file2);
        pc_state_free(&state1);
        return 1;
    }
    
    PCStateDelta delta;
    PCError err = pc_delta_compute(&state1, &state2, &delta);
    if (err != PC_OK) {
        printf("Error computing delta: %s\n", pc_strerror(err));
        pc_state_free(&state1);
        pc_state_free(&state2);
        return 1;
    }
    
    printf("═══ State Delta ═══\n\n");
    pc_delta_print(&delta);
    
    printf("\nDelta size: %zu bytes\n", pc_delta_size(&delta));
    
    // Compare to full state sizes
    uint8_t buf1[65536], buf2[65536];
    size_t s1 = pc_state_serialize(&state1, buf1, sizeof(buf1));
    size_t s2 = pc_state_serialize(&state2, buf2, sizeof(buf2));
    
    printf("State 1 size: %zu bytes\n", s1);
    printf("State 2 size: %zu bytes\n", s2);
    printf("Savings: %.1f%%\n", 100.0 * (1.0 - (double)pc_delta_size(&delta) / s2));
    
    pc_state_free(&state1);
    pc_state_free(&state2);
    return 0;
}

// ============ Demo ============

int cmd_demo(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║             PHYSICSCOIN INTERACTIVE DEMO                      ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
    
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
    
    printf("═══ BALANCE PROOF DEMO ═══\n");
    PCBalanceProof proof;
    pc_proof_generate(&state, alice.public_key, &proof);
    printf("Generated proof for Alice:\n");
    printf("  Balance: %.8f at state ", proof.balance);
    for (int i = 0; i < 8; i++) printf("%02x", proof.state_hash[i]);
    printf("...\n");
    printf("  Proof hash: ");
    for (int i = 0; i < 8; i++) printf("%02x", proof.proof_hash[i]);
    printf("...\n");
    printf("  Verification: %s\n", pc_proof_verify(&state, &proof) == PC_OK ? "✓ VALID" : "✗ INVALID");
    
    printf("\n═══ CONSERVATION VERIFICATION ═══\n");
    err = pc_state_verify_conservation(&state);
    printf("Energy Conservation: %s\n", err == PC_OK ? "✓ VERIFIED" : "✗ FAILED");
    
    uint8_t buffer[4096];
    size_t size = pc_state_serialize(&state, buffer, sizeof(buffer));
    printf("\n═══ STATE COMPRESSION ═══\n");
    printf("State size: %zu bytes\n", size);
    printf("Compression ratio: %.0f million : 1 vs Bitcoin\n", 500.0 * 1024 * 1024 * 1024 / size / 1000000);
    
    pc_state_free(&state);
    printf("\n");
    return 0;
}

// ============ Main entry point ============

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return 0;
    }
    
    const char* cmd = argv[1];
    
    // Basic commands
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
    // Proof commands
    else if (strcmp(cmd, "prove") == 0) {
        if (argc < 3) {
            printf("Usage: physicscoin prove <address>\n");
            return 1;
        }
        return cmd_prove(argv[2]);
    }
    else if (strcmp(cmd, "verify-proof") == 0) {
        if (argc < 3) {
            printf("Usage: physicscoin verify-proof <file>\n");
            return 1;
        }
        return cmd_verify_proof(argv[2]);
    }
    // Stream commands
    else if (strcmp(cmd, "stream") == 0) {
        if (argc < 3) {
            printf("Usage: physicscoin stream <open|info|settle|close|demo>\n");
            return 1;
        }
        if (strcmp(argv[2], "open") == 0) {
            if (argc < 5) {
                printf("Usage: physicscoin stream open <to> <rate>\n");
                return 1;
            }
            return cmd_stream_open(argv[3], atof(argv[4]));
        }
        else if (strcmp(argv[2], "demo") == 0) {
            return cmd_stream_demo();
        }
        printf("Unknown stream command\n");
        return 1;
    }
    // Delta commands
    else if (strcmp(cmd, "delta") == 0) {
        if (argc < 4) {
            printf("Usage: physicscoin delta <state1.pcs> <state2.pcs>\n");
            return 1;
        }
        return cmd_delta(argv[2], argv[3]);
    }
    else {
        printf("Unknown command: %s\n", cmd);
        print_usage();
        return 1;
    }
}
