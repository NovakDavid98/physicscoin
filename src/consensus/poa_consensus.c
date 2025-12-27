// poa_consensus.c - Proof-of-Authority Consensus
// Multi-validator consensus with voting and leader election

#include "../include/physicscoin.h"
#include "../crypto/sha256.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <sodium.h>

#define MAX_VALIDATORS 50
#define MIN_VALIDATORS_FOR_QUORUM 3
#define QUORUM_THRESHOLD_PERCENT 67  // 2/3 majority
#define BLOCK_TIME_SECONDS 5
#define VALIDATOR_TIMEOUT 30

// Validator info
typedef struct {
    uint8_t pubkey[32];
    char name[64];
    uint64_t joined_at;
    uint64_t last_seen;
    uint64_t blocks_proposed;
    uint64_t blocks_validated;
    int active;
    double stake;  // For future PoS migration
} POAValidator;

// Block proposal
typedef struct {
    uint64_t height;
    uint8_t prev_block_hash[32];
    uint8_t state_hash[32];
    uint64_t timestamp;
    uint8_t proposer_pubkey[32];
    uint8_t proposer_signature[64];
    uint32_t num_transactions;
    double total_supply;
} POABlock;

// Vote on a block
typedef struct {
    uint64_t block_height;
    uint8_t block_hash[32];
    uint8_t validator_pubkey[32];
    uint8_t signature[64];
    uint64_t timestamp;
    int vote;  // 1 = approve, 0 = reject
} POAVote;

// Consensus state
typedef struct {
    POAValidator validators[MAX_VALIDATORS];
    uint32_t num_validators;
    
    POABlock current_block;
    POAVote votes[MAX_VALIDATORS];
    uint32_t num_votes;
    
    uint64_t block_height;
    uint64_t leader_index;  // Round-robin leader election
    uint64_t last_block_time;
    
    int finalized;
} POAConsensus;

// Global consensus state
static POAConsensus g_consensus = {0};
static int g_consensus_initialized = 0;

// Initialize consensus
void poa_init(void) {
    if (g_consensus_initialized) return;
    
    memset(&g_consensus, 0, sizeof(POAConsensus));
    g_consensus.block_height = 0;
    g_consensus.leader_index = 0;
    g_consensus.last_block_time = time(NULL);
    g_consensus_initialized = 1;
    
    printf("PoA Consensus initialized\n");
}

// Add validator
PCError poa_add_validator(const uint8_t* pubkey, const char* name) {
    if (!pubkey || !name) return PC_ERR_IO;
    if (g_consensus.num_validators >= MAX_VALIDATORS) return PC_ERR_MAX_WALLETS;
    
    // Check if already exists
    for (uint32_t i = 0; i < g_consensus.num_validators; i++) {
        if (memcmp(g_consensus.validators[i].pubkey, pubkey, 32) == 0) {
            return PC_ERR_WALLET_EXISTS;
        }
    }
    
    POAValidator* v = &g_consensus.validators[g_consensus.num_validators];
    memcpy(v->pubkey, pubkey, 32);
    strncpy(v->name, name, 63);
    v->name[63] = '\0';
    v->joined_at = time(NULL);
    v->last_seen = time(NULL);
    v->blocks_proposed = 0;
    v->blocks_validated = 0;
    v->active = 1;
    v->stake = 0;
    
    g_consensus.num_validators++;
    
    printf("Added validator: %s (", name);
    for (int i = 0; i < 8; i++) printf("%02x", pubkey[i]);
    printf("...)\n");
    
    return PC_OK;
}

// Remove validator
PCError poa_remove_validator(const uint8_t* pubkey) {
    if (!pubkey) return PC_ERR_IO;
    
    for (uint32_t i = 0; i < g_consensus.num_validators; i++) {
        if (memcmp(g_consensus.validators[i].pubkey, pubkey, 32) == 0) {
            // Mark as inactive
            g_consensus.validators[i].active = 0;
            
            printf("Removed validator: %s\n", g_consensus.validators[i].name);
            return PC_OK;
        }
    }
    
    return PC_ERR_WALLET_NOT_FOUND;
}

// Get active validator count
uint32_t poa_active_validator_count(void) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < g_consensus.num_validators; i++) {
        if (g_consensus.validators[i].active) count++;
    }
    return count;
}

// Check if validator exists and is active
int poa_is_validator(const uint8_t* pubkey) {
    for (uint32_t i = 0; i < g_consensus.num_validators; i++) {
        if (g_consensus.validators[i].active &&
            memcmp(g_consensus.validators[i].pubkey, pubkey, 32) == 0) {
            return 1;
        }
    }
    return 0;
}

