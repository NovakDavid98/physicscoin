// test_consensus.c - Unit tests for Consensus Modules
// Tests vector clocks, ordering, checkpoints, validators

#include "../include/physicscoin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    printf("  %-40s ", name); \
    tests_run++; \
} while(0)

#define PASS() do { \
    printf("✓ PASS\n"); \
    tests_passed++; \
} while(0)

#define FAIL(msg) do { \
    printf("✗ FAIL: %s\n", msg); \
} while(0)

// ===== Test Double-Spend Prevention =====
void test_double_spend_prevention(void) {
    printf("\n═══ Double-Spend Prevention Tests ═══\n\n");
    
    PCKeypair alice, bob, charlie;
    pc_keypair_generate(&alice);
    pc_keypair_generate(&bob);
    pc_keypair_generate(&charlie);
    
    PCState state;
    pc_state_genesis(&state, alice.public_key, 100.0);
    pc_state_create_wallet(&state, bob.public_key, 0);
    pc_state_create_wallet(&state, charlie.public_key, 0);
    
    // Test 1: Valid transaction
    TEST("Valid TX succeeds");
    {
        PCTransaction tx = {0};
        memcpy(tx.from, alice.public_key, 32);
        memcpy(tx.to, bob.public_key, 32);
        tx.amount = 50.0;
        tx.nonce = 0;
        tx.timestamp = time(NULL);
        pc_transaction_sign(&tx, &alice);
        
        PCError err = pc_state_execute_tx(&state, &tx);
        if (err == PC_OK) PASS(); else FAIL(pc_strerror(err));
    }
    
    // Test 2: Insufficient funds
    TEST("Insufficient funds blocked");
    {
        PCTransaction tx = {0};
        memcpy(tx.from, alice.public_key, 32);
        memcpy(tx.to, charlie.public_key, 32);
        tx.amount = 100.0;  // Only has 50 left
        tx.nonce = 1;
        tx.timestamp = time(NULL);
        pc_transaction_sign(&tx, &alice);
        
        PCError err = pc_state_execute_tx(&state, &tx);
        if (err == PC_ERR_INSUFFICIENT_FUNDS) PASS(); else FAIL("Should fail");
    }
    
    // Test 3: Nonce replay blocked
    TEST("Nonce replay blocked");
    {
        PCTransaction tx = {0};
        memcpy(tx.from, alice.public_key, 32);
        memcpy(tx.to, bob.public_key, 32);
        tx.amount = 10.0;
        tx.nonce = 0;  // Already used!
        tx.timestamp = time(NULL);
        pc_transaction_sign(&tx, &alice);
        
        PCError err = pc_state_execute_tx(&state, &tx);
        if (err != PC_OK) PASS(); else FAIL("Replay should fail");
    }
    
    // Test 4: Wrong signature blocked (simplified crypto may not catch)
    TEST("Invalid signature check");
    {
        PCTransaction tx = {0};
        memcpy(tx.from, alice.public_key, 32);
        memcpy(tx.to, bob.public_key, 32);
        tx.amount = 10.0;
        tx.nonce = 1;
        tx.timestamp = time(NULL);
        pc_transaction_sign(&tx, &bob);  // Wrong key!
        
        PCError err = pc_state_execute_tx(&state, &tx);
        // Simplified crypto may pass sig check but fail elsewhere
        if (err != PC_OK) PASS(); else FAIL("Should reject");
    }
    
    // Test 5: Conservation maintained
    TEST("Conservation after TXs");
    {
        double total = 0;
        for (uint32_t i = 0; i < state.num_wallets; i++) {
            total += state.wallets[i].energy;
        }
        if (total == 100.0) PASS(); else FAIL("Conservation violated");
    }
    
    pc_state_free(&state);
}

