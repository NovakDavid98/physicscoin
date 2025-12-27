# PhysicsCoin Testnet Guide

## Quick Start

### 1. Start Testnet Node

```bash
# Start a single testnet node with API
./physicscoin --network testnet api serve 18545
```

The testnet will start with:
- **Genesis Supply**: 10,000,000 PCS
- **API Port**: 18545
- **P2P Port**: 19333
- **Faucet**: Enabled (100 PCS per request, 1 hour cooldown)

### 2. Using the Web Wallet

```bash
cd web
npm install
npm run dev
```

Open http://localhost:5173 in your browser:
1. Select "Testnet" network
2. Click "Create New Wallet"
3. Save your 12-word recovery phrase
4. Click the "💧 Faucet" button to get 100 test coins
5. Start sending transactions!

### 3. Using the CLI

```bash
# Check network info
./physicscoin --network testnet network info

# Create a wallet
./physicscoin --network testnet wallet create

# Check balance
./physicscoin --network testnet balance <address>
```

## Network Types

PhysicsCoin supports three network types:

| Network | Port | API Port | Supply | Faucet | Purpose |
|---------|------|----------|--------|--------|---------|
| **Mainnet** | 9333 | 8545 | 21M | ❌ No | Production |
| **Testnet** | 19333 | 18545 | 10M | ✅ Yes | Public testing |
| **Devnet** | 29333 | 28545 | 1M | ✅ Yes | Local development |

## Faucet API

### Get Faucet Info

```bash
curl http://localhost:18545/faucet/info | jq .
```

Response:
```json
{
  "enabled": true,
  "amount": 100.00000000,
  "cooldown": 3600,
  "network": "testnet"
}
```

### Request Faucet Funds

```bash
curl -X POST http://localhost:18545/faucet/request \
  -H "Content-Type: application/json" \
  -d '{"address":"YOUR_WALLET_ADDRESS"}' | jq .
```

Response (Success):
```json
{
  "success": true,
  "address": "3b92f50f...",
  "amount": 100.00000000,
  "message": "Faucet funds sent successfully"
}
```

Response (Rate Limited):
```json
{
  "error": {
    "code": -32000,
    "message": "Faucet cooldown active. Try again in 3542 seconds"
  }
}
```

## Multi-Node Testnet

### Start 3-Node Testnet

```bash
./scripts/start-testnet.sh
```

This will start 3 nodes:
- Node 1: http://localhost:18545
- Node 2: http://localhost:18546
- Node 3: http://localhost:18547

### Stop Testnet

```bash
./scripts/stop-testnet.sh
```

### Custom Node Setup

```bash
# Start node 1 (seed node)
./physicscoin --network testnet node start --port 19333 &
./physicscoin --network testnet api serve 18545 &

# Start node 2 (connect to seed)
./physicscoin --network testnet node start --port 19334 --connect localhost:19333 &
./physicscoin --network testnet api serve 18546 &
```

## Frontend Integration

The React frontend automatically supports all networks:

### Features

- **Network Selector**: Switch between mainnet, testnet, devnet
- **Faucet Button**: Get test coins (testnet/devnet only)
- **Real-time Balance**: Auto-updates every 10 seconds
- **Transaction History**: View all your transactions
- **Streaming Payments**: Continuous payment flows
- **Balance Proofs**: Cryptographic balance verification
- **HD Wallet**: 12-word mnemonic backup

### API Configuration

The frontend automatically connects to the correct port based on network:

```typescript
// Automatic network detection
const ports = { testnet: 18545, mainnet: 8545, devnet: 28545 }
API.setApiUrl(`http://localhost:${ports[network]}`)
```

## Testing Scenarios

### Scenario 1: Basic Transaction Flow

```bash
# 1. Create two wallets
ALICE=$(./physicscoin --network testnet wallet create | grep "Address:" | awk '{print $2}')
BOB=$(./physicscoin --network testnet wallet create | grep "Address:" | awk '{print $2}')

# 2. Fund Alice from faucet
curl -X POST http://localhost:18545/faucet/request \
  -H "Content-Type: application/json" \
  -d "{\"address\":\"$ALICE\"}"

# 3. Check Alice's balance
curl http://localhost:18545/balance/$ALICE | jq .

# 4. Send transaction (requires signing)
# Use the web wallet or SDK for this step

