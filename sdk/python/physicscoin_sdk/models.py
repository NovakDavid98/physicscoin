"""
Data models for PhysicsCoin SDK
"""

from dataclasses import dataclass
from typing import Optional, Literal


@dataclass
class Wallet:
    """Wallet information"""
    address: str
    balance: float
    nonce: int
    exists: bool
    rank: Optional[int] = None
    percent_of_supply: Optional[float] = None


@dataclass
class Transaction:
    """Transaction data"""
    from_address: str
    to_address: str
    amount: float
    nonce: int
    timestamp: int
    signature: str


@dataclass
class Stream:
    """Payment stream"""
    stream_id: int
    from_address: str
    to_address: str
    rate: float
    status: Literal['active', 'paused', 'closed']


@dataclass
class NetworkStats:
    """Network statistics"""
    block_height: int
    total_supply: float
    total_wallets: int
    avg_balance: float
    max_balance: float
    validators: int
    state_version: int
    timestamp: int


@dataclass
class SubscriptionPlan:
    """Subscription plan"""
    plan_id: int
    name: str
    description: str
    price: float
    period: Literal['monthly', 'yearly', 'custom']


@dataclass
class HealthStatus:
    """System health status"""
    status: Literal['healthy', 'unhealthy']
    conservation_verified: bool
    conservation_error: float
    total_supply: float
    wallet_sum: float
    wallets: int
    state_version: int


@dataclass
class BalanceProof:
    """Cryptographic balance proof"""
    address: str
    balance: float
    nonce: int
    state_hash: str
    timestamp: int
    exists: bool

