#!/usr/bin/env python3
"""
PhysicsCoin Enhanced Visualization
Creates a premium, informative animated GIF for the repo.
Optimized for readability, pacing, and visual appeal.
"""

import matplotlib.pyplot as plt
import matplotlib.patches as patches
import matplotlib.animation as animation
from matplotlib.patches import FancyBboxPatch, Circle, Arrow, FancyArrowPatch
import numpy as np
from matplotlib import patheffects
import os
import math

# ---- CONFIGURATION ----
FPS = 20
SECONDS_PER_SCENE = 5.0
SCENES = [
    'intro',        # The Problem vs Solution
    'equation',     # The Math
    'state',        # 244 Bytes vs 500 GB
    'performance',  # 116K TPS vs Others
    'streaming',    # Money as fluid
    'consensus',    # POC & Conservation
    'iot',          # M2M Economy
    'summary'       # The "Why"
]
TOTAL_FRAMES = int(FPS * SECONDS_PER_SCENE * len(SCENES))

# Theme Colors (GitHub Dark Dimmed inspired + Neon accents)
C = {
    'bg': '#1c2128',       # Darker background
    'text': '#adbac7',     # Main text
    'muted': '#768390',    # Subtitles
    'blue': '#539bf5',     # Primary accent
    'green': '#57ab5a',    # Success
    'red': '#e5534b',      # Error/Blockchain
    'gold': '#db6d28',     # Warning/Gold
    'purple': '#8957e5',   # Math/Science
    'cyan': '#39c5cf',     # Future/IoT
    'card': '#2d333b',     # Card background
}

def setup_plot():
    """Setup the matplotlib figure"""
    plt.style.use('dark_background')
    fig, ax = plt.subplots(figsize=(12, 6.75), dpi=120) # 16:9 aspect ratio
    fig.patch.set_facecolor(C['bg'])
    ax.set_facecolor(C['bg'])
    
    # Remove all axes
    ax.set_xlim(0, 16)
    ax.set_ylim(0, 9)
    ax.axis('off')
    return fig, ax

def draw_card(ax, x, y, w, h, color=C['card'], alpha=1.0, edge_color=None):
    """Refined helper for drawing rounded cards"""
    if edge_color is None: edge_color = color
    box = FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.1,rounding_size=0.2",
                         facecolor=color, edgecolor=edge_color, alpha=alpha, zorder=1)
    ax.add_patch(box)
    return box

def draw_text(ax, x, y, text, size=12, color=C['text'], align='center', weight='normal', alpha=1.0, glow=None):
    """Helper for drawing text with optional glow"""
    effects = [patheffects.withStroke(linewidth=3, foreground=C['bg'])]
    if glow:
        effects.append(patheffects.withStroke(linewidth=5, foreground=glow, alpha=0.5))
        
    t = ax.text(x, y, text, fontsize=size, ha=align, va='center', 
                color=color, fontweight=weight, alpha=alpha, zorder=10,
                path_effects=effects)
    return t

def render_frame(frame_num, ax):
    """Render a single frame"""
    ax.clear()
    ax.set_xlim(0, 16)
    ax.set_ylim(0, 9)
    ax.axis('off')
    
    # Global Header (Fixed position)
    draw_text(ax, 8, 8.2, "PHYSICSCOIN", size=28, weight='bold', color=C['blue'], glow=C['blue'])
    
    # Calculate scene
    scene_idx = frame_num // int(FPS * SECONDS_PER_SCENE)
    if scene_idx >= len(SCENES): scene_idx = len(SCENES) - 1
    scene_name = SCENES[scene_idx]
    
    # Scene progress (0.0 to 1.0)
    scene_frames = FPS * SECONDS_PER_SCENE
    local_frame = frame_num % scene_frames
    p = local_frame / scene_frames
    
    # Draw scene content
    if scene_name == 'intro':
        scene_intro(ax, p)
    elif scene_name == 'equation':
        scene_equation(ax, p)
    elif scene_name == 'state':
        scene_state(ax, p)
    elif scene_name == 'performance':
        scene_performance(ax, p)
    elif scene_name == 'streaming':
        scene_streaming(ax, p)
    elif scene_name == 'consensus':
        scene_consensus(ax, p)
    elif scene_name == 'iot':
        scene_iot(ax, p)
    elif scene_name == 'summary':
        scene_summary(ax, p)
        
    # Progress Bar at bottom
    total_p = frame_num / TOTAL_FRAMES
    draw_card(ax, 0, 0, 16 * total_p, 0.15, color=C['blue'])

