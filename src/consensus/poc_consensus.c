// poc_consensus.c - Proof-of-Conservation PBFT Consensus Implementation
// Novel consensus mechanism combining Byzantine fault tolerance with physics-based validation
// Conservation law becomes the source of truth - double-spending is mathematically impossible

#include "../include/poc_consensus.h"
#include "../include/physicscoin.h"
#include "../crypto/sha256.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <sodium.h>

#define POC_FILE "poc_consensus.dat"
#define POC_MAGIC 0x504F4343  // "POCC"

// ============ Initialization ============

PCError poc_init(POCConsensus* consensus) {
    if (!consensus) return PC_ERR_IO;
    
    memset(consensus, 0, sizeof(POCConsensus));
    
    consensus->current_height = 0;
    consensus->current_round = 0;
    consensus->phase = POC_PHASE_IDLE;
    consensus->leader_index = 0;
    consensus->round_start_time = (uint64_t)time(NULL);
    consensus->last_finalized_time = (uint64_t)time(NULL);
    consensus->is_validator = 0;
    consensus->has_proposal = 0;
    
    printf("POC Consensus initialized (Proof-of-Conservation PBFT)\n");
    
    return PC_OK;
}

PCError poc_load(POCConsensus* consensus, const char* filename) {
    if (!consensus || !filename) return PC_ERR_IO;
    
    FILE* f = fopen(filename, "rb");
    if (!f) {
        // No existing file - initialize fresh
        return poc_init(consensus);
    }
    
    // Read magic number
    uint32_t magic;
    if (fread(&magic, sizeof(magic), 1, f) != 1 || magic != POC_MAGIC) {
        fclose(f);
        return PC_ERR_IO;
    }
    
    // Read consensus state
    if (fread(consensus, sizeof(POCConsensus), 1, f) != 1) {
        fclose(f);
        return PC_ERR_IO;
    }
    
    fclose(f);
    printf("Loaded POC consensus: height=%lu, validators=%u\n", 
           consensus->current_height, consensus->num_validators);
    
    return PC_OK;
}

PCError poc_save(const POCConsensus* consensus, const char* filename) {
    if (!consensus || !filename) return PC_ERR_IO;
    
    FILE* f = fopen(filename, "wb");
    if (!f) return PC_ERR_IO;
    
    uint32_t magic = POC_MAGIC;
    fwrite(&magic, sizeof(magic), 1, f);
    fwrite(consensus, sizeof(POCConsensus), 1, f);
    
    fclose(f);
    return PC_OK;
}

// ============ Validator Management ============

PCError poc_add_validator(POCConsensus* consensus, const uint8_t* pubkey, const char* name) {
    if (!consensus || !pubkey || !name) return PC_ERR_IO;
    
    if (consensus->num_validators >= POC_MAX_VALIDATORS) {
        printf("Error: Maximum validators reached\n");
        return PC_ERR_MAX_WALLETS;
    }
    
    // Check if already exists
    for (uint32_t i = 0; i < consensus->num_validators; i++) {
        if (memcmp(consensus->validators[i].pubkey, pubkey, 32) == 0) {
            printf("Validator already exists\n");
            return PC_ERR_WALLET_EXISTS;
        }
    }
    
    POCValidator* v = &consensus->validators[consensus->num_validators];
    memcpy(v->pubkey, pubkey, 32);
    strncpy(v->name, name, 63);
    v->name[63] = '\0';
    v->joined_at = (uint64_t)time(NULL);
    v->last_seen = (uint64_t)time(NULL);
    v->proposals = 0;
    v->validations = 0;
    v->reputation = 1.0;
    v->active = 1;
    
    consensus->num_validators++;
    
    printf("Added validator: %s (", name);
    for (int i = 0; i < 8; i++) printf("%02x", pubkey[i]);
    printf("...)\n");
    
    return PC_OK;
}

PCError poc_remove_validator(POCConsensus* consensus, const uint8_t* pubkey) {
    if (!consensus || !pubkey) return PC_ERR_IO;
    
    for (uint32_t i = 0; i < consensus->num_validators; i++) {
        if (memcmp(consensus->validators[i].pubkey, pubkey, 32) == 0) {
            consensus->validators[i].active = 0;
            printf("Validator %s deactivated\n", consensus->validators[i].name);
            return PC_OK;
        }
    }
    
    return PC_ERR_WALLET_NOT_FOUND;
}