// ===== Test State Serialization =====
void test_serialization(void) {
    printf("\n═══ Serialization Tests ═══\n\n");
    
    PCKeypair kp;
    pc_keypair_generate(&kp);
    
    // Test 1: Round-trip state
    TEST("State serialize/deserialize");
    {
        PCState original;
        pc_state_genesis(&original, kp.public_key, 1000.0);
        
        uint8_t buffer[4096];
        size_t size = pc_state_serialize(&original, buffer, sizeof(buffer));
        
        PCState restored;
        memset(&restored, 0, sizeof(PCState));
        PCError err = pc_state_deserialize(&restored, buffer, size);
        
        if (err == PC_OK && 
            restored.total_supply == original.total_supply &&
            memcmp(restored.state_hash, original.state_hash, 32) == 0) {
            PASS();
        } else {
            FAIL("Round-trip mismatch");
        }
        
        pc_state_free(&original);
        if (restored.wallets) pc_state_free(&restored);
    }
    
    // Test 2: State hash determinism
    TEST("Deterministic state hash");
    {
        PCState s1, s2;
        pc_state_genesis(&s1, kp.public_key, 500.0);
        pc_state_genesis(&s2, kp.public_key, 500.0);
        
        if (memcmp(s1.state_hash, s2.state_hash, 32) == 0) PASS();
        else FAIL("Hash not deterministic");
        
        pc_state_free(&s1);
        pc_state_free(&s2);
    }
}

// ===== Test Streaming Payments =====
void test_streaming(void) {
    printf("\n═══ Streaming Payment Tests ═══\n\n");
    
    PCKeypair alice, bob;
    pc_keypair_generate(&alice);
    pc_keypair_generate(&bob);
    
    PCState state;
    pc_state_genesis(&state, alice.public_key, 1000.0);
    pc_state_create_wallet(&state, bob.public_key, 0);
    
    // Test: Multiple small TXs
    TEST("100 micro-transactions");
    {
        int success = 0;
        for (int i = 0; i < 100; i++) {
            PCTransaction tx = {0};
            memcpy(tx.from, alice.public_key, 32);
            memcpy(tx.to, bob.public_key, 32);
            tx.amount = 1.0;
            tx.nonce = i;
            tx.timestamp = time(NULL);
            pc_transaction_sign(&tx, &alice);
            
            if (pc_state_execute_tx(&state, &tx) == PC_OK) success++;
        }
        
        if (success == 100) PASS(); else FAIL("Some TXs failed");
    }
    
    // Test: Final balances
    TEST("Final balances correct");
    {
        double alice_bal = pc_state_get_wallet(&state, alice.public_key)->energy;
        double bob_bal = pc_state_get_wallet(&state, bob.public_key)->energy;
        
        if (alice_bal == 900.0 && bob_bal == 100.0) PASS();
        else FAIL("Wrong balances");
    }
    
    pc_state_free(&state);
}

// ===== Test Performance =====
void test_performance(void) {
    printf("\n═══ Performance Tests ═══\n\n");
    
    PCKeypair alice, bob;
    pc_keypair_generate(&alice);
    pc_keypair_generate(&bob);
    
    PCState state;
    pc_state_genesis(&state, alice.public_key, 1000000.0);
    pc_state_create_wallet(&state, bob.public_key, 0);
    
    TEST("10000 TXs performance");
    {
        clock_t start = clock();
        
        for (int i = 0; i < 10000; i++) {
            PCTransaction tx = {0};
            memcpy(tx.from, alice.public_key, 32);
            memcpy(tx.to, bob.public_key, 32);
            tx.amount = 1.0;
            tx.nonce = i;
            tx.timestamp = time(NULL);
            pc_transaction_sign(&tx, &alice);
            pc_state_execute_tx(&state, &tx);
        }
        
        clock_t end = clock();
        double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
        double tps = 10000.0 / elapsed;
        
        printf("%.0f tx/sec ", tps);
        if (tps > 5000) PASS(); else FAIL("Too slow");
    }
    
    pc_state_free(&state);
}

int main(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║             PHYSICSCOIN UNIT TESTS                            ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    
    test_double_spend_prevention();
    test_serialization();
    test_streaming();
    test_performance();
    
    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf("  Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}
