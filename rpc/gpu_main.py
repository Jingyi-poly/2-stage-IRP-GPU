from gpu_data import *
from gpu_solver import *
import json

def gpu_run(data_dict: dict):
    """
    Process dict parameters and return GPU solver results
    data_dict: dict containing scenario_infos and other parameters
    """
    try:
        params = base_params_scale(data_dict)
    except Exception as e:
        print(f"Error processing data: {e}")
        return None, None, None, None

    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')

    # Prepare scenario data tensors - ensure scenario_start_inventories included
    all_scenarios_data = {
        'demands': torch.tensor(params['dailyDemand'], dtype=torch.float32),
        'delivery_costs': torch.tensor(params['dailyDeliveryCosts'], dtype=torch.float32),
        'capacity_thresholds': torch.tensor(params['dailyCapacityThresholds'], dtype=torch.float32),
        'capacity_slopes': torch.tensor(params['dailyCapacitySlopes'], dtype=torch.float32),
        'scenario_start_inventories': torch.tensor(params['scenarioStartInventories'], dtype=torch.float32)
    }
    
    client_id = 0 
    batch_size = 512 

    # solve_all_scenarios_fused returns list of ScenarioResult objects
    all_scenario_results = solve_all_scenarios(
        params=params,
        client_id=client_id,
        all_scenarios_data=all_scenarios_data,
        batch_size=batch_size,
        device=device
    )

    
    costs, flags, quantities, num = get_all_scenario_results_as_lists(all_scenario_results)
    return costs, flags, quantities, num

def run(path: str):
    """Maintain original interface compatibility"""
    try:
        with open(path, 'r') as f:
            data = json.load(f)
        return gpu_run(data)
    except FileNotFoundError:
        print("Error: data.json file not found")
        return None, None, None, None