# ---- SCENE FUNCTIONS ----

def scene_intro(ax, p):
    draw_text(ax, 8, 7.2, "The Paradigm Shift", size=16, color=C['muted'])
    
    # animate items entering
    if p > 0.1:
        # Left side: Old Way
        a1 = min(1, (p-0.1)*3)
        draw_card(ax, 1, 2, 6, 4, color=C['card'], alpha=a1, edge_color=C['red'])
        draw_text(ax, 4, 5.5, "OLD PARADIGM", size=14, color=C['red'], weight='bold', alpha=a1)
        draw_text(ax, 4, 4.5, "Accountants & Ledgers", size=12, alpha=a1)
        draw_text(ax, 4, 3.8, "Mining (Wasted Energy)", size=12, alpha=a1)
        draw_text(ax, 4, 3.1, "Discrete Logic (Check rules)", size=12, alpha=a1)

    if p > 0.4:
        # Arrow
        a2 = min(1, (p-0.4)*3)
        draw_text(ax, 8, 4, "VS", size=20, weight='bold', alpha=a2)

    if p > 0.6:
        # Right side: New Way
        a3 = min(1, (p-0.6)*3)
        draw_card(ax, 9, 2, 6, 4, color=C['card'], alpha=a3, edge_color=C['green'])
        draw_text(ax, 12, 5.5, "NEW PARADIGM", size=14, color=C['green'], weight='bold', alpha=a3)
        draw_text(ax, 12, 4.5, "Physics & State Vectors", size=12, alpha=a3)
        draw_text(ax, 12, 3.8, "Conservation (Nature's Law)", size=12, alpha=a3)
        draw_text(ax, 12, 3.1, "Continuous Dynamics (Calculus)", size=12, alpha=a3)

def scene_equation(ax, p):
    draw_text(ax, 8, 7.2, "Security Through Mathematics", size=16, color=C['muted'])
    
    # The Equation central
    a_eq = min(1, p*2)
    latex = r'$\frac{d\Psi}{dt} = \alpha I - \beta R - \gamma \Psi$'
    ax.text(8, 5, latex, fontsize=36, ha='center', va='center', color=C['text'], alpha=a_eq)
    
    # Annotations appearing sequence
    if p > 0.3:
        draw_text(ax, 4, 3, "Transactions (Force)", color=C['blue'], size=14, alpha=min(1, (p-0.3)*4))
        # Line to alpha I
        if p > 0.35: ax.add_patch(Arrow(4, 3.3, 2, 1.2, width=0.5, color=C['blue'], alpha=0.5))

    if p > 0.5:
        draw_text(ax, 12, 3, "Conservation (Resistance)", color=C['gold'], size=14, alpha=min(1, (p-0.5)*4))
        # Line to beta R
        if p > 0.55: ax.add_patch(Arrow(12, 3.3, -2, 1.2, width=0.5, color=C['gold'], alpha=0.5))

    if p > 0.7:
        draw_text(ax, 8, 2, "State Stability (Decay)", color=C['purple'], size=14, alpha=min(1, (p-0.7)*4))
        
    if p > 0.85:
         draw_text(ax, 8, 1, "A Dynamical System, Not a Ledger", color=C['green'], size=14, weight='bold', glow=C['green'])

