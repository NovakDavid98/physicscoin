// test_performance.c - Performance benchmarks
// Measure transaction throughput and state size

#include "../include/physicscoin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

static double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

void benchmark_transactions(void) {
    printf("═══ TRANSACTION THROUGHPUT ═══\n\n");
    
    PCState state;
    PCKeypair alice, bob;
    
    pc_keypair_generate(&alice);
    pc_keypair_generate(&bob);
    
    pc_state_genesis(&state, alice.public_key, 1000000.0);
    pc_state_create_wallet(&state, bob.public_key, 0);
    
    int num_tx = 10000;
    double start = get_time_ms();
    
    for (int i = 0; i < num_tx; i++) {
        PCTransaction tx = {0};
        memcpy(tx.from, alice.public_key, PHYSICSCOIN_KEY_SIZE);
        memcpy(tx.to, bob.public_key, PHYSICSCOIN_KEY_SIZE);
        tx.amount = 1.0;
        tx.nonce = i;
        tx.timestamp = time(NULL);
        pc_transaction_sign(&tx, &alice);
        pc_state_execute_tx(&state, &tx);
    }
    
    double end = get_time_ms();
    double elapsed = end - start;
    double tps = num_tx / (elapsed / 1000.0);
    
    printf("Transactions:    %d\n", num_tx);
    printf("Time:            %.2f ms\n", elapsed);
    printf("Throughput:      %.0f tx/sec\n", tps);
    printf("Latency:         %.4f ms/tx\n", elapsed / num_tx);
    printf("\n");
    
    if (tps > 10000) {
        printf("✓ PASS: Throughput > 10,000 tx/sec\n");
    } else {
        printf("⚠ WARNING: Throughput below target\n");
    }
    
    pc_state_free(&state);
}

void benchmark_state_size(void) {
    printf("\n═══ STATE SIZE SCALING ═══\n\n");
    
    printf("| Wallets | State Size | Per Wallet |\n");
    printf("|---------|------------|------------|\n");
    
    int wallet_counts[] = {1, 10, 100, 1000, 5000};
    
    for (int w = 0; w < 5; w++) {
        int n = wallet_counts[w];
        
        PCState state;
        PCKeypair kp;
        
        pc_keypair_generate(&kp);
        pc_state_genesis(&state, kp.public_key, 1000000.0);
        
        for (int i = 1; i < n; i++) {
            pc_keypair_generate(&kp);
            pc_state_create_wallet(&state, kp.public_key, 0);
        }
        
        uint8_t buffer[1024 * 1024];  // 1 MB
        size_t size = pc_state_serialize(&state, buffer, sizeof(buffer));
        
        printf("| %7d | %10zu | %10.1f |\n", n, size, (double)size / n);
        
        pc_state_free(&state);
    }
    
    printf("\n");
}

void benchmark_hash_computation(void) {
    printf("═══ HASH COMPUTATION ═══\n\n");
    
    PCState state;
    PCKeypair kp;
    
    pc_keypair_generate(&kp);
    pc_state_genesis(&state, kp.public_key, 1000000.0);
    
    // Add 100 wallets
    for (int i = 0; i < 100; i++) {
        pc_keypair_generate(&kp);
        pc_state_create_wallet(&state, kp.public_key, 0);
    }
    
    int iterations = 100000;
    double start = get_time_ms();
    
    for (int i = 0; i < iterations; i++) {
        pc_state_compute_hash(&state);
    }
    
    double end = get_time_ms();
    double elapsed = end - start;
    
    printf("Hash computations: %d\n", iterations);
    printf("Time:              %.2f ms\n", elapsed);
    printf("Rate:              %.0f hashes/sec\n", iterations / (elapsed / 1000.0));
    printf("Per hash:          %.4f µs\n", (elapsed * 1000.0) / iterations);
    printf("\n");
    
    pc_state_free(&state);
}

void benchmark_keypair_generation(void) {
    printf("═══ KEY GENERATION ═══\n\n");
    
    int iterations = 10000;
    double start = get_time_ms();
    
    for (int i = 0; i < iterations; i++) {
        PCKeypair kp;
        pc_keypair_generate(&kp);
    }
    
    double end = get_time_ms();
    double elapsed = end - start;
    
    printf("Keys generated:    %d\n", iterations);
    printf("Time:              %.2f ms\n", elapsed);
    printf("Rate:              %.0f keys/sec\n", iterations / (elapsed / 1000.0));
    printf("\n");
}

int main(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║         PHYSICSCOIN PERFORMANCE BENCHMARKS                    ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
    
    benchmark_transactions();
    benchmark_state_size();
    benchmark_hash_computation();
    benchmark_keypair_generation();
    
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("BENCHMARKS COMPLETE\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    return 0;
}
