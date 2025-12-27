// API Service Layer for PhysicsCoin
let API_URL = 'http://localhost:18545' // Default to testnet

export function setApiUrl(url: string) {
    API_URL = url
}

export interface WalletResponse {
    mnemonic: string
    address: string
}

export interface BalanceResponse {
    address: string
    balance: number
    nonce: number
}

export interface StatusResponse {
    version: string
    wallets: number
    total_supply: number
    timestamp: number
    tx_count: number
    peers: number
}

export interface TransactionResponse {
    success: boolean
    amount: number
}

export interface StreamResponse {
    stream_id: string
    from: string
    to: string
    rate: number
}

export interface ProofResponse {
    address: string
    balance: number
    state_hash: string
    timestamp: number
}

export interface FaucetInfoResponse {
    enabled: boolean
    amount: number
    cooldown: number
    network: string
}

export interface FaucetRequestResponse {
    success: boolean
    address: string
    amount: number
    message: string
}

// Wallet endpoints
export async function createWallet(): Promise<WalletResponse> {
    const res = await fetch(`${API_URL}/wallet/create`, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        }
    })

    if (!res.ok) throw new Error('Failed to create wallet')
    return await res.json()
}

export async function recoverWallet(mnemonic: string): Promise<WalletResponse> {
    return {
        mnemonic,
        address: '0000000000000000000000000000000000000000000000000000000000000000'
    }
}

// Balance endpoint
export async function getBalance(address: string): Promise<BalanceResponse> {
    const res = await fetch(`${API_URL}/balance/${address}`)

    if (!res.ok) throw new Error('Failed to fetch balance')
    return await res.json()
}

// Transaction endpoint
export async function sendTransaction(from: string, to: string, amount: number): Promise<TransactionResponse> {
    const res = await fetch(`${API_URL}/transaction/send`, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({ from, to, amount })
    })

    if (!res.ok) throw new Error('Transaction failed')
    return await res.json()
}

// Streaming endpoints
export async function openStream(from: string, to: string, rate: number): Promise<StreamResponse> {
    const res = await fetch(`${API_URL}/stream/open`, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({ from, to, rate })
    })

    if (!res.ok) throw new Error('Failed to open stream')
    return await res.json()
}

// Proof endpoint
export async function generateProof(address: string): Promise<ProofResponse> {
    const res = await fetch(`${API_URL}/proof/generate`, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({ address })
    })

    if (!res.ok) throw new Error('Failed to generate proof')
    return await res.json()
}

// Status endpoint
export async function getStatus(): Promise<StatusResponse> {
    const res = await fetch(`${API_URL}/status`)

    if (!res.ok) throw new Error('Failed to fetch status')
    return await res.json()
}

// Faucet endpoints (testnet/devnet only)
export async function getFaucetInfo(): Promise<FaucetInfoResponse> {
    const res = await fetch(`${API_URL}/faucet/info`)
    
    if (!res.ok) throw new Error('Failed to fetch faucet info')
    return await res.json()
}

export async function requestFaucet(address: string): Promise<FaucetRequestResponse> {
    const res = await fetch(`${API_URL}/faucet/request`, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({ address })
    })
    
    if (!res.ok) {
        const error = await res.json()
        throw new Error(error.error?.message || 'Faucet request failed')
    }
    return await res.json()
}

export default {
    setApiUrl,
    createWallet,
    recoverWallet,
    getBalance,
    sendTransaction,
    openStream,
    generateProof,
    getStatus,
    getFaucetInfo,
    requestFaucet
}
