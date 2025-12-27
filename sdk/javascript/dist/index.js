"use strict";
/**
 * PhysicsCoin JavaScript/TypeScript SDK
 *
 * Official SDK for integrating with PhysicsCoin - the physics-based cryptocurrency
 *
 * @packageDocumentation
 */
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.PhysicsCoinWallet = exports.PhysicsCoinAPI = exports.Utils = exports.PhysicsCoinCrypto = exports.PhysicsCoinClient = void 0;
const axios_1 = __importDefault(require("axios"));
const tweetnacl_1 = __importDefault(require("tweetnacl"));
/**
 * Main PhysicsCoin SDK client
 */
class PhysicsCoinClient {
    /**
     * Create a new PhysicsCoin client
     * @param baseURL - API server URL (default: http://localhost:8545)
     */
    constructor(baseURL = 'http://localhost:8545') {
        this.baseURL = baseURL;
        this.api = axios_1.default.create({
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
    async getNetworkStats() {
        const response = await this.api.get('/explorer/stats');
        return response.data;
    }
    /**
     * Get wallet information
     * @param address - Wallet address (hex public key)
     */
    async getWallet(address) {
        const response = await this.api.get(`/balance/${address}`);
        return response.data;
    }
    /**
     * Get detailed wallet info from explorer
     * @param address - Wallet address
     */
    async getWalletDetails(address) {
        const response = await this.api.get(`/explorer/wallet/${address}`);
        return response.data;
    }
    /**
     * Get rich list (top wallets)
     */
    async getRichList() {
        const response = await this.api.get('/explorer/rich');
        return response.data.rich_list;
    }
    /**
     * Get balance distribution
     */
    async getDistribution() {
        const response = await this.api.get('/explorer/distribution');
        return response.data;
    }
    /**
     * Get supply analytics
     */
    async getSupplyAnalytics() {
        const response = await this.api.get('/explorer/supply');
        return response.data;
    }
    /**
     * Get system health
     */
    async getHealth() {
        const response = await this.api.get('/explorer/health');
        return response.data;
    }
    /**
     * Verify conservation law
     */
    async verifyConservation() {
        const response = await this.api.get('/conservation');
        return response.data;
    }
    /**
     * Create a new wallet
     * WARNING: This creates a wallet with 0 balance. You must receive funds from existing wallets.
     */
    async createWallet() {
        const response = await this.api.post('/wallet/create');
        return response.data;
    }
    /**
     * Send a signed transaction
     * @param transaction - Signed transaction object
     */
    async sendTransaction(transaction) {
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
    async generateProof(address) {
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
    async openStream(from, to, rate, signature) {
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
    async search(query) {
        const response = await this.api.get(`/explorer/search/${query}`);
        return response.data;
    }
}
exports.PhysicsCoinClient = PhysicsCoinClient;
exports.PhysicsCoinAPI = PhysicsCoinClient;
/**
 * Cryptographic utilities for signing transactions
 */
class PhysicsCoinCrypto {
    /**
     * Generate a new keypair
     */
    static generateKeypair() {
        const keypair = tweetnacl_1.default.sign.keyPair();
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
    static signTransaction(transaction, secretKey) {
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
        const signature = tweetnacl_1.default.sign.detached(message, secretKeyBytes);
        return {
            ...transaction,
            signature: Buffer.from(signature).toString('hex'),
        };
    }
    /**
     * Verify a transaction signature
     * @param transaction - Signed transaction
     */
    static verifyTransaction(transaction) {
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
        return tweetnacl_1.default.sign.detached.verify(message, signature, publicKey);
    }
    /**
     * Convert mnemonic to seed (simplified BIP39-like)
     * NOTE: For production, use a proper BIP39 library
     */
    static mnemonicToSeed(mnemonic) {
        // This is a simplified implementation
        // In production, use proper BIP39/BIP32 libraries
        const crypto = require('crypto');
        return crypto.createHash('sha256').update(mnemonic).digest();
    }
}
exports.PhysicsCoinCrypto = PhysicsCoinCrypto;
exports.PhysicsCoinWallet = PhysicsCoinCrypto;
/**
 * Helper utilities
 */
class Utils {
    /**
     * Convert coins to smallest unit (femto-precision)
     */
    static toFemto(coins) {
        return BigInt(Math.floor(coins * 1e15));
    }
    /**
     * Convert from smallest unit to coins
     */
    static fromFemto(femto) {
        return Number(femto) / 1e15;
    }
    /**
     * Validate address format
     */
    static isValidAddress(address) {
        return /^[0-9a-f]{64}$/i.test(address);
    }
    /**
     * Format balance for display
     */
    static formatBalance(balance, decimals = 8) {
        return balance.toFixed(decimals);
    }
    /**
     * Calculate transaction fee (if applicable)
     * NOTE: PhysicsCoin currently has no fees due to conservation law
     */
    static calculateFee(amount) {
        return 0; // No fees in PhysicsCoin
    }
}
exports.Utils = Utils;
// Export default client
exports.default = PhysicsCoinClient;
