// poc_consensus.h - Proof-of-Conservation PBFT Consensus
// Combines Byzantine fault tolerance with physics-based validation
// Conservation law becomes the arbiter of validity

#ifndef POC_CONSENSUS_H
#define POC_CONSENSUS_H

#include "physicscoin.h"

#ifdef __cplusplus
extern "C" {
#endif

// Maximum validators in the network
#define POC_MAX_VALIDATORS 100

// Quorum threshold (2/3 majority for Byzantine fault tolerance)
#define POC_QUORUM_PERCENT 67

// Block time in seconds
#define POC_BLOCK_TIME 5

// Conservation-aware PBFT phases
typedef enum {
    POC_PHASE_IDLE = 0,           // Waiting for next round
    POC_PHASE_PRE_PREPARE = 1,    // Leader proposes state transition
    POC_PHASE_PREPARE = 2,        // Validators verify conservation
    POC_PHASE_COMMIT = 3,         // 2/3 agreement achieved
    POC_PHASE_FINALIZED = 4       // State transition finalized
} POCPhase;

// Validator information
typedef struct {
    uint8_t pubkey[32];           // Ed25519 public key
    char name[64];                // Human-readable name
    uint64_t joined_at;           // Timestamp when joined
    uint64_t last_seen;           // Last activity timestamp
    uint64_t proposals;           // Number of proposals made
    uint64_t validations;         // Number of validations performed
    double reputation;            // Reputation score (0.0 - 1.0)
    int active;                   // Is currently active
} POCValidator;

// State transition proposal (replaces traditional "block")
typedef struct {
    uint64_t sequence_num;        // Monotonically increasing sequence
    uint64_t round;               // Consensus round
    uint8_t prev_state_hash[32];  // Hash of previous state
    uint8_t new_state_hash[32];   // Hash of proposed new state
    double total_supply;          // Must be unchanged (conservation)
    double delta_sum;             // Sum of balance changes (must be 0)
    uint64_t timestamp;           // Proposal timestamp
    uint8_t proposer_pubkey[32];  // Who proposed this
    uint8_t proposer_sig[64];     // Ed25519 signature
    uint32_t num_transactions;    // Number of transactions in proposal
} POCProposal;

// Vote on a proposal
typedef enum {
    POC_VOTE_APPROVE = 1,
    POC_VOTE_REJECT = 0,
    POC_VOTE_ABSTAIN = 2
} POCVoteType;

typedef struct {
    uint64_t sequence_num;        // Which proposal this votes on
    uint64_t round;               // Which round
    uint8_t proposal_hash[32];    // Hash of the proposal
    uint8_t validator_pubkey[32]; // Who is voting
    uint8_t signature[64];        // Ed25519 signature of vote
    POCVoteType vote;             // Approve/Reject/Abstain
    uint64_t timestamp;           // When vote was cast
    char reason[128];             // Reason for reject (if applicable)
} POCVote;

// Cross-shard lock for preventing double-spend across shards
typedef struct {
    uint8_t sender_pubkey[32];    // Who is sending
    uint8_t lock_hash[32];        // Hash of the lock
    double amount;                // Amount being locked
    uint8_t source_shard;         // Source shard ID
    uint8_t target_shard;         // Target shard ID
    uint64_t sequence;            // Lock sequence number
    uint64_t expiry;              // When lock expires
    int committed;                // Has this lock been committed?
} POCCrossShardLock;

// Main consensus state
typedef struct {
    // Validator registry
    POCValidator validators[POC_MAX_VALIDATORS];
    uint32_t num_validators;
    
    // Current consensus state
    uint64_t current_height;      // Current finalized height
    uint64_t current_round;       // Current round within height
    POCPhase phase;               // Current phase
    
    // Current proposal being voted on
    POCProposal current_proposal;
    int has_proposal;
    
    // Votes for current proposal
    POCVote votes[POC_MAX_VALIDATORS];
    uint32_t num_votes;
    
    // Leader selection (round-robin)
    uint32_t leader_index;
    
    // Timing
    uint64_t round_start_time;
    uint64_t last_finalized_time;
    
    // Cross-shard locks
    POCCrossShardLock pending_locks[1000];
    uint32_t num_pending_locks;
    
    // Local validator keypair (if this node is a validator)
    uint8_t local_pubkey[32];
    uint8_t local_secret[64];
    int is_validator;
    
    // Conservation tracking
    double expected_total_supply;
} POCConsensus;

// ============ Initialization ============

// Initialize consensus system
PCError poc_init(POCConsensus* consensus);

// Load/save consensus state
PCError poc_load(POCConsensus* consensus, const char* filename);
PCError poc_save(const POCConsensus* consensus, const char* filename);

// ============ Validator Management ============

// Add a validator to the registry
PCError poc_add_validator(POCConsensus* consensus, const uint8_t* pubkey, const char* name);

// Remove a validator from the registry
PCError poc_remove_validator(POCConsensus* consensus, const uint8_t* pubkey);

// Set local validator identity
PCError poc_set_local_validator(POCConsensus* consensus, const uint8_t* pubkey, const uint8_t* secret);

// Get active validator count
uint32_t poc_active_validator_count(const POCConsensus* consensus);

// Check if pubkey is a registered validator
int poc_is_validator(const POCConsensus* consensus, const uint8_t* pubkey);

// Get current leader
POCValidator* poc_get_current_leader(POCConsensus* consensus);

// ============ Consensus Protocol ============

// Propose a state transition (called by leader)
// Verifies conservation before proposing
PCError poc_propose_transition(POCConsensus* consensus, 
                               const PCState* before, 
                               const PCState* after,
                               const PCKeypair* proposer);

// Validate a received proposal
// Returns PC_OK if proposal is valid (conservation verified)
// Returns PC_ERR_CONSERVATION_VIOLATED if physics would be violated
PCError poc_validate_proposal(const POCConsensus* consensus, 
                              const POCProposal* proposal,
                              const PCState* current_state);

// Cast a vote on the current proposal
PCError poc_vote(POCConsensus* consensus, 
                 POCVoteType vote_type, 
                 const char* reason);

// Receive a vote from another validator
PCError poc_receive_vote(POCConsensus* consensus, const POCVote* vote);

// Check if quorum has been reached
// Returns: 1 if approved, -1 if rejected, 0 if still waiting
int poc_check_quorum(const POCConsensus* consensus);

// Finalize the current proposal (call after quorum)
PCError poc_finalize(POCConsensus* consensus, PCState* state);

// Advance to next round
void poc_next_round(POCConsensus* consensus);

// ============ Cross-Shard Operations ============

// Acquire a lock for cross-shard transaction
PCError poc_acquire_lock(POCConsensus* consensus,
                         const uint8_t* sender,
                         double amount,
                         uint8_t source_shard,
                         uint8_t target_shard);

// Release a lock (on success or timeout)
PCError poc_release_lock(POCConsensus* consensus, const uint8_t* lock_hash);

// Check if a sender has a pending lock
int poc_has_pending_lock(const POCConsensus* consensus, const uint8_t* sender);

// ============ Utilities ============

// Hash a proposal
void poc_hash_proposal(const POCProposal* proposal, uint8_t hash[32]);

// Check if it's time for a new round
int poc_should_advance(const POCConsensus* consensus);

// Print consensus status
void poc_print_status(const POCConsensus* consensus);

// Get human-readable phase name
const char* poc_phase_name(POCPhase phase);

#ifdef __cplusplus
}
#endif

#endif // POC_CONSENSUS_H
