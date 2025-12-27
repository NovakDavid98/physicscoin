// ordering.c - Transaction Ordering and Conflict Resolution
// Part of Conservation-Based Consensus (CBC)

#include "../include/physicscoin.h"
#include "../crypto/sha256.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#define MAX_PENDING_TXS 10000

// Forward declarations from vector_clock.c
typedef struct {
    uint32_t node_id;
    uint64_t clock;
} VCEntry;

typedef struct {
    VCEntry entries[256];
    uint32_t num_entries;
    uint32_t local_node_id;
} VectorClock;

void vc_init(VectorClock* vc, uint32_t node_id);
void vc_increment(VectorClock* vc);
void vc_merge(VectorClock* local, const VectorClock* received);
int vc_compare(const VectorClock* a, const VectorClock* b);
int vc_happened_before(const VectorClock* a, const VectorClock* b);
size_t vc_serialize(const VectorClock* vc, uint8_t* buffer, size_t max_size);
PCError vc_deserialize(VectorClock* vc, const uint8_t* buffer, size_t size);

// Extended transaction with vector clock
typedef struct {
    PCTransaction tx;
    VectorClock vc;
    uint8_t tx_hash[32];
    uint64_t received_at;
    int executed;
} PCOrderedTx;

// Pending transaction pool
typedef struct {
    PCOrderedTx txs[MAX_PENDING_TXS];
    uint32_t count;
    VectorClock local_vc;
} PCTxPool;

// Initialize transaction pool
void txpool_init(PCTxPool* pool, uint32_t node_id) {
    if (!pool) return;
    
    memset(pool, 0, sizeof(PCTxPool));
    vc_init(&pool->local_vc, node_id);
}

// Hash a transaction
static void hash_tx(const PCTransaction* tx, uint8_t hash[32]) {
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, tx->from, 32);
    sha256_update(&ctx, tx->to, 32);
    sha256_update(&ctx, (uint8_t*)&tx->amount, sizeof(tx->amount));
    sha256_update(&ctx, (uint8_t*)&tx->nonce, sizeof(tx->nonce));
    sha256_final(&ctx, hash);
}

// Check if two TXs conflict (same sender, same nonce, different content)
static int txs_conflict(const PCOrderedTx* a, const PCOrderedTx* b) {
    // Same sender
    if (memcmp(a->tx.from, b->tx.from, 32) != 0) return 0;
    
    // Same nonce
    if (a->tx.nonce != b->tx.nonce) return 0;
    
    // Different hash (different recipient or amount)
    if (memcmp(a->tx_hash, b->tx_hash, 32) == 0) return 0;
    
    return 1;  // Conflict detected
}

// Add transaction to pool (returns 0 on success, -1 if conflict, -2 if full)
int txpool_add(PCTxPool* pool, const PCTransaction* tx, const VectorClock* remote_vc) {
    if (!pool || !tx) return -2;
    if (pool->count >= MAX_PENDING_TXS) return -2;
    
    PCOrderedTx* ordered = &pool->txs[pool->count];
    memcpy(&ordered->tx, tx, sizeof(PCTransaction));
    
    // Store vector clock
    if (remote_vc) {
        memcpy(&ordered->vc, remote_vc, sizeof(VectorClock));
    } else {
        memcpy(&ordered->vc, &pool->local_vc, sizeof(VectorClock));
    }
    
    // Hash transaction
    hash_tx(tx, ordered->tx_hash);
    
    ordered->received_at = (uint64_t)time(NULL);
    ordered->executed = 0;
    
    // Check for conflicts with existing TXs
    for (uint32_t i = 0; i < pool->count; i++) {
        if (txs_conflict(ordered, &pool->txs[i])) {
            // Conflict! Use vector clock to determine winner
            int cmp = vc_compare(&pool->txs[i].vc, &ordered->vc);
            
            if (cmp == -1) {
                // Existing TX happened before → existing wins, reject new
                printf("Conflict: Existing TX wins (happened before)\n");
                return -1;
            } else if (cmp == 1) {
                // New TX happened before → new wins, replace existing
                printf("Conflict: New TX wins (happened before)\n");
                memcpy(&pool->txs[i], ordered, sizeof(PCOrderedTx));
                return 0;
            } else {
                // Concurrent! Tie-breaker: lower TX hash wins
                if (memcmp(ordered->tx_hash, pool->txs[i].tx_hash, 32) < 0) {
                    printf("Conflict (concurrent): New TX wins (lower hash)\n");
                    memcpy(&pool->txs[i], ordered, sizeof(PCOrderedTx));
                    return 0;
                } else {
                    printf("Conflict (concurrent): Existing TX wins (lower hash)\n");
                    return -1;
                }
            }
        }
    }
    
    // No conflict, merge vector clock and add
    if (remote_vc) {
        vc_merge(&pool->local_vc, remote_vc);
    } else {
        vc_increment(&pool->local_vc);
    }
    
    pool->count++;
    
    return 0;
}

