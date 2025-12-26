// test_conservation.c - Comprehensive conservation law testing
// Stress tests for energy conservation across many transactions

#include "../include/physicscoin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define NUM_WALLETS 100
#define NUM_TRANSACTIONS 1000
#define INITIAL_SUPPLY 1000000.0

static PCKeypair wallets[NUM_WALLETS];
static int tests_passed = 0;
static int tests_failed = 0;

void test_start(const char* name) {
    printf("TEST: %-50s ", name);
}

void test_pass(void) {
    printf("✓ PASS\n");
    tests_passed++;
}

void test_fail(const char* reason) {
    printf("✗ FAIL: %s\n", reason);
    tests_failed++;
}

// Test 1: Genesis conservation
void test_genesis_conservation(void) {
    test_start("Genesis creates exact supply");
    
    PCState state;
    pc_keypair_generate(&wallets[0]);
    pc_state_genesis(&state, wallets[0].public_key, INITIAL_SUPPLY);
    
    double sum = 0;
    for (uint32_t i = 0; i < state.num_wallets; i++) {
        sum += state.wallets[i].energy;
    }
    
    if (fabs(sum - INITIAL_SUPPLY) < 1e-10) {
        test_pass();
    } else {
        test_fail("Supply mismatch");
    }
    
    pc_state_free(&state);
}

// Test 2: Single transaction conservation
void test_single_tx_conservation(void) {
    test_start("Single transaction preserves energy");
    
    PCState state;
    pc_keypair_generate(&wallets[0]);
    pc_keypair_generate(&wallets[1]);
    
    pc_state_genesis(&state, wallets[0].public_key, 1000.0);
    pc_state_create_wallet(&state, wallets[1].public_key, 0);
    
    double before = state.total_supply;
    
    PCTransaction tx = {0};
    memcpy(tx.from, wallets[0].public_key, PHYSICSCOIN_KEY_SIZE);
    memcpy(tx.to, wallets[1].public_key, PHYSICSCOIN_KEY_SIZE);
    tx.amount = 100.0;
    tx.nonce = 0;
    tx.timestamp = time(NULL);
    pc_transaction_sign(&tx, &wallets[0]);
    
    pc_state_execute_tx(&state, &tx);
    
    double after = 0;
    for (uint32_t i = 0; i < state.num_wallets; i++) {
        after += state.wallets[i].energy;
    }
    
    if (fabs(before - after) < 1e-10) {
        test_pass();
    } else {
        test_fail("Energy leaked");
    }
    
    pc_state_free(&state);
}

// Test 3: Mass transaction stress test
void test_mass_transactions(void) {
    test_start("1000 random transactions preserve energy");
    
    PCState state;
    
    // Create all wallets
    for (int i = 0; i < NUM_WALLETS; i++) {
        pc_keypair_generate(&wallets[i]);
    }
    
    pc_state_genesis(&state, wallets[0].public_key, INITIAL_SUPPLY);
    for (int i = 1; i < NUM_WALLETS; i++) {
        pc_state_create_wallet(&state, wallets[i].public_key, 0);
    }
    
    // Track nonces
    uint64_t nonces[NUM_WALLETS] = {0};
    
    // Execute random transactions
    srand(42);
    int successful = 0;
    
    for (int t = 0; t < NUM_TRANSACTIONS; t++) {
        int from_idx = rand() % NUM_WALLETS;
        int to_idx = rand() % NUM_WALLETS;
        if (from_idx == to_idx) continue;
        
        PCWallet* from_wallet = pc_state_get_wallet(&state, wallets[from_idx].public_key);
        if (!from_wallet || from_wallet->energy < 1.0) continue;
        
        double amount = ((double)(rand() % 100) + 1.0);
        if (amount > from_wallet->energy) amount = from_wallet->energy * 0.5;
        
        PCTransaction tx = {0};
        memcpy(tx.from, wallets[from_idx].public_key, PHYSICSCOIN_KEY_SIZE);
        memcpy(tx.to, wallets[to_idx].public_key, PHYSICSCOIN_KEY_SIZE);
        tx.amount = amount;
        tx.nonce = nonces[from_idx];
        tx.timestamp = time(NULL);
        pc_transaction_sign(&tx, &wallets[from_idx]);
        
        if (pc_state_execute_tx(&state, &tx) == PC_OK) {
            nonces[from_idx]++;
            successful++;
        }
    }
    
    // Verify conservation
    double final_sum = 0;
    for (uint32_t i = 0; i < state.num_wallets; i++) {
        final_sum += state.wallets[i].energy;
    }
    
    double error = fabs(final_sum - INITIAL_SUPPLY);
    
    if (error < 1e-9) {
        test_pass();
        printf("       (Executed %d transactions, error: %.2e)\n", successful, error);
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "Conservation error: %.12f", error);
        test_fail(buf);
    }
    
    pc_state_free(&state);
}

