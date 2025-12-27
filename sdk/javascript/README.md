# PhysicsCoin JavaScript/TypeScript SDK

Official SDK for integrating with PhysicsCoin - the physics-based cryptocurrency.

## Installation

```bash
npm install @physicscoin/sdk
# or
yarn add @physicscoin/sdk
```

## Quick Start

```typescript
import { PhysicsCoinClient, PhysicsCoinCrypto } from '@physicscoin/sdk';

// Create client
const client = new PhysicsCoinClient('http://localhost:8545');

// Get network stats
const stats = await client.getNetworkStats();
console.log(`Total supply: ${stats.total_supply}`);

// Get wallet balance
const wallet = await client.getWallet('your-address-here');
console.log(`Balance: ${wallet.balance}`);
```

## Features

- ✅ **TypeScript Support** - Full type definitions
- ✅ **Ed25519 Signatures** - Cryptographic transaction signing
- ✅ **Conservation Law** - Built-in verification
- ✅ **Streaming Payments** - Native support for continuous payments
- ✅ **Block Explorer** - Rich query API
- ✅ **Zero Dependencies** - Minimal footprint

## API Reference

### PhysicsCoinClient

#### Constructor

```typescript
const client = new PhysicsCoinClient(baseURL?: string);
```

#### Methods

**Network Information**

```typescript
// Get network statistics
await client.getNetworkStats(): Promise<NetworkStats>

// Get system health
await client.getHealth(): Promise<HealthStatus>

// Verify conservation law
await client.verifyConservation(): Promise<{ verified: boolean }>
```

**Wallet Operations**

```typescript
// Create new wallet
await client.createWallet(): Promise<{ mnemonic: string, address: string }>

// Get wallet balance
await client.getWallet(address: string): Promise<Wallet>

// Get detailed wallet info (with rank)
await client.getWalletDetails(address: string): Promise<WalletDetails>
```

**Transactions**

```typescript
// Send signed transaction
await client.sendTransaction(transaction: Transaction): Promise<Result>

// Generate balance proof
await client.generateProof(address: string): Promise<Proof>
```

**Streaming Payments**

```typescript
// Open payment stream
await client.openStream(
  from: string,
  to: string,
  rate: number,
  signature: string
): Promise<{ stream_id: string }>
```

**Explorer**

```typescript
// Get rich list (top 20 wallets)
await client.getRichList(): Promise<Wallet[]>

// Get balance distribution
await client.getDistribution(): Promise<Distribution>

// Search for address
await client.search(query: string): Promise<SearchResult>

// Get supply analytics
await client.getSupplyAnalytics(): Promise<SupplyInfo>
```

### PhysicsCoinCrypto

Cryptographic utilities for signing and verification.

```typescript
// Generate new keypair
const { publicKey, secretKey } = PhysicsCoinCrypto.generateKeypair();

// Sign transaction
const signedTx = PhysicsCoinCrypto.signTransaction(
  {
    from: 'sender-address',
    to: 'receiver-address',
    amount: 100.0,
    nonce: 0,
    timestamp: Date.now()
  },
  secretKey
);

// Verify signature
const isValid = PhysicsCoinCrypto.verifyTransaction(signedTx);
```

### Utils

Helper functions for common operations.

```typescript
// Convert to femto-precision
const femto = Utils.toFemto(100.5);

// Convert from femto
const coins = Utils.fromFemto(100500000000000000n);

// Validate address
const valid = Utils.isValidAddress('0123456789abcdef...');

// Format balance
const formatted = Utils.formatBalance(123.45678, 2); // "123.46"
```

## Examples

### Send a Transaction

```typescript
import { PhysicsCoinClient, PhysicsCoinCrypto } from '@physicscoin/sdk';

const client = new PhysicsCoinClient();

// Generate keypair
const { publicKey, secretKey } = PhysicsCoinCrypto.generateKeypair();

// Get current nonce
const wallet = await client.getWallet(publicKey);

// Create and sign transaction
const tx = PhysicsCoinCrypto.signTransaction(
  {
    from: publicKey,
    to: 'receiver-address',
    amount: 50.0,
    nonce: wallet.nonce,
    timestamp: Date.now()
  },
  secretKey
);

// Send transaction
const result = await client.sendTransaction(tx);
console.log('Transaction sent:', result);
```

### Open a Streaming Payment

```typescript
// Open a stream paying 0.001 coins per second
const stream = await client.openStream(
  senderAddress,
  receiverAddress,
  0.001,
  authorizationSignature
);

console.log('Stream opened:', stream.stream_id);
```

### Query Rich List

```typescript
const richList = await client.getRichList();

richList.forEach((wallet, index) => {
  console.log(`#${index + 1}: ${wallet.address} - ${wallet.balance} coins`);
});
```

### Monitor Network Health

```typescript
setInterval(async () => {
  const health = await client.getHealth();
  
  if (health.status === 'healthy') {
    console.log('✓ Network healthy');
    console.log(`  Conservation error: ${health.conservation_error}`);
  } else {
    console.error('✗ Network unhealthy!');
  }
}, 60000); // Check every minute
```

## Advanced Usage

### Conservation Law Verification

PhysicsCoin enforces energy conservation at the protocol level:

```typescript
const { verified, error } = await client.verifyConservation();

if (verified) {
  console.log(`✓ Conservation law verified (error: ${error})`);
} else {
  console.error('✗ Conservation law violated!');
}
```

### Balance Proofs

Generate cryptographic proofs of balance at any state:

```typescript
const proof = await client.generateProof(walletAddress);

console.log('Proof generated:');
console.log(`  Balance: ${proof.balance}`);
console.log(`  State hash: ${proof.state_hash}`);
console.log(`  Timestamp: ${proof.timestamp}`);

// Proofs can be verified independently without trusting the API
```

### Femto-Precision Payments

PhysicsCoin supports 10^-15 precision (1 quadrillion times smaller than Bitcoin's satoshi):

```typescript
// Pay 0.000000000000001 coins
const microPayment = 0.000000000000001;

const tx = PhysicsCoinCrypto.signTransaction(
  {
    from: sender,
    to: receiver,
    amount: microPayment,
    nonce: currentNonce,
    timestamp: Date.now()
  },
  secretKey
);

await client.sendTransaction(tx);
```

## Error Handling

```typescript
try {
  const tx = await client.sendTransaction(signedTx);
} catch (error) {
  if (error.response) {
    // API error
    console.error('API error:', error.response.data.error.message);
  } else if (error.request) {
    // Network error
    console.error('Network error:', error.message);
  } else {
    // Other error
    console.error('Error:', error.message);
  }
}
```

## TypeScript Types

Full TypeScript definitions are included:

```typescript
import type {
  Wallet,
  Transaction,
  Stream,
  NetworkStats,
  SubscriptionPlan
} from '@physicscoin/sdk';
```

## Development

```bash
# Install dependencies
npm install

# Build
npm run build

# Run tests
npm test
```

## License

MIT

## Support

- Documentation: https://github.com/NovakDavid98/physicscoin
- Issues: https://github.com/NovakDavid98/physicscoin/issues
- Discord: Coming soon

## Contributing

Contributions are welcome! Please see CONTRIBUTING.md for details.

