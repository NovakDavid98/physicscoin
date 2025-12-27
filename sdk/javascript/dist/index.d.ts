/**
 * PhysicsCoin JavaScript/TypeScript SDK
 *
 * Official SDK for integrating with PhysicsCoin - the physics-based cryptocurrency
 *
 * @packageDocumentation
 */
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
export declare class PhysicsCoinClient {
    private api;
    private baseURL;
    /**
     * Create a new PhysicsCoin client
     * @param baseURL - API server URL (default: http://localhost:8545)
     */
    constructor(baseURL?: string);
    /**
     * Get network statistics
     */
    getNetworkStats(): Promise<NetworkStats>;
    /**
     * Get wallet information
     * @param address - Wallet address (hex public key)
     */
    getWallet(address: string): Promise<Wallet>;
    /**
     * Get detailed wallet info from explorer
     * @param address - Wallet address
     */
    getWalletDetails(address: string): Promise<Wallet & {
        rank: number;
        percent_of_supply: number;
    }>;
    /**
     * Get rich list (top wallets)
     */
    getRichList(): Promise<any[]>;
    /**
     * Get balance distribution
     */
    getDistribution(): Promise<any>;
    /**
     * Get supply analytics
     */
    getSupplyAnalytics(): Promise<any>;
    /**
     * Get system health
     */
    getHealth(): Promise<any>;
    /**
     * Verify conservation law
     */
    verifyConservation(): Promise<{
        verified: boolean;
        error: number;
    }>;
    /**
     * Create a new wallet
     * WARNING: This creates a wallet with 0 balance. You must receive funds from existing wallets.
     */
    createWallet(): Promise<{
        mnemonic: string;
        address: string;
        balance: number;
    }>;
    /**
     * Send a signed transaction
     * @param transaction - Signed transaction object
     */
    sendTransaction(transaction: Transaction): Promise<{
        success: boolean;
        amount: number;
    }>;
    /**
     * Generate a balance proof
     * @param address - Wallet address
     */
    generateProof(address: string): Promise<any>;
    /**
     * Open a payment stream
     * @param from - Payer address
     * @param to - Receiver address
     * @param rate - Payment rate per second
     * @param signature - Authorization signature
     */
    openStream(from: string, to: string, rate: number, signature: string): Promise<{
        stream_id: string;
    }>;
    /**
     * Search for address or transaction
     * @param query - Search query
     */
    search(query: string): Promise<any>;
}
/**
 * Cryptographic utilities for signing transactions
 */
export declare class PhysicsCoinCrypto {
    /**
     * Generate a new keypair
     */
    static generateKeypair(): {
        publicKey: string;
        secretKey: string;
    };
    /**
     * Sign a transaction
     * @param transaction - Transaction data
     * @param secretKey - Secret key (hex string)
     */
    static signTransaction(transaction: Omit<Transaction, 'signature'>, secretKey: string): Transaction;
    /**
     * Verify a transaction signature
     * @param transaction - Signed transaction
     */
    static verifyTransaction(transaction: Transaction): boolean;
    /**
     * Convert mnemonic to seed (simplified BIP39-like)
     * NOTE: For production, use a proper BIP39 library
     */
    static mnemonicToSeed(mnemonic: string): Buffer;
}
/**
 * Helper utilities
 */
export declare class Utils {
    /**
     * Convert coins to smallest unit (femto-precision)
     */
    static toFemto(coins: number): bigint;
    /**
     * Convert from smallest unit to coins
     */
    static fromFemto(femto: bigint): number;
    /**
     * Validate address format
     */
    static isValidAddress(address: string): boolean;
    /**
     * Format balance for display
     */
    static formatBalance(balance: number, decimals?: number): string;
    /**
     * Calculate transaction fee (if applicable)
     * NOTE: PhysicsCoin currently has no fees due to conservation law
     */
    static calculateFee(amount: number): number;
}
export default PhysicsCoinClient;
export { PhysicsCoinClient as PhysicsCoinAPI };
export { PhysicsCoinCrypto as PhysicsCoinWallet };