// Hash a block
static void poa_block_hash(const POABlock* block, uint8_t hash[32]) {
    SHA256_CTX ctx;
    sha256_init(&ctx);
    
    sha256_update(&ctx, (uint8_t*)&block->height, 8);
    sha256_update(&ctx, block->prev_block_hash, 32);
    sha256_update(&ctx, block->state_hash, 32);
    sha256_update(&ctx, (uint8_t*)&block->timestamp, 8);
    sha256_update(&ctx, block->proposer_pubkey, 32);
    sha256_update(&ctx, (uint8_t*)&block->num_transactions, 4);
    sha256_update(&ctx, (uint8_t*)&block->total_supply, 8);
    
    sha256_final(&ctx, hash);
}

// Get current leader (round-robin)
POAValidator* poa_get_current_leader(void) {
    uint32_t active_count = poa_active_validator_count();
    if (active_count == 0) return NULL;
    
    // Find the leader_index'th active validator
    uint32_t count = 0;
    uint32_t target = g_consensus.leader_index % active_count;
    
    for (uint32_t i = 0; i < g_consensus.num_validators; i++) {
        if (g_consensus.validators[i].active) {
            if (count == target) {
                return &g_consensus.validators[i];
            }
            count++;
        }
    }
    
    return NULL;
}

// Rotate leader
void poa_rotate_leader(void) {
    g_consensus.leader_index++;
    printf("Leader rotated (now index %lu)\n", g_consensus.leader_index);
}

// Propose block (called by current leader)
PCError poa_propose_block(const PCState* state, const PCKeypair* proposer_keypair) {
    if (!state || !proposer_keypair) return PC_ERR_IO;
    
    // Check if proposer is current leader
    POAValidator* leader = poa_get_current_leader();
    if (!leader || memcmp(leader->pubkey, proposer_keypair->public_key, 32) != 0) {
        return PC_ERR_INVALID_SIGNATURE;  // Not your turn
    }
    
    // Check block time
    uint64_t now = time(NULL);
    if (now - g_consensus.last_block_time < BLOCK_TIME_SECONDS) {
        return PC_ERR_INVALID_SIGNATURE;  // Too soon
    }
    
    // Create block
    POABlock* block = &g_consensus.current_block;
    memset(block, 0, sizeof(POABlock));
    
    block->height = g_consensus.block_height + 1;
    memcpy(block->state_hash, state->state_hash, 32);
    block->timestamp = now;
    memcpy(block->proposer_pubkey, proposer_keypair->public_key, 32);
    block->num_transactions = 0;  // TODO: track from state
    block->total_supply = state->total_supply;
    
    // Compute block hash
    uint8_t block_hash[32];
    poa_block_hash(block, block_hash);
    
    // Sign block
    crypto_sign_detached(block->proposer_signature, NULL, 
                         block_hash, 32, proposer_keypair->secret_key);
    
    // Reset votes
    g_consensus.num_votes = 0;
    g_consensus.finalized = 0;
    
    // Leader auto-votes for their own block
    POAVote* vote = &g_consensus.votes[g_consensus.num_votes++];
    vote->block_height = block->height;
    memcpy(vote->block_hash, block_hash, 32);
    memcpy(vote->validator_pubkey, proposer_keypair->public_key, 32);
    vote->timestamp = now;
    vote->vote = 1;
    crypto_sign_detached(vote->signature, NULL, block_hash, 32, proposer_keypair->secret_key);
    
    leader->blocks_proposed++;
    
    printf("Block #%lu proposed by %s\n", block->height, leader->name);
    
    return PC_OK;
}

// Vote on current block
PCError poa_vote(const PCKeypair* validator_keypair, int approve) {
    if (!validator_keypair) return PC_ERR_IO;
    
    // Check if validator
    if (!poa_is_validator(validator_keypair->public_key)) {
        return PC_ERR_INVALID_SIGNATURE;
    }
    
    // Check if already voted
    for (uint32_t i = 0; i < g_consensus.num_votes; i++) {
        if (memcmp(g_consensus.votes[i].validator_pubkey, 
                   validator_keypair->public_key, 32) == 0) {
            return PC_ERR_WALLET_EXISTS;  // Already voted
        }
    }
    
    // Create vote
    uint8_t block_hash[32];
    poa_block_hash(&g_consensus.current_block, block_hash);
    
    POAVote* vote = &g_consensus.votes[g_consensus.num_votes++];
    vote->block_height = g_consensus.current_block.height;
    memcpy(vote->block_hash, block_hash, 32);
    memcpy(vote->validator_pubkey, validator_keypair->public_key, 32);
    vote->timestamp = time(NULL);
    vote->vote = approve ? 1 : 0;
    
    // Sign vote
    crypto_sign_detached(vote->signature, NULL, block_hash, 32, 
                         validator_keypair->secret_key);
    
    // Update validator stats
    for (uint32_t i = 0; i < g_consensus.num_validators; i++) {
        if (memcmp(g_consensus.validators[i].pubkey, 
                   validator_keypair->public_key, 32) == 0) {
            g_consensus.validators[i].blocks_validated++;
            g_consensus.validators[i].last_seen = time(NULL);
            break;
        }
    }
    
    printf("Vote received: %s (block #%lu)\n", 
           approve ? "APPROVE" : "REJECT", vote->block_height);
    
    return PC_OK;
}

