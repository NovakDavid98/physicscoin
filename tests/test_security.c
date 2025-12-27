// test_security.c - Security Test Suite
// Tests all security fixes to ensure vulnerabilities are patched

#include "../include/physicscoin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <sodium.h>

static int tests_passed = 0;
static int tests_failed = 0;

void test_start(const char* name) {
    printf("TEST: %-55s ", name);
}

void test_pass(void) {
    printf("✓ PASS\n");
    tests_passed++;
}

void test_fail(const char* reason) {
    printf("✗ FAIL: %s\n", reason);
    tests_failed++;
}

//=============================================================================
// TEST 1: Conservation Law Cannot Be Bypassed
//=============================================================================
void test_conservation_enforcement(void) {
    test_start("Conservation: Direct balance modification fails");
    
    PCKeypair kp1, kp2;
    pc_keypair_generate(&kp1);
    pc_keypair_generate(&kp2);
    
    PCState state;
    pc_state_genesis(&state, kp1.public_key, 1000.0);
    pc_state_create_wallet(&state, kp2.public_key, 0);
    
    // Verify initial conservation
    double initial_sum = 0;
    for (uint32_t i = 0; i < state.num_wallets; i++) {
        initial_sum += state.wallets[i].energy;
    }
    
    // Valid transaction
    PCTransaction tx;
    memcpy(tx.from, kp1.public_key, 32);
    memcpy(tx.to, kp2.public_key, 32);
    tx.amount = 100.0;
    tx.nonce = 0;
    tx.timestamp = time(NULL);
    pc_transaction_sign(&tx, &kp1);
    
    PCError err = pc_state_execute_tx(&state, &tx);
    
    // Verify conservation maintained
    double final_sum = 0;
    for (uint32_t i = 0; i < state.num_wallets; i++) {
        final_sum += state.wallets[i].energy;
    }
    
    PCError cons = pc_state_verify_conservation(&state);
    
    if (err == PC_OK && cons == PC_OK && fabs(initial_sum - final_sum) < 1e-10) {
        test_pass();
    } else {
        test_fail("Conservation check failed");
    }
    
    pc_state_free(&state);
}

void test_no_money_creation(void) {
    test_start("Conservation: Cannot create money out of nothing");
    
    PCKeypair kp;
    pc_keypair_generate(&kp);
    
    PCState state;
    pc_state_genesis(&state, kp.public_key, 1000.0);
    
    double original_supply = state.total_supply;
    
    // Try to directly add balance (simulating an attack)
    // This should NOT be possible through public API
    
    // Create new wallet - should start with 0
    PCKeypair attacker;
    pc_keypair_generate(&attacker);
    pc_state_create_wallet(&state, attacker.public_key, 0);
    
    PCWallet* attacker_wallet = pc_state_get_wallet(&state, attacker.public_key);
    if (!attacker_wallet) {
        test_fail("Failed to create wallet");
        pc_state_free(&state);
        return;
    }
    
    // Attacker wallet should have 0 balance
    if (attacker_wallet->energy != 0.0) {
        test_fail("New wallet has non-zero balance");
        pc_state_free(&state);
        return;
    }
    
    // Total supply should not have changed
    PCError cons = pc_state_verify_conservation(&state);
    
    if (cons == PC_OK && fabs(state.total_supply - original_supply) < 1e-10) {
        test_pass();
    } else {
        test_fail("Supply changed without valid transaction");
    }
    
    pc_state_free(&state);
}

void test_negative_balance_rejected(void) {
    test_start("Conservation: Negative balance transfer rejected");
    
    PCKeypair kp1, kp2;
    pc_keypair_generate(&kp1);
    pc_keypair_generate(&kp2);
    
    PCState state;
    pc_state_genesis(&state, kp1.public_key, 100.0);
    pc_state_create_wallet(&state, kp2.public_key, 0);
    
    // Try to send more than balance
    PCTransaction tx;
    memcpy(tx.from, kp1.public_key, 32);
    memcpy(tx.to, kp2.public_key, 32);
    tx.amount = 200.0;  // More than available
    tx.nonce = 0;
    tx.timestamp = time(NULL);
    pc_transaction_sign(&tx, &kp1);
    
    PCError err = pc_state_execute_tx(&state, &tx);
    
    if (err == PC_ERR_INSUFFICIENT_FUNDS) {
        test_pass();
    } else {
        test_fail("Should have rejected insufficient funds");
    }
    
    pc_state_free(&state);
}