def scene_state(ax, p):
    draw_text(ax, 8, 7.2, "2,000,000,000:1 Compression", size=16, color=C['muted'])
    
    # Visual comparison scale
    
    # 500 GB Block
    draw_card(ax, 2, 2, 4, 4, C['red'], alpha=0.3)
    draw_text(ax, 4, 4.5, "Blockchain", color=C['red'], size=18, weight='bold')
    draw_text(ax, 4, 3.5, "500 GB", color=C['text'], size=24, weight='bold')
    draw_text(ax, 4, 2.8, "History Log", color=C['muted'], size=12)

    # Transition
    if p > 0.3:
        draw_text(ax, 8, 4, ">>>", size=20, alpha=min(1, (p-0.3)*3))
        
    # 244 Bytes
    if p > 0.5:
        a = min(1, (p-0.5)*3)
        draw_card(ax, 10, 3.5, 4, 1, C['green'], alpha=a) # Tiny box
        draw_text(ax, 12, 4.0, "State Vector", color=C['green'], size=18, weight='bold', alpha=a)
        draw_text(ax, 12, 3.0, "244 BYTES", color=C['text'], size=24, weight='bold', alpha=a)
        draw_text(ax, 12, 2.3, "Current State Only", color=C['muted'], size=12, alpha=a)

def scene_performance(ax, p):
    draw_text(ax, 8, 7.2, "Speed & Throughput", size=16, color=C['muted'])
    
    # Bar Chart
    names = ['Bitcoin', 'Ethereum', 'Solana', 'PhysicsCoin']
    vals = [7, 30, 65000, 116000] # Log scale for visualization? No, linear bar just shooting off screen
    colors = [C['red'], C['purple'], C['blue'], C['green']]
    
    y_pos = [5.5, 4.5, 3.5, 2.5]
    
    for i, (name, val, col, y) in enumerate(zip(names, vals, colors, y_pos)):
        appear = 0.2 + i*0.15
        if p > appear:
            w_max = 12
            # normalize roughly to 120k
            w = (val / 120000) * w_max
            w = max(0.2, w)
            draw_card(ax, 2, y-0.3, w, 0.6, col)
            draw_text(ax, 1.5, y, name, align='right', size=12)
            draw_text(ax, 2 + w + 0.5, y, f"{val:,} TPS", align='left', size=12, weight='bold', color=col)

def scene_streaming(ax, p):
    draw_text(ax, 8, 7.2, "Native Streaming Payments", size=16, color=C['muted'])

    # Equation integral
    draw_text(ax, 8, 5.5, r"$\Delta \Psi = \int_{t_0}^{t_1} I(t) dt$", size=24, color=C['blue'])
    
    if p > 0.3:
        # Pipe visualization
        draw_card(ax, 3, 3, 10, 1, color='#333333')
        
        # Flowing particles
        offset = (p * 20) % 2
        for i in range(10):
            cx = 3.5 + i + offset
            if 3 <= cx <= 13:
                c =  Circle((cx, 3.5), 0.2, color=C['blue'])
                ax.add_patch(c)
                
        draw_text(ax, 8, 2.5, "Continuous Money Flow (Not Discrete Blocks)", size=14)
        draw_text(ax, 2, 3.5, "Alice", size=14, align='right')
        draw_text(ax, 14, 3.5, "Bob", size=14, align='left')

