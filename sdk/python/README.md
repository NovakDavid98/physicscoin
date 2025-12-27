# PhysicsCoin Python SDK

Official Python SDK for PhysicsCoin - the physics-based cryptocurrency.

## Installation

```bash
pip install physicscoin-sdk
```

## Quick Start

```python
from physicscoin_sdk import PhysicsCoinClient, PhysicsCoinCrypto
import time

# Create client
client = PhysicsCoinClient('http://localhost:8545')

# Get network stats
stats = client.get_network_stats()
print(f"Total supply: {stats.total_supply}")
print(f"Total wallets: {stats.total_wallets}")

# Get wallet balance
wallet = client.get_wallet('your-address-here')
print(f"Balance: {wallet.balance}")
```

## Features

- ✅ **Type Hints** - Full typing support
- ✅ **Ed25519 Signatures** - Via PyNaCl
- ✅ **Conservation Law** - Built-in verification
- ✅ **Streaming Payments** - Native support
- ✅ **Block Explorer** - Rich query API
- ✅ **Async Support** - Coming soon

## Examples

### Send a Transaction

```python
from physicscoin_sdk import PhysicsCoinClient, PhysicsCoinCrypto
import time

client = PhysicsCoinClient()

# Generate keypair
public_key, secret_key = PhysicsCoinCrypto.generate_keypair()
print(f"New address: {public_key}")

# Get current nonce
wallet = client.get_wallet(public_key)

# Create and sign transaction
tx = PhysicsCoinCrypto.sign_transaction(
    from_address=public_key,
    to_address='receiver-address',
    amount=50.0,
    nonce=wallet.nonce,
    timestamp=int(time.time()),
    secret_key=secret_key
)

# Send transaction
result = client.send_transaction(tx)
print(f"Transaction sent: {result}")
```

### Query Rich List

```python
from physicscoin_sdk import PhysicsCoinClient

client = PhysicsCoinClient()

# Get top wallets
rich_list = client.get_rich_list()

for wallet in rich_list:
    print(f"#{wallet['rank']}: {wallet['address']} - {wallet['balance']} coins")
```

### Monitor Network Health

```python
from physicscoin_sdk import PhysicsCoinClient
import time

client = PhysicsCoinClient()

while True:
    health = client.get_health()
    
    if health.status == 'healthy':
        print('✓ Network healthy')
        print(f"  Conservation error: {health.conservation_error}")
    else:
        print('✗ Network unhealthy!')
    
    time.sleep(60)  # Check every minute
```

### Streaming Payments

```python
# Open a stream paying 0.001 coins per second
stream = client.open_stream(
    from_address=sender_address,
    to_address=receiver_address,
    rate=0.001,
    signature=authorization_signature
)

print(f"Stream opened: {stream['stream_id']}")
```

### Balance Proofs

```python
# Generate cryptographic proof of balance
proof = client.generate_proof(wallet_address)

print(f"Proof generated:")
print(f"  Balance: {proof.balance}")
print(f"  State hash: {proof.state_hash}")
print(f"  Timestamp: {proof.timestamp}")
```

## API Reference

### PhysicsCoinClient

#### Network Information

```python
# Get network statistics
stats = client.get_network_stats()

# Get system health
health = client.get_health()

# Verify conservation law
result = client.verify_conservation()
```

#### Wallet Operations

```python
# Create new wallet
wallet_info = client.create_wallet()

# Get wallet balance
wallet = client.get_wallet(address)

# Get detailed wallet info (with rank)
details = client.get_wallet_details(address)
```

#### Transactions

```python
# Send signed transaction
result = client.send_transaction(transaction)

# Generate balance proof
proof = client.generate_proof(address)
```

#### Explorer

```python
# Get rich list (top 20 wallets)
rich_list = client.get_rich_list()

# Get balance distribution
distribution = client.get_distribution()

# Search for address
result = client.search(query)

# Get supply analytics
supply = client.get_supply_analytics()
```

### PhysicsCoinCrypto

```python
# Generate new keypair
public_key, secret_key = PhysicsCoinCrypto.generate_keypair()

# Sign transaction
tx = PhysicsCoinCrypto.sign_transaction(
    from_address, to_address, amount, nonce, timestamp, secret_key
)

# Verify signature
is_valid = PhysicsCoinCrypto.verify_transaction(tx)

# Derive from mnemonic
seed = PhysicsCoinCrypto.mnemonic_to_seed(mnemonic)
public_key, secret_key = PhysicsCoinCrypto.derive_keypair_from_seed(seed)
```

### Utils

```python
from physicscoin_sdk import Utils

# Convert to femto-precision
femto = Utils.to_femto(100.5)

# Convert from femto
coins = Utils.from_femto(100500000000000000)

# Validate address
valid = Utils.is_valid_address('0123456789abcdef...')

# Format balance
formatted = Utils.format_balance(123.45678, decimals=2)  # "123.46"

# Format address (shortened)
short = Utils.format_address('0123456789abcdef' * 4)  # "0123456789abcdef..."
```

## Advanced Usage

### Context Manager

```python
with PhysicsCoinClient() as client:
    stats = client.get_network_stats()
    print(f"Supply: {stats.total_supply}")
# Client automatically closed
```

### Conservation Law Verification

```python
result = client.verify_conservation()

if result['verified']:
    print(f"✓ Conservation law verified (error: {result['error']})")
else:
    print('✗ Conservation law violated!')
```

### Femto-Precision Payments

PhysicsCoin supports 10^-15 precision:

```python
from physicscoin_sdk import Utils

# Pay 0.000000000000001 coins
micro_payment = 0.000000000000001

# Validate amount
if Utils.validate_amount(micro_payment):
    # Create and send transaction
    tx = PhysicsCoinCrypto.sign_transaction(
        sender, receiver, micro_payment, nonce, timestamp, secret_key
    )
    client.send_transaction(tx)
```

## Error Handling

```python
from physicscoin_sdk import PhysicsCoinClient
from requests.exceptions import RequestException

client = PhysicsCoinClient()

try:
    tx = client.send_transaction(signed_tx)
except RequestException as e:
    print(f"Network error: {e}")
except Exception as e:
    print(f"Error: {e}")
```

## Type Hints

Full typing support:

```python
from physicscoin_sdk import (
    PhysicsCoinClient,
    Wallet,
    Transaction,
    NetworkStats,
)

client: PhysicsCoinClient = PhysicsCoinClient()
wallet: Wallet = client.get_wallet(address)
stats: NetworkStats = client.get_network_stats()
```

## Development

```bash
# Install with dev dependencies
pip install -e ".[dev]"

# Run tests
pytest

# Format code
black physicscoin_sdk/

# Type checking
mypy physicscoin_sdk/
```

## Requirements

- Python >= 3.8
- requests >= 2.28.0
- PyNaCl >= 1.5.0

## License

MIT

## Support

- Documentation: https://github.com/NovakDavid98/physicscoin
- Issues: https://github.com/NovakDavid98/physicscoin/issues
- Discord: Coming soon

## Contributing

Contributions are welcome! Please see CONTRIBUTING.md for details.

