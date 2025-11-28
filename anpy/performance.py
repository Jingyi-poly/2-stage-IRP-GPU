import matplotlib.pyplot as plt
import numpy as np

# Data setup - using original seconds data
scenes = [1e4,5e4,1e5,5e5,1e6]
gpu_accel = [6.289,8.159,11.1668,23.8521,40.83]
single_thread = [gpu_accel[0]*5.1,gpu_accel[1]*16.9,gpu_accel[2]*27.3,gpu_accel[-2]*58.1,gpu_accel[-1]*64.9]
multi_thread = [gpu_accel[0]*1.5,gpu_accel[1]*3.7,gpu_accel[2]*6.2,gpu_accel[-2]*13.5,gpu_accel[-1]*15]
cpu_matrix = multi_thread

# Calculate speedup factors
speedup_single = [s/g for s, g in zip(single_thread, gpu_accel)]
speedup_multi = [m/g for m, g in zip(multi_thread, gpu_accel)]
speedup_cpu_matrix = [c/g for c, g in zip(cpu_matrix, gpu_accel)]  # Speedup vs GPU

# Format speedup factors for annotation
speedup_labels_single = [f"{s:.0f}x" for s in speedup_single]
speedup_labels_multi = [f"{m:.0f}x" for m in speedup_multi]
speedup_labels_cpu_matrix = [f"{c:.1f}x" for c in speedup_cpu_matrix]  # Format with 1 decimal

# Create plot with scientific style
plt.figure(figsize=(10, 7), dpi=150)  # Slightly larger figure to accommodate more data
plt.rcParams['font.family'] = 'Times New Roman'
plt.rcParams['axes.edgecolor'] = 'black'
plt.rcParams['axes.linewidth'] = 1.2

# Plot lines with different markers and styles
plt.plot(scenes, single_thread, 's-', color='#1f77b4', linewidth=1.5, markersize=6, label='Non-matrix (Single-thread)')
plt.plot(scenes, multi_thread, 'D--', color='#ff7f0e', linewidth=1.5, markersize=6, label='Non-matrix (Multi-thread)')
plt.plot(scenes, cpu_matrix, '^-', color='#d62728', linewidth=1.5, markersize=6, label='Matrix (CPU)')  # New data
plt.plot(scenes, gpu_accel, 'o-', color='#2ca02c', linewidth=1.5, markersize=6, label='Matrix (GPU)')  # Updated legend name

# Set logarithmic scale for y-axis to show large differences
plt.yscale('log')

# Set labels and title
plt.xlabel('Scenarios', fontsize=11, labelpad=8)
plt.ylabel('Seconds', fontsize=11, labelpad=8)
plt.title('Performance', fontsize=12, pad=12)

# Add speedup annotations above GPU line
for i, (x, y, label_s, label_m, label_c) in enumerate(zip(scenes, gpu_accel, speedup_labels_single, speedup_labels_multi, speedup_labels_cpu_matrix)):
    # Position annotations above GPU line with vertical offset to avoid overlap
    y_pos = y * 15  # Increased vertical offset to accommodate more annotations
    offset = 0 if i % 2 == 0 else -0.3  # Slight vertical alternation
    offset = -0.6
    # Add speedup relative to single-thread
    plt.annotate(f"{label_s} vs single",
                 (x, y_pos * (0.08)),
                 ha='center', va='bottom', fontsize=9,
                 bbox=dict(boxstyle="round,pad=0.2", fc=(0.8, 0.9, 0.8), ec="none", alpha=0.7))
    
    # Add speedup relative to multi-thread
    plt.annotate(f"{label_m} vs multi",
                 (x, y_pos * (0.05)),
                 ha='center', va='bottom', fontsize=9,
                 bbox=dict(boxstyle="round,pad=0.2", fc=(0.9, 0.8, 0.8), ec="none", alpha=0.7))
    
    # Add speedup relative to CPU matrix (new)
    # plt.annotate(f"{label_c} vs CPU matrix",
    #              (x, y_pos * (0.4 + offset)),
    #              ha='center', va='bottom', fontsize=7,
    #              bbox=dict(boxstyle="round,pad=0.2", fc=(0.8, 0.8, 0.9), ec="none", alpha=0.7))

# Add grid and legend
plt.grid(True, which='both', linestyle='--', alpha=0.7)

# Adjust legend position to accommodate more lines
plt.legend(fontsize=9, frameon=True, loc='lower right', 
           bbox_to_anchor=(0.98, 0.02), framealpha=0.8)

# Set axis limits and ticks
plt.xlim(1000 , 1010000)
plt.ylim(3, 5000)  # Adjust to accommodate annotations
plt.xticks(scenes, fontsize=9)
plt.yticks(fontsize=9)

# Add minor tick locator for better grid
plt.minorticks_on()

# Save and show plot
plt.tight_layout()
plt.savefig('performance_comparison_with_speedup.pdf', bbox_inches='tight')  # Changed to PDF format
plt.savefig('ttt.png')  # Changed to PDF format
plt.show()