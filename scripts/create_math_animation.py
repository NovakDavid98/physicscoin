#!/usr/bin/env python3
"""
PhysicsCoin Mathematical Foundation Visualization
Creates an animated GIF explaining the core concepts:
1. The DiffEqAuth equation
2. State vector representation
3. Conservation law enforcement
4. Consensus mechanism
5. Cross-shard protection
"""

import matplotlib.pyplot as plt
import matplotlib.patches as patches
import matplotlib.animation as animation
from matplotlib.patches import FancyBboxPatch, Circle, Arrow, FancyArrowPatch
import numpy as np
from matplotlib import patheffects
import os

# Set up figure with dark theme
plt.style.use('dark_background')
fig, ax = plt.subplots(figsize=(16, 9), dpi=100)
fig.patch.set_facecolor('#0d1117')
ax.set_facecolor('#0d1117')

# Remove axes
ax.set_xlim(0, 16)
ax.set_ylim(0, 9)
ax.axis('off')

# Color scheme
COLORS = {
    'primary': '#58a6ff',
    'secondary': '#8b949e',
    'success': '#3fb950',
    'warning': '#d29922',
    'error': '#f85149',
    'purple': '#a371f7',
    'bg_card': '#161b22',
    'bg_card2': '#21262d',
    'white': '#ffffff',
    'gold': '#ffd700'
}

# Animation frames
frames = []
TOTAL_FRAMES = 180  # 6 seconds at 30fps

