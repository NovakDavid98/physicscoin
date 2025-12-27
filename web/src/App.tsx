import { useState, useEffect } from 'react'
import './App.css'
import * as API from './services/api'

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

function App() {
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
  const [activeTab, setActiveTab] = useState<'dashboard' | 'send' | 'receive' | 'history'>('dashboard')
  const [showMnemonic, setShowMnemonic] = useState(false)
  const [nodeStatus, setNodeStatus] = useState({ peers: 0, version: 0, wallets: 0 })

  // Create wallet using real API
  const createWallet = async () => {
    try {
      const result = await API.createWallet()
      setWallet({
        address: result.address,
        balance: 0,
        mnemonic: result.mnemonic,
        connected: true
      })
      setShowMnemonic(true)

      // Fetch initial balance
      setTimeout(async () => {
        try {
          const balanceData = await API.getBalance(result.address)
          setWallet(prev => ({ ...prev, balance: balanceData.balance }))
        } catch {
          // Wallet not in state yet, that's ok
        }
      }, 500)
    } catch (error) {
      console.error('Failed to create wallet:', error)
      alert('Failed to create wallet. Is the API server running?')
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
    } catch (error) {
      console.error('Transaction failed:', error)
      alert('Transaction failed. Check console for details.')
    } finally {
      setLoading(false)
    }
  }

  const copyToClipboard = (text: string) => {
    navigator.clipboard.writeText(text)
  }

  const formatAddress = (addr: string) => {
    return `${addr.slice(0, 8)}...${addr.slice(-8)}`
  }

  // Check API status and update balance
  useEffect(() => {
    const checkStatus = async () => {
      try {
        const data = await API.getStatus()
        setNodeStatus({
          peers: 0,
          version: parseFloat(data.version) || 1,
          wallets: data.wallets
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
        </div>
        <div className="header-status">
          <div className="status-item">
            <span className="status-dot online"></span>
            <span>{nodeStatus.peers} peers</span>
          </div>
          <div className="status-item">
            <span>v{nodeStatus.version}</span>
          </div>
        </div>
      </header>

      {/* Navigation */}
      <nav className="nav">
        {(['dashboard', 'send', 'receive', 'history'] as const).map(tab => (
          <button
            key={tab}
            className={`nav-item ${activeTab === tab ? 'active' : ''}`}
            onClick={() => setActiveTab(tab)}
          >
            {tab === 'dashboard' && '📊'}
            {tab === 'send' && '📤'}
            {tab === 'receive' && '📥'}
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
                <div className="stat-label">Network TPS</div>
              </div>
              <div className="stat-card">
                <div className="stat-value">{transactions.length}</div>
                <div className="stat-label">Transactions</div>
              </div>
              <div className="stat-card">
                <div className="stat-value">0</div>
                <div className="stat-label">Pending</div>
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
                <div className="qr-code">
                  📱
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
      </main>
    </div>
  )
}

export default App
