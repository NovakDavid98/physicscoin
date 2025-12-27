// streams.c - Persistent Streaming Payments
// Implements continuous pay-per-second payment flows
// SECURITY HARDENED: Persisted to disk for crash recovery

#include "../include/physicscoin.h"
#include "../crypto/sha256.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <unistd.h>

#define MAX_STREAMS 1000
#define STREAM_FILE "streams.dat"
#define STREAM_MAGIC 0x53545245  // "STRE"

// Stream status
typedef enum {
    STREAM_ACTIVE = 1,
    STREAM_PAUSED = 2,
    STREAM_CLOSED = 3
} StreamStatus;

// Payment stream structure
typedef struct {
    uint64_t stream_id;
    uint8_t payer[32];
    uint8_t receiver[32];
    double rate_per_second;  // Coins per second
    uint64_t start_time;
    uint64_t pause_time;     // When paused, 0 if active
    uint64_t total_paused_seconds;  // Accumulated pause time
    double max_amount;       // Cap on total transfer
    double settled_amount;   // Already settled
    StreamStatus status;
    uint8_t authorization_sig[64];  // Payer's signature authorizing stream
} PCPaymentStream;

// Persistent stream registry
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint64_t next_stream_id;
    uint32_t num_streams;
    PCPaymentStream streams[MAX_STREAMS];
} PCStreamRegistry;

// Global registry
static PCStreamRegistry g_stream_registry = {0};
static int g_registry_initialized = 0;

// Save registry to disk
static PCError save_stream_registry(void) {
    FILE* f = fopen(STREAM_FILE ".tmp", "wb");
    if (!f) {
        perror("Failed to open stream file");
        return PC_ERR_IO;
    }
    
    g_stream_registry.magic = STREAM_MAGIC;
    g_stream_registry.version = 1;
    
    // Write header
    if (fwrite(&g_stream_registry, sizeof(uint32_t) * 4, 1, f) != 1) {
        fclose(f);
        return PC_ERR_IO;
    }
    
    // Write streams
    for (uint32_t i = 0; i < g_stream_registry.num_streams; i++) {
        if (fwrite(&g_stream_registry.streams[i], sizeof(PCPaymentStream), 1, f) != 1) {
            fclose(f);
            return PC_ERR_IO;
        }
    }
    
    // Sync to disk
    fflush(f);
    fsync(fileno(f));
    fclose(f);
    
    // Atomic rename
    if (rename(STREAM_FILE ".tmp", STREAM_FILE) != 0) {
        perror("Failed to rename stream file");
        return PC_ERR_IO;
    }
    
    return PC_OK;
}

// Load registry from disk
static PCError load_stream_registry(void) {
    FILE* f = fopen(STREAM_FILE, "rb");
    if (!f) {
        // No existing file - start fresh
        memset(&g_stream_registry, 0, sizeof(g_stream_registry));
        g_stream_registry.magic = STREAM_MAGIC;
        g_stream_registry.version = 1;
        g_stream_registry.next_stream_id = 1;
        g_registry_initialized = 1;
        return PC_OK;
    }
    
    // Read header
    uint32_t header[4];
    if (fread(header, sizeof(uint32_t), 4, f) != 4) {
        fclose(f);
        return PC_ERR_IO;
    }
    
    if (header[0] != STREAM_MAGIC) {
        printf("Invalid stream file magic\n");
        fclose(f);
        return PC_ERR_IO;
    }
    
    g_stream_registry.magic = header[0];
    g_stream_registry.version = header[1];
    g_stream_registry.next_stream_id = header[2] | ((uint64_t)header[3] << 32);
    
    // Read stream count first
    if (fread(&g_stream_registry.num_streams, sizeof(uint32_t), 1, f) != 1) {
        fclose(f);
        return PC_ERR_IO;
    }
    
    // Seek back to proper position
    fseek(f, sizeof(uint32_t) * 4, SEEK_SET);
    
    // Re-read properly
    fclose(f);
    f = fopen(STREAM_FILE, "rb");
    fseek(f, sizeof(uint32_t) * 2, SEEK_SET);  // Skip magic and version
    
    if (fread(&g_stream_registry.next_stream_id, sizeof(uint64_t), 1, f) != 1) {
        fclose(f);
        return PC_ERR_IO;
    }
    
    if (fread(&g_stream_registry.num_streams, sizeof(uint32_t), 1, f) != 1) {
        fclose(f);
        return PC_ERR_IO;
    }
    
    if (g_stream_registry.num_streams > MAX_STREAMS) {
        g_stream_registry.num_streams = MAX_STREAMS;
    }
    
    // Read streams
    for (uint32_t i = 0; i < g_stream_registry.num_streams; i++) {
        if (fread(&g_stream_registry.streams[i], sizeof(PCPaymentStream), 1, f) != 1) {
            break;
        }
    }
    
    fclose(f);
    g_registry_initialized = 1;
    
    printf("Loaded %u streams from disk\n", g_stream_registry.num_streams);
    
    return PC_OK;
}

