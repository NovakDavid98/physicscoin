/**
 * PhysicsCoin JavaScript/TypeScript SDK
 * 
 * Official SDK for integrating with PhysicsCoin - the physics-based cryptocurrency
 * 
 * @packageDocumentation
 */

import axios, { AxiosInstance } from 'axios';
import nacl from 'tweetnacl';

/**
 * Wallet interface
 */
export interface Wallet {
  address: string;
  balance: number;
  nonce: number;
  exists: boolean;
}

/**
 * Transaction interface
 */
export interface Transaction {
  from: string;
  to: string;
  amount: number;
  nonce: number;
  timestamp: number;
  signature: string;
}

/**
 * Stream interface
 */
export interface Stream {
  stream_id: number;
  from: string;
  to: string;
  rate: number;
  status: 'active' | 'paused' | 'closed';
}

/**
 * Network stats interface
 */
export interface NetworkStats {
  block_height: number;
  total_supply: number;
  total_wallets: number;
  avg_balance: number;
  max_balance: number;
  validators: number;
  state_version: number;
  timestamp: number;
}

/**
 * Subscription plan interface
 */
export interface SubscriptionPlan {
  plan_id: number;
  name: string;
  description: string;
  price: number;
  period: 'monthly' | 'yearly' | 'custom';
}

/**
 * Main PhysicsCoin SDK client
 */
export class PhysicsCoinClient {
  private api: AxiosInstance;
  private baseURL: string;

  /**
   * Create a new PhysicsCoin client
   * @param baseURL - API server URL (default: http://localhost:8545)
   */
  constructor(baseURL: string = 'http://localhost:8545') {
    this.baseURL = baseURL;
    this.api = axios.create({
      baseURL,
      timeout: 30000,
      headers: {
        'Content-Type': 'application/json',
      },
    });
  }

  /**
   * Get network statistics
   */
  async getNetworkStats(): Promise<NetworkStats> {
    const response = await this.api.get('/explorer/stats');
    return response.data;
  }

  /**
   * Get wallet information
   * @param address - Wallet address (hex public key)
   */
  async getWallet(address: string): Promise<Wallet> {
    const response = await this.api.get(`/balance/${address}`);
    return response.data;
  }

  /**
   * Get detailed wallet info from explorer
   * @param address - Wallet address
   */
  async getWalletDetails(address: string): Promise<Wallet & { rank: number; percent_of_supply: number }> {
    const response = await this.api.get(`/explorer/wallet/${address}`);
    return response.data;
  }

  /**
   * Get rich list (top wallets)
   */
  async getRichList(): Promise<any[]> {
    const response = await this.api.get('/explorer/rich');
    return response.data.rich_list;
  }

  /**
   * Get balance distribution
   */
  async getDistribution(): Promise<any> {
    const response = await this.api.get('/explorer/distribution');
    return response.data;
  }

  /**
   * Get supply analytics
   */
  async getSupplyAnalytics(): Promise<any> {
    const response = await this.api.get('/explorer/supply');
    return response.data;
  }

  /**
   * Get system health
   */
  async getHealth(): Promise<any> {
    const response = await this.api.get('/explorer/health');
    return response.data;
  }

  /**
   * Verify conservation law
   */
  async verifyConservation(): Promise<{ verified: boolean; error: number }> {
    const response = await this.api.get('/conservation');
    return response.data;
  }

  /**
   * Create a new wallet
   * WARNING: This creates a wallet with 0 balance. You must receive funds from existing wallets.
   */
  async createWallet(): Promise<{ mnemonic: string; address: string; balance: number }> {
    const response = await this.api.post('/wallet/create');
    return response.data;
  }

  /**
   * Send a signed transaction
   * @param transaction - Signed transaction object
   */
  async sendTransaction(transaction: Transaction): Promise<{ success: boolean; amount: number }> {
    const response = await this.api.post('/transaction/send', {
      from: transaction.from,
      to: transaction.to,
      amount: transaction.amount,
      nonce: transaction.nonce,
      timestamp: transaction.timestamp,
      signature: transaction.signature,
    });
    return response.data;
  }

  /**
   * Generate a balance proof
   * @param address - Wallet address
   */
  async generateProof(address: string): Promise<any> {
    const response = await this.api.post('/proof/generate', { address });
    return response.data;
  }