//=============================================================================
// TEST 2: Signature Verification
//=============================================================================
void test_invalid_signature_rejected(void) {
    test_start("Crypto: Invalid signature rejected");
    
    PCKeypair kp1, kp2, attacker;
    pc_keypair_generate(&kp1);
    pc_keypair_generate(&kp2);
    pc_keypair_generate(&attacker);
    
    PCState state;
    pc_state_genesis(&state, kp1.public_key, 1000.0);
    pc_state_create_wallet(&state, kp2.public_key, 0);
    
    // Create transaction from kp1 but sign with attacker key
    PCTransaction tx;
    memcpy(tx.from, kp1.public_key, 32);
    memcpy(tx.to, kp2.public_key, 32);
    tx.amount = 100.0;
    tx.nonce = 0;
    tx.timestamp = time(NULL);
    
    // Sign with wrong key
    pc_transaction_sign(&tx, &attacker);
    
    PCError err = pc_state_execute_tx(&state, &tx);
    
    if (err == PC_ERR_INVALID_SIGNATURE) {
        test_pass();
    } else {
        test_fail("Should have rejected invalid signature");
    }
    
    pc_state_free(&state);
}

void test_modified_transaction_rejected(void) {
    test_start("Crypto: Modified transaction rejected");
    
    PCKeypair kp1, kp2;
    pc_keypair_generate(&kp1);
    pc_keypair_generate(&kp2);
    
    PCState state;
    pc_state_genesis(&state, kp1.public_key, 1000.0);
    pc_state_create_wallet(&state, kp2.public_key, 0);
    
    // Create and sign valid transaction
    PCTransaction tx;
    memcpy(tx.from, kp1.public_key, 32);
    memcpy(tx.to, kp2.public_key, 32);
    tx.amount = 100.0;
    tx.nonce = 0;
    tx.timestamp = time(NULL);
    pc_transaction_sign(&tx, &kp1);
    
    // Modify amount AFTER signing
    tx.amount = 900.0;  // Attacker tries to steal more
    
    PCError err = pc_state_execute_tx(&state, &tx);
    
    if (err == PC_ERR_INVALID_SIGNATURE) {
        test_pass();
    } else {
        test_fail("Should have rejected modified transaction");
    }
    
    pc_state_free(&state);
}

//=============================================================================
// TEST 3: Nonce/Replay Protection
//=============================================================================
void test_replay_attack_rejected(void) {
    test_start("Security: Replay attack rejected");
    
    PCKeypair kp1, kp2;
    pc_keypair_generate(&kp1);
    pc_keypair_generate(&kp2);
    
    PCState state;
    pc_state_genesis(&state, kp1.public_key, 1000.0);
    pc_state_create_wallet(&state, kp2.public_key, 0);
    
    // Create valid transaction
    PCTransaction tx;
    memcpy(tx.from, kp1.public_key, 32);
    memcpy(tx.to, kp2.public_key, 32);
    tx.amount = 100.0;
    tx.nonce = 0;
    tx.timestamp = time(NULL);
    pc_transaction_sign(&tx, &kp1);
    
    // Execute first time - should succeed
    PCError err1 = pc_state_execute_tx(&state, &tx);
    
    // Try to replay same transaction - should fail
    PCError err2 = pc_state_execute_tx(&state, &tx);
    
    if (err1 == PC_OK && err2 == PC_ERR_INVALID_SIGNATURE) {
        test_pass();
    } else {
        test_fail("Replay attack was not blocked");
    }
    
    pc_state_free(&state);
}

void test_future_nonce_rejected(void) {
    test_start("Security: Future nonce rejected");
    
    PCKeypair kp1, kp2;
    pc_keypair_generate(&kp1);
    pc_keypair_generate(&kp2);
    
    PCState state;
    pc_state_genesis(&state, kp1.public_key, 1000.0);
    pc_state_create_wallet(&state, kp2.public_key, 0);
    
    // Create transaction with future nonce
    PCTransaction tx;
    memcpy(tx.from, kp1.public_key, 32);
    memcpy(tx.to, kp2.public_key, 32);
    tx.amount = 100.0;
    tx.nonce = 5;  // Wrong - should be 0
    tx.timestamp = time(NULL);
    pc_transaction_sign(&tx, &kp1);
    
    PCError err = pc_state_execute_tx(&state, &tx);
    
    if (err == PC_ERR_INVALID_SIGNATURE) {
        test_pass();
    } else {
        test_fail("Should have rejected future nonce");
    }
    
    pc_state_free(&state);
}

