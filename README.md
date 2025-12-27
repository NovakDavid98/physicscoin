# PhysicsCoin v2.0

**The world's first physics-based cryptocurrency with streaming payments.**

Replace **500 GB blockchain** with a **244-byte state vector**.

[![Build](https://img.shields.io/badge/build-passing-brightgreen)]()
[![Tests](https://img.shields.io/badge/tests-12%20passed-brightgreen)]()
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

---

## 📐 [Mathematical Foundation →](MATHEMATICAL_FOUNDATION.md)

PhysicsCoin is built on **DiffEqAuth** principles, where cryptocurrency is modeled as a **dynamical system** governed by differential equations rather than blockchain logs.

**Read how:** [$\frac{d\Psi}{dt} = \alpha \cdot I - \beta \cdot R - \gamma \cdot \Psi$](MATHEMATICAL_FOUNDATION.md) became a cryptocurrency.

---

## 🚀 What's New in v2.0

| Feature | Description |
|---------|-------------|
| **Streaming Payments** | Pay-per-second continuous flows |
| **Balance Proofs** | Cryptographic proof of balance at any state |
| **Delta Sync** | Only 100 bytes to sync instead of full state |
| **Transaction Batching** | Process thousands of TX in parallel |

---

## 📊 Performance Visualizations

### Summary

![Performance Summary](datagraphics/summary_infographic.png)

### Storage: 2 Billion to 1 Compression

![Storage Comparison](datagraphics/storage_comparison.png)

### 216K+ Transactions Per Second

![Throughput Comparison](datagraphics/throughput_comparison.png)

### Perfect Energy Conservation

![Conservation Error](datagraphics/conservation_error.png)

### Streaming Payments in Real-Time

![Streaming Payment](datagraphics/streaming_payment.png)

### Horizontal Scaling with Sharding

![Sharding Scaling](datagraphics/sharding_scaling.png)

<details>
<summary>📈 More Visualizations</summary>

#### State Size Scaling
![State Scaling](datagraphics/state_scaling.png)

#### Transaction Latency Distribution
![Latency Distribution](datagraphics/latency_distribution.png)

#### Delta Sync Bandwidth Savings
![Delta Sync](datagraphics/delta_sync.png)

#### Key Generation Speed
![Keygen Speed](datagraphics/keygen_speed.png)

#### Storage Efficiency
![Bytes Per Wallet](datagraphics/bytes_per_wallet.png)

</details>

---

## ⚡ Quick Start

```bash
# Build
make

# Run demo (includes proof generation)
./physicscoin demo

# Streaming payments demo
./physicscoin stream demo
```

---

## 📊 Performance

```
Throughput:      216,230 tx/sec
Latency:         4.6 µs per transaction
State Size:      48 bytes per wallet
Key Generation:  985,000 keys/sec
```

---

## 🎬 Streaming Payments

Pay continuously over time — perfect for:
- Video streaming subscriptions
- API usage billing
- IoT sensor data payments

```bash
# Open stream: pay 0.001 coins/second
./physicscoin stream open <recipient> 0.001

# Settle accumulated payments
./physicscoin stream settle <stream_id>

# Close stream
./physicscoin stream close <stream_id>
```

**Demo output:**
```
Stream Opened: 1.0 coin/sec
t=1: Accumulated: 1.00000000
t=2: Accumulated: 2.00000000
t=3: Accumulated: 3.00000000
t=4: Accumulated: 4.00000000
t=5: Accumulated: 5.00000000
Settlement: ✓ Success
Alice: 995.00000000
Bob:   5.00000000
Conservation: ✓ VERIFIED
```

---

## 🔐 Balance Proofs

Generate cryptographic proofs of balance at any state:

```bash
# Generate proof
./physicscoin prove <address>

# Verify proof
./physicscoin verify-proof proof_abc123.pcp
```

**Output:**
```
Balance Proof:
  State Hash: 0d31b14f52db34b6...
  Balance:    850.00000000
  Proof Hash: aa4f3875a0aab2ca...
  Verification: ✓ VALID
```

---

## 📦 Delta Sync (Light Clients)

Only sync what changed — massively reduces bandwidth:

```bash
./physicscoin delta state_before.pcs state_after.pcs
```

**Output:**
```
Delta size: 100 bytes
Full state: 4900 bytes
Savings: 97.9%
```

---

## 🏗️ Architecture

```
physicscoin/
├── src/
│   ├── core/
│   │   ├── state.c          # Universe state
│   │   ├── proofs.c         # Balance proofs
│   │   ├── streams.c        # Streaming payments
│   │   └── batch.c          # Transaction batching
│   ├── crypto/
│   │   ├── sha256.c         # Self-contained SHA-256
│   │   └── crypto.c         # Key generation
│   ├── utils/
│   │   ├── serialize.c      # State persistence
│   │   └── delta.c          # Delta sync
│   └── cli/main.c           # CLI with all commands
├── tests/                    # 12 test files
└── docs/                     # Whitepaper, reports
```

---

## 📈 Comparison

| Property | Bitcoin | PhysicsCoin |
|----------|---------|-------------|
| Storage | ~550 GB | **244 bytes** |
| Speed | 7 tx/sec | **216,000 tx/sec** |
| Streaming | Not supported | **✓ Native** |
| Balance Proofs | Merkle (complex) | **Self-contained** |
| Light Client | SPV (headers) | **100-byte delta** |

---

## 🧪 Run Tests

```bash
make test-all
```

All 12 tests pass:
- 8 conservation tests
- 4 serialization tests  
- Performance benchmarks

---

## 📖 Documentation

- [Technical Whitepaper](docs/whitepaper.md)
- [Strategic Development Report](docs/strategic_report.md)

---

## ⚠️ Limitations

This is a **proof of concept**:
- Single-node only (no P2P network)
- No consensus mechanism yet
- Simplified cryptography

---

## 📄 License

MIT License - see [LICENSE](LICENSE)
