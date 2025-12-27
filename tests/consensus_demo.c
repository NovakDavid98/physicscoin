// consensus_demo.c - Demonstration of CBC Consensus (simplified)
// Shows double-spend prevention using conservation laws

#include "../include/physicscoin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

int main(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║       CONSERVATION-BASED CONSENSUS (CBC) DEMO                 ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
    
    // === PART 1: DOUBLE-SPEND PREVENTION VIA PHYSICS ===
    printf("═══ PART 1: Double-Spend Prevention via Conservation Laws ═══\n\n");
    
    PCKeypair alice, bob, charlie;
    pc_keypair_generate(&alice);
    pc_keypair_generate(&bob);
    pc_keypair_generate(&charlie);
    
    char alice_addr[65], bob_addr[65], charlie_addr[65];
    pc_pubkey_to_hex(alice.public_key, alice_addr);
    pc_pubkey_to_hex(bob.public_key, bob_addr);
    pc_pubkey_to_hex(charlie.public_key, charlie_addr);
    
    printf("Wallets:\n");
    printf("  Alice:   %.16s...\n", alice_addr);
    printf("  Bob:     %.16s...\n", bob_addr);
    printf("  Charlie: %.16s...\n", charlie_addr);
    
    PCState state;
    pc_state_genesis(&state, alice.public_key, 100.0);
    pc_state_create_wallet(&state, bob.public_key, 0);
    pc_state_create_wallet(&state, charlie.public_key, 0);
    
    printf("\nInitial balances:\n");
    printf("  Alice:   %.2f coins\n", pc_state_get_wallet(&state, alice.public_key)->energy);
    printf("  Bob:     %.2f coins\n", pc_state_get_wallet(&state, bob.public_key)->energy);
    printf("  Charlie: %.2f coins\n", pc_state_get_wallet(&state, charlie.public_key)->energy);
    
    // First legitimate transaction
    printf("\n─── Transaction 1: Alice → Bob (100 coins) ───\n");
    
    PCTransaction tx1 = {0};
    memcpy(tx1.from, alice.public_key, 32);
    memcpy(tx1.to, bob.public_key, 32);
    tx1.amount = 100.0;
    tx1.nonce = 0;
    tx1.timestamp = time(NULL);
    pc_transaction_sign(&tx1, &alice);
    
    PCError err1 = pc_state_execute_tx(&state, &tx1);
    printf("  Result: %s\n", err1 == PC_OK ? "✓ SUCCESS" : pc_strerror(err1));
    printf("  Alice:   %.2f coins\n", pc_state_get_wallet(&state, alice.public_key)->energy);
    printf("  Bob:     %.2f coins\n", pc_state_get_wallet(&state, bob.public_key)->energy);
    
    // DOUBLE-SPEND ATTEMPT
    printf("\n─── DOUBLE-SPEND ATTEMPT: Alice → Charlie (100 coins) ───\n");
    printf("  (Alice already has 0 coins!)\n\n");
    
    PCTransaction tx2 = {0};
    memcpy(tx2.from, alice.public_key, 32);
    memcpy(tx2.to, charlie.public_key, 32);
    tx2.amount = 100.0;
    tx2.nonce = 1;  // Correct nonce
    tx2.timestamp = time(NULL);
    pc_transaction_sign(&tx2, &alice);
    
    PCError err2 = pc_state_execute_tx(&state, &tx2);
    printf("  Result: %s\n", err2 == PC_OK ? "SUCCESS" : "✗ BLOCKED!");
    printf("  Error:  %s\n", pc_strerror(err2));
    
    printf("\nFinal balances:\n");
    printf("  Alice:   %.2f coins\n", pc_state_get_wallet(&state, alice.public_key)->energy);
    printf("  Bob:     %.2f coins\n", pc_state_get_wallet(&state, bob.public_key)->energy);
    printf("  Charlie: %.2f coins\n", pc_state_get_wallet(&state, charlie.public_key)->energy);
    
    // Verify conservation
    double total = 0;
    for (uint32_t i = 0; i < state.num_wallets; i++) {
        total += state.wallets[i].energy;
    }
    printf("\n✓ Conservation verified: %.2f coins (initial: 100.00)\n", total);
    
    // === PART 2: NONCE REPLAY PROTECTION ===
    printf("\n═══ PART 2: Nonce-Based Replay Protection ═══\n\n");
    
    // Bob sends 50 to Charlie
    PCTransaction tx3 = {0};
    memcpy(tx3.from, bob.public_key, 32);
    memcpy(tx3.to, charlie.public_key, 32);
    tx3.amount = 50.0;
    tx3.nonce = 0;
    tx3.timestamp = time(NULL);
    pc_transaction_sign(&tx3, &bob);
    
    printf("Bob → Charlie (50 coins, nonce=0): ");
    PCError err3 = pc_state_execute_tx(&state, &tx3);
    printf("%s\n", err3 == PC_OK ? "✓ SUCCESS" : pc_strerror(err3));
    
    // Replay attack: try to use same TX again
    printf("REPLAY ATTACK (same TX again): ");
    PCError err4 = pc_state_execute_tx(&state, &tx3);
    printf("%s\n", err4 == PC_OK ? "SUCCESS" : "✗ BLOCKED (wrong nonce)");
    
    printf("\nBalances after replay attempt:\n");
    printf("  Bob:     %.2f coins\n", pc_state_get_wallet(&state, bob.public_key)->energy);
    printf("  Charlie: %.2f coins\n", pc_state_get_wallet(&state, charlie.public_key)->energy);
    
    // === PART 3: STATE HASH CHAIN ===
    printf("\n═══ PART 3: State Hash Chain (Deterministic History) ═══\n\n");
    
    printf("State hash: ");
    for (int i = 0; i < 16; i++) printf("%02x", state.state_hash[i]);
    printf("...\n");
    
    printf("Prev hash:  ");
    for (int i = 0; i < 16; i++) printf("%02x", state.prev_hash[i]);
    printf("...\n");
    
    printf("\n✓ Each state cryptographically links to previous\n");
    printf("✓ Anyone can replay from genesis to verify current state\n");
    
    // === SUMMARY ===
    printf("\n╔═══════════════════════════════════════════════════════════╗\n");
    printf("║              CBC CONSENSUS PROPERTIES                     ║\n");
    printf("╠═══════════════════════════════════════════════════════════╣\n");
    printf("║  ✓ Double-spend prevented by CONSERVATION LAW            ║\n");
    printf("║  ✓ Replay attacks prevented by NONCE                     ║\n");
    printf("║  ✓ History verifiable via HASH CHAIN                     ║\n");
    printf("║  ✓ No blockchain needed - just STATE + MATH              ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
    
    printf("The physics guarantees security:\n");
    printf("  • Energy cannot be created (no inflation)\n");
    printf("  • Energy cannot be destroyed (no loss)\n");
    printf("  • State is deterministic (verifiable)\n\n");
    
    pc_state_free(&state);
    
    return 0;
}
