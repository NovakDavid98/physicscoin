// batch_benchmark.c - OpenMP Parallel Verification Benchmark

#include "../include/physicscoin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <omp.h>

#define NUM_TXS 50000

int main(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║           OpenMP PARALLEL BENCHMARK                           ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
    
    printf("CPU Threads: %d\n\n", omp_get_max_threads());
    
    // Generate keypairs
    PCKeypair sender, receiver;
    pc_keypair_generate(&sender);
    pc_keypair_generate(&receiver);
    
    // Create transactions
    printf("Creating %d transactions...\n", NUM_TXS);
    PCTransaction* txs = malloc(NUM_TXS * sizeof(PCTransaction));
    
    for (int i = 0; i < NUM_TXS; i++) {
        memset(&txs[i], 0, sizeof(PCTransaction));
        memcpy(txs[i].from, sender.public_key, 32);
        memcpy(txs[i].to, receiver.public_key, 32);
        txs[i].amount = 1.0;
        txs[i].nonce = i;
        txs[i].timestamp = time(NULL);
        pc_transaction_sign(&txs[i], &sender);
    }
    printf("Done.\n\n");
    
    // Benchmark sequential verification
    printf("═══ Sequential Verification ═══\n");
    {
        double start = omp_get_wtime();
        int pass = 0;
        for (int i = 0; i < NUM_TXS; i++) {
            if (pc_transaction_verify(&txs[i]) == PC_OK) pass++;
        }
        double elapsed = omp_get_wtime() - start;
        double tps = NUM_TXS / elapsed;
        
        printf("  Time:       %.3f sec\n", elapsed);
        printf("  Verified:   %d / %d\n", pass, NUM_TXS);
        printf("  Throughput: %.0f verify/sec\n\n", tps);
    }
    
    // Benchmark OpenMP parallel verification
    printf("═══ OpenMP Parallel Verification (%d threads) ═══\n", omp_get_max_threads());
    {
        double start = omp_get_wtime();
        int pass = 0;
        
        #pragma omp parallel for reduction(+:pass) schedule(dynamic, 256)
        for (int i = 0; i < NUM_TXS; i++) {
            if (pc_transaction_verify(&txs[i]) == PC_OK) pass++;
        }
        double elapsed = omp_get_wtime() - start;
        double tps = NUM_TXS / elapsed;
        
        printf("  Time:       %.3f sec\n", elapsed);
        printf("  Verified:   %d / %d\n", pass, NUM_TXS);
        printf("  Throughput: %.0f verify/sec\n\n", tps);
    }
    
    // Calculate speedup
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║  Speedup from parallelization measured above                  ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
    
    free(txs);
    
    return 0;
}
