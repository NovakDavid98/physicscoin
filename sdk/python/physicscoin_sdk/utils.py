"""
Utility functions for PhysicsCoin
"""

import re
from decimal import Decimal


class Utils:
    """Helper utilities for PhysicsCoin operations"""
    
    @staticmethod
    def to_femto(coins: float) -> int:
        """
        Convert coins to femto-precision (10^-15).
        
        Args:
            coins: Amount in coins
            
        Returns:
            Amount in femto units
            
        Example:
            >>> femto = Utils.to_femto(100.5)
            >>> print(femto)  # 100500000000000000
        """
        return int(Decimal(str(coins)) * Decimal('1e15'))
    
    @staticmethod
    def from_femto(femto: int) -> float:
        """
        Convert from femto-precision to coins.
        
        Args:
            femto: Amount in femto units
            
        Returns:
            Amount in coins
        """
        return float(Decimal(femto) / Decimal('1e15'))
    
    @staticmethod
    def is_valid_address(address: str) -> bool:
        """
        Validate address format (64 hex characters).
        
        Args:
            address: Address string
            
        Returns:
            True if valid, False otherwise
        """
        pattern = r'^[0-9a-fA-F]{64}$'
        return bool(re.match(pattern, address))
    
    @staticmethod
    def format_balance(balance: float, decimals: int = 8) -> str:
        """
        Format balance for display.
        
        Args:
            balance: Balance amount
            decimals: Number of decimal places (default: 8)
            
        Returns:
            Formatted string
        """
        return f"{balance:.{decimals}f}"
    
    @staticmethod
    def calculate_fee(amount: float) -> float:
        """
        Calculate transaction fee.
        
        NOTE: PhysicsCoin currently has no fees due to conservation law.
        
        Args:
            amount: Transaction amount
            
        Returns:
            Fee amount (always 0 in PhysicsCoin)
        """
        return 0.0  # No fees in PhysicsCoin
    
    @staticmethod
    def format_address(address: str, length: int = 16) -> str:
        """
        Format address for display (shortened).
        
        Args:
            address: Full address
            length: Number of characters to show from start
            
        Returns:
            Shortened address with ellipsis
        """
        if len(address) <= length:
            return address
        return f"{address[:length]}..."
    
    @staticmethod
    def validate_amount(amount: float, min_amount: float = 0.000000000000001) -> bool:
        """
        Validate transaction amount.
        
        Args:
            amount: Amount to validate
            min_amount: Minimum valid amount (default: 10^-15)
            
        Returns:
            True if valid, False otherwise
        """
        return amount >= min_amount
    
    @staticmethod
    def seconds_to_readable(seconds: int) -> str:
        """
        Convert seconds to human-readable format.
        
        Args:
            seconds: Number of seconds
            
        Returns:
            Readable string (e.g., "2h 30m", "3d")
        """
        if seconds < 60:
            return f"{seconds}s"
        elif seconds < 3600:
            return f"{seconds // 60}m"
        elif seconds < 86400:
            return f"{seconds // 3600}h"
        else:
            return f"{seconds // 86400}d"