PCError poc_set_local_validator(POCConsensus* consensus, const uint8_t* pubkey, const uint8_t* secret) {
    if (!consensus || !pubkey || !secret) return PC_ERR_IO;
    
    memcpy(consensus->local_pubkey, pubkey, 32);
    memcpy(consensus->local_secret, secret, 64);
    consensus->is_validator = poc_is_validator(consensus, pubkey);
    
    if (consensus->is_validator) {
        printf("Local validator configured: ");
        for (int i = 0; i < 8; i++) printf("%02x", pubkey[i]);
        printf("...\n");
    } else {
        printf("Warning: Local key not in validator set\n");
    }
    
    return PC_OK;
}

uint32_t poc_active_validator_count(const POCConsensus* consensus) {
    if (!consensus) return 0;
    
    uint32_t count = 0;
    for (uint32_t i = 0; i < consensus->num_validators; i++) {
        if (consensus->validators[i].active) count++;
    }
    return count;
}

int poc_is_validator(const POCConsensus* consensus, const uint8_t* pubkey) {
    if (!consensus || !pubkey) return 0;
    
    for (uint32_t i = 0; i < consensus->num_validators; i++) {
        if (consensus->validators[i].active &&
            memcmp(consensus->validators[i].pubkey, pubkey, 32) == 0) {
            return 1;
        }
    }
    return 0;
}

POCValidator* poc_get_current_leader(POCConsensus* consensus) {
    if (!consensus) return NULL;
    
    uint32_t active_count = poc_active_validator_count(consensus);
    if (active_count == 0) return NULL;
    
    uint32_t target = consensus->leader_index % active_count;
    uint32_t count = 0;
    
    for (uint32_t i = 0; i < consensus->num_validators; i++) {
        if (consensus->validators[i].active) {
            if (count == target) {
                return &consensus->validators[i];
            }
            count++;
        }
    }
    
    return NULL;
}

// ============ Proposal Hashing ============

void poc_hash_proposal(const POCProposal* proposal, uint8_t hash[32]) {
    if (!proposal || !hash) return;
    
    SHA256_CTX ctx;
    sha256_init(&ctx);
    
    sha256_update(&ctx, (uint8_t*)&proposal->sequence_num, 8);
    sha256_update(&ctx, (uint8_t*)&proposal->round, 8);
    sha256_update(&ctx, proposal->prev_state_hash, 32);
    sha256_update(&ctx, proposal->new_state_hash, 32);
    sha256_update(&ctx, (uint8_t*)&proposal->total_supply, sizeof(double));
    sha256_update(&ctx, (uint8_t*)&proposal->delta_sum, sizeof(double));
    sha256_update(&ctx, (uint8_t*)&proposal->timestamp, 8);
    sha256_update(&ctx, proposal->proposer_pubkey, 32);
    sha256_update(&ctx, (uint8_t*)&proposal->num_transactions, 4);
    
    sha256_final(&ctx, hash);
}

// ============ Conservation Verification ============

// THE KEY INNOVATION: Verify that a state transition preserves conservation
static PCError verify_conservation(const PCState* before, const PCState* after) {
    if (!before || !after) return PC_ERR_IO;
    
    // Check 1: Total supply must not change
    if (fabs(before->total_supply - after->total_supply) > 1e-12) {
        printf("POC: Conservation violated - total supply changed from %.8f to %.8f\n",
               before->total_supply, after->total_supply);
        return PC_ERR_CONSERVATION_VIOLATED;
    }
    
    // Check 2: Sum of balances must equal total supply (before)
    double sum_before = 0.0;
    for (uint32_t i = 0; i < before->num_wallets; i++) {
        sum_before += before->wallets[i].energy;
    }
    if (fabs(sum_before - before->total_supply) > 1e-9) {
        printf("POC: Conservation violated - before state sum mismatch\n");
        return PC_ERR_CONSERVATION_VIOLATED;
    }
    
    // Check 3: Sum of balances must equal total supply (after)
    double sum_after = 0.0;
    for (uint32_t i = 0; i < after->num_wallets; i++) {
        sum_after += after->wallets[i].energy;
    }
    if (fabs(sum_after - after->total_supply) > 1e-9) {
        printf("POC: Conservation violated - after state sum mismatch\n");
        return PC_ERR_CONSERVATION_VIOLATED;
    }
    
    // Check 4: No negative balances
    for (uint32_t i = 0; i < after->num_wallets; i++) {
        if (after->wallets[i].energy < 0) {
            printf("POC: Conservation violated - negative balance detected\n");
            return PC_ERR_INVALID_AMOUNT;
        }
    }
    
    return PC_OK;
}

