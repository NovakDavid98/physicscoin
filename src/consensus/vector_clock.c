// vector_clock.c - Logical clocks for distributed transaction ordering
// Part of Conservation-Based Consensus (CBC)

#include "../include/physicscoin.h"
#include "../crypto/sha256.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_NODES 256

// Vector clock entry for one node
typedef struct {
    uint32_t node_id;
    uint64_t clock;
} VCEntry;

// Full vector clock
typedef struct {
    VCEntry entries[MAX_NODES];
    uint32_t num_entries;
    uint32_t local_node_id;
} VectorClock;

// Initialize vector clock for a node
void vc_init(VectorClock* vc, uint32_t node_id) {
    if (!vc) return;
    
    memset(vc, 0, sizeof(VectorClock));
    vc->local_node_id = node_id;
    vc->num_entries = 1;
    vc->entries[0].node_id = node_id;
    vc->entries[0].clock = 0;
}

// Find entry for a node (returns index, or -1 if not found)
static int vc_find_entry(const VectorClock* vc, uint32_t node_id) {
    for (uint32_t i = 0; i < vc->num_entries; i++) {
        if (vc->entries[i].node_id == node_id) {
            return (int)i;
        }
    }
    return -1;
}

// Get clock value for a node
uint64_t vc_get(const VectorClock* vc, uint32_t node_id) {
    int idx = vc_find_entry(vc, node_id);
    if (idx < 0) return 0;
    return vc->entries[idx].clock;
}

// Increment local clock (call before sending/local event)
void vc_increment(VectorClock* vc) {
    if (!vc) return;
    
    int idx = vc_find_entry(vc, vc->local_node_id);
    if (idx >= 0) {
        vc->entries[idx].clock++;
    }
}

// Merge received clock into local clock (call on receive)
void vc_merge(VectorClock* local, const VectorClock* received) {
    if (!local || !received) return;
    
    // For each entry in received clock
    for (uint32_t i = 0; i < received->num_entries; i++) {
        uint32_t node_id = received->entries[i].node_id;
        uint64_t recv_clock = received->entries[i].clock;
        
        int idx = vc_find_entry(local, node_id);
        
        if (idx >= 0) {
            // Take max
            if (recv_clock > local->entries[idx].clock) {
                local->entries[idx].clock = recv_clock;
            }
        } else if (local->num_entries < MAX_NODES) {
            // Add new entry
            local->entries[local->num_entries].node_id = node_id;
            local->entries[local->num_entries].clock = recv_clock;
            local->num_entries++;
        }
    }
    
    // Increment local clock after merge
    vc_increment(local);
}

// Compare two vector clocks
// Returns: -1 if a < b (a happened before b)
//           0 if concurrent (neither happened before the other)
//           1 if a > b (a happened after b)
int vc_compare(const VectorClock* a, const VectorClock* b) {
    if (!a || !b) return 0;
    
    int a_less = 0;   // At least one entry where a < b
    int b_less = 0;   // At least one entry where b < a
    
    // Check all nodes in both clocks
    for (uint32_t i = 0; i < a->num_entries; i++) {
        uint64_t a_val = a->entries[i].clock;
        uint64_t b_val = vc_get(b, a->entries[i].node_id);
        
        if (a_val < b_val) a_less = 1;
        if (a_val > b_val) b_less = 1;
    }
    
    for (uint32_t i = 0; i < b->num_entries; i++) {
        uint64_t b_val = b->entries[i].clock;
        uint64_t a_val = vc_get(a, b->entries[i].node_id);
        
        if (a_val < b_val) a_less = 1;
        if (a_val > b_val) b_less = 1;
    }
    
    if (a_less && !b_less) return -1;  // a happened before b
    if (b_less && !a_less) return 1;   // a happened after b
    return 0;  // Concurrent
}

// Check if a happened before b (useful for ordering)
int vc_happened_before(const VectorClock* a, const VectorClock* b) {
    return vc_compare(a, b) == -1;
}

// Serialize vector clock to bytes
size_t vc_serialize(const VectorClock* vc, uint8_t* buffer, size_t max_size) {
    if (!vc || !buffer) return 0;
    
    size_t needed = 4 + 4 + (vc->num_entries * 12);  // local_id + count + entries
    if (max_size < needed) return 0;
    
    size_t offset = 0;
    
    // Local node ID
    memcpy(buffer + offset, &vc->local_node_id, 4);
    offset += 4;
    
    // Number of entries
    memcpy(buffer + offset, &vc->num_entries, 4);
    offset += 4;
    
    // Entries
    for (uint32_t i = 0; i < vc->num_entries; i++) {
        memcpy(buffer + offset, &vc->entries[i].node_id, 4);
        offset += 4;
        memcpy(buffer + offset, &vc->entries[i].clock, 8);
        offset += 8;
    }
    
    return offset;
}

// Deserialize vector clock from bytes
PCError vc_deserialize(VectorClock* vc, const uint8_t* buffer, size_t size) {
    if (!vc || !buffer || size < 8) return PC_ERR_IO;
    
    size_t offset = 0;
    
    memcpy(&vc->local_node_id, buffer + offset, 4);
    offset += 4;
    
    memcpy(&vc->num_entries, buffer + offset, 4);
    offset += 4;
    
    if (vc->num_entries > MAX_NODES) return PC_ERR_IO;
    if (size < 8 + (vc->num_entries * 12)) return PC_ERR_IO;
    
    for (uint32_t i = 0; i < vc->num_entries; i++) {
        memcpy(&vc->entries[i].node_id, buffer + offset, 4);
        offset += 4;
        memcpy(&vc->entries[i].clock, buffer + offset, 8);
        offset += 8;
    }
    
    return PC_OK;
}

// Print vector clock for debugging
void vc_print(const VectorClock* vc) {
    if (!vc) return;
    
    printf("VectorClock[node=%u]: {", vc->local_node_id);
    for (uint32_t i = 0; i < vc->num_entries; i++) {
        printf("%u:%lu", vc->entries[i].node_id, vc->entries[i].clock);
        if (i < vc->num_entries - 1) printf(", ");
    }
    printf("}\n");
}

// Hash the vector clock (for inclusion in TX signatures)
void vc_hash(const VectorClock* vc, uint8_t hash[32]) {
    if (!vc || !hash) return;
    
    uint8_t buffer[4096];
    size_t size = vc_serialize(vc, buffer, sizeof(buffer));
    
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, buffer, size);
    sha256_final(&ctx, hash);
}