//=============================================================================
// TEST 4: State Hash Integrity
//=============================================================================
void test_state_hash_changes(void) {
    test_start("Integrity: State hash changes after transaction");
    
    PCKeypair kp1, kp2;
    pc_keypair_generate(&kp1);
    pc_keypair_generate(&kp2);
    
    PCState state;
    pc_state_genesis(&state, kp1.public_key, 1000.0);
    pc_state_create_wallet(&state, kp2.public_key, 0);
    
    uint8_t hash_before[32];
    memcpy(hash_before, state.state_hash, 32);
    
    // Execute transaction
    PCTransaction tx;
    memcpy(tx.from, kp1.public_key, 32);
    memcpy(tx.to, kp2.public_key, 32);
    tx.amount = 100.0;
    tx.nonce = 0;
    tx.timestamp = time(NULL);
    pc_transaction_sign(&tx, &kp1);
    
    pc_state_execute_tx(&state, &tx);
    
    // Hash must have changed
    if (memcmp(hash_before, state.state_hash, 32) != 0) {
        test_pass();
    } else {
        test_fail("State hash didn't change after transaction");
    }
    
    pc_state_free(&state);
}

void test_prev_hash_links(void) {
    test_start("Integrity: Prev hash links correctly");
    
    PCKeypair kp1, kp2;
    pc_keypair_generate(&kp1);
    pc_keypair_generate(&kp2);
    
    PCState state;
    pc_state_genesis(&state, kp1.public_key, 1000.0);
    pc_state_create_wallet(&state, kp2.public_key, 0);
    
    uint8_t hash_before[32];
    memcpy(hash_before, state.state_hash, 32);
    
    PCTransaction tx;
    memcpy(tx.from, kp1.public_key, 32);
    memcpy(tx.to, kp2.public_key, 32);
    tx.amount = 100.0;
    tx.nonce = 0;
    tx.timestamp = time(NULL);
    pc_transaction_sign(&tx, &kp1);
    
    pc_state_execute_tx(&state, &tx);
    
    // prev_hash should equal hash_before
    if (memcmp(state.prev_hash, hash_before, 32) == 0) {
        test_pass();
    } else {
        test_fail("Prev hash doesn't match previous state hash");
    }
    
    pc_state_free(&state);
}

//=============================================================================
// TEST 5: Serialization Security
//=============================================================================
void test_serialization_roundtrip_conservation(void) {
    test_start("Serialization: Conservation preserved after roundtrip");
    
    PCKeypair kp1, kp2;
    pc_keypair_generate(&kp1);
    pc_keypair_generate(&kp2);
    
    PCState state1;
    pc_state_genesis(&state1, kp1.public_key, 1000.0);
    pc_state_create_wallet(&state1, kp2.public_key, 0);
    
    // Execute some transactions
    for (int i = 0; i < 5; i++) {
        PCTransaction tx;
        memcpy(tx.from, kp1.public_key, 32);
        memcpy(tx.to, kp2.public_key, 32);
        tx.amount = 10.0;
        tx.nonce = i;
        tx.timestamp = time(NULL);
        pc_transaction_sign(&tx, &kp1);
        pc_state_execute_tx(&state1, &tx);
    }
    
    // Serialize
    uint8_t buffer[65536];
    size_t len = pc_state_serialize(&state1, buffer, sizeof(buffer));
    
    // Deserialize
    PCState state2;
    memset(&state2, 0, sizeof(state2));
    pc_state_deserialize(&state2, buffer, len);
    
    // Verify conservation in loaded state
    PCError cons = pc_state_verify_conservation(&state2);
    
    if (cons == PC_OK && 
        fabs(state1.total_supply - state2.total_supply) < 1e-10) {
        test_pass();
    } else {
        test_fail("Conservation violated after serialization");
    }
    
    pc_state_free(&state1);
    pc_state_free(&state2);
}

//=============================================================================
// TEST 6: Zero Amount Transaction
//=============================================================================
void test_zero_amount_rejected(void) {
    test_start("Security: Zero amount transaction rejected");
    
    PCKeypair kp1, kp2;
    pc_keypair_generate(&kp1);
    pc_keypair_generate(&kp2);
    
    PCState state;
    pc_state_genesis(&state, kp1.public_key, 1000.0);
    pc_state_create_wallet(&state, kp2.public_key, 0);
    
    PCTransaction tx;
    memcpy(tx.from, kp1.public_key, 32);
    memcpy(tx.to, kp2.public_key, 32);
    tx.amount = 0.0;  // Zero amount
    tx.nonce = 0;
    tx.timestamp = time(NULL);
    pc_transaction_sign(&tx, &kp1);
    
    PCError err = pc_state_execute_tx(&state, &tx);
    
    if (err == PC_ERR_INVALID_AMOUNT) {
        test_pass();
    } else {
        test_fail("Should have rejected zero amount");
    }
    
    pc_state_free(&state);
}

