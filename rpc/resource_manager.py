#!/usr/bin/env python3
"""
Resource Manager for IRP Solver
Detects system resources and provides intelligent resource allocation strategies
"""

import os
import sys
import json
import argparse
from typing import Dict, List, Tuple, Optional


def detect_system_resources() -> Dict:
  """
  Detect available system resources (CPU, RAM, GPU)

  Returns:
    dict: System resource information including CPU cores, RAM, and GPU details
  """
  resources = {
    'cpu_cores': 0,
    'total_ram_gb': 0,
    'available_ram_gb': 0,
    'gpu_available': False,
    'gpu_name': None,
    'gpu_memory_gb': 0,
    'gpu_memory_free_gb': 0,
  }

  # Detect CPU cores
  resources['cpu_cores'] = os.cpu_count() or 4

  # Detect RAM using psutil if available, fallback to /proc/meminfo
  try:
    import psutil
    memory = psutil.virtual_memory()
    resources['total_ram_gb'] = memory.total / (1024**3)
    resources['available_ram_gb'] = memory.available / (1024**3)
  except ImportError:
    # Fallback: parse /proc/meminfo (Linux only)
    try:
      with open('/proc/meminfo', 'r') as f:
        meminfo = {}
        for line in f:
          parts = line.split(':')
          if len(parts) == 2:
            key = parts[0].strip()
            value = parts[1].strip().split()[0] # Get numeric value
            meminfo[key] = int(value)

        resources['total_ram_gb'] = meminfo.get('MemTotal', 0) / (1024**2)
        resources['available_ram_gb'] = meminfo.get('MemAvailable', 0) / (1024**2)
    except Exception as e:
      print(f"Warning: Could not detect RAM: {e}", file=sys.stderr)
      resources['total_ram_gb'] = 8.0 # Conservative fallback
      resources['available_ram_gb'] = 4.0

  # Detect GPU using PyTorch
  try:
    import torch
    if torch.cuda.is_available():
      resources['gpu_available'] = True
      props = torch.cuda.get_device_properties(0)
      resources['gpu_name'] = props.name
      resources['gpu_memory_gb'] = props.total_memory / (1024**3)

      # Get free memory
      free_mem = props.total_memory - torch.cuda.memory_allocated(0)
      resources['gpu_memory_free_gb'] = free_mem / (1024**3)
  except ImportError:
    print("Warning: PyTorch not available, GPU detection skipped", file=sys.stderr)
  except Exception as e:
    print(f"Warning: GPU detection failed: {e}", file=sys.stderr)

  return resources


def estimate_memory_per_scenario(T: int = 6, max_inventory: int = 144) -> float:
  """
  Estimate GPU memory required per scenario

  Args:
    T: Number of planning days
    max_inventory: Maximum inventory level

  Returns:
    float: Memory in MB per scenario
  """
  num_states = max_inventory + 1

  # Memory components (based on gpu_solver.py analysis):
  # C matrix: (T+1) x states x 4 bytes (float32)
  C_size = (T + 1) * num_states * 4
  # D matrix: T x states x 1 byte (bool)
  D_size = T * num_states * 1
  # P matrix: T x states x 8 bytes (long/int64)
  P_size = T * num_states * 8

  # Add 50% overhead for intermediate tensors and PyTorch overhead
  total_size = (C_size + D_size + P_size) * 1.5

  return total_size / (1024 * 1024) # Convert to MB


def calculate_optimal_batch_size(
  scenario_count: int,
  gpu_memory_gb: float,
  T: int = 6,
  max_inventory: int = 144
) -> int:
  """
  Calculate optimal GPU batch size based on scenario count and available GPU memory

  Args:
    scenario_count: Total number of scenarios
    gpu_memory_gb: Available GPU memory in GB
    T: Number of planning days
    max_inventory: Maximum inventory level

  Returns:
    int: Optimal batch size
  """
  if gpu_memory_gb == 0:
    return 128 # Conservative fallback for CPU mode

  # Calculate memory per scenario
  mem_per_scenario_mb = estimate_memory_per_scenario(T, max_inventory)

  # Available memory (use 70% to leave safety margin)
  available_mem_mb = gpu_memory_gb * 1024 * 0.7

  # Theoretical maximum batch size
  max_batch = int(available_mem_mb / mem_per_scenario_mb)

  # Decision logic based on scenario count
  if scenario_count < 128:
    # Small scenario count: process all at once
    return scenario_count
  elif scenario_count < max_batch:
    # Can fit in one batch but still use reasonable size
    # Use power of 2 for efficiency
    candidate = 128
    while candidate < scenario_count and candidate < 512:
      candidate *= 2
    return min(candidate, 512)
  else:
    # Large scenario count: use maximum safe batch size
    # Cap at 512 (well-tested default) or 768 for large GPUs
    if gpu_memory_gb >= 16:
      return min(768, max_batch)
    else:
      return min(512, max_batch)


