// streams.c - Streaming Payments
// Enable continuous payment flows between parties

#include "../include/physicscoin.h"
#include "../crypto/sha256.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#define MAX_STREAMS 100

// Payment stream structure
typedef struct {
    uint8_t stream_id[16];       // Unique stream identifier
    uint8_t payer[32];           // Who pays
    uint8_t receiver[32];        // Who receives
    double rate_per_second;      // Payment rate
    uint64_t start_time;         // When stream started
    uint64_t last_settlement;    // Last settlement timestamp
    double total_streamed;       // Total amount flowed
    int active;                  // Is stream active?
} PCPaymentStream;

// Stream registry (in-memory for now)
static PCPaymentStream streams[MAX_STREAMS];
static int num_streams = 0;

// Generate a random stream ID
static void generate_stream_id(uint8_t* id) {
    for (int i = 0; i < 16; i++) {
        id[i] = rand() & 0xFF;
    }
}

// Find stream by ID
static PCPaymentStream* find_stream(const uint8_t* stream_id) {
    for (int i = 0; i < num_streams; i++) {
        if (memcmp(streams[i].stream_id, stream_id, 16) == 0 && streams[i].active) {
            return &streams[i];
        }
    }
    return NULL;
}

// Open a new payment stream
PCError pc_stream_open(PCState* state, const PCKeypair* payer_kp,
                       const uint8_t* receiver, double rate_per_second,
                       uint8_t* stream_id_out) {
    if (num_streams >= MAX_STREAMS) return PC_ERR_MAX_WALLETS;
    if (rate_per_second <= 0) return PC_ERR_INVALID_AMOUNT;
    
    // Verify payer exists and has funds
    PCWallet* payer_wallet = pc_state_get_wallet(state, payer_kp->public_key);
    if (!payer_wallet) return PC_ERR_WALLET_NOT_FOUND;
    
    // Create stream
    PCPaymentStream* stream = &streams[num_streams];
    generate_stream_id(stream->stream_id);
    memcpy(stream->payer, payer_kp->public_key, 32);
    memcpy(stream->receiver, receiver, 32);
    stream->rate_per_second = rate_per_second;
    stream->start_time = (uint64_t)time(NULL);
    stream->last_settlement = stream->start_time;
    stream->total_streamed = 0;
    stream->active = 1;
    
    // Ensure receiver wallet exists
    if (!pc_state_get_wallet(state, receiver)) {
        pc_state_create_wallet(state, receiver, 0);
    }
    
    memcpy(stream_id_out, stream->stream_id, 16);
    num_streams++;
    
    return PC_OK;
}

// Calculate accumulated unsettled amount
double pc_stream_accumulated(const uint8_t* stream_id) {
    PCPaymentStream* stream = find_stream(stream_id);
    if (!stream) return 0;
    
    uint64_t now = (uint64_t)time(NULL);
    uint64_t elapsed = now - stream->last_settlement;
    return elapsed * stream->rate_per_second;
}

// Settle accumulated payments
PCError pc_stream_settle(PCState* state, const uint8_t* stream_id, const PCKeypair* payer_kp) {
    PCPaymentStream* stream = find_stream(stream_id);
    if (!stream) return PC_ERR_WALLET_NOT_FOUND;
    
    // Verify caller is payer
    if (memcmp(stream->payer, payer_kp->public_key, 32) != 0) {
        return PC_ERR_INVALID_SIGNATURE;
    }
    
    uint64_t now = (uint64_t)time(NULL);
    uint64_t elapsed = now - stream->last_settlement;
    double amount = elapsed * stream->rate_per_second;
    
    if (amount <= 0) return PC_OK;  // Nothing to settle
    
    // Create and execute transaction
    PCWallet* payer_wallet = pc_state_get_wallet(state, stream->payer);
    if (!payer_wallet) return PC_ERR_WALLET_NOT_FOUND;
    
    if (payer_wallet->energy < amount) {
        // Settle what we can
        amount = payer_wallet->energy;
    }
    
    PCTransaction tx = {0};
    memcpy(tx.from, stream->payer, 32);
    memcpy(tx.to, stream->receiver, 32);
    tx.amount = amount;
    tx.nonce = payer_wallet->nonce;
    tx.timestamp = now;
    pc_transaction_sign(&tx, payer_kp);
    
    PCError err = pc_state_execute_tx(state, &tx);
    if (err != PC_OK) return err;
    
    stream->last_settlement = now;
    stream->total_streamed += amount;
    
    return PC_OK;
}

// Close a stream (final settlement)
PCError pc_stream_close(PCState* state, const uint8_t* stream_id, const PCKeypair* payer_kp) {
    PCPaymentStream* stream = find_stream(stream_id);
    if (!stream) return PC_ERR_WALLET_NOT_FOUND;
    
    // Final settlement
    PCError err = pc_stream_settle(state, stream_id, payer_kp);
    if (err != PC_OK && err != PC_ERR_INSUFFICIENT_FUNDS) return err;
    
    stream->active = 0;
    return PC_OK;
}

// Get stream info
PCError pc_stream_info(const uint8_t* stream_id, double* rate, double* total, int* active) {
    PCPaymentStream* stream = find_stream(stream_id);
    if (!stream) return PC_ERR_WALLET_NOT_FOUND;
    
    *rate = stream->rate_per_second;
    *total = stream->total_streamed + pc_stream_accumulated(stream_id);
    *active = stream->active;
    
    return PC_OK;
}

// List all active streams for a wallet
int pc_stream_list(const uint8_t* pubkey, uint8_t stream_ids[][16], int max_streams) {
    int count = 0;
    for (int i = 0; i < num_streams && count < max_streams; i++) {
        if (!streams[i].active) continue;
        
        if (memcmp(streams[i].payer, pubkey, 32) == 0 ||
            memcmp(streams[i].receiver, pubkey, 32) == 0) {
            memcpy(stream_ids[count], streams[i].stream_id, 16);
            count++;
        }
    }
    return count;
}

// Print stream details
void pc_stream_print(const uint8_t* stream_id) {
    PCPaymentStream* stream = find_stream(stream_id);
    if (!stream) {
        printf("Stream not found\n");
        return;
    }
    
    printf("Stream ID: ");
    for (int i = 0; i < 8; i++) printf("%02x", stream->stream_id[i]);
    printf("...\n");
    
    printf("Payer:     ");
    for (int i = 0; i < 8; i++) printf("%02x", stream->payer[i]);
    printf("...\n");
    
    printf("Receiver:  ");
    for (int i = 0; i < 8; i++) printf("%02x", stream->receiver[i]);
    printf("...\n");
    
    printf("Rate:      %.12f /sec\n", stream->rate_per_second);
    printf("Total:     %.8f\n", stream->total_streamed);
    printf("Pending:   %.8f\n", pc_stream_accumulated(stream_id));
    printf("Active:    %s\n", stream->active ? "Yes" : "No");
}
