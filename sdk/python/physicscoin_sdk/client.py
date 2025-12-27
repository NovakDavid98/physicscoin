"""
Main PhysicsCoin API client
"""

import requests
from typing import Dict, List, Any, Optional
from .models import (
    Wallet,
    Transaction,
    Stream,
    NetworkStats,
    HealthStatus,
    BalanceProof,
)


class PhysicsCoinClient:
    """
    Main client for interacting with PhysicsCoin API.
    
    Example:
        >>> client = PhysicsCoinClient("http://localhost:8545")
        >>> stats = client.get_network_stats()
        >>> print(f"Total supply: {stats.total_supply}")
    """
    
    def __init__(self, base_url: str = "http://localhost:8545", timeout: int = 30):
        """
        Initialize PhysicsCoin client.
        
        Args:
            base_url: API server URL (default: http://localhost:8545)
            timeout: Request timeout in seconds (default: 30)
        """
        self.base_url = base_url.rstrip('/')
        self.timeout = timeout
        self.session = requests.Session()
        self.session.headers.update({
            'Content-Type': 'application/json',
        })
    
    def _get(self, endpoint: str) -> Dict[str, Any]:
        """Make GET request"""
        response = self.session.get(
            f"{self.base_url}{endpoint}",
            timeout=self.timeout
        )
        response.raise_for_status()
        return response.json()
    
    def _post(self, endpoint: str, data: Dict[str, Any]) -> Dict[str, Any]:
        """Make POST request"""
        response = self.session.post(
            f"{self.base_url}{endpoint}",
            json=data,
            timeout=self.timeout
        )
        response.raise_for_status()
        return response.json()
    
    # Network Information
    
    def get_network_stats(self) -> NetworkStats:
        """
        Get network statistics.
        
        Returns:
            NetworkStats object with current network information
        """
        data = self._get('/explorer/stats')
        return NetworkStats(**data)
    
    def get_health(self) -> HealthStatus:
        """
        Get system health status.
        
        Returns:
            HealthStatus object
        """
        data = self._get('/explorer/health')
        return HealthStatus(**data)
    
    def verify_conservation(self) -> Dict[str, Any]:
        """
        Verify conservation law.
        
        Returns:
            dict with 'verified' (bool) and 'error' (float)
        """
        return self._get('/conservation')
    
    # Wallet Operations
    
    def create_wallet(self) -> Dict[str, Any]:
        """
        Create a new wallet.
        
        WARNING: Wallet starts with 0 balance. Must receive funds from existing wallets.
        
        Returns:
            dict with 'mnemonic', 'address', and 'balance' (0)
        """
        return self._post('/wallet/create', {})
    
    def get_wallet(self, address: str) -> Wallet:
        """
        Get wallet balance and information.
        
        Args:
            address: Wallet address (hex public key)
            
        Returns:
            Wallet object
        """
        data = self._get(f'/balance/{address}')
        return Wallet(**data)
    
    def get_wallet_details(self, address: str) -> Wallet:
        """
        Get detailed wallet information including rank and percentage.
        
        Args:
            address: Wallet address
            
        Returns:
            Wallet object with additional details
        """
        data = self._get(f'/explorer/wallet/{address}')
        return Wallet(**data)
    
    # Transactions
    
    def send_transaction(self, transaction: Transaction) -> Dict[str, Any]:
        """
        Send a signed transaction.
        
        Args:
            transaction: Signed Transaction object
            
        Returns:
            dict with 'success' (bool) and 'amount' (float)
        """
        return self._post('/transaction/send', {
            'from': transaction.from_address,
            'to': transaction.to_address,
            'amount': transaction.amount,
            'nonce': transaction.nonce,
            'timestamp': transaction.timestamp,
            'signature': transaction.signature,
        })
    
    def generate_proof(self, address: str) -> BalanceProof:
        """
        Generate a balance proof.
        
        Args:
            address: Wallet address
            
        Returns:
            BalanceProof object
        """
        data = self._post('/proof/generate', {'address': address})
        return BalanceProof(**data)
    
    # Streaming Payments
    
    def open_stream(
        self,
        from_address: str,
        to_address: str,
        rate: float,
        signature: str
    ) -> Dict[str, str]:
        """
        Open a payment stream.
        
        Args:
            from_address: Payer address
            to_address: Receiver address
            rate: Payment rate per second
            signature: Authorization signature
            
        Returns:
            dict with 'stream_id'
        """
        return self._post('/stream/open', {
            'from': from_address,
            'to': to_address,
            'rate': rate,
            'signature': signature,
        })
    
    # Explorer
    
    def get_rich_list(self) -> List[Dict[str, Any]]:
        """
        Get top 20 wallets by balance.
        
        Returns:
            List of wallet dicts with rank, address, balance, percent
        """
        data = self._get('/explorer/rich')
        return data['rich_list']
    
    def get_distribution(self) -> Dict[str, Any]:
        """
        Get balance distribution statistics.
        
        Returns:
            dict with distribution buckets
        """
        return self._get('/explorer/distribution')
    
    def get_supply_analytics(self) -> Dict[str, Any]:
        """
        Get supply analytics.
        
        Returns:
            dict with total_supply, circulating_supply, velocity, etc.
        """
        return self._get('/explorer/supply')
    
    def search(self, query: str) -> Dict[str, Any]:
        """
        Search for address or transaction.
        
        Args:
            query: Search query (address)
            
        Returns:
            Search result dict
        """
        return self._get(f'/explorer/search/{query}')
    
    def get_consensus_info(self) -> Dict[str, Any]:
        """
        Get consensus mechanism information.
        
        Returns:
            dict with consensus details
        """
        return self._get('/explorer/consensus')
    
    def get_state_hash(self) -> Dict[str, Any]:
        """
        Get current state hash information.
        
        Returns:
            dict with current_hash, prev_hash, version, timestamp
        """
        return self._get('/explorer/state/hash')
    
    def close(self):
        """Close the session"""
        self.session.close()
    
    def __enter__(self):
        """Context manager entry"""
        return self
    
    def __exit__(self, *args):
        """Context manager exit"""
        self.close()