// ============ Consensus Protocol ============

PCError poc_propose_transition(POCConsensus* consensus, 
                               const PCState* before, 
                               const PCState* after,
                               const PCKeypair* proposer) {
    if (!consensus || !before || !after || !proposer) return PC_ERR_IO;
    
    // Check if this node is the current leader
    POCValidator* leader = poc_get_current_leader(consensus);
    if (!leader || memcmp(leader->pubkey, proposer->public_key, 32) != 0) {
        printf("POC: Not the current leader\n");
        return PC_ERR_INVALID_SIGNATURE;
    }
    
    // CRITICAL: Verify conservation BEFORE proposing
    PCError cons_err = verify_conservation(before, after);
    if (cons_err != PC_OK) {
        printf("POC: Cannot propose - conservation would be violated\n");
        return cons_err;
    }
    
    // Calculate delta sum (should be 0 for valid transition)
    double delta_sum = 0.0;
    for (uint32_t i = 0; i < after->num_wallets; i++) {
        // Find corresponding wallet in before state
        double before_balance = 0.0;
        for (uint32_t j = 0; j < before->num_wallets; j++) {
            if (memcmp(before->wallets[j].public_key, 
                       after->wallets[i].public_key, 32) == 0) {
                before_balance = before->wallets[j].energy;
                break;
            }
        }
        delta_sum += (after->wallets[i].energy - before_balance);
    }
    
    // Create proposal
    POCProposal* proposal = &consensus->current_proposal;
    memset(proposal, 0, sizeof(POCProposal));
    
    proposal->sequence_num = consensus->current_height + 1;
    proposal->round = consensus->current_round;
    memcpy(proposal->prev_state_hash, before->state_hash, 32);
    memcpy(proposal->new_state_hash, after->state_hash, 32);
    proposal->total_supply = after->total_supply;
    proposal->delta_sum = delta_sum;
    proposal->timestamp = (uint64_t)time(NULL);
    memcpy(proposal->proposer_pubkey, proposer->public_key, 32);
    proposal->num_transactions = 0; // TODO: track actual TX count
    
    // Hash and sign proposal
    uint8_t proposal_hash[32];
    poc_hash_proposal(proposal, proposal_hash);
    crypto_sign_detached(proposal->proposer_sig, NULL, 
                         proposal_hash, 32, proposer->secret_key);
    
    consensus->has_proposal = 1;
    consensus->phase = POC_PHASE_PRE_PREPARE;
    consensus->num_votes = 0;
    
    // Update leader stats
    leader->proposals++;
    leader->last_seen = (uint64_t)time(NULL);
    
    printf("POC: Proposal #%lu created (delta_sum=%.12f)\n", 
           proposal->sequence_num, proposal->delta_sum);
    
    // Leader auto-votes for their own proposal
    poc_vote(consensus, POC_VOTE_APPROVE, "Proposer");
    
    return PC_OK;
}

