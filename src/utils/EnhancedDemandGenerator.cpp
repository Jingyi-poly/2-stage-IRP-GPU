#include "irp/utils/EnhancedDemandGenerator.h"
#include <iomanip>
#include <map>
#include <stdexcept>

// CustomerTypedefine:5differentCustomerType
const vector<EnhancedDemandGenerator::ClientTypeConfig>
EnhancedDemandGenerator::CLIENT_TYPES = {
  // {based_rate, volatility, phi, name, probability}

 // Type1: Large customer - Demand,lowfluctuation,Time Corr
  {0.65, 0.18, 0.75, "Large_Stable", 0.20}, // Adjusted: 0.80 → 0.65 (ensure first day delivery possible)

  // Type2: Medium stable customer - medium demand, low fluctuation
  {0.55, 0.20, 0.60, "Medium_Stable", 0.25}, // Adjusted: 0.70 → 0.55 (ensure first day delivery possible)

  // Type3: Standard customer - medium demand and fluctuation
  {0.50, 0.30, 0.50, "Standard", 0.30}, // Adjusted: 0.65 → 0.50 (ensure first day delivery possible)

 // Type4: Small volatile customer - lowDemand,fluctuation,lowTime Corr
  {0.40, 0.45, 0.35, "Small_Volatile", 0.15}, // Adjusted: 0.50 → 0.40 (ensure first day delivery possible)

 // Type5: Irregular customer - lowDemand,fluctuation,lowTime Corr
  {0.35, 0.55, 0.20, "Irregular", 0.10} // Adjusted: 0.45 → 0.35 (ensure first day delivery possible)
};

EnhancedDemandGenerator::EnhancedDemandGenerator(
  int scenarios, int days, int clients, int seed,
  const vector<double>& max_inventories)
  : nb_scenarios_(scenarios),
   nb_days_(days),
   nb_clients_(clients),
   demand_seed_(seed),
   vec_client_max_inventory_(max_inventories) {

  // Input validation
  if (scenarios <= 0 || days <= 0 || clients <= 0) {
 throw std::invalid_argument("Scenarios,DaysandClientsRequiredfor");
  }

  if (max_inventories.size() != static_cast<size_t>(clients)) {
 throw std::invalid_argument("Max Invarray sizeRequiredat/toClients");
  }

  for (const auto& inv : max_inventories) {
    if (inv <= 0) {
 throw std::invalid_argument("Max InvRequiredfor");
    }
  }

  initialize_client_profiles();
  initialize_stratum_mapping();
}

void EnhancedDemandGenerator::initialize_client_profiles() {
 // UsefixedSeedInitializeCustomerType(CustomerTypeAssignmentcan/may)
  mt19937 rng(demand_seed_);
  uniform_real_distribution<double> dist(0.0, 1.0);

  cout << "Initializing client demand profiles..." << endl;

  for (int c = 0; c < nb_clients_; c++) {
    double rand = dist(rng);
    double cumulative_prob = 0.0;
    bool assigned = false;

    // based onProbabilityDistributionSelectionCustomerType
    for (const auto& type : CLIENT_TYPES) {
      cumulative_prob += type.probability;
      if (rand <= cumulative_prob) {
        client_profiles_.emplace_back(
          type.based_rate,
          type.volatility,
          type.phi,
          type.name
        );
        assigned = true;
        break;
      }
    }

 // Fallback to last type if floating point precision issue
    if (!assigned) {
      const auto& last_type = CLIENT_TYPES.back();
      client_profiles_.emplace_back(
        last_type.based_rate,
        last_type.volatility,
        last_type.phi,
        last_type.name
      );
    }
  }

  cout << "Client profiles initialization completed." << endl;
}

void EnhancedDemandGenerator::initialize_stratum_mapping() {
 // Initializing stratum mapping(notagainUse,butretainFunctiontocompilationError)
  // Actual stratum mapping now in get_scenario_stratum() computed dynamically
  cout << "Initializing stratum mapping(deterministic random based on scenario ID)..." << endl;

  // Output mapping info for first 20 scenarios (debug)
  const int NUM_STRATA = 10;
  cout << "Stratum mapping for first 20 scenarios: ";
  for (int i = 0; i < min(20, nb_scenarios_); i++) {
    // Use scenario ID to generate deterministic"random"stratum number
    mt19937 rng(demand_seed_ + i * 12345);
    uniform_int_distribution<int> dist(0, NUM_STRATA - 1);
    int stratum = dist(rng);
    cout << stratum;
    if (i < min(19, nb_scenarios_ - 1)) cout << ",";
  }
  cout << endl;
  cout << "Stratum mapping initialization completed(ensuring scenario nesting)." << endl;
}

