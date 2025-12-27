"""
PhysicsCoin Python SDK

Official Python SDK for PhysicsCoin - the physics-based cryptocurrency.

Features:
- Full API client with type hints
- Ed25519 transaction signing
- Conservation law verification
- Streaming payments
- Block explorer integration
"""

__version__ = "1.0.0"
__author__ = "PhysicsCoin Team"

from .client import PhysicsCoinClient
from .crypto import PhysicsCoinCrypto
from .models import (
    Wallet,
    Transaction,
    Stream,
    NetworkStats,
    SubscriptionPlan,
)
from .utils import Utils

__all__ = [
    "PhysicsCoinClient",
    "PhysicsCoinCrypto",
    "Wallet",
    "Transaction",
    "Stream",
    "NetworkStats",
    "SubscriptionPlan",
    "Utils",
]

