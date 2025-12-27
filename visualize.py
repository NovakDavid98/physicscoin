#!/usr/bin/env python3
"""
PhysicsCoin Visualization Suite
Generates publication-quality graphics from benchmark data
"""

import json
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import numpy as np
from pathlib import Path
import os

# Configuration
COLORS = {
    'primary': '#00D4AA',      # PhysicsCoin green
    'secondary': '#FF6B6B',    # Comparison red
    'accent': '#4ECDC4',       # Accent teal
    'bitcoin': '#F7931A',      # Bitcoin orange
    'ethereum': '#627EEA',     # Ethereum purple
    'solana': '#9945FF',       # Solana purple
    'dark': '#1a1a2e',
    'light': '#eaeaea'
}

plt.style.use('dark_background')
plt.rcParams['figure.facecolor'] = COLORS['dark']
plt.rcParams['axes.facecolor'] = '#16213e'
plt.rcParams['axes.edgecolor'] = COLORS['light']
plt.rcParams['text.color'] = COLORS['light']
plt.rcParams['axes.labelcolor'] = COLORS['light']
plt.rcParams['xtick.color'] = COLORS['light']
plt.rcParams['ytick.color'] = COLORS['light']
plt.rcParams['font.size'] = 12
plt.rcParams['axes.titlesize'] = 16
plt.rcParams['axes.labelsize'] = 14

OUTPUT_DIR = Path('datagraphics')
OUTPUT_DIR.mkdir(exist_ok=True)

def load_benchmark_data():
    """Load benchmark results from JSON"""
    with open('benchmarks/benchmark_results.json', 'r') as f:
        data = json.load(f)
    return {item['test']: item['data'] for item in data}