vector<vector<vector<double>>> EnhancedDemandGenerator::generate() {
  cout << "========================================" << endl;
  cout << "Using Enhanced Demand Generator" << endl;
  cout << "Scenarios: " << nb_scenarios_ << endl;
  cout << "Days: " << nb_days_ << endl;
  cout << "Clients: " << nb_clients_ << endl;
  cout << "Base seed: " << demand_seed_ << endl;
  cout << "========================================" << endl;

  vector<vector<vector<double>>> demand_matrix(nb_scenarios_);

  // Generate independent demands for each scenario
  for (int s = 0; s < nb_scenarios_; s++) {
    demand_matrix[s] = generate_scenario_demands(s);

    if (s % 100 == 0 && s > 0) {
 cout << "alreadyGenerate " << s << " ScenarioDemandmatrix..." << endl;
    }
  }

  cout << "Demand matrix generated!" << endl;
  cout << "========================================" << endl;

  return demand_matrix;
}

vector<vector<double>> EnhancedDemandGenerator::generate_scenario_demands(int scenario_id) {
 // eachScenarioUserandomSeed,Scenariocompletely
 // Usebig/largeinterval(100000)randomsequencenot
  mt19937 rng(demand_seed_ + scenario_id * 100000);

  vector<vector<double>> scenario_demands(nb_days_ + 1);

 // InitializeDemandArray(Indexfrom1Start,0PositionnotUse)
  for (int d = 0; d <= nb_days_; d++) {
    scenario_demands[d].resize(nb_clients_, 0.0);
  }

  // foreachCustomerGenerateDemandsequence
  for (int c = 0; c < nb_clients_; c++) {
    vector<double> client_demands = generate_client_demand_series(
      rng,
      client_profiles_[c],
      vec_client_max_inventory_[c],
 scenario_id // pass inScenarioIDfordivide/partlayer
    );

 // Copy generated demand sequence to matrix
    for (int d = 1; d <= nb_days_; d++) {
      scenario_demands[d][c] = client_demands[d - 1];
    }
  }

  return scenario_demands;
}

vector<double> EnhancedDemandGenerator::generate_client_demand_series(
  mt19937& rng,
  const ClientDemandProfile& profile,
  double max_inventory,
  int scenario_id) {

  vector<double> demand_series(nb_days_);

 // Getdivide/partlayeroffset(toStandard deviationforsingle)
  double stratified_offset = get_stratified_demand_offset(scenario_id);

 // randomGenerateDemand(toAR(1)Time Corr)
  for (int d = 0; d < nb_days_; d++) {
    double demand_rate;

    if (d == 0) {
 // first:eachCustomerrandom,UseUniformDistributionDiversity
 // Range [0.3, 0.85]: haveDeliveryDemandbutchangeRangebig/large
      uniform_real_distribution<double> first_day_dist(0.3, 0.85);
      double based_demand_rate = first_day_dist(rng);

 // addScenariooffset,make/usedifferentScenarioinfirstthen/justproduce
 // Usesmalloffset(0.15),offsetbig/large
      demand_rate = based_demand_rate + stratified_offset * 0.15;

 // restrictDemandinreasonableRangeinside
      demand_rate = clamp_demand_rate(demand_rate);
    } else {
 // other:based onCustomerTypeNormalDistribution + divide/partlayeroffset
      // N(based_rate, volatility) + divide/partlayerAdjusted
      normal_distribution<double> normal_dist(0.0, 1.0);
      double epsilon = normal_dist(rng) * profile.volatility;

 // Base Rate + randomfluctuation + divide/partlayeroffset(onlyApplytofirst)
      demand_rate = profile.based_rate + epsilon + stratified_offset * profile.volatility;

 // restrictDemandinreasonableRangeinside
      demand_rate = clamp_demand_rate(demand_rate);
    }

 // Calculate actual demand(take/get)
    double actual_demand = round(max_inventory * demand_rate);

 // DemandinreasonableRangeinside:Minimum1,MaximumforMax Inv
    actual_demand = max(1.0, min(actual_demand, max_inventory));

    demand_series[d] = actual_demand;
  }

  return demand_series;
}

