import json
from typing import Dict, Any, List, Tuple

def based_params_scale(data: dict) -> dict:
  """
  Process input data to generate solver parameters
  data: dict containing scenario_infos and other parameters
  """
  
  T = data['nb_days']

  cli_info = {
    'maxInventory': data['max_inventory'],
    'startingInventory': data['start_inventory'],
    'inventoryCost': data['inventory_cost'],
    'stockoutCost': data['stockout_cost'],
    # REFACTOR: Add supplier_minus_cost to enable GPU-CPU cost consistency
    'supplierMinusCost': data.get('supplier_minus_cost', 0.0)
  }

  daily_demand = []
  daily_delivery_costs = []
  daily_capacity_thresholds = []
  daily_capacity_slopes = []
  scenario_start_inventories = [] 

  for scen in data['scenario_infos']:
    daily_demand.append(scen['daily_demands'])
    daily_delivery_costs.append(scen['daily_delivery_costs'][:T])
    
    # Handle each scenario's start_inventory
    if 'start_inventory' in scen:
      scenario_start_inventories.append(scen['start_inventory'])
    else:
      scenario_start_inventories.append(data['start_inventory'])
    
    cap_pen_costs = scen['daily_capa_penalty_costs']
    if len(cap_pen_costs) < T:
      last_cap = cap_pen_costs[-1] if cap_pen_costs else {'threshold': 0, 'slope': 0}
      cap_pen_costs.extend([last_cap] * (T - len(cap_pen_costs)))

    thresholds = [c['threshold'] for c in cap_pen_costs[:T]]
    slopes = [c['slope'] for c in cap_pen_costs[:T]]
    
    daily_capacity_thresholds.append(thresholds)
    daily_capacity_slopes.append(slopes)

  return {
    'ancienNbDays': T,
    'cli': [cli_info],
    'dailyDemand': daily_demand,
    'dailyDeliveryCosts': daily_delivery_costs,
    'dailyCapacityThresholds': daily_capacity_thresholds,
    'dailyCapacitySlopes': daily_capacity_slopes,
    'scenarioStartInventories': scenario_start_inventories,
  }