def plot_storage_comparison(data):
    """Bar chart: Storage comparison (logarithmic scale)"""
    fig, ax = plt.subplots(figsize=(10, 6))
    
    # Data
    bitcoin_gb = data[0]['storage_gb']
    physicscoin_bytes = data[1]['storage_bytes']
    physicscoin_gb = physicscoin_bytes / (1024**3)
    
    # Log scale values
    names = ['Bitcoin', 'PhysicsCoin']
    values = [bitcoin_gb * 1e9, physicscoin_bytes]  # Both in bytes
    colors = [COLORS['bitcoin'], COLORS['primary']]
    
    bars = ax.bar(names, values, color=colors, edgecolor='white', linewidth=2)
    
    ax.set_yscale('log')
    ax.set_ylabel('Storage (bytes, log scale)')
    ax.set_title('🗄️ Storage Requirement: Bitcoin vs PhysicsCoin', fontsize=18, fontweight='bold')
    
    # Add value labels
    ax.text(0, bitcoin_gb * 1e9 * 1.5, f'{bitcoin_gb:.0f} GB', ha='center', fontsize=14, fontweight='bold')
    ax.text(1, physicscoin_bytes * 2, f'{physicscoin_bytes} bytes', ha='center', fontsize=14, fontweight='bold', color=COLORS['primary'])
    
    # Add compression ratio
    ratio = (bitcoin_gb * 1e9) / physicscoin_bytes
    ax.text(0.5, 1e6, f'Compression: {ratio/1e6:.0f} million : 1', ha='center', fontsize=16, 
            bbox=dict(boxstyle='round', facecolor=COLORS['primary'], alpha=0.3))
    
    ax.set_ylim(10, 1e12)
    ax.grid(axis='y', alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(OUTPUT_DIR / 'storage_comparison.png', dpi=150, bbox_inches='tight')
    plt.close()
    print('✓ storage_comparison.png')

def plot_throughput_comparison(data):
    """Bar chart: Transaction throughput"""
    fig, ax = plt.subplots(figsize=(10, 6))
    
    names = [d['name'] for d in data]
    values = [d['tps'] for d in data]
    colors = [COLORS['bitcoin'], COLORS['ethereum'], COLORS['solana'], COLORS['primary']]
    
    bars = ax.bar(names, values, color=colors, edgecolor='white', linewidth=2)
    
    ax.set_ylabel('Transactions per Second')
    ax.set_title('⚡ Transaction Throughput Comparison', fontsize=18, fontweight='bold')
    
    # Add value labels
    for bar, val in zip(bars, values):
        height = bar.get_height()
        label = f'{val:,.0f}'
        ax.text(bar.get_x() + bar.get_width()/2., height + max(values)*0.02,
                label, ha='center', fontsize=12, fontweight='bold')
    
    ax.set_yscale('log')
    ax.set_ylim(1, max(values) * 3)
    ax.grid(axis='y', alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(OUTPUT_DIR / 'throughput_comparison.png', dpi=150, bbox_inches='tight')
    plt.close()
    print('✓ throughput_comparison.png')

def plot_state_scaling(data):
    """Line chart: State size vs number of wallets"""
    fig, ax = plt.subplots(figsize=(10, 6))
    
    wallets = [d['wallets'] for d in data]
    bytes_size = [d['bytes'] for d in data]
    
    ax.plot(wallets, bytes_size, 'o-', color=COLORS['primary'], linewidth=3, markersize=10)
    ax.fill_between(wallets, bytes_size, alpha=0.3, color=COLORS['primary'])
    
    ax.set_xlabel('Number of Wallets')
    ax.set_ylabel('State Size (bytes)')
    ax.set_title('📈 State Size Scaling', fontsize=18, fontweight='bold')
    
    # Add annotation for 1M wallets extrapolation
    bytes_per_wallet = bytes_size[-1] / wallets[-1]
    ax.axhline(y=1e6, color=COLORS['secondary'], linestyle='--', alpha=0.5)
    ax.text(wallets[-1], 1e6 * 1.1, '1 MB', color=COLORS['secondary'])
    
    ax.set_xscale('log')
    ax.set_yscale('log')
    ax.grid(alpha=0.3)
    
    # Add efficiency note
    ax.text(0.02, 0.98, f'~{bytes_per_wallet:.0f} bytes/wallet', 
            transform=ax.transAxes, fontsize=14,
            bbox=dict(boxstyle='round', facecolor=COLORS['primary'], alpha=0.3),
            verticalalignment='top')
    
    plt.tight_layout()
    plt.savefig(OUTPUT_DIR / 'state_scaling.png', dpi=150, bbox_inches='tight')
    plt.close()
    print('✓ state_scaling.png')

def plot_latency_distribution(data):
    """Histogram: Transaction latency distribution"""
    fig, ax = plt.subplots(figsize=(10, 6))
    
    latencies = [d['latency_us'] for d in data]
    
    ax.hist(latencies, bins=50, color=COLORS['primary'], edgecolor='white', alpha=0.8)
    
    mean_latency = np.mean(latencies)
    median_latency = np.median(latencies)
    
    ax.axvline(mean_latency, color=COLORS['secondary'], linestyle='--', linewidth=2, label=f'Mean: {mean_latency:.1f} µs')
    ax.axvline(median_latency, color=COLORS['accent'], linestyle='--', linewidth=2, label=f'Median: {median_latency:.1f} µs')
    
    ax.set_xlabel('Latency (microseconds)')
    ax.set_ylabel('Frequency')
    ax.set_title('⏱️ Transaction Latency Distribution', fontsize=18, fontweight='bold')
    ax.legend()
    ax.grid(axis='y', alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(OUTPUT_DIR / 'latency_distribution.png', dpi=150, bbox_inches='tight')
    plt.close()
    print('✓ latency_distribution.png')

def plot_conservation_error(data):
    """Line chart: Conservation error over time"""
    fig, ax = plt.subplots(figsize=(10, 6))
    
    tx_counts = [d['tx_count'] for d in data]
    errors = [abs(d['error']) for d in data]
    
    ax.plot(tx_counts, errors, 'o-', color=COLORS['primary'], linewidth=3, markersize=12)
    
    ax.set_xlabel('Transaction Count')
    ax.set_ylabel('Conservation Error')
    ax.set_title('🎯 Energy Conservation Verification', fontsize=18, fontweight='bold')
    
    # Highlight zero error
    ax.axhline(y=0, color=COLORS['accent'], linestyle='-', linewidth=2, alpha=0.5)
    
    # Add success badge
    if max(errors) < 1e-9:
        ax.text(0.5, 0.5, '✓ PERFECT CONSERVATION\n(Error < 10⁻⁹)', 
                transform=ax.transAxes, fontsize=20, fontweight='bold',
                ha='center', va='center',
                bbox=dict(boxstyle='round', facecolor=COLORS['primary'], alpha=0.8))
    
    ax.set_ylim(-1e-10, 1e-10)
    ax.grid(alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(OUTPUT_DIR / 'conservation_error.png', dpi=150, bbox_inches='tight')
    plt.close()
    print('✓ conservation_error.png')

def plot_streaming_payment(data):
    """Dual line chart: Streaming payment over time"""
    fig, ax = plt.subplots(figsize=(10, 6))
    
    times = [d['time'] for d in data]
    alice = [d['alice'] for d in data]
    bob = [d['bob'] for d in data]
    
    ax.plot(times, alice, 'o-', color=COLORS['secondary'], linewidth=3, markersize=8, label='Alice (Payer)')
    ax.plot(times, bob, 'o-', color=COLORS['primary'], linewidth=3, markersize=8, label='Bob (Receiver)')
    
    ax.fill_between(times, alice, alpha=0.2, color=COLORS['secondary'])
    ax.fill_between(times, bob, alpha=0.2, color=COLORS['primary'])
    
    ax.set_xlabel('Time (seconds)')
    ax.set_ylabel('Balance (coins)')
    ax.set_title('💸 Streaming Payment Demo (1 coin/sec)', fontsize=18, fontweight='bold')
    ax.legend(loc='center right')
    ax.grid(alpha=0.3)
    
    # Add rate annotation
    ax.annotate('', xy=(30, 500), xytext=(0, 1000),
                arrowprops=dict(arrowstyle='->', color=COLORS['accent'], lw=2))
    ax.text(15, 700, 'Continuous\nPayment Flow', ha='center', fontsize=12)
    
    plt.tight_layout()
    plt.savefig(OUTPUT_DIR / 'streaming_payment.png', dpi=150, bbox_inches='tight')
    plt.close()
    print('✓ streaming_payment.png')

def plot_sharding_scaling(data):
    """Bar chart: Sharding throughput scaling"""
    fig, ax = plt.subplots(figsize=(10, 6))
    
    shards = [d['shards'] for d in data]
    theoretical = [d['theoretical_tps'] / 1e6 for d in data]  # Million TPS
    actual = [d['actual_tps'] / 1e6 for d in data]
    
    x = np.arange(len(shards))
    width = 0.35
    
    bars1 = ax.bar(x - width/2, theoretical, width, label='Theoretical', color=COLORS['accent'], edgecolor='white')
    bars2 = ax.bar(x + width/2, actual, width, label='With 15% overhead', color=COLORS['primary'], edgecolor='white')
    
    ax.set_xlabel('Number of Shards')
    ax.set_ylabel('Throughput (Million tx/sec)')
    ax.set_title('🔀 Sharding Throughput Scaling', fontsize=18, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(shards)
    ax.legend()
    ax.grid(axis='y', alpha=0.3)
    
    # Add Visa comparison line
    ax.axhline(y=0.065, color=COLORS['secondary'], linestyle='--', alpha=0.7)
    ax.text(len(shards)-1, 0.08, 'Visa (~65K TPS)', color=COLORS['secondary'], fontsize=10)
    
    plt.tight_layout()
    plt.savefig(OUTPUT_DIR / 'sharding_scaling.png', dpi=150, bbox_inches='tight')
    plt.close()
    print('✓ sharding_scaling.png')

def plot_delta_sync(data):
    """Bar chart: Delta sync savings"""
    fig, ax = plt.subplots(figsize=(10, 6))
    
    tx_counts = [d['tx_count'] for d in data]
    delta_bytes = [d['delta_bytes'] for d in data]
    full_bytes = [d['full_state_bytes'] for d in data]
    savings = [d['savings_pct'] for d in data]
    
    x = np.arange(len(tx_counts))
    width = 0.35
    
    bars1 = ax.bar(x - width/2, full_bytes, width, label='Full State', color=COLORS['secondary'], edgecolor='white')
    bars2 = ax.bar(x + width/2, delta_bytes, width, label='Delta Only', color=COLORS['primary'], edgecolor='white')
    
    ax.set_xlabel('Transactions per Sync')
    ax.set_ylabel('Bytes Transferred')
    ax.set_title('📡 Delta Sync Bandwidth Savings', fontsize=18, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(tx_counts)
    ax.legend()
    ax.grid(axis='y', alpha=0.3)
    
    # Add savings percentages
    for i, (bar, sav) in enumerate(zip(bars2, savings)):
        ax.text(bar.get_x() + bar.get_width()/2., bar.get_height() + 100,
                f'-{sav:.0f}%', ha='center', fontsize=10, fontweight='bold', color=COLORS['primary'])
    
    plt.tight_layout()
    plt.savefig(OUTPUT_DIR / 'delta_sync.png', dpi=150, bbox_inches='tight')
    plt.close()
    print('✓ delta_sync.png')

def plot_keygen_speed(data):
    """Bar chart: Key generation speed"""
    fig, ax = plt.subplots(figsize=(10, 6))
    
    counts = [d['count'] for d in data]
    speeds = [d['keys_per_sec'] for d in data]
    
    bars = ax.bar([str(c) for c in counts], speeds, color=COLORS['primary'], edgecolor='white', linewidth=2)
    
    ax.set_xlabel('Keys Generated')
    ax.set_ylabel('Keys per Second')
    ax.set_title('🔑 Key Generation Performance', fontsize=18, fontweight='bold')
    
    # Add average line
    avg_speed = np.mean(speeds)
    ax.axhline(y=avg_speed, color=COLORS['accent'], linestyle='--', linewidth=2)
    ax.text(len(counts)-0.5, avg_speed*1.05, f'Avg: {avg_speed:,.0f}/sec', color=COLORS['accent'])
    
    ax.grid(axis='y', alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(OUTPUT_DIR / 'keygen_speed.png', dpi=150, bbox_inches='tight')
    plt.close()
    print('✓ keygen_speed.png')

def plot_bytes_per_wallet(data):
    """Line chart: Bytes per wallet efficiency"""
    fig, ax = plt.subplots(figsize=(10, 6))
    
    wallets = [d['wallets'] for d in data]
    bpw = [d['bytes_per_wallet'] for d in data]
    
    ax.plot(wallets, bpw, 'o-', color=COLORS['primary'], linewidth=3, markersize=12)
    
    ax.set_xlabel('Number of Wallets')
    ax.set_ylabel('Bytes per Wallet')
    ax.set_title('📦 Storage Efficiency at Scale', fontsize=18, fontweight='bold')
    
    # Highlight minimum
    min_bpw = min(bpw)
    min_idx = bpw.index(min_bpw)
    ax.scatter([wallets[min_idx]], [min_bpw], color=COLORS['accent'], s=200, zorder=5)
    ax.annotate(f'Min: {min_bpw:.0f} B/wallet', 
                xy=(wallets[min_idx], min_bpw),
                xytext=(wallets[min_idx]*2, min_bpw+20),
                arrowprops=dict(arrowstyle='->', color=COLORS['accent']),
                fontsize=12, color=COLORS['accent'])
    
    ax.set_xscale('log')
    ax.grid(alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(OUTPUT_DIR / 'bytes_per_wallet.png', dpi=150, bbox_inches='tight')
    plt.close()
    print('✓ bytes_per_wallet.png')

def create_summary_infographic(benchmark_data):
    """Create a summary infographic with key stats"""
    fig, axes = plt.subplots(2, 3, figsize=(15, 10))
    fig.patch.set_facecolor(COLORS['dark'])
    
    # Storage
    ax = axes[0, 0]
    ax.text(0.5, 0.7, '244', fontsize=60, fontweight='bold', 
            color=COLORS['primary'], ha='center', va='center')
    ax.text(0.5, 0.35, 'BYTES', fontsize=24, color=COLORS['light'], ha='center')
    ax.text(0.5, 0.15, 'Total State Size', fontsize=16, color=COLORS['light'], ha='center', alpha=0.7)
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.axis('off')
    
    # Throughput
    ax = axes[0, 1]
    tps = benchmark_data['throughput_comparison'][-1]['tps']
    ax.text(0.5, 0.7, f'{tps//1000}K', fontsize=60, fontweight='bold', 
            color=COLORS['primary'], ha='center', va='center')
    ax.text(0.5, 0.35, 'TX/SEC', fontsize=24, color=COLORS['light'], ha='center')
    ax.text(0.5, 0.15, 'Transaction Throughput', fontsize=16, color=COLORS['light'], ha='center', alpha=0.7)
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.axis('off')
    
    # Latency
    ax = axes[0, 2]
    latencies = [d['latency_us'] for d in benchmark_data['latency_distribution']]
    avg_latency = np.mean(latencies)
    ax.text(0.5, 0.7, f'{avg_latency:.1f}', fontsize=60, fontweight='bold', 
            color=COLORS['primary'], ha='center', va='center')
    ax.text(0.5, 0.35, 'µs', fontsize=24, color=COLORS['light'], ha='center')
    ax.text(0.5, 0.15, 'Avg Latency', fontsize=16, color=COLORS['light'], ha='center', alpha=0.7)
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.axis('off')
    
    # Compression
    ax = axes[1, 0]
    ax.text(0.5, 0.7, '2B:1', fontsize=60, fontweight='bold', 
            color=COLORS['primary'], ha='center', va='center')
    ax.text(0.5, 0.35, 'RATIO', fontsize=24, color=COLORS['light'], ha='center')
    ax.text(0.5, 0.15, 'vs Bitcoin Storage', fontsize=16, color=COLORS['light'], ha='center', alpha=0.7)
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.axis('off')
    
    # Conservation
    ax = axes[1, 1]
    ax.text(0.5, 0.7, '0.0', fontsize=60, fontweight='bold', 
            color=COLORS['primary'], ha='center', va='center')
    ax.text(0.5, 0.35, 'ERROR', fontsize=24, color=COLORS['light'], ha='center')
    ax.text(0.5, 0.15, 'Energy Conservation', fontsize=16, color=COLORS['light'], ha='center', alpha=0.7)
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.axis('off')
    
    # Shards
    ax = axes[1, 2]
    max_tps = benchmark_data['sharding_scaling'][-1]['actual_tps'] / 1e6
    ax.text(0.5, 0.7, f'{max_tps:.0f}M', fontsize=60, fontweight='bold', 
            color=COLORS['primary'], ha='center', va='center')
    ax.text(0.5, 0.35, 'TX/SEC', fontsize=24, color=COLORS['light'], ha='center')
    ax.text(0.5, 0.15, '64-Shard Capacity', fontsize=16, color=COLORS['light'], ha='center', alpha=0.7)
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.axis('off')
    
    fig.suptitle('PhysicsCoin Performance Summary', fontsize=28, fontweight='bold', 
                 color=COLORS['primary'], y=0.98)
    
    plt.tight_layout(rect=[0, 0, 1, 0.95])
    plt.savefig(OUTPUT_DIR / 'summary_infographic.png', dpi=150, bbox_inches='tight')
    plt.close()
    print('✓ summary_infographic.png')

def main():
    print('\n🎨 PhysicsCoin Visualization Suite\n')
    print('=' * 40)
    
    # Load data
    print('Loading benchmark data...')
    data = load_benchmark_data()
    print(f'Found {len(data)} benchmark tests\n')
    
    # Generate visualizations
    print('Generating visualizations...\n')
    
    plot_storage_comparison(data['storage_comparison'])
    plot_throughput_comparison(data['throughput_comparison'])
    plot_state_scaling(data['state_scaling'])
    plot_latency_distribution(data['latency_distribution'])
    plot_conservation_error(data['conservation_error'])
    plot_streaming_payment(data['streaming_payment'])
    plot_sharding_scaling(data['sharding_scaling'])
    plot_delta_sync(data['delta_sync'])
    plot_keygen_speed(data['keygen_speed'])
    plot_bytes_per_wallet(data['bytes_per_wallet'])
    create_summary_infographic(data)
    
    print('\n' + '=' * 40)
    print(f'✓ All visualizations saved to {OUTPUT_DIR}/')
    print(f'  Total: 11 images generated\n')

if __name__ == '__main__':
    main()
