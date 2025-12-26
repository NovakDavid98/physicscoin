// test_serialization.c - Serialization round-trip tests
// Verify state can be saved and loaded correctly

#include "../include/physicscoin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

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

// Test 1: Basic save/load
void test_save_load(void) {
    test_start("Save and load state");
    
    PCKeypair kp;
    pc_keypair_generate(&kp);
    
    PCState state1;
    pc_state_genesis(&state1, kp.public_key, 12345.67);
    
    // Save
    PCError err = pc_state_save(&state1, "/tmp/test_state.pcs");
    if (err != PC_OK) {
        test_fail("Save failed");
        pc_state_free(&state1);
        return;
    }
    
    // Load
    PCState state2 = {0};
    err = pc_state_load(&state2, "/tmp/test_state.pcs");
    if (err != PC_OK) {
        test_fail("Load failed");
        pc_state_free(&state1);
        return;
    }
    
    // Compare
    if (state1.total_supply == state2.total_supply &&
        state1.num_wallets == state2.num_wallets &&
        memcmp(state1.state_hash, state2.state_hash, PHYSICSCOIN_HASH_SIZE) == 0) {
        test_pass();
    } else {
        test_fail("State mismatch after load");
    }
    
    pc_state_free(&state1);
    pc_state_free(&state2);
}

// Test 2: Multi-wallet serialization
void test_multi_wallet_serialization(void) {
    test_start("Serialize 50 wallets");
    
    PCState state1;
    PCKeypair wallets[50];
    
    pc_keypair_generate(&wallets[0]);
    pc_state_genesis(&state1, wallets[0].public_key, 100000.0);
    
    for (int i = 1; i < 50; i++) {
        pc_keypair_generate(&wallets[i]);
        pc_state_create_wallet(&state1, wallets[i].public_key, 0);
    }
    
    // Execute some transactions
    for (int i = 0; i < 20; i++) {
        PCTransaction tx = {0};
        memcpy(tx.from, wallets[0].public_key, PHYSICSCOIN_KEY_SIZE);
        memcpy(tx.to, wallets[i + 1].public_key, PHYSICSCOIN_KEY_SIZE);
        tx.amount = 100.0;
        tx.nonce = i;
        tx.timestamp = time(NULL);
        pc_transaction_sign(&tx, &wallets[0]);
        pc_state_execute_tx(&state1, &tx);
    }
    
    // Save and load
    pc_state_save(&state1, "/tmp/test_multi.pcs");
    
    PCState state2 = {0};
    pc_state_load(&state2, "/tmp/test_multi.pcs");
    
    // Verify all wallets
    int match = 1;
    for (int i = 0; i < 50; i++) {
        PCWallet* w1 = pc_state_get_wallet(&state1, wallets[i].public_key);
        PCWallet* w2 = pc_state_get_wallet(&state2, wallets[i].public_key);
        
        if (!w1 || !w2 || fabs(w1->energy - w2->energy) > 1e-10) {
            match = 0;
            break;
        }
    }
    
    if (match && pc_state_verify_conservation(&state2) == PC_OK) {
        test_pass();
    } else {
        test_fail("Wallet data corrupted");
    }
    
    pc_state_free(&state1);
    pc_state_free(&state2);
}

// Test 3: Buffer serialization
void test_buffer_serialization(void) {
    test_start("Serialize to buffer and back");
    
    PCKeypair kp;
    pc_keypair_generate(&kp);
    
    PCState state1;
    pc_state_genesis(&state1, kp.public_key, 999.99);
    
    // Serialize to buffer
    uint8_t buffer[4096];
    size_t size = pc_state_serialize(&state1, buffer, sizeof(buffer));
    
    if (size == 0) {
        test_fail("Serialize returned 0");
        pc_state_free(&state1);
        return;
    }
    
    // Deserialize
    PCState state2 = {0};
    PCError err = pc_state_deserialize(&state2, buffer, size);
    
    if (err != PC_OK) {
        test_fail("Deserialize failed");
        pc_state_free(&state1);
        return;
    }
    
    if (fabs(state1.total_supply - state2.total_supply) < 1e-10) {
        test_pass();
        printf("       (Serialized size: %zu bytes)\n", size);
    } else {
        test_fail("Supply mismatch");
    }
    
    pc_state_free(&state1);
    pc_state_free(&state2);
}

// Test 4: Hash chain integrity
void test_hash_chain(void) {
    test_start("Hash chain links correctly");
    
    PCKeypair kp1, kp2;
    pc_keypair_generate(&kp1);
    pc_keypair_generate(&kp2);
    
    PCState state;
    pc_state_genesis(&state, kp1.public_key, 1000.0);
    pc_state_create_wallet(&state, kp2.public_key, 0);
    
    uint8_t hash_before[32];
    memcpy(hash_before, state.state_hash, 32);
    
    PCTransaction tx = {0};
    memcpy(tx.from, kp1.public_key, PHYSICSCOIN_KEY_SIZE);
    memcpy(tx.to, kp2.public_key, PHYSICSCOIN_KEY_SIZE);
    tx.amount = 100.0;
    tx.nonce = 0;
    tx.timestamp = time(NULL);
    pc_transaction_sign(&tx, &kp1);
    
    pc_state_execute_tx(&state, &tx);
    
    // prev_hash should equal hash_before
    if (memcmp(state.prev_hash, hash_before, 32) == 0 &&
        memcmp(state.state_hash, hash_before, 32) != 0) {
        test_pass();
    } else {
        test_fail("Hash chain broken");
    }
    
    pc_state_free(&state);
}

int main(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║         PHYSICSCOIN SERIALIZATION TEST SUITE                  ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
    
    test_save_load();
    test_multi_wallet_serialization();
    test_buffer_serialization();
    test_hash_chain();
    
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("RESULTS: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    return tests_failed > 0 ? 1 : 0;
}