def estimate_resource_requirements(
  scenarios: List[int],
  threads_per_process: int = 8,
  T: int = 6,
  max_inventory: int = 144
) -> Dict:
  """
  Estimate resource requirements for running multiple scenario counts

  Args:
    scenarios: List of scenario counts to run (e.g., [100, 500, 1000])
    threads_per_process: Number of CPU threads per process
    T: Number of planning days
    max_inventory: Maximum inventory level

  Returns:
    dict: Resource requirements including RAM, GPU memory, CPU threads
  """
  # Estimate RAM per process (conservative)
  # Based on analysis: scenarios x days x clients x 8 bytes + overhead
  max_scenario_count = max(scenarios)
  ram_per_process_gb = (max_scenario_count * T * 50 * 8) / (1024**3) + 1.0 # +1GB overhead

  # GPU memory per process
  gpu_mem_per_scenario_mb = estimate_memory_per_scenario(T, max_inventory)
  max_gpu_mem_gb = max(s * gpu_mem_per_scenario_mb / 1024 for s in scenarios)

  # CPU threads
  total_threads = len(scenarios) * threads_per_process if threads_per_process > 0 else 0

  return {
    'ram_per_process_gb': ram_per_process_gb,
    'total_ram_gb': ram_per_process_gb * len(scenarios),
    'gpu_memory_per_process_gb': max_gpu_mem_gb,
    'cpu_threads_total': total_threads,
    'num_processes': len(scenarios)
  }


def decide_parallel_strategy(
  scenarios: List[int],
  resources: Dict,
  threads_per_process: int = 8,
  T: int = 6,
  max_inventory: int = 144
) -> Tuple[bool, int, str]:
  """
  Decide whether to run scenarios in parallel or serial

  Args:
    scenarios: List of scenario counts
    resources: Available system resources from detect_system_resources()
    threads_per_process: CPU threads per process
    T: Number of planning days
    max_inventory: Maximum inventory level

  Returns:
    tuple: (can_parallel, max_parallel_jobs, reason)
  """
  # Get resource requirements
  requirements = estimate_resource_requirements(scenarios, threads_per_process, T, max_inventory)

  # Check GPU memory (if using GPU mode, threads_per_process should be 0)
  if threads_per_process == 0 and resources['gpu_available']:
    # GPU mode: check if GPU can handle concurrent processes
    # Conservative: each process should use < 50% of total GPU memory
    gpu_ok = requirements['gpu_memory_per_process_gb'] < (resources['gpu_memory_gb'] * 0.5)

    if not gpu_ok:
      return False, 1, f"GPU memory insufficient for parallel (need {requirements['gpu_memory_per_process_gb']:.1f}GB per process, have {resources['gpu_memory_gb']:.1f}GB total)"
  else:
    gpu_ok = True # Not using GPU or CPU mode

  # Check system RAM
  # Use 60% of available RAM to leave margin for system
  ram_ok = requirements['total_ram_gb'] < (resources['available_ram_gb'] * 0.6)

  if not ram_ok:
    # Calculate how many parallel jobs we can support
    max_jobs = max(1, int(resources['available_ram_gb'] * 0.6 / requirements['ram_per_process_gb']))
    return False, max_jobs, f"RAM insufficient for full parallel (need {requirements['total_ram_gb']:.1f}GB, have {resources['available_ram_gb']:.1f}GB available)"

  # Check CPU threads
  # Allow up to 1.5x oversubscription
  cpu_ok = requirements['cpu_threads_total'] <= (resources['cpu_cores'] * 1.5)

  if not cpu_ok:
    max_jobs = max(1, int(resources['cpu_cores'] * 1.5 / threads_per_process))
    return False, max_jobs, f"CPU threads insufficient for full parallel (need {requirements['cpu_threads_total']}, have {resources['cpu_cores']} cores)"

  # All checks passed
  return True, len(scenarios), "All resources sufficient for parallel execution"


def main():
  """CLI interface for resource manager"""
  parser = argparse.ArgumentParser(description='IRP Solver Resource Manager')

  subparsers = parser.add_subparsers(dest='command', help='Command to execute')

  # Detect command
  subparsers.add_parser('detect', help='Detect system resources')

  # Decide command
  decide_parser = subparsers.add_parser('decide', help='Decide parallel strategy')
  decide_parser.add_argument('--scenarios', type=int, nargs='+', required=True,
               help='Scenario counts (e.g., 100 500 1000)')
  decide_parser.add_argument('--threads', type=int, default=8,
               help='Threads per process (0 for GPU mode)')
  decide_parser.add_argument('--days', type=int, default=6,
               help='Number of planning days')
  decide_parser.add_argument('--max-inventory', type=int, default=144,
               help='Maximum inventory level')

  # Batch size command
  batch_parser = subparsers.add_parser('batch-size', help='Calculate optimal batch size')
  batch_parser.add_argument('--scenarios', type=int, required=True,
               help='Number of scenarios')
  batch_parser.add_argument('--days', type=int, default=6,
               help='Number of planning days')
  batch_parser.add_argument('--max-inventory', type=int, default=144,
               help='Maximum inventory level')

  args = parser.parse_args()

  if args.command == 'detect':
    resources = detect_system_resources()
    print(json.dumps(resources, indent=2))

  elif args.command == 'decide':
    resources = detect_system_resources()
    can_parallel, max_jobs, reason = decide_parallel_strategy(
      args.scenarios,
      resources,
      args.threads,
      args.days,
      args.max_inventory
    )

    result = {
      'can_parallel': can_parallel,
      'max_parallel_jobs': max_jobs,
      'reason': reason,
      'scenarios': args.scenarios,
      'resources': resources
    }
    print(json.dumps(result, indent=2))

  elif args.command == 'batch-size':
    resources = detect_system_resources()
    batch_size = calculate_optimal_batch_size(
      args.scenarios,
      resources.get('gpu_memory_free_gb', resources.get('gpu_memory_gb', 0)),
      args.days,
      args.max_inventory
    )

    result = {
      'batch_size': batch_size,
      'scenario_count': args.scenarios,
      'gpu_memory_gb': resources.get('gpu_memory_gb', 0)
    }
    print(json.dumps(result, indent=2))

  else:
    parser.print_help()
    sys.exit(1)


if __name__ == '__main__':
  main()