double EnhancedDemandGenerator::clamp_demand_rate(double demand_rate) const {
 // AdjustedRange:[0.1, 0.85](on/abovetosupportfirstDemand)
 // DemandMutationandVehicleCapacityConstraint
  const double MIN_RATE = 0.1;
 const double MAX_RATE = 0.85; // Adjusted: 0.75 → 0.85(supportfirstDemand)

  if (demand_rate < MIN_RATE) return MIN_RATE;
  if (demand_rate > MAX_RATE) return MAX_RATE;
  return demand_rate;
}

void EnhancedDemandGenerator::print_client_profiles() const {
  cout << "\n========================================" << endl;
  cout << "Client Demand Profiles" << endl;
  cout << "========================================" << endl;

 // StatisticseachTypeClients
  map<string, int> type_counts;
  for (const auto& profile : client_profiles_) {
    type_counts[profile.type_name]++;
  }

  cout << "\nClient type distribution:" << endl;
  for (const auto& pair : type_counts) {
    double percentage = 100.0 * pair.second / nb_clients_;
    cout << " " << pair.first << ": " << pair.second
       << " (" << fixed << setprecision(1) << percentage << "%)" << endl;
  }

  cout << "\nDetailed config for first 5 clients:" << endl;
  cout << "Client ID | Type      | Base Rate | Volatility | Time Corr | Max Inv" << endl;
  cout << "-------|----------------|--------|--------|------------|----------" << endl;

  for (int c = 0; c < min(5, nb_clients_); c++) {
    const auto& profile = client_profiles_[c];
    cout << setw(6) << c << " | ";
    cout << setw(14) << left << profile.type_name << " | ";
    cout << fixed << setprecision(2);
    cout << setw(6) << profile.based_rate << " | ";
    cout << setw(6) << profile.volatility << " | ";
    cout << setw(10) << profile.phi << " | ";
    cout << setw(8) << vec_client_max_inventory_[c] << endl;
  }

  cout << "========================================\n" << endl;
}

int EnhancedDemandGenerator::get_scenario_stratum(int scenario_id) const {
 // based onScenarioIDGenerateDeterministic"random"stratum number
 // ensuring scenario nesting:before/frontNScenarioinanyScenariosall underhavesamestratum number
  const int NUM_STRATA = 10;
  mt19937 rng(demand_seed_ + scenario_id * 12345);
  uniform_int_distribution<int> dist(0, NUM_STRATA - 1);
  return dist(rng);
}

double EnhancedDemandGenerator::get_stratified_demand_offset(int scenario_id) const {
 // divide/partlayerconstant
  const int NUM_STRATA = 10;     // divide/partlayerCount
  const double MIN_OFFSET = -1.0;   // Minimumoffset(Standard deviationmultiple)
  const double MAX_OFFSET = +1.0;   // Maximumoffset(Standard deviationmultiple)

 // Userandomdivide/partlayerMapping
  int stratum = get_scenario_stratum(scenario_id);

 // Calculate stratum offset
  // layer0: -0.95σ, layer1: -0.75σ, ..., layer4: -0.05σ, layer5: +0.05σ, ..., layer9: +0.95σ
  double stratum_center = MIN_OFFSET +
              (MAX_OFFSET - MIN_OFFSET) * (stratum + 0.5) / NUM_STRATA;

 // addlayerinsiderandom,UseScenarioIDas/makeforSeedcan/may
  mt19937 rng(demand_seed_ + scenario_id * 777);
 normal_distribution<double> jitter_dist(0.0, 0.08); // layersmall amplitude withinrandom
  double intra_jitter = jitter_dist(rng);

  double final_offset = stratum_center + intra_jitter;

  // OutputDebugInfo(only before20Scenario)
  if (scenario_id < 20) {
    cout << "Scenario" << scenario_id << ": randomly assigned to stratum" << stratum
       << " → offset=" << fixed << setprecision(2) << final_offset << "σ" << endl;
  }

  return final_offset;
}
