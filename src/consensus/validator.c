// validator.c - Proof of Stake Validator Management
// Part of Conservation-Based Consensus (CBC)

#include "../include/physicscoin.h"
#include "../crypto/sha256.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#define MAX_VALIDATORS 100
#define MIN_STAKE 1000.0
#define MAX_VOTING_POWER_PCT 10  // No validator can have >10% voting power

// Validator status
typedef enum {
    VALIDATOR_ACTIVE,
    VALIDATOR_JAILED,      // Temporarily banned
    VALIDATOR_UNBONDING,   // Waiting to withdraw stake
    VALIDATOR_INACTIVE
} ValidatorStatus;

// Validator record
typedef struct {
    uint8_t pubkey[32];
    double staked_amount;
    uint64_t joined_at;
    uint64_t last_active;
    uint32_t blocks_signed;
    uint32_t blocks_missed;
    uint32_t slashes;
    ValidatorStatus status;
} PCValidator;

// Validator registry
typedef struct {
    PCValidator validators[MAX_VALIDATORS];
    uint32_t count;
    double total_staked;
    uint32_t active_count;
} PCValidatorRegistry;

// Initialize registry
void validator_registry_init(PCValidatorRegistry* reg) {
    if (!reg) return;
    
    memset(reg, 0, sizeof(PCValidatorRegistry));
}

// Find validator by pubkey
static PCValidator* find_validator(PCValidatorRegistry* reg, const uint8_t* pubkey) {
    for (uint32_t i = 0; i < reg->count; i++) {
        if (memcmp(reg->validators[i].pubkey, pubkey, 32) == 0) {
            return &reg->validators[i];
        }
    }
    return NULL;
}

// Register new validator (stake coins)
PCError validator_stake(PCValidatorRegistry* reg, PCState* state,
                        const uint8_t* pubkey, double amount) {
    if (!reg || !state || !pubkey) return PC_ERR_IO;
    if (reg->count >= MAX_VALIDATORS) return PC_ERR_MAX_WALLETS;
    if (amount < MIN_STAKE) return PC_ERR_INSUFFICIENT_FUNDS;
    
    // Check if already a validator
    if (find_validator(reg, pubkey)) {
        return PC_ERR_WALLET_EXISTS;
    }
    
    // Check wallet has enough coins
    PCWallet* wallet = pc_state_get_wallet(state, pubkey);
    if (!wallet || wallet->energy < amount) {
        return PC_ERR_INSUFFICIENT_FUNDS;
    }
    
    // Lock coins (deduct from wallet)
    wallet->energy -= amount;
    
    // Add validator
    PCValidator* v = &reg->validators[reg->count];
    memcpy(v->pubkey, pubkey, 32);
    v->staked_amount = amount;
    v->joined_at = (uint64_t)time(NULL);
    v->last_active = v->joined_at;
    v->blocks_signed = 0;
    v->blocks_missed = 0;
    v->slashes = 0;
    v->status = VALIDATOR_ACTIVE;
    
    reg->count++;
    reg->active_count++;
    reg->total_staked += amount;
    
    printf("Validator staked: %.2f coins (total staked: %.2f)\n", 
           amount, reg->total_staked);
    
    return PC_OK;
}

// Unstake (begin unbonding period)
PCError validator_unstake(PCValidatorRegistry* reg, const uint8_t* pubkey) {
    if (!reg || !pubkey) return PC_ERR_IO;
    
    PCValidator* v = find_validator(reg, pubkey);
    if (!v) return PC_ERR_WALLET_NOT_FOUND;
    
    if (v->status != VALIDATOR_ACTIVE) {
        return PC_ERR_INVALID_SIGNATURE;  // Not active
    }
    
    v->status = VALIDATOR_UNBONDING;
    reg->active_count--;
    
    printf("Validator entering unbonding period\n");
    
    return PC_OK;
}

// Complete unbonding (return stake)
PCError validator_withdraw(PCValidatorRegistry* reg, PCState* state,
                           const uint8_t* pubkey) {
    if (!reg || !state || !pubkey) return PC_ERR_IO;
    
    PCValidator* v = find_validator(reg, pubkey);
    if (!v) return PC_ERR_WALLET_NOT_FOUND;
    
    if (v->status != VALIDATOR_UNBONDING) {
        return PC_ERR_INVALID_SIGNATURE;  // Not unbonding
    }
    
    // Return stake to wallet
    PCWallet* wallet = pc_state_get_wallet(state, pubkey);
    if (!wallet) return PC_ERR_WALLET_NOT_FOUND;
    
    wallet->energy += v->staked_amount;
    reg->total_staked -= v->staked_amount;
    
    v->status = VALIDATOR_INACTIVE;
    v->staked_amount = 0;
    
    printf("Validator withdrawn stake\n");
    
    return PC_OK;
}

