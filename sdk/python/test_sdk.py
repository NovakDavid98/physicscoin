#!/usr/bin/env python3
"""Test script for PhysicsCoin Python SDK"""

from physicscoin_sdk import PhysicsCoinClient, PhysicsCoinCrypto
import time

def test_sdk():
    print("╔═══════════════════════════════════════════════════════════════╗")
    print("║        PHYSICSCOIN PYTHON SDK TEST SUITE                     ║")
    print("╚═══════════════════════════════════════════════════════════════╝\n")
    
    client = PhysicsCoinClient("http://localhost:8545")
    passed = 0
    failed = 0
    
    # Test 1: Get Network Stats
    try:
        print("TEST 1: Get network stats...")
        stats = client.get_network_stats()
        print(f"  ✓ Wallets: {stats.total_wallets}, Supply: {stats.total_supply}")
        passed += 1
    except Exception as e:
        print(f"  ✗ Failed: {e}")
        failed += 1
    time.sleep(0.1)
    
    # Test 2: Get Supply Analytics
    try:
        print("\nTEST 2: Get supply analytics...")
        supply = client.get_supply_analytics()
        print(f"  ✓ Total supply: {supply['total_supply']}, Active: {supply['active_wallets']}")
        passed += 1
    except Exception as e:
        print(f"  ✗ Failed: {e}")
        failed += 1
    time.sleep(0.1)
    
    # Test 3: Verify Conservation
    try:
        print("\nTEST 3: Verify conservation law...")
        conservation = client.verify_conservation()
        print(f"  ✓ Verified: {conservation['verified']}, Error: {conservation['error']}")
        passed += 1
    except Exception as e:
        print(f"  ✗ Failed: {e}")
        failed += 1
    time.sleep(0.1)
    
    # Test 4: Get Rich List
    try:
        print("\nTEST 4: Get rich list...")
        rich_list = client.get_rich_list()
        print(f"  ✓ Found {len(rich_list)} top wallets")
        if len(rich_list) > 0:
            print(f"    - Top: {rich_list[0]['balance']} (rank {rich_list[0]['rank']})")
        passed += 1
    except Exception as e:
        print(f"  ✗ Failed: {e}")
        failed += 1
    time.sleep(0.1)
    
    # Test 5: Get Health
    try:
        print("\nTEST 5: Get system health...")
        health = client.get_health()
        print(f"  ✓ Status: {health.status}")
        passed += 1
    except Exception as e:
        print(f"  ✗ Failed: {e}")
        failed += 1
    time.sleep(0.1)
    
    # Test 6: Generate Keypair
    try:
        print("\nTEST 6: Generate new keypair...")
        public_key, secret_key = PhysicsCoinCrypto.generate_keypair()
        print(f"  ✓ Public key: {public_key[:16]}...")
        print(f"  ✓ Secret key: {secret_key[:16]}...")
        passed += 1
    except Exception as e:
        print(f"  ✗ Failed: {e}")
        failed += 1
    time.sleep(0.1)
    
    # Test 7: Create Wallet via API
    try:
        print("\nTEST 7: Create wallet via API...")
        result = client.create_wallet()
        print(f"  ✓ Address: {result['address'][:16]}..., Balance: {result['balance']}")
        passed += 1
    except Exception as e:
        print(f"  ✗ Failed: {e}")
        failed += 1
    
    # Summary
    print("\n═══════════════════════════════════════════════════════════════")
    print(f"RESULTS: {passed} passed, {failed} failed")
    print("═══════════════════════════════════════════════════════════════\n")
    
    if failed == 0:
        print("✓ ALL TESTS PASSED\n")
        return 0
    else:
        print("✗ SOME TESTS FAILED\n")
        return 1

if __name__ == "__main__":
    exit(test_sdk())

