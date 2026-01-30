import json
import torch
import numpy as np
import time
from typing import List, Dict, Any, Tuple, Optional

class Solver:
  def __init__(
    self,
    params: dict,
    client_id: int,
    batch_scenarios_data: Dict[str, torch.Tensor],
    device: torch.device = None
  ):
    self.device = device or torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    self.client = params['cli'][client_id]
    self.B = batch_scenarios_data['demands'].size(0)
    self.T = params['ancienNbDays']

    self.demands = batch_scenarios_data['demands'][:, :self.T].t().contiguous().to(self.device, non_blocking=True)
    self.delivery_costs = batch_scenarios_data['delivery_costs'][:, :self.T].t().contiguous().to(self.device, non_blocking=True)
    self.capacity_thresholds = batch_scenarios_data['capacity_thresholds'][:, :self.T].t().contiguous().to(self.device, non_blocking=True)
    self.capacity_slopes = batch_scenarios_data['capacity_slopes'][:, :self.T].t().contiguous().to(self.device, non_blocking=True)
    
    self.scenario_start_inv = batch_scenarios_data['scenario_start_inventories'].to(self.device, non_blocking=True)

    self.i_max = self.client['maxInventory']
    self.min_inv = 0
    self.max_inv = int(self.i_max)
    self.num_states = self.max_inv - self.min_inv + 1
    self.inv_cost = float(self.client['inventoryCost'])
    self.stockout_cost = float(self.client['stockoutCost'])
    # REFACTOR: Add supplier_minus_cost to match CPU cost formula
    # This discount reduces the cost of delivery based on avoided supplier holding costs
    self.supplier_cost = float(self.client.get('supplierMinusCost', 0.0))
    
    # Precompute inventory penalty vector and delivery quantities
    self.states_arr = torch.arange(self.min_inv, self.max_inv + 1, 
                   dtype=torch.float32, device=self.device)
    self.inv_penalty = torch.where(
      self.states_arr >= 0,
      self.states_arr * self.inv_cost,
      -self.states_arr * self.stockout_cost
    )
    self.quantities_based = self.i_max - self.states_arr
    
    # Create CUDA stream
    self.stream = torch.cuda.Stream() if self.device.type == 'cuda' else None
    
    # Memory pool
    self.memory_pool = {}

  def get_tensor(self, shape, dtype, name, fill_value=None):
    """Get or create tensor from memory pool"""
    key = (shape, dtype, name)
    if key in self.memory_pool:
      tensor = self.memory_pool[key]
      if fill_value is not None:
        tensor.fill_(fill_value)
      return tensor
    else:
      tensor = torch.empty(shape, dtype=dtype, device=self.device)
      if fill_value is not None:
        tensor.fill_(fill_value)
      self.memory_pool[key] = tensor
      return tensor

  @torch.no_grad()
  def solve_batch(self) -> Tuple[torch.Tensor, List[List[int]], List[List[float]]]:
    B, T, num_states = self.B, self.T, self.num_states
    min_inv, max_inv = self.min_inv, self.max_inv
    large_number = 1e18
    
    # Get tensors from memory pool
    C_shape = (T + 1, B, num_states)
    C = self.get_tensor(C_shape, torch.float32, "C", large_number)
    C[T].fill_(0.0) # Initialize final state
    
    # Decision and transition matrices
    D = self.get_tensor((T, B, num_states), torch.bool, "D", False)
    P = self.get_tensor((T, B, num_states), torch.long, "P", 0)
    
    if self.device.type == 'cuda':
      with torch.cuda.stream(self.stream):
        self._solve_dp(B, T, num_states, min_inv, max_inv, C, D, P)
    else:
      self._solve_dp(B, T, num_states, min_inv, max_inv, C, D, P)
    
    if self.device.type == 'cuda':
      torch.cuda.current_stream().wait_stream(self.stream)
    
    # Backtrack path - using each scenario's own initial inventory
    return self._backtrack(B, T, min_inv, max_inv, C, D, P)

  def _solve_dp(self, B, T, num_states, min_inv, max_inv, C, D, P):
    """Execute dynamic programming computation"""

    with torch.amp.autocast(device_type='cuda', dtype=torch.float16):
      for t in range(T - 1, -1, -1):
        # Get day t demand
        d_t = self.demands[t]
        
        # 1. No delivery decision
        after_demand = self.states_arr.view(1, -1) - d_t.view(-1, 1)
        
        # Combined calculation: inventory cost + state transition
        next_state_idx = (after_demand.clamp(min_inv, max_inv) - min_inv).clamp(0, num_states - 1).long()
        daily_cost = torch.where(
          after_demand >= 0,
          after_demand * self.inv_cost,
          -after_demand * self.stockout_cost
        )
        future_cost = torch.gather(C[t + 1], 1, next_state_idx)
        cost_no_delivery = daily_cost + future_cost
        
        # 2. Delivery decision
        delivery_state = torch.empty((B, 1), dtype=torch.float32, device=self.device)
        delivery_state.fill_(max_inv)
        after_delivery_demand = delivery_state - d_t.view(-1, 1)
        
        # Combined calculation: inventory cost + state transition
        next_state_idx_delivery = (after_delivery_demand.clamp(min_inv, max_inv) - min_inv).clamp(0, num_states - 1).long()
        daily_cost_delivery = torch.where(
          after_delivery_demand >= 0,
          after_delivery_demand * self.inv_cost,
          -after_delivery_demand * self.stockout_cost
        )
        future_cost_delivery = torch.gather(C[t + 1], 1, next_state_idx_delivery)
        
        # Calculate delivery quantities and capacity penalty
        quantities = self.quantities_based.view(1, -1)
        capacity_penalty = self.capacity_slopes[t].view(-1, 1) * torch.clamp(
          quantities - self.capacity_thresholds[t].view(-1, 1), min=0.0
        )

        fixed_delivery_cost = self.delivery_costs[t].view(-1, 1)

        # REFACTOR: Calculate supplier discount to match CPU formula
        # CPU formula: supplier_discount = supplier_cost * (ancienNbDays - day + 1) * quantity
        # where day is 1-based. For GPU with 0-based t, this becomes:
        # supplier_discount = supplier_cost * (T - t) * quantity
        # This discount represents avoided supplier holding costs
        remaining_days = T - t
        supplier_discount = self.supplier_cost * remaining_days * quantities

        # Total delivery cost (note: supplier_discount is SUBTRACTED)
        cost_with_delivery = (fixed_delivery_cost + capacity_penalty +
                   daily_cost_delivery + future_cost_delivery - supplier_discount)
        
        # 3. Select optimal decision - use minimum function to reduce memory allocation
        C[t] = torch.minimum(cost_with_delivery, cost_no_delivery)
        D[t] = cost_with_delivery < cost_no_delivery
        P[t] = torch.where(
          D[t],
          next_state_idx_delivery,
          next_state_idx
        )

  def _backtrack(self, B, T, min_inv, max_inv, C, D, P):
    """Execute backtracking path calculation - using each scenario's initial inventory"""
    # Calculate initial state index for each scenario
    start_idx = (self.scenario_start_inv - min_inv).clamp(0, self.num_states - 1).long()
    
    # Initial state
    current_state = start_idx
    final_costs = C[0, torch.arange(B), current_state]
    
    # Get backtracking tensors from memory pool
    all_flags = self.get_tensor((B, T), torch.int32, "flags", 0)
    all_quantities = self.get_tensor((B, T), torch.float32, "quantities", 0)
    state_tensor = self.get_tensor((B, T+1), torch.long, "state_tensor", 0)
    state_tensor[:, 0] = current_state
    
    # Batch backtracking
    for t in range(T):
      # Get current state
      current_state_idx = state_tensor[:, t]
      
      # Get decision
      is_delivery = D[t, torch.arange(B), current_state_idx].bool()
      current_inv = self.states_arr[current_state_idx]
      
      # Calculate delivery quantity
      quantities = torch.where(is_delivery, self.i_max - current_inv, 0.0)
      
      # Store results
      all_flags[:, t] = is_delivery.int()
      all_quantities[:, t] = quantities
      
      # Update to next state
      next_state_idx = P[t, torch.arange(B), current_state_idx]
      state_tensor[:, t+1] = next_state_idx
    
    # Convert to list format
    all_flags_list = all_flags.cpu().numpy().tolist()
    all_quantities_list = all_quantities.cpu().numpy().tolist()

    return final_costs, all_flags_list, all_quantities_list