// Slash validator (punishment for misbehavior)
PCError validator_slash(PCValidatorRegistry* reg, PCState* state,
                        const uint8_t* pubkey, double slash_pct) {
    if (!reg || !state || !pubkey) return PC_ERR_IO;
    if (slash_pct <= 0 || slash_pct > 100) return PC_ERR_INVALID_AMOUNT;
    
    PCValidator* v = find_validator(reg, pubkey);
    if (!v) return PC_ERR_WALLET_NOT_FOUND;
    
    double slash_amount = v->staked_amount * (slash_pct / 100.0);
    
    v->staked_amount -= slash_amount;
    reg->total_staked -= slash_amount;
    v->slashes++;
    
    // Jail if slashed too many times
    if (v->slashes >= 3) {
        v->status = VALIDATOR_JAILED;
        reg->active_count--;
    }
    
    printf("Validator slashed %.2f%% (%.2f coins)\n", slash_pct, slash_amount);
    
    // Slashed coins are burned (removed from supply)
    state->total_supply -= slash_amount;
    
    return PC_OK;
}

// Calculate voting power (stake-weighted, capped)
double validator_voting_power(const PCValidatorRegistry* reg, const uint8_t* pubkey) {
    if (!reg || !pubkey || reg->total_staked == 0) return 0;
    
    const PCValidator* v = NULL;
    for (uint32_t i = 0; i < reg->count; i++) {
        if (memcmp(reg->validators[i].pubkey, pubkey, 32) == 0) {
            v = &reg->validators[i];
            break;
        }
    }
    
    if (!v || v->status != VALIDATOR_ACTIVE) return 0;
    
    double power = (v->staked_amount / reg->total_staked) * 100.0;
    
    // Cap at MAX_VOTING_POWER_PCT
    if (power > MAX_VOTING_POWER_PCT) {
        power = MAX_VOTING_POWER_PCT;
    }
    
    return power;
}

// Get total voting power of all active validators (should be ~100%)
double validator_total_power(const PCValidatorRegistry* reg) {
    if (!reg) return 0;
    
    double total = 0;
    
    for (uint32_t i = 0; i < reg->count; i++) {
        if (reg->validators[i].status == VALIDATOR_ACTIVE) {
            double power = (reg->validators[i].staked_amount / reg->total_staked) * 100.0;
            if (power > MAX_VOTING_POWER_PCT) power = MAX_VOTING_POWER_PCT;
            total += power;
        }
    }
    
    return total;
}

// Select proposer for checkpoint (round-robin by stake weight)
const PCValidator* validator_select_proposer(const PCValidatorRegistry* reg, 
                                              uint64_t checkpoint_id) {
    if (!reg || reg->active_count == 0) return NULL;
    
    // Simple round-robin among active validators
    uint32_t active_idx = checkpoint_id % reg->active_count;
    uint32_t current_active = 0;
    
    for (uint32_t i = 0; i < reg->count; i++) {
        if (reg->validators[i].status == VALIDATOR_ACTIVE) {
            if (current_active == active_idx) {
                return &reg->validators[i];
            }
            current_active++;
        }
    }
    
    return NULL;
}

// Record that validator signed a checkpoint
void validator_record_sign(PCValidatorRegistry* reg, const uint8_t* pubkey) {
    PCValidator* v = find_validator(reg, pubkey);
    if (v) {
        v->blocks_signed++;
        v->last_active = (uint64_t)time(NULL);
    }
}

// Record that validator missed signing
void validator_record_miss(PCValidatorRegistry* reg, const uint8_t* pubkey) {
    PCValidator* v = find_validator(reg, pubkey);
    if (v) {
        v->blocks_missed++;
    }
}

// Print registry
void validator_registry_print(const PCValidatorRegistry* reg) {
    if (!reg) return;
    
    printf("\n╔═══════════════════════════════════════════════════════════╗\n");
    printf("║              VALIDATOR REGISTRY                           ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
    
    printf("Total validators: %u (active: %u)\n", reg->count, reg->active_count);
    printf("Total staked: %.2f coins\n\n", reg->total_staked);
    
    printf("┌──────────┬────────────┬────────────┬────────────┬──────────┐\n");
    printf("│ Pubkey   │ Staked     │ Power      │ Signed     │ Status   │\n");
    printf("├──────────┼────────────┼────────────┼────────────┼──────────┤\n");
    
    for (uint32_t i = 0; i < reg->count; i++) {
        const PCValidator* v = &reg->validators[i];
        
        const char* status_str = "?";
        switch (v->status) {
            case VALIDATOR_ACTIVE: status_str = "ACTIVE"; break;
            case VALIDATOR_JAILED: status_str = "JAILED"; break;
            case VALIDATOR_UNBONDING: status_str = "UNBOND"; break;
            case VALIDATOR_INACTIVE: status_str = "INACT"; break;
        }
        
        printf("│ ");
        for (int j = 0; j < 4; j++) printf("%02x", v->pubkey[j]);
        
        double power = (reg->total_staked > 0) ? 
                       (v->staked_amount / reg->total_staked) * 100.0 : 0;
        if (power > MAX_VOTING_POWER_PCT) power = MAX_VOTING_POWER_PCT;
        
        printf(" │ %10.2f │ %9.1f%% │ %10u │ %-8s │\n",
               v->staked_amount, power, v->blocks_signed, status_str);
    }
    
    printf("└──────────┴────────────┴────────────┴────────────┴──────────┘\n");
}
