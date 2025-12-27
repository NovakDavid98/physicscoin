import { useState, useEffect } from 'react'
import './App.css'
import * as API from './services/api'
import { QRCodeSVG } from 'qrcode.react'

interface WalletState {
  address: string | null
  balance: number
  mnemonic: string | null
  connected: boolean
}

interface Transaction {
  id: string
  from: string
  to: string
  amount: number
  timestamp: Date
  incoming: boolean
}

type Network = 'testnet' | 'mainnet' | 'devnet'

function App() {
  const [network, setNetwork] = useState<Network>('testnet')
  const [faucetInfo, setFaucetInfo] = useState<{enabled: boolean, amount: number, cooldown: number} | null>(null)
  const [wallet, setWallet] = useState<WalletState>({
    address: null,
    balance: 0,
    mnemonic: null,
    connected: false
  })
  const [transactions, setTransactions] = useState<Transaction[]>([])
  const [sendTo, setSendTo] = useState('')
  const [sendAmount, setSendAmount] = useState('')
  const [loading, setLoading] = useState(false)
  const [activeTab, setActiveTab] = useState<'dashboard' | 'send' | 'receive' | 'streams' | 'proofs' | 'history'>('dashboard')
  const [showMnemonic, setShowMnemonic] = useState(false)
  const [nodeStatus, setNodeStatus] = useState({ peers: 0, version: 0, wallets: 0, tx_count: 0 })

  // Streaming state
  const [streams, setStreams] = useState<Array<{ id: string, to: string, rate: number, started: number, accumulated: number }>>([])
  const [streamTo, setStreamTo] = useState('')
  const [streamRate, setStreamRate] = useState('')

  // Proof state
  const [proof, setProof] = useState<{ address: string, balance: number, hash: string, timestamp: number } | null>(null)

  // Notification state
  const [notification, setNotification] = useState<{ message: string, type: 'success' | 'error' | 'info' } | null>(null)

  const notify = (message: string, type: 'success' | 'error' | 'info' = 'info') => {
    setNotification({ message, type })
    setTimeout(() => setNotification(null), 3000)
  }

  // Network configuration
  useEffect(() => {
    const ports = { testnet: 18545, mainnet: 8545, devnet: 28545 }
    API.setApiUrl(`http://localhost:${ports[network]}`)
    
    // Fetch faucet info
    API.getFaucetInfo().then(info => {
      setFaucetInfo(info)
    }).catch(() => {
      setFaucetInfo(null)
    })
  }, [network])

  // Wallet persistence functions
  const saveWallet = (address: string, mnemonic: string) => {
    const walletData = { address, mnemonic }
    localStorage.setItem('physicscoin_wallet', JSON.stringify(walletData))
  }

  const loadWallet = () => {
    const saved = localStorage.getItem('physicscoin_wallet')
    if (saved) {
      try {
        return JSON.parse(saved) as { address: string, mnemonic: string }
      } catch {
        return null
      }
    }
    return null
  }

  const logout = () => {
    localStorage.removeItem('physicscoin_wallet')
    setWallet({ address: null, balance: 0, mnemonic: null, connected: false })
    setTransactions([])
    setStreams([])
    setProof(null)
  }

  // Auto-load wallet on startup
  useEffect(() => {
    const saved = loadWallet()
    if (saved) {
      setWallet({
        address: saved.address,
        balance: 0,
        mnemonic: saved.mnemonic,
        connected: true
      })
      // Fetch balance
      API.getBalance(saved.address).then(data => {
        setWallet(prev => ({ ...prev, balance: data.balance }))
      }).catch(() => { })
    }
  }, [])

  // Create wallet using real API
  const createWallet = async () => {
    try {
      const result = await API.createWallet()
      setWallet({
        address: result.address,
        balance: (result as { balance?: number }).balance || 1000,
        mnemonic: result.mnemonic,
        connected: true
      })

      // Save to localStorage
      saveWallet(result.address, result.mnemonic)
      setShowMnemonic(true)
      notify('Wallet created successfully!', 'success')

      // Fetch real balance
      setTimeout(async () => {
        try {
          const balanceData = await API.getBalance(result.address)
          setWallet(prev => ({ ...prev, balance: balanceData.balance }))
        } catch {
          // Wallet might not be in state yet
        }
      }, 500)
    } catch (error) {
      console.error('Failed to create wallet:', error)
      notify('Failed to create wallet. Is API running?', 'error')
    }
  }

  // Send transaction using real API
  const sendTransaction = async () => {
    if (!sendTo || !sendAmount || parseFloat(sendAmount) <= 0 || !wallet.address) return

    setLoading(true)
    try {
      const amount = parseFloat(sendAmount)
      await API.sendTransaction(wallet.address, sendTo, amount)

      // Update local state
      setWallet(prev => ({ ...prev, balance: prev.balance - amount }))
      setTransactions(prev => [{
        id: Date.now().toString(16),
        from: wallet.address!,
        to: sendTo,
        amount,
        timestamp: new Date(),
        incoming: false
      }, ...prev])

      setSendTo('')
      setSendAmount('')
      setActiveTab('history')
      notify(`Sent ${amount} PCS successfully!`, 'success')
    } catch (error) {
      console.error('Transaction failed:', error)
      notify('Transaction failed. Check console.', 'error')
    } finally {
      setLoading(false)
    }
  }

  const copyToClipboard = (text: string) => {
    navigator.clipboard.writeText(text)
    notify('Copied to clipboard!', 'success')
  }

  const formatAddress = (addr: string) => {
    return `${addr.slice(0, 8)}...${addr.slice(-8)}`
  }

  // Request faucet funds (testnet only)
  const requestFaucet = async () => {
    if (!wallet.address || !faucetInfo?.enabled) return
    
    setLoading(true)
    try {
      const result = await API.requestFaucet(wallet.address)
      setWallet(prev => ({ ...prev, balance: prev.balance + result.amount }))
      notify(`Received ${result.amount} PCS from faucet!`, 'success')
    } catch (error: any) {
      notify(error.message || 'Faucet request failed', 'error')
    } finally {
      setLoading(false)
    }
  }

  // Check API status and update balance
  useEffect(() => {
    const checkStatus = async () => {
      try {
        const data = await API.getStatus()
        setNodeStatus({
          peers: data.peers || 1,
          version: parseFloat(data.version) || 1,
          wallets: data.wallets,
          tx_count: data.tx_count || 0
        })

        // Update balance if wallet is connected
        if (wallet.address) {
          try {
            const balanceData = await API.getBalance(wallet.address)
            setWallet(prev => ({ ...prev, balance: balanceData.balance }))
          } catch {
            // Wallet might not exist in state
          }
        }
      } catch {
        // API not available
      }
    }
    checkStatus()
    const interval = setInterval(checkStatus, 10000)
    return () => clearInterval(interval)
  }, [wallet.address])

  if (!wallet.connected) {
    return (
      <div className="app">
        <div className="welcome-screen">
          {/* Network Selector */}
          <div className="network-selector" style={{ marginBottom: '2rem' }}>
            <label style={{ marginBottom: '0.5rem', display: 'block', color: 'var(--text-secondary)' }}>Select Network:</label>
            <div style={{ display: 'flex', gap: '0.5rem', justifyContent: 'center' }}>
              {(['testnet', 'mainnet', 'devnet'] as Network[]).map(net => (
                <button
                  key={net}
                  className={`btn ${network === net ? 'btn-primary' : 'btn-secondary'}`}
                  onClick={() => setNetwork(net)}
                  style={{ textTransform: 'capitalize' }}
                >
                  {net}
                  {net === 'testnet' && faucetInfo?.enabled && ' 💧'}
                </button>
              ))}
            </div>
          </div>

          <div className="logo">
            <div className="logo-icon">₿</div>
            <h1>PhysicsCoin</h1>
            <p className="tagline">The world's first physics-based cryptocurrency</p>
          </div>

          <div className="welcome-stats">
            <div className="welcome-stat">
              <span className="stat-value">116K</span>
              <span className="stat-label">TX/sec</span>
            </div>
            <div className="welcome-stat">
              <span className="stat-value">244</span>
              <span className="stat-label">Bytes</span>
            </div>
            <div className="welcome-stat">
              <span className="stat-value">0.0</span>
              <span className="stat-label">Fees</span>
            </div>
          </div>

          <div className="welcome-actions">
            <button className="btn btn-primary btn-lg" onClick={createWallet}>
              <span>✨</span> Create New Wallet
            </button>
            <button className="btn btn-secondary btn-lg">
              <span>🔑</span> Recover from Mnemonic
            </button>
          </div>

          <div className="features">
            <div className="feature">
              <div className="feature-icon">⚡</div>
              <h3>Blazing Fast</h3>
              <p>116,000 tx/sec with Ed25519</p>
            </div>
            <div className="feature">
              <div className="feature-icon">💾</div>
              <h3>Ultra Light</h3>
              <p>244 bytes replaces 500GB blockchain</p>
            </div>
            <div className="feature">
              <div className="feature-icon">🔐</div>
              <h3>HD Wallet</h3>
              <p>12-word mnemonic backup</p>
            </div>
          </div>
        </div>
      </div>
    )
  }

  return (
    <div className="app">
      {/* Notifications */}
      {notification && (
        <div className={`notification ${notification.type}`}>
          {notification.type === 'success' && '✅ '}
          {notification.type === 'error' && '❌ '}
          {notification.type === 'info' && 'ℹ️ '}
          {notification.message}
        </div>
      )}
      {/* Mnemonic Modal */}
      {showMnemonic && wallet.mnemonic && (
        <div className="modal-overlay" onClick={() => setShowMnemonic(false)}>
          <div className="modal" onClick={e => e.stopPropagation()}>
            <div className="modal-header">
              <h2>⚠️ Backup Your Recovery Phrase</h2>
            </div>
            <div className="modal-body">
              <p className="warning">
                Write these 12 words down and store them safely.
                This is the ONLY way to recover your wallet!
              </p>
              <div className="mnemonic-grid">
                {wallet.mnemonic.split(' ').map((word, i) => (
                  <div key={i} className="mnemonic-word">
                    <span className="word-num">{i + 1}</span>
                    <span className="word">{word}</span>
                  </div>
                ))}
              </div>
              <button className="btn btn-primary" onClick={() => setShowMnemonic(false)}>
                I've Written It Down
              </button>
            </div>
          </div>
        </div>
      )}

      {/* Header */}
      <header className="header">
        <div className="header-brand">
          <span className="logo-small">₿</span>
          <span>PhysicsCoin Wallet</span>
          <span style={{ 
            marginLeft: '1rem', 
            padding: '0.25rem 0.75rem', 
            background: network === 'testnet' ? 'var(--warning)' : network === 'devnet' ? 'var(--info)' : 'var(--success)',
            borderRadius: 'var(--radius-sm)',
            fontSize: '0.75rem',
            fontWeight: '600',
            textTransform: 'uppercase'
          }}>
            {network}
          </span>
        </div>
        <div className="header-status">
          <div className="status-item">
            <span className="status-dot online"></span>
            <span>{nodeStatus.peers} peers</span>
          </div>
          <div className="status-item">
            <span>v{nodeStatus.version}</span>
          </div>
          <button className="btn btn-secondary btn-sm" onClick={logout} style={{ marginLeft: '1rem' }}>
            Logout
          </button>
        </div>
      </header>

      {/* Navigation */}
      <nav className="nav">
        {(['dashboard', 'send', 'receive', 'streams', 'proofs', 'history'] as const).map(tab => (
          <button
            key={tab}
            className={`nav-item ${activeTab === tab ? 'active' : ''}`}
            onClick={() => setActiveTab(tab)}
          >
            {tab === 'dashboard' && '📊'}
            {tab === 'send' && '📤'}
            {tab === 'receive' && '📥'}
            {tab === 'streams' && '💧'}
            {tab === 'proofs' && '🔐'}
            {tab === 'history' && '📜'}
            <span>{tab.charAt(0).toUpperCase() + tab.slice(1)}</span>
          </button>
        ))}
      </nav>

      {/* Main Content */}
      <main className="main">
        {activeTab === 'dashboard' && (
          <div className="dashboard">
            <div className="balance-card card">
              <div className="balance-label">Total Balance</div>
              <div className="balance-value">
                {wallet.balance.toLocaleString()} <span className="currency">PCS</span>
              </div>
              <div className="balance-usd">≈ ${(wallet.balance * 0.01).toLocaleString()}</div>
            </div>

            <div className="stats-grid">
              <div className="stat-card">
                <div className="stat-value">116K</div>
                <div className="stat-label">Max TPS</div>
              </div>
              <div className="stat-card">
                <div className="stat-value">{nodeStatus.tx_count}</div>
                <div className="stat-label">Total Transactions</div>
              </div>
              <div className="stat-card">
                <div className="stat-value">{nodeStatus.wallets}</div>
                <div className="stat-label">Active Wallets</div>
              </div>
            </div>

            <div className="card">
              <div className="card-header">
                <h3>Your Address</h3>
                <button className="btn btn-secondary btn-sm" onClick={() => copyToClipboard(wallet.address!)}>
                  📋 Copy
                </button>
              </div>
              <div className="address">{wallet.address}</div>
            </div>

            <div className="quick-actions">
              <button className="btn btn-primary" onClick={() => setActiveTab('send')}>
                📤 Send
              </button>
              <button className="btn btn-secondary" onClick={() => setActiveTab('receive')}>
                📥 Receive
              </button>
              {faucetInfo?.enabled && (
                <button 
                  className="btn btn-success" 
                  onClick={requestFaucet}
                  disabled={loading}
                  title={`Get ${faucetInfo.amount} test coins`}
                >
                  💧 Faucet
                </button>
              )}
            </div>
          </div>
        )}

        {activeTab === 'send' && (
          <div className="send-form">
            <div className="card">
              <h2>Send PhysicsCoin</h2>

              <div className="input-group">
                <label>Recipient Address</label>
                <input
                  type="text"
                  className="input"
                  placeholder="Enter recipient address..."
                  value={sendTo}
                  onChange={e => setSendTo(e.target.value)}
                />
              </div>

              <div className="input-group">
                <label>Amount</label>
                <div className="input-with-suffix">
                  <input
                    type="number"
                    className="input"
                    placeholder="0.00"
                    value={sendAmount}
                    onChange={e => setSendAmount(e.target.value)}
                  />
                  <span className="input-suffix">PCS</span>
                </div>
                <div className="input-hint">
                  Available: {wallet.balance.toLocaleString()} PCS
                </div>
              </div>

              <div className="fee-info">
                <span>Network Fee:</span>
                <span className="fee-value">0.00 PCS (Free!)</span>
              </div>

              <button
                className="btn btn-primary btn-full"
                onClick={sendTransaction}
                disabled={loading || !sendTo || !sendAmount}
              >
                {loading ? <div className="spinner-sm"></div> : '📤 Send Transaction'}
              </button>
            </div>
          </div>
        )}

        {activeTab === 'receive' && (
          <div className="receive">
            <div className="card">
              <h2>Receive PhysicsCoin</h2>

              <div className="qr-placeholder">
                <div className="qr-code" style={{ padding: '1rem' }}>
                  {wallet.address && <QRCodeSVG value={wallet.address} size={180} />}
                </div>
                <p>Scan to receive</p>
              </div>

              <div className="input-group">
                <label>Your Address</label>
                <div className="address-copy">
                  <input
                    type="text"
                    className="input"
                    value={wallet.address || ''}
                    readOnly
                  />
                  <button className="btn btn-primary" onClick={() => copyToClipboard(wallet.address!)}>
                    📋
                  </button>
                </div>
              </div>
            </div>
          </div>
        )}

        {activeTab === 'history' && (
          <div className="history">
            <div className="card">
              <h2>Transaction History</h2>

              {transactions.length === 0 ? (
                <div className="empty-state">
                  <div className="empty-icon">📭</div>
                  <p>No transactions yet</p>
                </div>
              ) : (
                <div className="tx-list">
                  {transactions.map(tx => (
                    <div key={tx.id} className={`tx-item ${tx.incoming ? '' : 'outgoing'}`}>
                      <div className="tx-info">
                        <div className="tx-type">{tx.incoming ? '📥 Received' : '📤 Sent'}</div>
                        <div className="tx-address">{formatAddress(tx.incoming ? tx.from : tx.to)}</div>
                        <div className="tx-time">{tx.timestamp.toLocaleString()}</div>
                      </div>
                      <div className={`tx-amount ${tx.incoming ? 'positive' : 'negative'}`}>
                        {tx.incoming ? '+' : '-'}{tx.amount.toLocaleString()} PCS
                      </div>
                    </div>
                  ))}
                </div>
              )}
            </div>
          </div>
        )}

        {activeTab === 'streams' && (
          <div className="streams">
            <div className="card">
              <h2>💧 Streaming Payments</h2>
              <p style={{ color: 'var(--text-secondary)', marginBottom: '1.5rem' }}>
                Pay continuously over time - perfect for subscriptions, API usage, IoT payments
              </p>

              <div className="input-group">
                <label>Recipient Address</label>
                <input
                  type="text"
                  className="input"
                  placeholder="Enter recipient address..."
                  value={streamTo}
                  onChange={e => setStreamTo(e.target.value)}
                />
              </div>

              <div className="input-group">
                <label>Rate (PCS/second)</label>
                <input
                  type="number"
                  className="input"
                  placeholder="0.001"
                  value={streamRate}
                  onChange={e => setStreamRate(e.target.value)}
                />
              </div>

              <button
                className="btn btn-primary btn-full"
                onClick={async () => {
                  if (!wallet.address || !streamTo || !streamRate) return
                  try {
                    const result = await API.openStream(wallet.address, streamTo, parseFloat(streamRate))
                    setStreams(prev => [...prev, {
                      id: result.stream_id,
                      to: streamTo,
                      rate: parseFloat(streamRate),
                      started: Date.now(),
                      accumulated: 0
                    }])
                    setStreamTo('')
                    setStreamRate('')
                  } catch (error) {
                    console.error('Failed to open stream:', error)
                  }
                }}
                disabled={!streamTo || !streamRate}
              >
                💧 Open Stream
              </button>

              {streams.length > 0 && (
                <div style={{ marginTop: '2rem' }}>
                  <h3>Active Streams</h3>
                  <div className="tx-list">
                    {streams.map(stream => {
                      const elapsed = (Date.now() - stream.started) / 1000
                      const accumulated = stream.rate * elapsed
                      return (
                        <div key={stream.id} className="tx-item">
                          <div className="tx-info">
                            <div className="tx-type">💧 Streaming to {formatAddress(stream.to)}</div>
                            <div className="tx-address">{stream.rate} PCS/sec</div>
                            <div className="tx-time">Accumulated: {accumulated.toFixed(2)} PCS</div>
                          </div>
                          <button className="btn btn-secondary btn-sm">Close</button>
                        </div>
                      )
                    })}
                  </div>
                </div>
              )}
            </div>
          </div>
        )}

        {activeTab === 'proofs' && (
          <div className="proofs">
            <div className="card">
              <h2>🔐 Balance Proofs</h2>
              <p style={{ color: 'var(--text-secondary)', marginBottom: '1.5rem' }}>
                Generate cryptographic proof of your balance at current state
              </p>

              <button
                className="btn btn-primary btn-full"
                onClick={async () => {
                  if (!wallet.address) return
                  try {
                    const result = await API.generateProof(wallet.address)
                    setProof({
                      address: result.address,
                      balance: result.balance,
                      hash: result.state_hash,
                      timestamp: result.timestamp
                    })
                  } catch (error) {
                    console.error('Failed to generate proof:', error)
                  }
                }}
              >
                🔐 Generate Proof
              </button>

              {proof && (
                <div style={{ marginTop: '2rem', padding: '1.5rem', background: 'var(--bg-secondary)', borderRadius: 'var(--radius-md)' }}>
                  <h3>Balance Proof</h3>
                  <div style={{ marginTop: '1rem' }}>
                    <div style={{ marginBottom: '0.5rem', color: 'var(--text-secondary)' }}>Address:</div>
                    <div className="address">{proof.address}</div>
                  </div>
                  <div style={{ marginTop: '1rem' }}>
                    <div style={{ marginBottom: '0.5rem', color: 'var(--text-secondary)' }}>Balance:</div>
                    <div style={{ fontSize: '1.5rem', fontWeight: '700', color: 'var(--primary)' }}>{proof.balance.toFixed(8)} PCS</div>
                  </div>
                  <div style={{ marginTop: '1rem' }}>
                    <div style={{ marginBottom: '0.5rem', color: 'var(--text-secondary)' }}>State Hash:</div>
                    <div className="address">{proof.hash}</div>
                  </div>
                  <div style={{ marginTop: '1rem' }}>
                    <div style={{ marginBottom: '0.5rem', color: 'var(--text-secondary)' }}>Timestamp:</div>
                    <div>{new Date(proof.timestamp * 1000).toLocaleString()}</div>
                  </div>
                  <button
                    className="btn btn-secondary"
                    style={{ marginTop: '1rem' }}
                    onClick={() => {
                      const proofData = JSON.stringify(proof, null, 2)
                      const blob = new Blob([proofData], { type: 'application/json' })
                      const url = URL.createObjectURL(blob)
                      const a = document.createElement('a')
                      a.href = url
                      a.download = `proof_${proof.timestamp}.json`
                      a.click()
                    }}
                  >
                    📥 Download Proof
                  </button>
                </div>
              )}
            </div>
          </div>
        )}
      </main>
    </div>
  )
}

export default App