PCError poc_validate_proposal(const POCConsensus* consensus, 
                              const POCProposal* proposal,
                              const PCState* current_state) {
    if (!consensus || !proposal || !current_state) return PC_ERR_IO;
    
    // Check 1: Proposer is a registered validator
    if (!poc_is_validator(consensus, proposal->proposer_pubkey)) {
        printf("POC: Proposer is not a registered validator\n");
        return PC_ERR_INVALID_SIGNATURE;
    }
    
    // Check 2: Sequence number is correct
    if (proposal->sequence_num != consensus->current_height + 1) {
        printf("POC: Wrong sequence number (expected %lu, got %lu)\n",
               consensus->current_height + 1, proposal->sequence_num);
        return PC_ERR_INVALID_SIGNATURE;
    }
    
    // Check 3: Previous state hash matches
    if (memcmp(proposal->prev_state_hash, current_state->state_hash, 32) != 0) {
        printf("POC: Previous state hash mismatch\n");
        return PC_ERR_INVALID_SIGNATURE;
    }
    
    // Check 4: Total supply unchanged
    if (fabs(proposal->total_supply - current_state->total_supply) > 1e-12) {
        printf("POC: CONSERVATION VIOLATION - total supply changed\n");
        return PC_ERR_CONSERVATION_VIOLATED;
    }
    
    // Check 5: Delta sum is zero (conservation)
    if (fabs(proposal->delta_sum) > 1e-12) {
        printf("POC: CONSERVATION VIOLATION - delta sum is %.12f (should be 0)\n",
               proposal->delta_sum);
        return PC_ERR_CONSERVATION_VIOLATED;
    }
    
    // Check 6: Verify proposer signature
    uint8_t proposal_hash[32];
    poc_hash_proposal(proposal, proposal_hash);
    if (crypto_sign_verify_detached(proposal->proposer_sig, 
                                    proposal_hash, 32, 
                                    proposal->proposer_pubkey) != 0) {
        printf("POC: Invalid proposer signature\n");
        return PC_ERR_INVALID_SIGNATURE;
    }
    
    return PC_OK;
}

PCError poc_vote(POCConsensus* consensus, POCVoteType vote_type, const char* reason) {
    if (!consensus) return PC_ERR_IO;
    
    if (!consensus->is_validator) {
        printf("POC: Not a validator, cannot vote\n");
        return PC_ERR_INVALID_SIGNATURE;
    }
    
    if (!consensus->has_proposal) {
        printf("POC: No proposal to vote on\n");
        return PC_ERR_IO;
    }
    
    // Check if already voted
    for (uint32_t i = 0; i < consensus->num_votes; i++) {
        if (memcmp(consensus->votes[i].validator_pubkey, 
                   consensus->local_pubkey, 32) == 0) {
            printf("POC: Already voted\n");
            return PC_ERR_WALLET_EXISTS;
        }
    }
    
    if (consensus->num_votes >= POC_MAX_VALIDATORS) {
        return PC_ERR_IO;
    }
    
    // Create vote
    POCVote* vote = &consensus->votes[consensus->num_votes];
    memset(vote, 0, sizeof(POCVote));
    
    vote->sequence_num = consensus->current_proposal.sequence_num;
    vote->round = consensus->current_proposal.round;
    
    uint8_t proposal_hash[32];
    poc_hash_proposal(&consensus->current_proposal, proposal_hash);
    memcpy(vote->proposal_hash, proposal_hash, 32);
    memcpy(vote->validator_pubkey, consensus->local_pubkey, 32);
    vote->vote = vote_type;
    vote->timestamp = (uint64_t)time(NULL);
    if (reason) {
        strncpy(vote->reason, reason, 127);
        vote->reason[127] = '\0';
    }
    
    // Sign vote
    crypto_sign_detached(vote->signature, NULL, 
                         proposal_hash, 32, consensus->local_secret);
    
    consensus->num_votes++;
    
    // Update phase
    if (consensus->phase == POC_PHASE_PRE_PREPARE) {
        consensus->phase = POC_PHASE_PREPARE;
    }
    
    printf("POC: Vote cast (%s) - %u/%u validators\n",
           vote_type == POC_VOTE_APPROVE ? "APPROVE" : 
           (vote_type == POC_VOTE_REJECT ? "REJECT" : "ABSTAIN"),
           consensus->num_votes, poc_active_validator_count(consensus));
    
    return PC_OK;
}

PCError poc_receive_vote(POCConsensus* consensus, const POCVote* vote) {
    if (!consensus || !vote) return PC_ERR_IO;
    
    // Verify vote is from a registered validator
    if (!poc_is_validator(consensus, vote->validator_pubkey)) {
        printf("POC: Vote from non-validator rejected\n");
        return PC_ERR_INVALID_SIGNATURE;
    }
    
    // Verify vote signature
    if (crypto_sign_verify_detached(vote->signature, 
                                    vote->proposal_hash, 32, 
                                    vote->validator_pubkey) != 0) {
        printf("POC: Invalid vote signature\n");
        return PC_ERR_INVALID_SIGNATURE;
    }
    
    // Check for duplicate
    for (uint32_t i = 0; i < consensus->num_votes; i++) {
        if (memcmp(consensus->votes[i].validator_pubkey, 
                   vote->validator_pubkey, 32) == 0) {
            printf("POC: Duplicate vote ignored\n");
            return PC_ERR_WALLET_EXISTS;
        }
    }
    
    // Add vote
    if (consensus->num_votes < POC_MAX_VALIDATORS) {
        memcpy(&consensus->votes[consensus->num_votes], vote, sizeof(POCVote));
        consensus->num_votes++;
        
        printf("POC: Received vote from validator - %u/%u\n",
               consensus->num_votes, poc_active_validator_count(consensus));
    }
    
    return PC_OK;
}