class ScenarioResult:
  def __init__(self, global_idx: int, cost: float, flags: List[int], quantities: List[float]):
    self.global_idx = global_idx
    self.cost = cost
    self.flags = flags
    self.quantities = quantities

@torch.no_grad()
def solve_all_scenarios(
  params: dict,
  client_id: int,
  all_scenarios_data: Dict[str, torch.Tensor],
  batch_size: int = 2048,
  device: torch.device = None
) -> List[ScenarioResult]:
  device = device or torch.device('cuda' if torch.cuda.is_available() else 'cpu')
  N = all_scenarios_data['demands'].shape[0]
  
  # Automatically determine optimal batch size
  if device.type == 'cuda':
    # Adjust batch size based on GPU memory
    total_mem = torch.cuda.get_device_properties(device).total_memory
    free_mem = total_mem - torch.cuda.memory_allocated(device)
    T = params['ancienNbDays']
    num_states = params['cli'][client_id]['maxInventory'] + 1
    mem_per_scenario = 4 * (T + 1) * num_states * 4 # Conservative estimate
    max_batch = min(batch_size, max(1, int(free_mem * 0.7 / mem_per_scenario)))
    batch_size = max(128, max_batch)
  
  all_results: List[ScenarioResult] = []
  
  # Pin memory for faster data transfer
  if device.type == 'cuda':
    for key in all_scenarios_data:
      if isinstance(all_scenarios_data[key], torch.Tensor):
        all_scenarios_data[key] = all_scenarios_data[key].pin_memory()
  
  current_batch_size = batch_size
  start = 0
  
  while start < N:
    end = min(start + current_batch_size, N)
    batch_data = {key: val[start:end] for key, val in all_scenarios_data.items()}

    try:
      solver = Solver(params, client_id, batch_data, device)
      costs_batch, all_flags_batch, all_quantities_batch = solver.solve_batch()
      
      # Add results
      batch_size_actual = costs_batch.size(0)
      for i in range(batch_size_actual):
        global_idx = start + i
        result = ScenarioResult(
          global_idx=global_idx,
          cost=costs_batch[i].item(),
          flags=all_flags_batch[i],
          quantities=all_quantities_batch[i]
        )
        all_results.append(result)

      start = end
      
    except RuntimeError as e:
      if 'out of memory' in str(e).lower() and current_batch_size > 128:
        current_batch_size = max(128, current_batch_size // 2)
        print(f"Out of memory, reducing batch size to: {current_batch_size}")
        if device.type == 'cuda': 
          torch.cuda.empty_cache()
        continue
      raise e
    
    finally:
      if 'solver' in locals():
        del solver
      if device.type == 'cuda': 
        torch.cuda.empty_cache()

  print(f"Total scenarios processed: {len(all_results)}")
  return all_results

def get_all_scenario_results_as_lists(
  all_scenario_results: List[ScenarioResult]
) -> Tuple[List[float], List[List[int]], List[List[float]], int]:
  scenario_costs: List[float] = []
  scenario_flags: List[List[int]] = []
  scenario_quantities: List[List[float]] = []

  for result in all_scenario_results:
    scenario_costs.append(result.cost)
    scenario_flags.append(result.flags)
    scenario_quantities.append(result.quantities)
  
  total_scenarios_processed = len(all_scenario_results)

  return scenario_costs, scenario_flags, scenario_quantities, total_scenarios_processed