# 5. Check both balances
curl http://localhost:18545/balance/$ALICE | jq .
curl http://localhost:18545/balance/$BOB | jq .
```

### Scenario 2: Streaming Payment

```bash
# Open a payment stream (1 PCS/second)
curl -X POST http://localhost:18545/stream/open \
  -H "Content-Type: application/json" \
  -d '{
    "from": "SENDER_ADDRESS",
    "to": "RECEIVER_ADDRESS",
    "rate": 1.0
  }'
```

### Scenario 3: Balance Proof

```bash
# Generate cryptographic proof of balance
curl -X POST http://localhost:18545/proof/generate \
  -H "Content-Type: application/json" \
  -d '{"address":"YOUR_ADDRESS"}' | jq .
```

## Explorer API

All 11 explorer endpoints work on testnet:

```bash
# Network statistics
curl http://localhost:18545/explorer/stats | jq .

# Supply analytics
curl http://localhost:18545/explorer/supply | jq .

# Top 10 wallets
curl http://localhost:18545/explorer/wallets/top/10 | jq .

# Conservation law check
curl http://localhost:18545/explorer/conservation_check | jq .

# System health
curl http://localhost:18545/explorer/health | jq .
```

## SDKs

### JavaScript/TypeScript SDK

```bash
cd sdk/javascript
npm install
npm run build
```

```typescript
import { PhysicsCoinClient, PhysicsCoinCrypto } from '@physicscoin/sdk'

// Connect to testnet
const client = new PhysicsCoinClient('http://localhost:18545')

// Get network stats
const stats = await client.getNetworkStats()
console.log(`Supply: ${stats.total_supply}`)

// Request faucet (testnet only)
const keypair = PhysicsCoinCrypto.generateKeypair()
// Note: Faucet request requires backend implementation

// Check balance
const wallet = await client.getWallet(keypair.publicKey)
console.log(`Balance: ${wallet.balance}`)
```

### Python SDK

```bash
cd sdk/python
python3 -m venv venv
source venv/bin/activate
pip install -e .
```

```python
from physicscoin_sdk import PhysicsCoinClient, PhysicsCoinCrypto

# Connect to testnet
client = PhysicsCoinClient('http://localhost:18545')

# Get network stats
stats = client.get_network_stats()
print(f"Supply: {stats.total_supply}")

# Generate keypair
public_key, secret_key = PhysicsCoinCrypto.generate_keypair()

# Request faucet
# result = client.request_faucet(public_key)  # Implement in SDK
```

## Troubleshooting

### Port Already in Use

```bash
# Find process using port 18545
sudo lsof -i :18545

# Kill process
kill -9 <PID>
```

### API Not Responding

```bash
# Check if node is running
ps aux | grep physicscoin

# Check logs
tail -f testnet_node1.log

# Restart node
./physicscoin --network testnet api serve 18545
```

### Faucet Rate Limited

The faucet has a 1-hour cooldown per address. Either:
- Wait for cooldown to expire
- Create a new wallet
- Use devnet (60 second cooldown)

### Conservation Law Error

If you see conservation violations:

```bash
# Verify conservation
curl http://localhost:18545/explorer/conservation_check | jq .

# Check state integrity
curl http://localhost:18545/conservation | jq .
```

## Best Practices

1. **Always use testnet for development**: Never test with real mainnet funds
2. **Save your mnemonic**: Write down your 12-word recovery phrase
3. **Check faucet cooldown**: Wait between faucet requests
4. **Monitor node health**: Use `/explorer/health` endpoint
5. **Verify transactions**: Check balances after sending

## Network Reset

To completely reset testnet state:

```bash
# Stop all nodes
./scripts/stop-testnet.sh

# Delete all state files
rm -f state_testnet.pcs
rm -f faucet_history.dat
rm -f testnet_node_*/state_testnet.pcs

# Restart fresh
./physicscoin --network testnet api serve 18545
```

## Support

- **Documentation**: [/docs](/docs)
- **GitHub**: https://github.com/NovakDavid98/physicscoin
- **Issues**: Report bugs on GitHub Issues

## Next Steps

1. ✅ Start testnet node
2. ✅ Get test coins from faucet
3. ✅ Send your first transaction
4. ✅ Try streaming payments
5. ✅ Generate a balance proof
6. ✅ Explore the API
7. ✅ Build your own application

Happy testing! 🚀

