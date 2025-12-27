"""
Cryptographic utilities for PhysicsCoin
"""

import hashlib
import struct
from typing import Tuple
from nacl.signing import SigningKey, VerifyKey
from nacl.encoding import HexEncoder
from .models import Transaction


class PhysicsCoinCrypto:
    """
    Cryptographic utilities for signing and verification.
    
    Uses Ed25519 signatures via PyNaCl.
    """
    
    @staticmethod
    def generate_keypair() -> Tuple[str, str]:
        """
        Generate a new Ed25519 keypair.
        
        Returns:
            Tuple of (public_key_hex, secret_key_hex)
            
        Example:
            >>> public_key, secret_key = PhysicsCoinCrypto.generate_keypair()
            >>> print(f"Address: {public_key}")
        """
        signing_key = SigningKey.generate()
        public_key = signing_key.verify_key.encode(encoder=HexEncoder).decode('utf-8')
        secret_key = signing_key.encode(encoder=HexEncoder).decode('utf-8')
        return public_key, secret_key
    
    @staticmethod
    def sign_transaction(
        from_address: str,
        to_address: str,
        amount: float,
        nonce: int,
        timestamp: int,
        secret_key: str
    ) -> Transaction:
        """
        Sign a transaction.
        
        Args:
            from_address: Sender address (hex)
            to_address: Receiver address (hex)
            amount: Amount to send
            nonce: Current nonce
            timestamp: Unix timestamp
            secret_key: Sender's secret key (hex)
            
        Returns:
            Signed Transaction object
            
        Example:
            >>> tx = PhysicsCoinCrypto.sign_transaction(
            ...     from_addr, to_addr, 100.0, 0, 
            ...     int(time.time()), secret_key
            ... )
            >>> client.send_transaction(tx)
        """
        # Build message to sign
        message = b''
        message += bytes.fromhex(from_address)
        message += bytes.fromhex(to_address)
        message += struct.pack('d', amount)
        message += struct.pack('Q', nonce)
        message += struct.pack('Q', timestamp)
        
        # Sign with Ed25519
        signing_key = SigningKey(secret_key, encoder=HexEncoder)
        signed = signing_key.sign(message)
        signature = signed.signature.hex()
        
        return Transaction(
            from_address=from_address,
            to_address=to_address,
            amount=amount,
            nonce=nonce,
            timestamp=timestamp,
            signature=signature
        )
    
    @staticmethod
    def verify_transaction(transaction: Transaction) -> bool:
        """
        Verify a transaction signature.
        
        Args:
            transaction: Transaction object with signature
            
        Returns:
            True if signature is valid, False otherwise
        """
        try:
            # Rebuild message
            message = b''
            message += bytes.fromhex(transaction.from_address)
            message += bytes.fromhex(transaction.to_address)
            message += struct.pack('d', transaction.amount)
            message += struct.pack('Q', transaction.nonce)
            message += struct.pack('Q', transaction.timestamp)
            
            # Verify signature
            verify_key = VerifyKey(transaction.from_address, encoder=HexEncoder)
            verify_key.verify(message, bytes.fromhex(transaction.signature))
            return True
        except Exception:
            return False
    
    @staticmethod
    def mnemonic_to_seed(mnemonic: str) -> bytes:
        """
        Convert mnemonic to seed (simplified BIP39-like).
        
        NOTE: For production, use a proper BIP39 library.
        
        Args:
            mnemonic: Space-separated mnemonic words
            
        Returns:
            32-byte seed
        """
        # Simplified implementation
        return hashlib.sha256(mnemonic.encode('utf-8')).digest()
    
    @staticmethod
    def derive_keypair_from_seed(seed: bytes, index: int = 0) -> Tuple[str, str]:
        """
        Derive keypair from seed.
        
        Args:
            seed: 32-byte seed
            index: Derivation index
            
        Returns:
            Tuple of (public_key_hex, secret_key_hex)
        """
        # Simplified HD derivation
        derived = hashlib.sha256(seed + struct.pack('I', index)).digest()
        signing_key = SigningKey(derived[:32])
        public_key = signing_key.verify_key.encode(encoder=HexEncoder).decode('utf-8')
        secret_key = signing_key.encode(encoder=HexEncoder).decode('utf-8')
        return public_key, secret_key

