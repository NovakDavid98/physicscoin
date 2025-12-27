// Test script for PhysicsCoin JavaScript SDK
const { PhysicsCoinClient, PhysicsCoinCrypto } = require('./dist/index.js');

// Helper to add delay between tests
const delay = (ms) => new Promise(resolve => setTimeout(resolve, ms));

async function testSDK() {
    console.log("╔═══════════════════════════════════════════════════════════════╗");
    console.log("║      PHYSICSCOIN JAVASCRIPT SDK TEST SUITE                    ║");
    console.log("╚═══════════════════════════════════════════════════════════════╝\n");

    const api = new PhysicsCoinClient('http://localhost:8545');
    let passed = 0;
    let failed = 0;

    // Test 1: Get Stats
    try {
        console.log("TEST 1: Get network stats...");
        const stats = await api.getNetworkStats();
        console.log(`  ✓ Wallets: ${stats.total_wallets}, Supply: ${stats.total_supply}`);
        passed++;
    } catch (error) {
        console.log(`  ✗ Failed: ${error.message}`);
        failed++;
    }
    await delay(100);

    // Test 2: Get Supply Analytics
    try {
        console.log("\nTEST 2: Get supply analytics...");
        const supply = await api.getSupplyAnalytics();
        console.log(`  ✓ Total supply: ${supply.total_supply}, Active: ${supply.active_wallets}`);
        passed++;
    } catch (error) {
        console.log(`  ✗ Failed: ${error.message}`);
        failed++;
    }
    await delay(100);

    // Test 3: Verify Conservation
    try {
        console.log("\nTEST 3: Verify conservation law...");
        const conservation = await api.verifyConservation();
        console.log(`  ✓ Verified: ${conservation.verified}, Error: ${conservation.error}`);
        passed++;
    } catch (error) {
        console.log(`  ✗ Failed: ${error.message}`);
        failed++;
    }
    await delay(100);

    // Test 4: Get Rich List
    try {
        console.log("\nTEST 4: Get rich list...");
        const richList = await api.getRichList();
        console.log(`  ✓ Found ${richList.length} top wallets`);
        if (richList.length > 0) {
            console.log(`    - Top: ${richList[0].balance} (rank ${richList[0].rank})`);
        }
        passed++;
    } catch (error) {
        console.log(`  ✗ Failed: ${error.message}`);
        failed++;
    }
    await delay(100);

    // Test 5: Get Health
    try {
        console.log("\nTEST 5: Get system health...");
        const health = await api.getHealth();
        console.log(`  ✓ Status: ${health.status}`);
        passed++;
    } catch (error) {
        console.log(`  ✗ Failed: ${error.message}`);
        failed++;
    }
    await delay(100);

    // Test 6: Generate Keypair
    try {
        console.log("\nTEST 6: Generate new keypair...");
        const keypair = PhysicsCoinCrypto.generateKeypair();
        console.log(`  ✓ Public key: ${keypair.publicKey.substring(0, 16)}...`);
        console.log(`  ✓ Secret key: ${keypair.secretKey.substring(0, 16)}...`);
        passed++;
    } catch (error) {
        console.log(`  ✗ Failed: ${error.message}`);
        failed++;
    }
    await delay(100);

    // Test 7: Create wallet via API
    try {
        console.log("\nTEST 7: Create wallet via API...");
        const result = await api.createWallet();
        console.log(`  ✓ Address: ${result.address.substring(0, 16)}..., Balance: ${result.balance}`);
        passed++;
    } catch (error) {
        console.log(`  ✗ Failed: ${error.message}`);
        failed++;
    }

    // Summary
    console.log("\n═══════════════════════════════════════════════════════════════");
    console.log(`RESULTS: ${passed} passed, ${failed} failed`);
    console.log("═══════════════════════════════════════════════════════════════\n");

    if (failed === 0) {
        console.log("✓ ALL TESTS PASSED\n");
        process.exit(0);
    } else {
        console.log("✗ SOME TESTS FAILED\n");
        process.exit(1);
    }
}

testSDK().catch(err => {
    console.error("Fatal error:", err);
    process.exit(1);
});

