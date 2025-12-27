// test_poc_consensus.c - Test Proof-of-Conservation PBFT Consensus
// Tests the novel consensus mechanism that uses conservation law as source of truth

#include "physicscoin.h"
#include "poc_consensus.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sodium.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) printf("TEST: %-50s ", name)
#define PASS() do { printf("✓ PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("✗ FAIL (%s)\n", msg); tests_failed++; } while(0)

int main(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║     PROOF-OF-CONSERVATION CONSENSUS TEST SUITE                ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
    
    // Initialize sodium
    if (sodium_init() < 0) {
        printf("Error: Failed to initialize libsodium\n");
        return 1;
    }
    
    // ========== Test 1: Consensus Initialization ==========
    TEST("Consensus initialization");
    {
        POCConsensus consensus;
        PCError err = poc_init(&consensus);
        if (err == PC_OK && 
            consensus.phase == POC_PHASE_IDLE &&
            consensus.current_height == 0) {
            PASS();
        } else {
            FAIL("init failed");
        }
    }
    
    // ========== Test 2: Add Validators ==========
    TEST("Add validators");
    {
        POCConsensus consensus;
        poc_init(&consensus);
        
        PCKeypair v1, v2, v3;
        pc_keypair_generate(&v1);
        pc_keypair_generate(&v2);
        pc_keypair_generate(&v3);
        
        PCError e1 = poc_add_validator(&consensus, v1.public_key, "V1");
        PCError e2 = poc_add_validator(&consensus, v2.public_key, "V2");
        PCError e3 = poc_add_validator(&consensus, v3.public_key, "V3");
        
        if (e1 == PC_OK && e2 == PC_OK && e3 == PC_OK &&
            consensus.num_validators == 3 &&
            poc_active_validator_count(&consensus) == 3) {
            PASS();
        } else {
            FAIL("validators not added correctly");
        }
    }
    
    // ========== Test 3: Duplicate Validator Rejected ==========
    TEST("Duplicate validator rejected");
    {
        POCConsensus consensus;
        poc_init(&consensus);
        
        PCKeypair v1;
        pc_keypair_generate(&v1);
        
        poc_add_validator(&consensus, v1.public_key, "V1");
        PCError err = poc_add_validator(&consensus, v1.public_key, "V1Dup");
        
        if (err == PC_ERR_WALLET_EXISTS) {
            PASS();
        } else {
            FAIL("duplicate not rejected");
        }
    }
    
    // ========== Test 4: Leader Selection ==========
    TEST("Round-robin leader selection");
    {
        POCConsensus consensus;
        poc_init(&consensus);
        
        PCKeypair v1, v2, v3;
        pc_keypair_generate(&v1);
        pc_keypair_generate(&v2);
        pc_keypair_generate(&v3);
        
        poc_add_validator(&consensus, v1.public_key, "V1");
        poc_add_validator(&consensus, v2.public_key, "V2");
        poc_add_validator(&consensus, v3.public_key, "V3");
        
        POCValidator* leader1 = poc_get_current_leader(&consensus);
        consensus.leader_index++;
        POCValidator* leader2 = poc_get_current_leader(&consensus);
        consensus.leader_index++;
        POCValidator* leader3 = poc_get_current_leader(&consensus);
        
        if (leader1 && leader2 && leader3 &&
            memcmp(leader1->pubkey, v1.public_key, 32) == 0 &&
            memcmp(leader2->pubkey, v2.public_key, 32) == 0 &&
            memcmp(leader3->pubkey, v3.public_key, 32) == 0) {
            PASS();
        } else {
            FAIL("leader rotation incorrect");
        }
    }
    
    // ========== Test 5: Proposal Hash Computation ==========
    TEST("Proposal hash computation");
    {
        POCProposal p1, p2;
        memset(&p1, 0, sizeof(POCProposal));
        memset(&p2, 0, sizeof(POCProposal));
        
        p1.sequence_num = 1;
        p1.total_supply = 1000.0;
        
        p2.sequence_num = 1;
        p2.total_supply = 1000.0;
        
        uint8_t hash1[32], hash2[32];
        poc_hash_proposal(&p1, hash1);
        poc_hash_proposal(&p2, hash2);
        
        // Same proposals should have same hash
        if (memcmp(hash1, hash2, 32) == 0) {
            // Modify p2 and verify hash changes
            p2.total_supply = 1001.0;
            poc_hash_proposal(&p2, hash2);
            
            if (memcmp(hash1, hash2, 32) != 0) {
                PASS();
            } else {
                FAIL("hash didn't change");
            }
        } else {
            FAIL("identical proposals differ");
        }
    }
    
    // ========== Test 6: Cross-Shard Lock ==========
    TEST("Cross-shard lock acquisition");
    {
        POCConsensus consensus;
        poc_init(&consensus);
        
        PCKeypair sender;
        pc_keypair_generate(&sender);
        
        PCError err = poc_acquire_lock(&consensus, sender.public_key, 100.0, 0, 1);
        
        if (err == PC_OK && 
            consensus.num_pending_locks == 1 &&
            poc_has_pending_lock(&consensus, sender.public_key)) {
            PASS();
        } else {
            FAIL("lock not acquired");
        }
    }
    
    // ========== Test 7: Double Lock Prevented ==========
    TEST("Double lock prevented (same sender)");
    {
        POCConsensus consensus;
        poc_init(&consensus);
        
        PCKeypair sender;
        pc_keypair_generate(&sender);
        
        poc_acquire_lock(&consensus, sender.public_key, 100.0, 0, 1);
        PCError err = poc_acquire_lock(&consensus, sender.public_key, 50.0, 0, 2);
        
        if (err == PC_ERR_WALLET_EXISTS) {
            PASS();
        } else {
            FAIL("double lock not prevented");
        }
    }
    
    // ========== Test 8: Quorum Calculation ==========
    TEST("2/3 quorum calculation");
    {
        POCConsensus consensus;
        poc_init(&consensus);
        
        // Add 4 validators - need 2 approvals (67% of 4 = 2.68, rounds to 2)
        PCKeypair v1, v2, v3, v4;
        pc_keypair_generate(&v1);
        pc_keypair_generate(&v2);
        pc_keypair_generate(&v3);
        pc_keypair_generate(&v4);
        
        poc_add_validator(&consensus, v1.public_key, "V1");
        poc_add_validator(&consensus, v2.public_key, "V2");
        poc_add_validator(&consensus, v3.public_key, "V3");
        poc_add_validator(&consensus, v4.public_key, "V4");
        
        poc_set_local_validator(&consensus, v1.public_key, v1.secret_key);
        
        // Create dummy proposal
        POCProposal* p = &consensus.current_proposal;
        p->sequence_num = 1;
        consensus.has_proposal = 1;
        
        // Add 1 approval - not quorum yet
        consensus.votes[0].vote = POC_VOTE_APPROVE;
        consensus.num_votes = 1;
        
        int check1 = poc_check_quorum(&consensus);
        
        // Add 2nd approval - now quorum (67% of 4 = 2)
        consensus.votes[1].vote = POC_VOTE_APPROVE;
        consensus.num_votes = 2;
        
        int check2 = poc_check_quorum(&consensus);
        
        if (check1 == 0 && check2 == 1) {
            PASS();
        } else {
            FAIL("quorum calculation incorrect");
        }
    }
    
    // ========== Results ==========
    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf("RESULTS: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    if (tests_failed == 0) {
        printf("✓ ALL POC CONSENSUS TESTS PASSED\n\n");
    }
    
    return tests_failed;
}
