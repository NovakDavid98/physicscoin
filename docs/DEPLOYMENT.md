# PhysicsCoin Deployment Guide

## Requirements

- Linux (Ubuntu 24.04 recommended)
- GCC 11+
- libsecp256k1-dev

## Installation

```bash
# Install dependencies
sudo apt-get install -y build-essential libsecp256k1-dev

# Clone repository
git clone https://github.com/NovakDavid98/physicscoin.git
cd physicscoin

# Build
make

# Run tests
./test_consensus
```

## Quick Start

```bash
# Initialize with 1 million coins
./physicscoin init 1000000

# Create a wallet
./physicscoin wallet create

# Check balance
./physicscoin balance <address>

# Send coins
./physicscoin send <to_address> <amount>
```

## Running a Node

### Single Node (Development)
```bash
./physicscoin demo
```

### API Server
```bash
./physicscoin api serve 8545
```

### Multi-Node Demo
```bash
./p2p_demo
```

## Configuration

State is stored in:
- `state.pcs` - Current state
- `wallet.pcw` - Active wallet
- `physicscoin.wal` - Write-ahead log

## CLI Commands

| Command | Description |
|---------|-------------|
| `init <supply>` | Create genesis |
| `wallet create` | New wallet |
| `send <to> <amount>` | Transfer |
| `balance <addr>` | Check balance |
| `state` | Show state |
| `verify` | Verify conservation |
| `consensus demo` | Consensus demo |
| `api serve` | Start API |

## Performance

| Metric | Value |
|--------|-------|
| Throughput | 8,000 tx/sec (ECDSA) |
| State size | ~48 bytes/wallet |
| Sync size | ~100 bytes/delta |

## Security

- secp256k1 ECDSA signatures
- SHA-256 state hashing
- Conservation law enforcement