// Test 4: Insufficient funds rejection
void test_insufficient_funds(void) {
    test_start("Insufficient funds rejected");
    
    PCState state;
    pc_keypair_generate(&wallets[0]);
    pc_keypair_generate(&wallets[1]);
    
    pc_state_genesis(&state, wallets[0].public_key, 100.0);
    pc_state_create_wallet(&state, wallets[1].public_key, 0);
    
    PCTransaction tx = {0};
    memcpy(tx.from, wallets[0].public_key, PHYSICSCOIN_KEY_SIZE);
    memcpy(tx.to, wallets[1].public_key, PHYSICSCOIN_KEY_SIZE);
    tx.amount = 200.0;  // More than available
    tx.nonce = 0;
    tx.timestamp = time(NULL);
    pc_transaction_sign(&tx, &wallets[0]);
    
    PCError err = pc_state_execute_tx(&state, &tx);
    
    if (err == PC_ERR_INSUFFICIENT_FUNDS) {
        test_pass();
    } else {
        test_fail("Should have rejected");
    }
    
    pc_state_free(&state);
}

// Test 5: Negative amount rejection
void test_negative_amount(void) {
    test_start("Negative/zero amount rejected");
    
    PCState state;
    pc_keypair_generate(&wallets[0]);
    pc_keypair_generate(&wallets[1]);
    
    pc_state_genesis(&state, wallets[0].public_key, 100.0);
    pc_state_create_wallet(&state, wallets[1].public_key, 0);
    
    PCTransaction tx = {0};
    memcpy(tx.from, wallets[0].public_key, PHYSICSCOIN_KEY_SIZE);
    memcpy(tx.to, wallets[1].public_key, PHYSICSCOIN_KEY_SIZE);
    tx.amount = -50.0;
    tx.nonce = 0;
    tx.timestamp = time(NULL);
    pc_transaction_sign(&tx, &wallets[0]);
    
    PCError err = pc_state_execute_tx(&state, &tx);
    
    if (err == PC_ERR_INVALID_AMOUNT) {
        test_pass();
    } else {
        test_fail("Should have rejected negative amount");
    }
    
    pc_state_free(&state);
}

// Test 6: Nonce replay protection
void test_replay_protection(void) {
    test_start("Replay attack blocked by nonce");
    
    PCState state;
    pc_keypair_generate(&wallets[0]);
    pc_keypair_generate(&wallets[1]);
    
    pc_state_genesis(&state, wallets[0].public_key, 1000.0);
    pc_state_create_wallet(&state, wallets[1].public_key, 0);
    
    PCTransaction tx = {0};
    memcpy(tx.from, wallets[0].public_key, PHYSICSCOIN_KEY_SIZE);
    memcpy(tx.to, wallets[1].public_key, PHYSICSCOIN_KEY_SIZE);
    tx.amount = 100.0;
    tx.nonce = 0;
    tx.timestamp = time(NULL);
    pc_transaction_sign(&tx, &wallets[0]);
    
    // First execution should succeed
    PCError err1 = pc_state_execute_tx(&state, &tx);
    
    // Replay should fail (nonce already used)
    PCError err2 = pc_state_execute_tx(&state, &tx);
    
    if (err1 == PC_OK && err2 != PC_OK) {
        test_pass();
    } else {
        test_fail("Replay not blocked");
    }
    
    pc_state_free(&state);
}

// Test 7: Self-transfer
void test_self_transfer(void) {
    test_start("Self-transfer preserves balance");
    
    PCState state;
    pc_keypair_generate(&wallets[0]);
    
    pc_state_genesis(&state, wallets[0].public_key, 500.0);
    
    PCTransaction tx = {0};
    memcpy(tx.from, wallets[0].public_key, PHYSICSCOIN_KEY_SIZE);
    memcpy(tx.to, wallets[0].public_key, PHYSICSCOIN_KEY_SIZE);
    tx.amount = 100.0;
    tx.nonce = 0;
    tx.timestamp = time(NULL);
    pc_transaction_sign(&tx, &wallets[0]);
    
    PCError err = pc_state_execute_tx(&state, &tx);
    
    PCWallet* w = pc_state_get_wallet(&state, wallets[0].public_key);
    
    if (err == PC_OK && fabs(w->energy - 500.0) < 1e-10) {
        test_pass();
    } else {
        test_fail("Balance changed on self-transfer");
    }
    
    pc_state_free(&state);
}

// Test 8: Verify function accuracy
void test_verify_function(void) {
    test_start("pc_state_verify_conservation works");
    
    PCState state;
    pc_keypair_generate(&wallets[0]);
    
    pc_state_genesis(&state, wallets[0].public_key, 1000.0);
    
    PCError err = pc_state_verify_conservation(&state);
    
    if (err == PC_OK) {
        test_pass();
    } else {
        test_fail("Verify failed on valid state");
    }
    
    pc_state_free(&state);
}

int main(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║         PHYSICSCOIN CONSERVATION TEST SUITE                   ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
    
    test_genesis_conservation();
    test_single_tx_conservation();
    test_mass_transactions();
    test_insufficient_funds();
    test_negative_amount();
    test_replay_protection();
    test_self_transfer();
    test_verify_function();
    
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("RESULTS: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    return tests_failed > 0 ? 1 : 0;
}