// Initialize stream system
void pc_stream_init(void) {
    if (!g_registry_initialized) {
        load_stream_registry();
    }
}

// Open a new payment stream (with authorization signature)
uint64_t pc_stream_open(const uint8_t* payer, const uint8_t* receiver,
                        double rate_per_second, double max_amount,
                        const uint8_t* authorization_sig) {
    if (!g_registry_initialized) {
        load_stream_registry();
    }
    
    if (!payer || !receiver || rate_per_second <= 0) {
        return 0;
    }
    
    if (g_stream_registry.num_streams >= MAX_STREAMS) {
        printf("Maximum streams reached\n");
        return 0;
    }
    
    PCPaymentStream* stream = &g_stream_registry.streams[g_stream_registry.num_streams];
    
    stream->stream_id = g_stream_registry.next_stream_id++;
    memcpy(stream->payer, payer, 32);
    memcpy(stream->receiver, receiver, 32);
    stream->rate_per_second = rate_per_second;
    stream->start_time = (uint64_t)time(NULL);
    stream->pause_time = 0;
    stream->total_paused_seconds = 0;
    stream->max_amount = max_amount > 0 ? max_amount : 1e15;  // Default: very high
    stream->settled_amount = 0;
    stream->status = STREAM_ACTIVE;
    
    if (authorization_sig) {
        memcpy(stream->authorization_sig, authorization_sig, 64);
    } else {
        memset(stream->authorization_sig, 0, 64);
    }
    
    g_stream_registry.num_streams++;
    
    // Persist to disk
    save_stream_registry();
    
    printf("Opened stream #%lu: %.8f coins/sec\n", stream->stream_id, rate_per_second);
    
    return stream->stream_id;
}

// Get stream by ID
static PCPaymentStream* get_stream(uint64_t stream_id) {
    for (uint32_t i = 0; i < g_stream_registry.num_streams; i++) {
        if (g_stream_registry.streams[i].stream_id == stream_id) {
            return &g_stream_registry.streams[i];
        }
    }
    return NULL;
}

// Calculate accumulated amount owed
double pc_stream_accumulated(uint64_t stream_id) {
    PCPaymentStream* stream = get_stream(stream_id);
    if (!stream || stream->status == STREAM_CLOSED) return 0.0;
    
    uint64_t now = (uint64_t)time(NULL);
    uint64_t active_seconds;
    
    if (stream->status == STREAM_PAUSED) {
        // Use pause time as endpoint
        active_seconds = stream->pause_time - stream->start_time - stream->total_paused_seconds;
    } else {
        active_seconds = now - stream->start_time - stream->total_paused_seconds;
    }
    
    double total_owed = active_seconds * stream->rate_per_second;
    double unsettled = total_owed - stream->settled_amount;
    
    // Cap at max amount
    if (unsettled > stream->max_amount - stream->settled_amount) {
        unsettled = stream->max_amount - stream->settled_amount;
    }
    
    return unsettled > 0 ? unsettled : 0;
}

// Pause stream
PCError pc_stream_pause(uint64_t stream_id) {
    PCPaymentStream* stream = get_stream(stream_id);
    if (!stream) return PC_ERR_WALLET_NOT_FOUND;
    if (stream->status != STREAM_ACTIVE) return PC_ERR_INVALID_SIGNATURE;
    
    stream->pause_time = (uint64_t)time(NULL);
    stream->status = STREAM_PAUSED;
    
    save_stream_registry();
    
    printf("Stream #%lu paused\n", stream_id);
    return PC_OK;
}

