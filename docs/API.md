# PhysicsCoin API Documentation

## Overview

PhysicsCoin provides a JSON API for external wallet and application integration.

**Default Port:** 8545

## Starting the API Server

```bash
./physicscoin api serve [port]
```

---

## Endpoints

### GET /status

Returns node status and state summary.

**Response:**
```json
{
  "version": "1.0.0",
  "wallets": 5,
  "total_supply": 1000000.0,
  "timestamp": 1735307000
}
```

---

### GET /wallets

Returns list of all wallets (max 100).

**Response:**
```json
{
  "wallets": [
    {"address": "f92245948e445eef...", "balance": 999000.0},
    {"address": "a1b2c3d4e5f67890...", "balance": 1000.0}
  ]
}
```

---

### GET /balance/{address}

Returns balance for a specific wallet.

**Parameters:**
- `address` - 64-character hex public key

**Response:**
```json
{
  "address": "f92245948e445eef0123456789abcdef0123456789abcdef0123456789abcdef",
  "balance": 999000.0,
  "nonce": 5
}
```

**Errors:**
```json
{"error": {"code": -32602, "message": "Wallet not found"}}
```

---

## Error Codes

| Code | Meaning |
|------|---------|
| -32600 | Invalid request |
| -32601 | Method not found |
| -32602 | Invalid params |

---

## CORS

All endpoints support CORS with:
- `Access-Control-Allow-Origin: *`
- Methods: GET, POST, OPTIONS

---

## Example Usage

```bash
# Check status
curl http://localhost:8545/status

# Get balance
curl http://localhost:8545/balance/f92245948e445eef...

# List wallets
curl http://localhost:8545/wallets
```