int poc_check_quorum(const POCConsensus* consensus) {
    if (!consensus || !consensus->has_proposal) return 0;
    
    uint32_t active = poc_active_validator_count(consensus);
    if (active < 3) {
        printf("POC: Need at least 3 validators for consensus\n");
        return 0;
    }
    
    uint32_t required = (active * POC_QUORUM_PERCENT) / 100;
    if (required == 0) required = 1;
    
    uint32_t approvals = 0;
    uint32_t rejects = 0;
    
    for (uint32_t i = 0; i < consensus->num_votes; i++) {
        if (consensus->votes[i].vote == POC_VOTE_APPROVE) {
            approvals++;
        } else if (consensus->votes[i].vote == POC_VOTE_REJECT) {
            rejects++;
        }
    }
    
    if (approvals >= required) {
        printf("POC: QUORUM REACHED - %u/%u approvals (required: %u)\n",
               approvals, active, required);
        return 1;  // Approved
    }
    
    if (rejects > active - required) {
        printf("POC: PROPOSAL REJECTED - %u rejects\n", rejects);
        return -1;  // Rejected
    }
    
    return 0;  // Still waiting
}

PCError poc_finalize(POCConsensus* consensus, PCState* state) {
    if (!consensus || !state) return PC_ERR_IO;
    
    int quorum = poc_check_quorum(consensus);
    if (quorum != 1) {
        printf("POC: Cannot finalize - quorum not reached\n");
        return PC_ERR_INVALID_SIGNATURE;
    }
    
    // Update consensus state
    consensus->current_height = consensus->current_proposal.sequence_num;
    consensus->last_finalized_time = (uint64_t)time(NULL);
    consensus->phase = POC_PHASE_FINALIZED;
    
    // Rotate leader for next round
    consensus->leader_index++;
    consensus->current_round = 0;
    
    // Clear proposal
    consensus->has_proposal = 0;
    consensus->num_votes = 0;
    
    printf("POC: FINALIZED block #%lu\n", consensus->current_height);
    
    // Save consensus state
    poc_save(consensus, POC_FILE);
    
    return PC_OK;
}

void poc_next_round(POCConsensus* consensus) {
    if (!consensus) return;
    
    consensus->current_round++;
    consensus->phase = POC_PHASE_IDLE;
    consensus->has_proposal = 0;
    consensus->num_votes = 0;
    consensus->round_start_time = (uint64_t)time(NULL);
    
    // Rotate leader if round timeout (Byzantine leader suspected)
    if (consensus->current_round > 0) {
        consensus->leader_index++;
        printf("POC: Leader rotation due to timeout\n");
    }
}

// ============ Cross-Shard Operations ============

PCError poc_acquire_lock(POCConsensus* consensus,
                         const uint8_t* sender,
                         double amount,
                         uint8_t source_shard,
                         uint8_t target_shard) {
    if (!consensus || !sender) return PC_ERR_IO;
    
    // Check for existing lock
    if (poc_has_pending_lock(consensus, sender)) {
        printf("POC: Sender already has pending lock\n");
        return PC_ERR_WALLET_EXISTS;
    }
    
    if (consensus->num_pending_locks >= 1000) {
        printf("POC: Too many pending locks\n");
        return PC_ERR_IO;
    }
    
    POCCrossShardLock* lock = &consensus->pending_locks[consensus->num_pending_locks];
    
    memcpy(lock->sender_pubkey, sender, 32);
    lock->amount = amount;
    lock->source_shard = source_shard;
    lock->target_shard = target_shard;
    lock->sequence = consensus->num_pending_locks + 1;
    lock->expiry = (uint64_t)time(NULL) + 300;  // 5 minute expiry
    lock->committed = 0;
    
    // Compute lock hash
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, sender, 32);
    sha256_update(&ctx, (uint8_t*)&amount, sizeof(double));
    sha256_update(&ctx, &source_shard, 1);
    sha256_update(&ctx, &target_shard, 1);
    sha256_update(&ctx, (uint8_t*)&lock->sequence, 8);
    sha256_final(&ctx, lock->lock_hash);
    
    consensus->num_pending_locks++;
    
    printf("POC: Cross-shard lock acquired (shard %u -> %u, %.8f)\n",
           source_shard, target_shard, amount);
    
    return PC_OK;
}