// Check if block has reached quorum
int poa_check_quorum(void) {
    uint32_t active_validators = poa_active_validator_count();
    if (active_validators < MIN_VALIDATORS_FOR_QUORUM) {
        return 0;  // Not enough validators
    }
    
    uint32_t required_votes = (active_validators * QUORUM_THRESHOLD_PERCENT) / 100;
    if (required_votes == 0) required_votes = 1;
    
    // Count approvals
    uint32_t approvals = 0;
    uint32_t rejections = 0;
    
    for (uint32_t i = 0; i < g_consensus.num_votes; i++) {
        if (g_consensus.votes[i].vote == 1) {
            approvals++;
        } else {
            rejections++;
        }
    }
    
    // Check if quorum reached
    if (approvals >= required_votes) {
        printf("Quorum reached: %u/%u approvals (required: %u)\n", 
               approvals, active_validators, required_votes);
        return 1;
    }
    
    // Check if block is rejected
    if (rejections > active_validators - required_votes) {
        printf("Block rejected: %u rejections\n", rejections);
        return -1;
    }
    
    return 0;
}

// Finalize block
PCError poa_finalize_block(void) {
    int quorum = poa_check_quorum();
    
    if (quorum != 1) {
        return PC_ERR_INVALID_SIGNATURE;  // No quorum yet
    }
    
    g_consensus.finalized = 1;
    g_consensus.block_height = g_consensus.current_block.height;
    g_consensus.last_block_time = time(NULL);
    
    // Rotate leader for next block
    poa_rotate_leader();
    
    printf("Block #%lu finalized\n", g_consensus.block_height);
    
    return PC_OK;
}

// Get consensus info
void poa_print_status(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║          PROOF-OF-AUTHORITY CONSENSUS STATUS                  ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
    
    printf("Block Height: %lu\n", g_consensus.block_height);
    printf("Validators:   %u (%u active)\n", 
           g_consensus.num_validators, poa_active_validator_count());
    
    POAValidator* leader = poa_get_current_leader();
    if (leader) {
        printf("Current Leader: %s\n", leader->name);
    }
    
    printf("\nValidator List:\n");
    printf("┌──────────────────────┬────────┬──────────┬──────────┐\n");
    printf("│ Name                 │ Status │ Proposed │ Validated│\n");
    printf("├──────────────────────┼────────┼──────────┼──────────┤\n");
    
    for (uint32_t i = 0; i < g_consensus.num_validators; i++) {
        POAValidator* v = &g_consensus.validators[i];
        printf("│ %-20s │ %-6s │ %8lu │ %9lu│\n",
               v->name,
               v->active ? "ACTIVE" : "INACTIVE",
               v->blocks_proposed,
               v->blocks_validated);
    }
    
    printf("└──────────────────────┴────────┴──────────┴──────────┘\n");
    
    if (g_consensus.num_votes > 0) {
        printf("\nCurrent Block Votes:\n");
        uint32_t approvals = 0, rejections = 0;
        for (uint32_t i = 0; i < g_consensus.num_votes; i++) {
            if (g_consensus.votes[i].vote) approvals++;
            else rejections++;
        }
        printf("  Approvals: %u\n", approvals);
        printf("  Rejections: %u\n", rejections);
        printf("  Finalized: %s\n", g_consensus.finalized ? "YES" : "NO");
    }
    
    printf("\n");
}

// Check if it's time for a new block
int poa_should_propose(void) {
    uint64_t now = time(NULL);
    return (now - g_consensus.last_block_time >= BLOCK_TIME_SECONDS);
}

// Get current block height
uint64_t poa_get_height(void) {
    return g_consensus.block_height;
}

// Check if current validator is leader
int poa_is_leader(const uint8_t* pubkey) {
    POAValidator* leader = poa_get_current_leader();
    return (leader && memcmp(leader->pubkey, pubkey, 32) == 0);
}

// Save consensus state
PCError poa_save(const char* filename) {
    FILE* f = fopen(filename, "wb");
    if (!f) return PC_ERR_IO;
    
    fwrite(&g_consensus, sizeof(POAConsensus), 1, f);
    fclose(f);
    
    return PC_OK;
}

// Load consensus state
PCError poa_load(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return PC_ERR_IO;
    
    fread(&g_consensus, sizeof(POAConsensus), 1, f);
    fclose(f);
    
    g_consensus_initialized = 1;
    
    printf("Loaded PoA consensus state: block height %lu, %u validators\n",
           g_consensus.block_height, g_consensus.num_validators);
    
    return PC_OK;
}