  /**
   * Open a payment stream
   * @param from - Payer address
   * @param to - Receiver address
   * @param rate - Payment rate per second
   * @param signature - Authorization signature
   */
  async openStream(from: string, to: string, rate: number, signature: string): Promise<{ stream_id: string }> {
    const response = await this.api.post('/stream/open', {
      from,
      to,
      rate,
      signature,
    });
    return response.data;
  }

  /**
   * Search for address or transaction
   * @param query - Search query
   */
  async search(query: string): Promise<any> {
    const response = await this.api.get(`/explorer/search/${query}`);
    return response.data;
  }
}

/**
 * Cryptographic utilities for signing transactions
 */
export class PhysicsCoinCrypto {
  /**
   * Generate a new keypair
   */
  static generateKeypair(): { publicKey: string; secretKey: string } {
    const keypair = nacl.sign.keyPair();
    return {
      publicKey: Buffer.from(keypair.publicKey).toString('hex'),
      secretKey: Buffer.from(keypair.secretKey).toString('hex'),
    };
  }

  /**
   * Sign a transaction
   * @param transaction - Transaction data
   * @param secretKey - Secret key (hex string)
   */
  static signTransaction(
    transaction: Omit<Transaction, 'signature'>,
    secretKey: string
  ): Transaction {
    // Create message to sign
    const message = Buffer.concat([
      Buffer.from(transaction.from, 'hex'),
      Buffer.from(transaction.to, 'hex'),
      Buffer.from(new Float64Array([transaction.amount]).buffer),
      Buffer.from(new BigUint64Array([BigInt(transaction.nonce)]).buffer),
      Buffer.from(new BigUint64Array([BigInt(transaction.timestamp)]).buffer),
    ]);

    // Sign with Ed25519
    const secretKeyBytes = Buffer.from(secretKey, 'hex');
    const signature = nacl.sign.detached(message, secretKeyBytes);

    return {
      ...transaction,
      signature: Buffer.from(signature).toString('hex'),
    };
  }

  /**
   * Verify a transaction signature
   * @param transaction - Signed transaction
   */
  static verifyTransaction(transaction: Transaction): boolean {
    // Reconstruct message
    const message = Buffer.concat([
      Buffer.from(transaction.from, 'hex'),
      Buffer.from(transaction.to, 'hex'),
      Buffer.from(new Float64Array([transaction.amount]).buffer),
      Buffer.from(new BigUint64Array([BigInt(transaction.nonce)]).buffer),
      Buffer.from(new BigUint64Array([BigInt(transaction.timestamp)]).buffer),
    ]);

    const publicKey = Buffer.from(transaction.from, 'hex');
    const signature = Buffer.from(transaction.signature, 'hex');

    return nacl.sign.detached.verify(message, signature, publicKey);
  }

  /**
   * Convert mnemonic to seed (simplified BIP39-like)
   * NOTE: For production, use a proper BIP39 library
   */
  static mnemonicToSeed(mnemonic: string): Buffer {
    // This is a simplified implementation
    // In production, use proper BIP39/BIP32 libraries
    const crypto = require('crypto');
    return crypto.createHash('sha256').update(mnemonic).digest();
  }
}

/**
 * Helper utilities
 */
export class Utils {
  /**
   * Convert coins to smallest unit (femto-precision)
   */
  static toFemto(coins: number): bigint {
    return BigInt(Math.floor(coins * 1e15));
  }

  /**
   * Convert from smallest unit to coins
   */
  static fromFemto(femto: bigint): number {
    return Number(femto) / 1e15;
  }

  /**
   * Validate address format
   */
  static isValidAddress(address: string): boolean {
    return /^[0-9a-f]{64}$/i.test(address);
  }

  /**
   * Format balance for display
   */
  static formatBalance(balance: number, decimals: number = 8): string {
    return balance.toFixed(decimals);
  }

  /**
   * Calculate transaction fee (if applicable)
   * NOTE: PhysicsCoin currently has no fees due to conservation law
   */
  static calculateFee(amount: number): number {
    return 0; // No fees in PhysicsCoin
  }
}

// Export default client
export default PhysicsCoinClient;