PCError poc_release_lock(POCConsensus* consensus, const uint8_t* lock_hash) {
    if (!consensus || !lock_hash) return PC_ERR_IO;
    
    for (uint32_t i = 0; i < consensus->num_pending_locks; i++) {
        if (memcmp(consensus->pending_locks[i].lock_hash, lock_hash, 32) == 0) {
            // Remove by shifting
            memmove(&consensus->pending_locks[i],
                    &consensus->pending_locks[i + 1],
                    (consensus->num_pending_locks - i - 1) * sizeof(POCCrossShardLock));
            consensus->num_pending_locks--;
            
            printf("POC: Lock released\n");
            return PC_OK;
        }
    }
    
    return PC_ERR_WALLET_NOT_FOUND;
}

int poc_has_pending_lock(const POCConsensus* consensus, const uint8_t* sender) {
    if (!consensus || !sender) return 0;
    
    uint64_t now = (uint64_t)time(NULL);
    
    for (uint32_t i = 0; i < consensus->num_pending_locks; i++) {
        if (memcmp(consensus->pending_locks[i].sender_pubkey, sender, 32) == 0 &&
            consensus->pending_locks[i].expiry > now) {
            return 1;
        }
    }
    
    return 0;
}

// ============ Utilities ============

int poc_should_advance(const POCConsensus* consensus) {
    if (!consensus) return 0;
    
    uint64_t now = (uint64_t)time(NULL);
    return (now - consensus->round_start_time >= POC_BLOCK_TIME);
}

const char* poc_phase_name(POCPhase phase) {
    switch (phase) {
        case POC_PHASE_IDLE: return "IDLE";
        case POC_PHASE_PRE_PREPARE: return "PRE-PREPARE";
        case POC_PHASE_PREPARE: return "PREPARE";
        case POC_PHASE_COMMIT: return "COMMIT";
        case POC_PHASE_FINALIZED: return "FINALIZED";
        default: return "UNKNOWN";
    }
}

void poc_print_status(const POCConsensus* consensus) {
    if (!consensus) return;
    
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║      PROOF-OF-CONSERVATION CONSENSUS STATUS                   ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
    
    printf("Height:     %lu\n", consensus->current_height);
    printf("Round:      %lu\n", consensus->current_round);
    printf("Phase:      %s\n", poc_phase_name(consensus->phase));
    printf("Validators: %u active\n", poc_active_validator_count(consensus));
    
    POCValidator* leader = poc_get_current_leader((POCConsensus*)consensus);
    if (leader) {
        printf("Leader:     %s\n", leader->name);
    }
    
    printf("\nValidator Registry:\n");
    printf("┌──────────────────────┬────────┬──────────┬────────────┐\n");
    printf("│ Name                 │ Status │ Proposals│ Reputation │\n");
    printf("├──────────────────────┼────────┼──────────┼────────────┤\n");
    
    for (uint32_t i = 0; i < consensus->num_validators; i++) {
        const POCValidator* v = &consensus->validators[i];
        printf("│ %-20s │ %-6s │ %8lu │ %10.2f │\n",
               v->name,
               v->active ? "ACTIVE" : "INACTIVE",
               v->proposals,
               v->reputation);
    }
    
    printf("└──────────────────────┴────────┴──────────┴────────────┘\n");
    
    if (consensus->has_proposal) {
        printf("\nCurrent Proposal:\n");
        printf("  Sequence:    %lu\n", consensus->current_proposal.sequence_num);
        printf("  Delta Sum:   %.12f\n", consensus->current_proposal.delta_sum);
        printf("  Votes:       %u/%u\n", consensus->num_votes, 
               poc_active_validator_count(consensus));
    }
    
    if (consensus->num_pending_locks > 0) {
        printf("\nPending Cross-Shard Locks: %u\n", consensus->num_pending_locks);
    }
    
    printf("\n");
}
