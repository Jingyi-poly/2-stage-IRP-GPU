#include "irp/model/ParamsList.h"
#include "irp/utils/commandline.h"
#include <iostream>
#include <random>
#include <cmath>

ParamsList::ParamsList(commandline& c) : commandline_(c) {
  // Get number of scenarios from commandline
  int scenario_input = c.get_nb_scenario();
  // Scenario count must be positive; default to 1 if zero or negative
  if (scenario_input <= 0) {
    cout << "Warning: Invalid scenario count (" << scenario_input << "), defaulting to 1" << endl;
    scenario_input = 1;
  }
  nb_scenarios_ = static_cast<uint64_t>(scenario_input);
  cout << "nb_scenarios_: " << nb_scenarios_ << endl;
  // Initialize vec_params with nb_scenarios_ elements, all set to nullptr
  init_params_list();
  init_demand_matrix();
  set_params_demand_matrix();
}

ParamsList::~ParamsList() {
  // Clean up all Params objects
  for (auto* params : vec_params_) {
    if (params != nullptr) {
      delete params;
    }
  }
  if (general_params_ != nullptr) {
    delete general_params_;
  }
  vec_params_.clear();
}

void ParamsList::init_params_list() {
  vec_params_.resize(nb_scenarios_, nullptr);
  general_params_ = new Params(
    commandline_.get_path_to_instance(), commandline_.get_path_to_solution(), commandline_.get_type(),
    commandline_.get_nbVeh(), commandline_.get_path_to_BKS(), commandline_.get_seed(), commandline_.get_rou(), commandline_.get_stockout());

  for (int i = 0; i < nb_scenarios_; i++) {
    vec_params_[i] = new Params(
      commandline_.get_path_to_instance(), commandline_.get_path_to_solution(), commandline_.get_type(),
      commandline_.get_nbVeh(), commandline_.get_path_to_BKS(), commandline_.get_seed(), commandline_.get_rou(), commandline_.get_stockout());
  }

  control_day_1_ = 1;
  force_delivery_clients_ = commandline_.get_force_delivery_clients();
  set_general_params();
}

void ParamsList::set_general_params() {
  // Set common parameters for all scenarios
  if (vec_params_.size() == 0) {
    return;
  }
  general_params_->is_scenario_ = true;
  general_params_->demand_seed = commandline_.get_demand_seed();
  general_params_->control_day_1_ = control_day_1_;
  general_params_->force_delivery_clients_ = force_delivery_clients_;
  // Set force_delivery_clients_ for all scenario params
  for (int i = 0; i < nb_scenarios_; i++) {
    if (vec_params_[i] != nullptr) {
      vec_params_[i]->force_delivery_clients_ = force_delivery_clients_;
    }
  }
  nb_days_ = general_params_->nbDays;
  nb_clients_ = general_params_->nbClients;
  nb_depots_ = general_params_->nbDepots;
  nb_vehicles_ = general_params_->nbVehiculesPerDep;
  seed_ = general_params_->seed;
  demand_seed_ = general_params_->demand_seed;

  for (int c = 0; c < nb_clients_; c++) {
    double max_inv = general_params_->cli[c+nb_depots_].maxInventory;
    // Use default value 1 if maxInventory is zero or negative
    if (max_inv <= 0) {
      cout << "Warning: Client " << c << " (index=" << (c+nb_depots_) << ") has invalid maxInventory="
         << max_inv << ", using default value 1" << endl;
      max_inv = 1.0;
    }
    vec_client_max_inventory_.push_back(max_inv);
  }
}

void ParamsList::init_demand_matrix() {
  bool trace = true;

	cout << "\n========================================" << endl;
	cout << "Using Enhanced Demand Generation Strategy" << endl;
	cout << "========================================" << endl;

	// Use the enhanced demand generator
	EnhancedDemandGenerator generator(
		nb_scenarios_,
		nb_days_,
		nb_clients_,
		demand_seed_,
		vec_client_max_inventory_
	);

	// Print client profiles (optional)
	if (trace && nb_clients_ <= 20) {
		generator.print_client_profiles();
	}

	// Generate demand matrix
	vec_scenario_daily_client_demand_ = generator.generate();

	cout << "Demand matrix generation completed!" << endl;
	cout << "Matrix dimensions: [" << nb_scenarios_ << " scenarios] x ["
	   << nb_days_ << " days] x [" << nb_clients_ << " clients]" << endl;
	cout << "========================================\n" << endl;

	// PrintDemandmatrix(onlyinclients)
	// if (trace && nb_clients_ <= 10 && nb_scenarios_ <= 5) {
	// 	print_demand_matrix();
	// 	cin.get();
	// }
}

void ParamsList::print_demand_matrix() {
  cout << "Demand matrix initialization completed" << endl;
 // Print client demand for first 5 scenarios
  for (int c = 0; c < nb_clients_; c++) {
 cout << "client " << c << " maxInventory:" << vec_client_max_inventory_[c] << " many/multibefore/front5scenariosdaysDemand:" << endl;
    for (int s = 0; s < nb_scenarios_; s++) {
 cout << "scenarios " << s << " daysDemand:";
      for (int d = 1; d <= nb_days_; d++) {
        cout << vec_scenario_daily_client_demand_[s][d][c] << " ";
      }
      cout << endl;
      if (s == 4) {
        break;
      }
    }
    cout << "--------------------------------" << endl;
  }
  cout << endl;
}

void ParamsList::set_params_demand_matrix() {
 // Seteachscenariosclientdaily_demand
  for (int s = 0; s < nb_scenarios_; s++) {
    for (int c = 0; c < nb_clients_; c++) {
 // GetCurrentscenariosandclientsDemanddata
      // Note:clientsIndexneedadd/pluson/aboveDepotCount,becauseclivectorbefore innb_depots_isDepot
      int client_index = c + nb_depots_;
      
      // Clear existing dailyDemand and reset
      vec_params_[s]->cli[client_index].dailyDemand.clear();
 vec_params_[s]->cli[client_index].dailyDemand.push_back(0.0); // 0daysfor0
      
 // SetdaysDemand
      for (int d = 1; d <= nb_days_; d++) {
        double demand = vec_scenario_daily_client_demand_[s][d][c];
        vec_params_[s]->cli[client_index].dailyDemand.push_back(demand);
      }
    }
  }
  
  cout << "Demand matrix set for all scenarios and clients" << endl;
}