// Resume stream
PCError pc_stream_resume(uint64_t stream_id) {
    PCPaymentStream* stream = get_stream(stream_id);
    if (!stream) return PC_ERR_WALLET_NOT_FOUND;
    if (stream->status != STREAM_PAUSED) return PC_ERR_INVALID_SIGNATURE;
    
    uint64_t now = (uint64_t)time(NULL);
    stream->total_paused_seconds += now - stream->pause_time;
    stream->pause_time = 0;
    stream->status = STREAM_ACTIVE;
    
    save_stream_registry();
    
    printf("Stream #%lu resumed\n", stream_id);
    return PC_OK;
}

// Settle accumulated amount to state
PCError pc_stream_settle(uint64_t stream_id, PCState* state) {
    PCPaymentStream* stream = get_stream(stream_id);
    if (!stream) return PC_ERR_WALLET_NOT_FOUND;
    if (stream->status == STREAM_CLOSED) return PC_ERR_INVALID_SIGNATURE;
    
    double amount = pc_stream_accumulated(stream_id);
    if (amount <= 0) return PC_OK;  // Nothing to settle
    
    // Find wallets
    PCWallet* payer = pc_state_get_wallet(state, stream->payer);
    PCWallet* receiver = pc_state_get_wallet(state, stream->receiver);
    
    if (!payer || !receiver) {
        return PC_ERR_WALLET_NOT_FOUND;
    }
    
    // Check payer has funds
    if (payer->energy < amount) {
        // Settle what we can
        amount = payer->energy;
        if (amount <= 0) return PC_ERR_INSUFFICIENT_FUNDS;
    }
    
    // Execute settlement (atomic)
    double before_sum = payer->energy + receiver->energy;
    payer->energy -= amount;
    receiver->energy += amount;
    double after_sum = payer->energy + receiver->energy;
    
    // Verify conservation
    if (fabs(before_sum - after_sum) > 1e-12) {
        // Rollback
        payer->energy += amount;
        receiver->energy -= amount;
        return PC_ERR_CONSERVATION_VIOLATED;
    }
    
    stream->settled_amount += amount;
    payer->nonce++;
    
    // Update state
    pc_state_compute_hash(state);
    
    // Persist changes
    save_stream_registry();
    
    printf("Stream #%lu settled %.8f coins\n", stream_id, amount);
    
    return PC_OK;
}

// Close stream (final settlement)
PCError pc_stream_close(uint64_t stream_id, PCState* state) {
    PCPaymentStream* stream = get_stream(stream_id);
    if (!stream) return PC_ERR_WALLET_NOT_FOUND;
    if (stream->status == STREAM_CLOSED) return PC_OK;
    
    // Settle any remaining amount
    PCError err = pc_stream_settle(stream_id, state);
    if (err != PC_OK && err != PC_ERR_INSUFFICIENT_FUNDS) {
        return err;
    }
    
    stream->status = STREAM_CLOSED;
    
    // Persist
    save_stream_registry();
    
    printf("Stream #%lu closed (total settled: %.8f)\n", 
           stream_id, stream->settled_amount);
    
    return PC_OK;
}

// Get stream info
void pc_stream_info(uint64_t stream_id) {
    PCPaymentStream* stream = get_stream(stream_id);
    if (!stream) {
        printf("Stream not found\n");
        return;
    }
    
    const char* status_str = "UNKNOWN";
    switch (stream->status) {
        case STREAM_ACTIVE: status_str = "ACTIVE"; break;
        case STREAM_PAUSED: status_str = "PAUSED"; break;
        case STREAM_CLOSED: status_str = "CLOSED"; break;
    }
    
    printf("\nStream #%lu:\n", stream->stream_id);
    printf("  Status: %s\n", status_str);
    printf("  Payer: ");
    for (int i = 0; i < 8; i++) printf("%02x", stream->payer[i]);
    printf("...\n");
    printf("  Receiver: ");
    for (int i = 0; i < 8; i++) printf("%02x", stream->receiver[i]);
    printf("...\n");
    printf("  Rate: %.8f coins/sec\n", stream->rate_per_second);
    printf("  Started: %lu\n", stream->start_time);
    printf("  Settled: %.8f / %.8f\n", stream->settled_amount, stream->max_amount);
    printf("  Pending: %.8f\n", pc_stream_accumulated(stream_id));
}