def create_frame(frame_num):
    """Generate a single frame of the animation"""
    ax.clear()
    ax.set_xlim(0, 16)
    ax.set_ylim(0, 9)
    ax.axis('off')
    ax.set_facecolor('#0d1117')
    
    # Calculate which phase we're in (0-5)
    phase = min(5, frame_num // 30)
    phase_progress = (frame_num % 30) / 30.0
    
    # Always show title with glow effect
    title = ax.text(8, 8.5, 'PhysicsCoin', fontsize=36, ha='center', va='top',
                   color=COLORS['primary'], fontweight='bold',
                   path_effects=[patheffects.withStroke(linewidth=3, foreground='#1f6feb40')])
    
    subtitle = ax.text(8, 7.9, 'Mathematical Foundation', fontsize=18, ha='center',
                      color=COLORS['secondary'])
    
    if phase == 0:  # The Equation
        draw_equation_phase(ax, phase_progress)
    elif phase == 1:  # State Vector
        draw_state_vector_phase(ax, phase_progress)
    elif phase == 2:  # Conservation Law
        draw_conservation_phase(ax, phase_progress)
    elif phase == 3:  # POC Consensus
        draw_consensus_phase(ax, phase_progress)
    elif phase == 4:  # Cross-Shard
        draw_crossshard_phase(ax, phase_progress)
    else:  # Summary
        draw_summary_phase(ax, phase_progress)
    
    # Progress indicator
    progress = frame_num / TOTAL_FRAMES
    ax.add_patch(FancyBboxPatch((1, 0.3), 14 * progress, 0.15, 
                                boxstyle="round,pad=0.02",
                                facecolor=COLORS['primary'], alpha=0.8))
    ax.add_patch(FancyBboxPatch((1, 0.3), 14, 0.15, 
                                boxstyle="round,pad=0.02",
                                facecolor='none', edgecolor=COLORS['secondary'], linewidth=1))

def draw_equation_phase(ax, progress):
    """Phase 0: The DiffEqAuth Equation"""
    # Phase title
    ax.text(8, 7.2, '① The Core Equation', fontsize=20, ha='center',
           color=COLORS['gold'], fontweight='bold')
    
    # Main equation with fade-in
    alpha = min(1.0, progress * 2)
    eq_text = r'$\frac{d\Psi}{dt} = \alpha \cdot I - \beta \cdot R - \gamma \cdot \Psi$'
    ax.text(8, 5.5, eq_text, fontsize=28, ha='center', va='center',
           color=COLORS['white'], alpha=alpha)
    
    # Component labels appearing one by one
    if progress > 0.3:
        ax.text(3, 4, r'$\Psi$ = State Vector', fontsize=14, color=COLORS['primary'],
               alpha=min(1.0, (progress - 0.3) * 3))
    if progress > 0.5:
        ax.text(3, 3.3, r'$I$ = Transactions', fontsize=14, color=COLORS['success'],
               alpha=min(1.0, (progress - 0.5) * 3))
    if progress > 0.7:
        ax.text(10, 4, r'$R$ = Conservation', fontsize=14, color=COLORS['warning'],
               alpha=min(1.0, (progress - 0.7) * 3))
    if progress > 0.9:
        ax.text(10, 3.3, r'$\gamma\Psi$ = Hash Stability', fontsize=14, color=COLORS['purple'],
               alpha=min(1.0, (progress - 0.9) * 3))
    
    # Bottom message
    ax.text(8, 1.5, 'Security emerges from mathematics, not computation',
           fontsize=14, ha='center', color=COLORS['secondary'], style='italic')

def draw_state_vector_phase(ax, progress):
    """Phase 1: State Vector visualization"""
    ax.text(8, 7.2, '② State Vector: 244 Bytes', fontsize=20, ha='center',
           color=COLORS['gold'], fontweight='bold')
    
    # Draw blockchain on left (fading out)
    bc_alpha = max(0.3, 1 - progress)
    ax.add_patch(FancyBboxPatch((1, 3), 4, 3.5, boxstyle="round,pad=0.1",
                               facecolor=COLORS['bg_card'], edgecolor=COLORS['error'],
                               alpha=bc_alpha))
    ax.text(3, 6, 'Blockchain', fontsize=14, ha='center', color=COLORS['error'],
           fontweight='bold', alpha=bc_alpha)
    
    # Stack of blocks
    for i in range(4):
        y = 5.2 - i * 0.55
        ax.add_patch(FancyBboxPatch((1.5, y), 3, 0.45, boxstyle="round,pad=0.02",
                                   facecolor=COLORS['bg_card2'], edgecolor=COLORS['secondary'],
                                   alpha=bc_alpha * 0.7))
    ax.text(3, 3.5, '500 GB', fontsize=16, ha='center', color=COLORS['error'],
           fontweight='bold', alpha=bc_alpha)
    
    # Arrow
    arrow_progress = min(1.0, progress * 2)
    ax.annotate('', xy=(9, 4.75), xytext=(6.5, 4.75),
               arrowprops=dict(arrowstyle='->', color=COLORS['primary'],
                             lw=3, mutation_scale=20),
               alpha=arrow_progress)
    
    # State vector on right (fading in)
    sv_alpha = min(1.0, progress * 1.5)
    ax.add_patch(FancyBboxPatch((9.5, 3), 5, 3.5, boxstyle="round,pad=0.1",
                               facecolor=COLORS['bg_card'], edgecolor=COLORS['success'],
                               alpha=sv_alpha))
    ax.text(12, 6, 'State Vector', fontsize=14, ha='center', color=COLORS['success'],
           fontweight='bold', alpha=sv_alpha)
    
    # State components
    components = ['state_hash[32]', 'total_supply', 'wallets[]', 'timestamp']
    for i, comp in enumerate(components):
        y = 5.2 - i * 0.55
        ax.add_patch(FancyBboxPatch((10, y), 4, 0.45, boxstyle="round,pad=0.02",
                                   facecolor=COLORS['primary'], alpha=sv_alpha * 0.3))
        ax.text(12, y + 0.22, comp, fontsize=11, ha='center', va='center',
               color=COLORS['white'], alpha=sv_alpha)
    
    ax.text(12, 3.5, '244 bytes', fontsize=16, ha='center', color=COLORS['success'],
           fontweight='bold', alpha=sv_alpha)
    
    # Compression ratio
    ax.text(8, 1.5, '2,000,000,000:1 compression ratio',
           fontsize=14, ha='center', color=COLORS['gold'], fontweight='bold')

def draw_conservation_phase(ax, progress):
    """Phase 2: Conservation Law"""
    ax.text(8, 7.2, '③ Conservation Law', fontsize=20, ha='center',
           color=COLORS['gold'], fontweight='bold')
    
    # The constraint equation
    ax.text(8, 5.8, r'$\sum_{i=0}^{n} E_i = E_{total}$', fontsize=24, ha='center',
           color=COLORS['white'])
    
    # Wallets visualization
    total = 1000
    wallets = [300, 250, 200, 150, 100]
    colors = [COLORS['primary'], COLORS['success'], COLORS['warning'], 
              COLORS['purple'], COLORS['secondary']]
    
    x_start = 3
    for i, (bal, col) in enumerate(zip(wallets, colors)):
        width = bal / 100
        alpha = min(1.0, (progress - i * 0.15) * 4) if progress > i * 0.15 else 0
        rect = FancyBboxPatch((x_start, 3.5), width, 1, boxstyle="round,pad=0.02",
                             facecolor=col, alpha=alpha * 0.7)
        ax.add_patch(rect)
        if alpha > 0.5:
            ax.text(x_start + width/2, 4, f'{bal}', fontsize=10, ha='center',
                   va='center', color=COLORS['white'], alpha=alpha)
        x_start += width + 0.1
    
    # Sum arrow
    if progress > 0.7:
        ax.annotate('', xy=(14, 4), xytext=(13, 4),
                   arrowprops=dict(arrowstyle='->', color=COLORS['success'], lw=2))
        ax.text(14.5, 4, '= 1000', fontsize=16, ha='left', va='center',
               color=COLORS['success'], fontweight='bold')
    
    # Invalid state example
    if progress > 0.85:
        ax.add_patch(FancyBboxPatch((3, 1.5), 10, 1.2, boxstyle="round,pad=0.1",
                                   facecolor=COLORS['error'], alpha=0.2,
                                   edgecolor=COLORS['error']))
        ax.text(8, 2.1, 'If sum ≠ total → PC_ERR_CONSERVATION_VIOLATED',
               fontsize=13, ha='center', color=COLORS['error'], fontweight='bold')

def draw_consensus_phase(ax, progress):
    """Phase 3: POC Consensus"""
    ax.text(8, 7.2, '④ Proof-of-Conservation Consensus', fontsize=20, ha='center',
           color=COLORS['gold'], fontweight='bold')
    
    # Three phases
    phases = ['PRE-PREPARE', 'PREPARE', 'COMMIT']
    phase_colors = [COLORS['primary'], COLORS['warning'], COLORS['success']]
    
    for i, (phase_name, col) in enumerate(zip(phases, phase_colors)):
        x = 3 + i * 4.5
        alpha = min(1.0, (progress - i * 0.25) * 3) if progress > i * 0.25 else 0
        
        # Phase box
        ax.add_patch(FancyBboxPatch((x, 4), 3.5, 2, boxstyle="round,pad=0.1",
                                   facecolor=COLORS['bg_card'], edgecolor=col,
                                   alpha=alpha, linewidth=2))
        ax.text(x + 1.75, 5.5, phase_name, fontsize=12, ha='center',
               color=col, fontweight='bold', alpha=alpha)
        
        # Phase description
        if i == 0:
            desc = 'Leader proposes\nΨ → Ψ\''
        elif i == 1:
            desc = 'Validators verify\nΣ balances = total'
        else:
            desc = '2/3 quorum\n→ Finalize'
        ax.text(x + 1.75, 4.6, desc, fontsize=10, ha='center', va='center',
               color=COLORS['white'], alpha=alpha)
        
        # Arrows between phases
        if i < 2 and alpha > 0.5:
            ax.annotate('', xy=(x + 4, 5), xytext=(x + 3.7, 5),
                       arrowprops=dict(arrowstyle='->', color=COLORS['secondary'], lw=2))
    
    # Bottom: The key insight
    if progress > 0.8:
        ax.add_patch(FancyBboxPatch((2, 1.3), 12, 1.5, boxstyle="round,pad=0.1",
                                   facecolor=COLORS['success'], alpha=0.15,
                                   edgecolor=COLORS['success']))
        ax.text(8, 2.3, 'Byzantine validators cannot create money', fontsize=14,
               ha='center', color=COLORS['success'], fontweight='bold')
        ax.text(8, 1.7, 'Conservation law is verified by ALL honest nodes',
               fontsize=12, ha='center', color=COLORS['white'])

def draw_crossshard_phase(ax, progress):
    """Phase 4: Cross-Shard Protection"""
    ax.text(8, 7.2, '⑤ Cross-Shard Double-Spend Prevention', fontsize=20, ha='center',
           color=COLORS['gold'], fontweight='bold')
    
    # Draw two shards
    shard_alpha = min(1.0, progress * 2)
    
    # Shard A
    ax.add_patch(FancyBboxPatch((1.5, 3), 5, 3.5, boxstyle="round,pad=0.1",
                               facecolor=COLORS['bg_card'], edgecolor=COLORS['primary'],
                               alpha=shard_alpha))
    ax.text(4, 6, 'Shard A', fontsize=14, ha='center', color=COLORS['primary'],
           fontweight='bold', alpha=shard_alpha)
    
    # Shard B
    ax.add_patch(FancyBboxPatch((9.5, 3), 5, 3.5, boxstyle="round,pad=0.1",
                               facecolor=COLORS['bg_card'], edgecolor=COLORS['purple'],
                               alpha=shard_alpha))
    ax.text(12, 6, 'Shard B', fontsize=14, ha='center', color=COLORS['purple'],
           fontweight='bold', alpha=shard_alpha)
    
    # Alice trying to double-spend
    if progress > 0.3:
        alice_alpha = min(1.0, (progress - 0.3) * 3)
        # Draw Alice
        circle = Circle((4, 4.5), 0.4, facecolor=COLORS['warning'], alpha=alice_alpha)
        ax.add_patch(circle)
        ax.text(4, 4.5, 'A', fontsize=14, ha='center', va='center',
               color=COLORS['bg_card'], fontweight='bold', alpha=alice_alpha)
        ax.text(4, 3.8, 'Alice: 100', fontsize=10, ha='center',
               color=COLORS['white'], alpha=alice_alpha)
    
    # First transaction (valid)
    if progress > 0.5:
        tx1_alpha = min(1.0, (progress - 0.5) * 3)
        ax.annotate('', xy=(9.5, 5), xytext=(6.5, 5),
                   arrowprops=dict(arrowstyle='->', color=COLORS['success'], lw=2),
                   alpha=tx1_alpha)
        ax.text(8, 5.4, 'TX1: 100 → Bob', fontsize=10, ha='center',
               color=COLORS['success'], alpha=tx1_alpha)
        ax.text(8, 4.7, '🔒 LOCKED', fontsize=12, ha='center',
               color=COLORS['gold'], fontweight='bold', alpha=tx1_alpha)
    
    # Second transaction (blocked)
    if progress > 0.7:
        tx2_alpha = min(1.0, (progress - 0.7) * 3)
        ax.annotate('', xy=(9.5, 3.7), xytext=(6.5, 3.7),
                   arrowprops=dict(arrowstyle='->', color=COLORS['error'], lw=2,
                                 linestyle='dashed'),
                   alpha=tx2_alpha)
        ax.text(8, 4.1, 'TX2: 100 → Carol', fontsize=10, ha='center',
               color=COLORS['error'], alpha=tx2_alpha)
        ax.text(8, 3.4, '❌ BLOCKED', fontsize=12, ha='center',
               color=COLORS['error'], fontweight='bold', alpha=tx2_alpha)
    
    # Bottom message
    ax.text(8, 1.5, 'Global locks + Conservation = Zero double-spend',
           fontsize=14, ha='center', color=COLORS['gold'], fontweight='bold')

def draw_summary_phase(ax, progress):
    """Phase 5: Summary"""
    ax.text(8, 7.2, '⚛️ PhysicsCoin: Mathematics, Not Mining', fontsize=20, ha='center',
           color=COLORS['gold'], fontweight='bold')
    
    # Key points with icons
    points = [
        ('💾', '244 bytes vs 500 GB', COLORS['primary']),
        ('⚡', '116K verify/sec', COLORS['success']),
        ('🔒', 'Conservation = Security', COLORS['warning']),
        ('🌐', 'POC Consensus', COLORS['purple']),
        ('♾️', 'Double-spend impossible', COLORS['gold']),
    ]
    
    for i, (icon, text, color) in enumerate(points):
        alpha = min(1.0, (progress - i * 0.15) * 4) if progress > i * 0.15 else 0
        y = 5.5 - i * 0.8
        
        ax.add_patch(FancyBboxPatch((3, y - 0.3), 10, 0.6, boxstyle="round,pad=0.05",
                                   facecolor=color, alpha=alpha * 0.15,
                                   edgecolor=color))
        ax.text(4, y, f'{icon}  {text}', fontsize=16, ha='left', va='center',
               color=color, fontweight='bold', alpha=alpha)
    
    # Final tagline
    if progress > 0.8:
        ax.text(8, 1.5, 'The math doesn\'t lie. The physics doesn\'t break.',
               fontsize=16, ha='center', color=COLORS['white'], 
               fontweight='bold', style='italic')

# Create animation
def animate(frame):
    create_frame(frame)
    return []

print("Creating PhysicsCoin Mathematical Foundation animation...")
print(f"Total frames: {TOTAL_FRAMES}")

ani = animation.FuncAnimation(fig, animate, frames=TOTAL_FRAMES, 
                               interval=33, blit=False)

# Save as GIF
output_path = 'datagraphics/physicscoin_math_foundation.gif'
os.makedirs('datagraphics', exist_ok=True)

print(f"Saving animation to {output_path}...")
ani.save(output_path, writer='pillow', fps=30, dpi=100)

print(f"✓ Animation saved to {output_path}")
print(f"  Size: {os.path.getsize(output_path) / 1024:.1f} KB")