// Sort pool by vector clock order (for deterministic execution)
static int compare_ordered_tx(const void* a, const void* b) {
    const PCOrderedTx* ta = (const PCOrderedTx*)a;
    const PCOrderedTx* tb = (const PCOrderedTx*)b;
    
    int cmp = vc_compare(&ta->vc, &tb->vc);
    
    if (cmp != 0) return cmp;
    
    // Tie-breaker: lower hash first
    return memcmp(ta->tx_hash, tb->tx_hash, 32);
}

void txpool_sort(PCTxPool* pool) {
    if (!pool || pool->count < 2) return;
    
    qsort(pool->txs, pool->count, sizeof(PCOrderedTx), compare_ordered_tx);
}

// Execute pending transactions in order
uint32_t txpool_execute(PCTxPool* pool, PCState* state) {
    if (!pool || !state) return 0;
    
    // Sort by vector clock
    txpool_sort(pool);
    
    uint32_t executed = 0;
    
    for (uint32_t i = 0; i < pool->count; i++) {
        if (pool->txs[i].executed) continue;
        
        PCError err = pc_state_execute_tx(state, &pool->txs[i].tx);
        
        if (err == PC_OK) {
            pool->txs[i].executed = 1;
            executed++;
        } else {
            // TX failed (likely insufficient funds from earlier conflict)
            printf("TX execution failed: %s\n", pc_strerror(err));
        }
    }
    
    return executed;
}

// Remove executed transactions
void txpool_cleanup(PCTxPool* pool) {
    if (!pool) return;
    
    uint32_t write_idx = 0;
    
    for (uint32_t i = 0; i < pool->count; i++) {
        if (!pool->txs[i].executed) {
            if (write_idx != i) {
                memcpy(&pool->txs[write_idx], &pool->txs[i], sizeof(PCOrderedTx));
            }
            write_idx++;
        }
    }
    
    pool->count = write_idx;
}

// Get pool statistics
void txpool_stats(const PCTxPool* pool, uint32_t* pending, uint32_t* executed) {
    if (!pool) return;
    
    uint32_t p = 0, e = 0;
    
    for (uint32_t i = 0; i < pool->count; i++) {
        if (pool->txs[i].executed) e++;
        else p++;
    }
    
    if (pending) *pending = p;
    if (executed) *executed = e;
}

// Print pool for debugging
void txpool_print(const PCTxPool* pool) {
    if (!pool) return;
    
    printf("TX Pool (%u transactions):\n", pool->count);
    
    for (uint32_t i = 0; i < pool->count; i++) {
        const PCOrderedTx* otx = &pool->txs[i];
        
        printf("  [%u] ", i);
        for (int j = 0; j < 4; j++) printf("%02x", otx->tx_hash[j]);
        printf("... nonce=%lu amount=%.2f %s\n",
               otx->tx.nonce, otx->tx.amount,
               otx->executed ? "[EXECUTED]" : "[PENDING]");
    }
}