// List all streams
void pc_stream_list(void) {
    if (!g_registry_initialized) {
        load_stream_registry();
    }
    
    printf("\nPayment Streams (%u total):\n", g_stream_registry.num_streams);
    printf("┌────────┬──────────┬────────────────┬────────────────┐\n");
    printf("│ ID     │ Status   │ Rate/sec       │ Settled        │\n");
    printf("├────────┼──────────┼────────────────┼────────────────┤\n");
    
    for (uint32_t i = 0; i < g_stream_registry.num_streams; i++) {
        PCPaymentStream* s = &g_stream_registry.streams[i];
        const char* status = "???";
        switch (s->status) {
            case STREAM_ACTIVE: status = "ACTIVE"; break;
            case STREAM_PAUSED: status = "PAUSED"; break;
            case STREAM_CLOSED: status = "CLOSED"; break;
        }
        
        printf("│ %-6lu │ %-8s │ %14.8f │ %14.8f │\n",
               s->stream_id, status, s->rate_per_second, s->settled_amount);
    }
    
    printf("└────────┴──────────┴────────────────┴────────────────┘\n");
}

// Find streams for a wallet
void pc_stream_find_for_wallet(const uint8_t* pubkey) {
    if (!pubkey) return;
    
    if (!g_registry_initialized) {
        load_stream_registry();
    }
    
    printf("\nStreams involving wallet:\n");
    
    int found = 0;
    for (uint32_t i = 0; i < g_stream_registry.num_streams; i++) {
        PCPaymentStream* s = &g_stream_registry.streams[i];
        
        int is_payer = (memcmp(s->payer, pubkey, 32) == 0);
        int is_receiver = (memcmp(s->receiver, pubkey, 32) == 0);
        
        if (is_payer || is_receiver) {
            printf("  Stream #%lu: %s (%.8f/sec, settled: %.8f)\n",
                   s->stream_id, 
                   is_payer ? "PAYING" : "RECEIVING",
                   s->rate_per_second,
                   s->settled_amount);
            found++;
        }
    }
    
    if (found == 0) {
        printf("  No streams found\n");
    }
}

// Get total pending payments for a payer
double pc_stream_total_pending_out(const uint8_t* payer) {
    if (!payer) return 0.0;
    
    if (!g_registry_initialized) {
        load_stream_registry();
    }
    
    double total = 0.0;
    
    for (uint32_t i = 0; i < g_stream_registry.num_streams; i++) {
        PCPaymentStream* s = &g_stream_registry.streams[i];
        
        if (memcmp(s->payer, payer, 32) == 0 && s->status != STREAM_CLOSED) {
            total += pc_stream_accumulated(s->stream_id);
        }
    }
    
    return total;
}

// Get total pending payments for a receiver
double pc_stream_total_pending_in(const uint8_t* receiver) {
    if (!receiver) return 0.0;
    
    if (!g_registry_initialized) {
        load_stream_registry();
    }
    
    double total = 0.0;
    
    for (uint32_t i = 0; i < g_stream_registry.num_streams; i++) {
        PCPaymentStream* s = &g_stream_registry.streams[i];
        
        if (memcmp(s->receiver, receiver, 32) == 0 && s->status != STREAM_CLOSED) {
            total += pc_stream_accumulated(s->stream_id);
        }
    }
    
    return total;
}

// Cleanup closed streams
void pc_stream_cleanup(void) {
    if (!g_registry_initialized) return;
    
    uint32_t removed = 0;
    
    for (uint32_t i = 0; i < g_stream_registry.num_streams; ) {
        if (g_stream_registry.streams[i].status == STREAM_CLOSED) {
            // Shift remaining streams
            memmove(&g_stream_registry.streams[i],
                    &g_stream_registry.streams[i + 1],
                    (g_stream_registry.num_streams - i - 1) * sizeof(PCPaymentStream));
            g_stream_registry.num_streams--;
            removed++;
        } else {
            i++;
        }
    }
    
    if (removed > 0) {
        save_stream_registry();
        printf("Cleaned up %u closed streams\n", removed);
    }
}