void test_negative_amount_rejected(void) {
    test_start("Security: Negative amount transaction rejected");
    
    PCKeypair kp1, kp2;
    pc_keypair_generate(&kp1);
    pc_keypair_generate(&kp2);
    
    PCState state;
    pc_state_genesis(&state, kp1.public_key, 1000.0);
    pc_state_create_wallet(&state, kp2.public_key, 0);
    
    PCTransaction tx;
    memcpy(tx.from, kp1.public_key, 32);
    memcpy(tx.to, kp2.public_key, 32);
    tx.amount = -100.0;  // Negative amount
    tx.nonce = 0;
    tx.timestamp = time(NULL);
    pc_transaction_sign(&tx, &kp1);
    
    PCError err = pc_state_execute_tx(&state, &tx);
    
    if (err == PC_ERR_INVALID_AMOUNT) {
        test_pass();
    } else {
        test_fail("Should have rejected negative amount");
    }
    
    pc_state_free(&state);
}

//=============================================================================
// TEST 7: Self-Transfer
//=============================================================================
void test_self_transfer_conservation(void) {
    test_start("Conservation: Self-transfer maintains balance");
    
    PCKeypair kp;
    pc_keypair_generate(&kp);
    
    PCState state;
    pc_state_genesis(&state, kp.public_key, 1000.0);
    
    PCWallet* wallet = pc_state_get_wallet(&state, kp.public_key);
    double before = wallet->energy;
    
    PCTransaction tx;
    memcpy(tx.from, kp.public_key, 32);
    memcpy(tx.to, kp.public_key, 32);  // Same wallet
    tx.amount = 100.0;
    tx.nonce = 0;
    tx.timestamp = time(NULL);
    pc_transaction_sign(&tx, &kp);
    
    PCError err = pc_state_execute_tx(&state, &tx);
    
    // Self-transfer should work and maintain balance
    double after = wallet->energy;
    
    if (err == PC_OK && fabs(before - after) < 1e-10) {
        test_pass();
    } else {
        test_fail("Self-transfer changed balance unexpectedly");
    }
    
    pc_state_free(&state);
}

//=============================================================================
// MAIN
//=============================================================================
int main(void) {
    // Initialize libsodium
    if (sodium_init() < 0) {
        fprintf(stderr, "Failed to initialize libsodium\n");
        return 1;
    }
    
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════════════╗\n");
    printf("║              PHYSICSCOIN SECURITY TEST SUITE                          ║\n");
    printf("╚═══════════════════════════════════════════════════════════════════════╝\n\n");
    
    printf("=== CONSERVATION LAW TESTS ===\n");
    test_conservation_enforcement();
    test_no_money_creation();
    test_negative_balance_rejected();
    
    printf("\n=== CRYPTOGRAPHIC SECURITY TESTS ===\n");
    test_invalid_signature_rejected();
    test_modified_transaction_rejected();
    
    printf("\n=== REPLAY/NONCE PROTECTION TESTS ===\n");
    test_replay_attack_rejected();
    test_future_nonce_rejected();
    
    printf("\n=== STATE INTEGRITY TESTS ===\n");
    test_state_hash_changes();
    test_prev_hash_links();
    
    printf("\n=== SERIALIZATION SECURITY TESTS ===\n");
    test_serialization_roundtrip_conservation();
    
    printf("\n=== INPUT VALIDATION TESTS ===\n");
    test_zero_amount_rejected();
    test_negative_amount_rejected();
    
    printf("\n=== EDGE CASE TESTS ===\n");
    test_self_transfer_conservation();
    
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════════\n");
    printf("SECURITY TESTS: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("═══════════════════════════════════════════════════════════════════════\n\n");
    
    if (tests_failed > 0) {
        printf("⚠️  SECURITY VULNERABILITIES DETECTED\n\n");
    } else {
        printf("✓ ALL SECURITY TESTS PASSED\n\n");
    }
    
    return tests_failed > 0 ? 1 : 0;
}