def scene_consensus(ax, p):
    draw_text(ax, 8, 7.2, "Proof-of-Conservation Consensus", size=16, color=C['muted'])
    
    # Logic flow
    draw_text(ax, 8, 6, "Can a validator cheat?", size=14)
    
    if p > 0.2:
        draw_card(ax, 3, 3.5, 3, 1.5, C['red'], alpha=0.5)
        draw_text(ax, 4.5, 4.25, "Malicious Node", size=10, weight='bold')
        draw_text(ax, 4.5, 3.8, "Creates +100 Coins", size=10, color=C['red'])

    if p > 0.4:
         # Arrow
        ax.add_patch(FancyArrowPatch((6.5, 4.25), (8.5, 4.25), color=C['muted']))

    if p > 0.5:
        # Physics Check
        draw_card(ax, 9, 3, 4, 2.5, C['green'], alpha=0.2, edge_color=C['green'])
        draw_text(ax, 11, 4.8, "The Laws of Physics", color=C['green'], weight='bold')
        draw_text(ax, 11, 4.2, r"$\sum E_{in} \neq \sum E_{out}$", color=C['text'], size=14)
        draw_text(ax, 11, 3.6, "VIOLATION DETECTED", color=C['red'], weight='bold', size=12)
        
    if p > 0.8:
        draw_text(ax, 8, 1.5, "Math makes cheating IMPOSSIBLE (not just expensive)", color=C['gold'], size=14)

def scene_iot(ax, p):
    draw_text(ax, 8, 7.2, "The Economy of Things", size=16, color=C['muted'])
    
    # Network of devices
    devices = [
        (4, 5, "Toaster Node"),
        (8, 3, "Car Charger"),
        (12, 5, "Drone Delivery"),
        (8, 6, "AI Agent")
    ]
    
    # Connections
    if p > 0.2:
        for i in range(len(devices)):
            for j in range(i+1, len(devices)):
                x1, y1, _ = devices[i]
                x2, y2, _ = devices[j]
                ax.plot([x1, x2], [y1, y2], color=C['blue'], alpha=0.3, linewidth=1)
    
    # Nodes
    if p > 0.4:
        for x, y, labels in devices:
            alpha = min(1, (p-0.4)*3)
            # draw_circle(ax, x, y, 0.8, color=C['card'], border=C['cyan'])
            c = Circle((x, y), 0.6, facecolor=C['card'], edgecolor=C['cyan'], linewidth=2, zorder=5, alpha=alpha)
            ax.add_patch(c)
            draw_text(ax, x, y-1, labels, size=10, color=C['cyan'], alpha=alpha)

    if p > 0.7:
        draw_text(ax, 8, 1.5, "Micro-transactions + Low Power + No History = M2M Native", size=14)

def scene_summary(ax, p):
    # Summary list
    items = [
        ("⚛️ Physics-Based", "No mining, just math"),
        ("🚀 116,000 TPS", "Faster than Visa/Solana"),
        ("💾 244 Byte State", "Download in milliseconds"),
        ("🌊 Streaming Money", "Pay-per-second native"),
        ("🔒 Unbreakable", "Consensus via Conservation")
    ]
    
    for i, (head, sub) in enumerate(items):
        appear = i * 0.15
        if p > appear:
            y = 7.5 - i * 1.3
            alpha = min(1, (p-appear)*4)
            # Icon/Header
            draw_text(ax, 3, y, head, size=18, align='left', weight='bold', color=C['text'], alpha=alpha)
            # Desc
            draw_text(ax, 13, y, sub, size=14, align='right', color=C['blue'], alpha=alpha)
            
            # Divider line
            if i < len(items)-1:
                ax.plot([3, 13], [y-0.6, y-0.6], color=C['card'], lw=1, alpha=alpha*0.5)

# Initialize and run
fig, ax = setup_plot()
print(f"Rendering {TOTAL_FRAMES} frames ({len(SCENES)} scenes x {SECONDS_PER_SCENE}s @ {FPS}fps)")
ani = animation.FuncAnimation(fig, render_frame, fargs=(ax,), frames=TOTAL_FRAMES, interval=1000/FPS)

# Ensure directory exists
os.makedirs('datagraphics', exist_ok=True)
output_file = 'datagraphics/physicscoin_math_foundation.gif'
ani.save(output_file, writer='pillow', fps=FPS, dpi=100)
print(f"Done! Saved to {output_file}")
