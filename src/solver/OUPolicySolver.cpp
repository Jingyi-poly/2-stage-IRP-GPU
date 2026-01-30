#include "irp/solver/OUPolicySolver.h"
#include <climits>
#include <mutex>    // for unique_lock
#include <shared_mutex> // for shared_lock

// Useexplicitnobig/largeconstantGenerationINT_MAX,notclearandinoverflow
constexpr double INF_COST = 1e30;

std::pair<double, std::pair<double, Noeud*>> OUPolicySolver::getInsertionInfo(Noeud* client, int day, vector<Noeud*> noeudTravails, double quantity) {
 // Select lowest cost InsertInfo (cost, load, place)
  double cost = INF_COST;
  double load = 0;
  Noeud* place = nullptr;
  Noeud* nodetravail = noeudTravails[day];

  // safeCheck:ifnoneavailableInsertPosition,recordWarning
  if (nodetravail->allInsertions.empty()) {
    std::cerr << "Warning: No insertion positions available for client "
         << client->cour << " on day " << day
         << ". This may indicate incomplete route initialization." << std::endl;
    return make_pair(cost, make_pair(load, place));
  }

  for(int i = 0; i < nodetravail->allInsertions.size(); i++) {
    auto& insertion = nodetravail->allInsertions[i];

    double pre_load = - insertion.load + quantity;
    double post_load = - insertion.load;
    if(eq(pre_load,0)) pre_load = 0;
    if(eq(post_load,0)) post_load = 0;
    double capacityPenaltyCost = params->penalityCapa * (std::max<double>(0., pre_load) + std::max<double>(0., post_load));

    double totalCost = insertion.detour + capacityPenaltyCost;

    if(totalCost < cost) {
      cost = totalCost;
      load = insertion.load;
      place = insertion.place;
    }
  }

  // extraCheck:that ismake/usehaveInsertPosition,placealsopossiblyfornullptr
  if (place == nullptr) {
    std::cerr << "Warning: Selected insertion place is nullptr for client "
         << client->cour << " on day " << day << std::endl;
  }

  return make_pair(cost, make_pair(load, place));
}

OUPolicyResult OUPolicySolver::solve() {
  // Solve subproblem,callOUPolicyDP,store results in memory
  memory.clear();
  bool traces = false;

  // +++++ First day control:based on control_day_1_ choose different strategy +++++
  if (control_day_1_ == static_cast<int>(ControlDay1Mode::SEMI_CONTROL)) {
    // SEMI_CONTROL mode: Semi-control first day, compare delivery vs no-delivery, choose better option
    if (traces) {
      cout << "Using ControlDay1Mode::SEMI_CONTROL: comparing skip vs force delivery on day 1" << endl;
    }

    // Calculate cost without first day delivery
    OUPolicyResult no_delivery_result = solveSkipDay1();

    // Calculate cost with first day delivery
    OUPolicyResult with_delivery_result = solveForceDay1Delivery();

    if (traces) {
      cout << "No delivery day 1 cost: " << no_delivery_result.totalCost << endl;
      cout << "With delivery day 1 cost: " << with_delivery_result.totalCost << endl;
    }

    // Choose lower cost option
    memory.clear();
    if (no_delivery_result.totalCost < with_delivery_result.totalCost) {
      if (traces) cout << "Choosing: skip delivery on day 1" << endl;
      return no_delivery_result;
    } else {
      if (traces) cout << "Choosing: force delivery on day 1" << endl;
      return with_delivery_result;
    }
  }

  // mode0:Normal optimization(no first day control)
  DayResult result = OUPolicyDP(1, params->cli[clientId].startingInventory);
  double resultCost = result.totalCost;

  //Trace back the entire process
  vector<bool> resultPlans(params->ancienNbDays + 1, false);
  vector<double> resultQuantities(params->ancienNbDays + 1, 0);
  vector<Noeud*> resultPlaces(params->ancienNbDays + 1, nullptr);

  int t = 1;
  double I = params->cli[clientId].startingInventory;
  double prevI = I;
  while (t <= params->ancienNbDays) {
    auto result = memory[{t, I}];
    prevI = I;
    resultPlans[t - 1] = result.chooseDelivery;
    if (result.chooseDelivery) {
      resultQuantities[t - 1] = params->cli[clientId].maxInventory - I;
      resultPlaces[t - 1] = result.insertPlace;
      I = std::max<double>(0., params->cli[clientId].maxInventory - params->cli[clientId].dailyDemand[t]);
    } else {
      I = std::max<double>(0., I - params->cli[clientId].dailyDemand[t]);
    }
    t++;
    if (traces) {
      cout << "day: " << t << " I: " << I << " chooseDelivery: " << result.chooseDelivery << " resultInventory: " << I << " totalCost: " << result.totalCost << endl;
    }
  }
  memory.clear();
  return {resultCost, resultPlans, resultQuantities, resultPlaces};
}

DayResult OUPolicySolver::OUPolicyDP(int t, int I) {
  // Dynamic programming recursive function,discuss cases and store results
  bool traces = false;
  if (traces) {
    cout << "(t, I) = (" << t << ", " << I << ")" << endl;
  }
  if (memory.find({t, I}) != memory.end()) {
    if (traces) {
      cout << "use memory" << endl;
    }
    return memory[{t, I}];
  }

  auto& clientParams = params->cli[clientId];

  if (t > params->ancienNbDays) {
    if (traces) {
      cout << "reach the end of planning period" << endl;
    }
    DayResult emptyResult;
    emptyResult.totalCost = 0.0;
    emptyResult.insertPlace = nullptr;
    emptyResult.chooseDelivery = false;
    return emptyResult;
  }
  DayInfo todayInfo;
  // recordNumber of days,Inventory,Demand
  todayInfo.currentInventory = I;
  todayInfo.demand = clientParams.dailyDemand[t];
  todayInfo.totalCost = INF_COST;

  // No-delivery option
  double new_inventory = I - clientParams.dailyDemand[t];

  // adding inventory cost
  todayInfo.noDelivery.inventoryCost = clientParams.inventoryCost * std::max<double>(0., new_inventory);
  // stockout cost
  todayInfo.noDelivery.shortageCost = clientParams.stockoutCost * std::max<double>(0., -new_inventory);
  new_inventory = std::max<double>(0., new_inventory);
  // supplier minus cost = 0
  todayInfo.noDelivery.supplierMinusCost = 0;
  // delivery cost = 0
  todayInfo.noDelivery.deliveryCost = 0;
  // capacity penalty cost = 0
  todayInfo.noDelivery.capacityPenaltyCost = 0;
 // totalfirstnotCalculatefuturecost,throughafter/back,plusfuturecost
  todayInfo.noDelivery.totalCost = todayInfo.noDelivery.inventoryCost + todayInfo.noDelivery.shortageCost;
  todayInfo.noDelivery.resultInventory = new_inventory;
  
  // ================================================
  // Delivery option,CalculateDeliveryCostandInsertPosition
  new_inventory = clientParams.maxInventory - clientParams.dailyDemand[t];
  double quantity = clientParams.maxInventory - I;
  todayInfo.withDelivery.quantity = quantity;

  // adding inventory cost
  todayInfo.withDelivery.inventoryCost = clientParams.inventoryCost * std::max<double>(0., new_inventory);
  // stockout cost
  todayInfo.withDelivery.shortageCost = clientParams.stockoutCost * std::max<double>(0., -new_inventory);
  new_inventory = std::max<double>(0., new_inventory);
  // supplier minus cost
  todayInfo.withDelivery.supplierMinusCost = params->inventoryCostSupplier * (double)(params->ancienNbDays - t + 1) * quantity;
  // delivery cost ReturnValuefortwolayerpair,firstlayerforCost,secondlayerforplaceandload
  auto delivery_plan = getInsertionInfo(client, t, noeudTravails, quantity);
  todayInfo.withDelivery.deliveryCost = delivery_plan.first;
  todayInfo.withDelivery.load = delivery_plan.second.first;
  todayInfo.withDelivery.insertPlace = delivery_plan.second.second;
  todayInfo.withDelivery.totalCost = todayInfo.withDelivery.deliveryCost + 
                    todayInfo.withDelivery.inventoryCost + 
                    todayInfo.withDelivery.shortageCost - 
                    todayInfo.withDelivery.supplierMinusCost;
 // totalfirstnotCalculatefuturecost,throughafter/back,plusfuturecost
  todayInfo.withDelivery.resultInventory = new_inventory;

 // Calculatefuturecost
  DayResult futureResult;
  futureResult = OUPolicyDP(t + 1, std::max<double>(0., todayInfo.noDelivery.resultInventory));
  todayInfo.noDelivery.totalCost += futureResult.totalCost;
  futureResult = OUPolicyDP(t + 1, std::max<double>(0., todayInfo.withDelivery.resultInventory));
  todayInfo.withDelivery.totalCost += futureResult.totalCost;


  // compare,Choose lower cost option
  DayResult result;
  if (todayInfo.withDelivery.totalCost < todayInfo.noDelivery.totalCost) {
    result.totalCost = todayInfo.withDelivery.totalCost;
    result.insertPlace = todayInfo.withDelivery.insertPlace;
    result.chooseDelivery = true;
  } else {
    result.totalCost = todayInfo.noDelivery.totalCost;
    result.insertPlace = nullptr;
    result.chooseDelivery = false;
  }
  
  memory[{t, I}] = result;
  return result;
}

// +++++ New: solve function forcing no delivery on first day +++++
OUPolicyResult OUPolicySolver::solveSkipDay1() {
  memory.clear();
  bool traces = false;

  auto& clientParams = params->cli[clientId];

 // first day no delivery,directfrom2StartOptimize
  double day_1_end_inventory = clientParams.startingInventory - clientParams.dailyDemand[1];

 // CalculatefirstCost(onlyhaveInventoryCostorStockoutCost)
  double day_1_cost = 0.0;
  if (day_1_end_inventory > 0.0) {
    day_1_cost = clientParams.inventoryCost * day_1_end_inventory;
  } else {
    day_1_cost = clientParams.stockoutCost * (-day_1_end_inventory);
  }
  day_1_end_inventory = std::max<double>(0.0, day_1_end_inventory);

  if (traces) {
    cout << "solveSkipDay1: day_1_end_inventory=" << day_1_end_inventory
       << ", day_1_cost=" << day_1_cost << endl;
  }

 // from2StartOptimize
  DayResult result = OUPolicyDP(2, day_1_end_inventory);
  double totalCost = day_1_cost + result.totalCost;

 // return/back
  vector<bool> resultPlans(params->ancienNbDays + 1, false);
  vector<double> resultQuantities(params->ancienNbDays + 1, 0);
  vector<Noeud*> resultPlaces(params->ancienNbDays + 1, nullptr);

 // first day no delivery
  resultPlans[0] = false;
  resultQuantities[0] = 0.0;
  resultPlaces[0] = nullptr;

 // from2Startreturn/back
  int t = 2;
  double I = day_1_end_inventory;
  while (t <= params->ancienNbDays) {
    auto result = memory[{t, I}];
    resultPlans[t - 1] = result.chooseDelivery;
    if (result.chooseDelivery) {
      resultQuantities[t - 1] = clientParams.maxInventory - I;
      resultPlaces[t - 1] = result.insertPlace;
      I = std::max<double>(0., clientParams.maxInventory - clientParams.dailyDemand[t]);
    } else {
      I = std::max<double>(0., I - clientParams.dailyDemand[t]);
    }
    t++;
  }

 // memory.clear() alreadytomainsolve()Functionunified inProcess
  return {totalCost, resultPlans, resultQuantities, resultPlaces};
}

// +++++ New: solve function forcing delivery on first day +++++
OUPolicyResult OUPolicySolver::solveForceDay1Delivery() {
  memory.clear();
  bool traces = false;

  auto& clientParams = params->cli[clientId];

 // firstDelivery,ReplenishmenttoMaximumInventory
  double quantity = clientParams.maxInventory - clientParams.startingInventory;
  double day_1_end_inventory = clientParams.maxInventory - clientParams.dailyDemand[1];

 // CalculatefirstCost
  double day_1_cost = 0.0;

  // InventoryCost
  if (day_1_end_inventory > 0.0) {
    day_1_cost += clientParams.inventoryCost * day_1_end_inventory;
  } else {
    day_1_cost += clientParams.stockoutCost * (-day_1_end_inventory);
  }

  // DeliveryCost
  auto delivery_plan = getInsertionInfo(client, 1, noeudTravails, quantity);
  day_1_cost += delivery_plan.first;

  // supplierCost
  day_1_cost -= params->inventoryCostSupplier * (double)(params->ancienNbDays) * quantity;

  day_1_end_inventory = std::max<double>(0.0, day_1_end_inventory);

  if (traces) {
    cout << "solveForceDay1Delivery: day_1_end_inventory=" << day_1_end_inventory
       << ", day_1_cost=" << day_1_cost << ", delivery_cost=" << delivery_plan.first << endl;
  }

 // from2StartOptimize
  DayResult result = OUPolicyDP(2, day_1_end_inventory);
  double totalCost = day_1_cost + result.totalCost;

 // return/back
  vector<bool> resultPlans(params->ancienNbDays + 1, false);
  vector<double> resultQuantities(params->ancienNbDays + 1, 0);
  vector<Noeud*> resultPlaces(params->ancienNbDays + 1, nullptr);

 // firstDelivery
  resultPlans[0] = true;
  resultQuantities[0] = quantity;
  resultPlaces[0] = delivery_plan.second.second;

 // from2Startreturn/back
  int t = 2;
  double I = day_1_end_inventory;
  while (t <= params->ancienNbDays) {
    auto result = memory[{t, I}];
    resultPlans[t - 1] = result.chooseDelivery;
    if (result.chooseDelivery) {
      resultQuantities[t - 1] = clientParams.maxInventory - I;
      resultPlaces[t - 1] = result.insertPlace;
      I = std::max<double>(0., clientParams.maxInventory - clientParams.dailyDemand[t]);
    } else {
      I = std::max<double>(0., I - clientParams.dailyDemand[t]);
    }
    t++;
  }

 // memory.clear() alreadytomainsolve()Functionunified inProcess
  return {totalCost, resultPlans, resultQuantities, resultPlaces};
}


void OURemoteSolver::Solve() {
  bool traces = false;
  if (traces) {
    CheckParams();
  }
  std::vector<DayRes> vec_day_res(m_req.nb_scenario);
  switch (m_req.control_day_1) {
    case static_cast<int>(ControlDay1Mode::NO_CONTROL): {
      // NO_CONTROL mode:no first day control,Normal optimization
      for (int s = 0; s < m_req.nb_scenario; ++s) {
        vec_day_res[s] = SolveDayInventoryScenario(1, m_req.start_inventory, s);
      }
      break;
    }
    case static_cast<int>(ControlDay1Mode::SEMI_CONTROL): {
 // SEMI_CONTROL mode:Semi-control first day,compare delivery vs no-delivery,SelectionTotal cost
 // first day no delivery
      vector<DayRes> vec_day_1_no_delivery_res(m_req.nb_scenario);
      vector<DayRes> vec_day_1_with_delivery_res(m_req.nb_scenario);
      for (int s = 0; s < m_req.nb_scenario; ++s) {
        double day_1_end_inventory_no_delivery = m_req.start_inventory - m_req.scenario_infos[s].daily_demands[1];
        double day_1_cost_no_delivery = day_1_end_inventory_no_delivery > 0.0 ? m_req.inventory_cost * day_1_end_inventory_no_delivery : m_req.stockout_cost * -day_1_end_inventory_no_delivery;
        day_1_end_inventory_no_delivery = day_1_end_inventory_no_delivery > 0.0 ? day_1_end_inventory_no_delivery : 0.0;
        vec_day_1_no_delivery_res[s] = SolveDayInventoryScenario(2, day_1_end_inventory_no_delivery, s);
        vec_day_1_no_delivery_res[s].total_cost += day_1_cost_no_delivery;
        double day_1_end_inventory_with_delivery = m_req.max_inventory - m_req.scenario_infos[s].daily_demands[1];
        double day_1_cost_with_delivery = m_req.scenario_infos[s].daily_delivery_costs[1] + m_req.scenario_infos[s].daily_capa_penalty_costs[1].getCost(m_req.max_inventory - m_req.start_inventory);
        day_1_cost_with_delivery += m_req.inventory_cost * day_1_end_inventory_with_delivery;
        vec_day_1_with_delivery_res[s] = SolveDayInventoryScenario(2, day_1_end_inventory_with_delivery, s);
        vec_day_1_with_delivery_res[s].total_cost += day_1_cost_with_delivery;     
      }
      double no_delivery_cost = 0.0, with_delivery_cost = 0.0;
      for (int s = 0; s < m_req.nb_scenario; ++s) {
        no_delivery_cost += vec_day_1_no_delivery_res[s].total_cost;
        with_delivery_cost += vec_day_1_with_delivery_res[s].total_cost;
      }
      if (no_delivery_cost < with_delivery_cost) {
        vec_day_res = vec_day_1_no_delivery_res;
      } else {
        vec_day_res = vec_day_1_with_delivery_res;
      }
      for (int s = 0; s < m_req.nb_scenario; ++s) {
        map_memory_[{1, m_req.start_inventory, s}] = vec_day_res[s];
      }
      break;
    }
    case static_cast<int>(ControlDay1Mode::FORCE_CONTROL): {
 // FORCE_CONTROL mode:completelyControl first day,forceUsespecifiedfirst day decision
 // Note:OUPolicySolverCurrentnotsupportmode,onlyOURemoteSolversupport
      for (int s = 0; s < m_req.nb_scenario; ++s) {
        if (m_req.control_day_1_info.day_1_delivery) {
          double day_1_end_inventory_no_delivery = m_req.start_inventory - m_req.scenario_infos[s].daily_demands[1];
          double day_1_cost_no_delivery = day_1_end_inventory_no_delivery > 0.0 ? m_req.inventory_cost * day_1_end_inventory_no_delivery : m_req.stockout_cost * -day_1_end_inventory_no_delivery;
          day_1_end_inventory_no_delivery = day_1_end_inventory_no_delivery > 0.0 ? day_1_end_inventory_no_delivery : 0.0;
          vec_day_res[s] = SolveDayInventoryScenario(2, day_1_end_inventory_no_delivery, s);
          vec_day_res[s].total_cost += day_1_cost_no_delivery;
        } else {
          double day_1_end_inventory_with_delivery = m_req.max_inventory - m_req.scenario_infos[s].daily_demands[1];
          double day_1_cost_with_delivery = m_req.scenario_infos[s].daily_delivery_costs[1] + m_req.scenario_infos[s].daily_capa_penalty_costs[1].getCost(m_req.max_inventory - m_req.start_inventory);
          day_1_cost_with_delivery += m_req.inventory_cost * day_1_end_inventory_with_delivery;
          vec_day_res[s] = SolveDayInventoryScenario(2, day_1_end_inventory_with_delivery, s);
          vec_day_res[s].total_cost += day_1_cost_with_delivery;
        }
        map_memory_[{1, m_req.start_inventory, s}] = vec_day_res[s];
      }
      break;
    }
    default: {
      std::string error_msg = "Invalid control_day_1 mode: " + std::to_string(m_req.control_day_1) +
                  ". Valid modes are: NO_CONTROL(0), SEMI_CONTROL(1), FORCE_CONTROL(2)";
      throw std::runtime_error(error_msg);
    }
  }
  if (traces) cout << "dp_ok!" << endl;

  // backtracking 
 // willresizeoperationtoloopexternal,AdjustedvectorSize
  m_resp.scenario_info_results.resize(m_req.nb_scenario);
  
  for (int s = 0; s < m_req.nb_scenario; ++s) {
    vector<bool> res_daily_plans(m_req.nb_days + 1, false);
    vector<double> res_daily_quantities(m_req.nb_days + 1, 0.0);
    int t = 1;
    double I = m_req.start_inventory;
    double prevI = I;
    while (t <= m_req.nb_days) {
      if (map_memory_.find({t, I, s}) == map_memory_.end()) {
        cerr << "ERROR: map_memory_ not found: (t, I, s) = (" << t << ", " << I << ", " << s << ")" << endl;
        break;
      }
      auto result = map_memory_[{t, I, s}];
      prevI = I;
      res_daily_plans[t - 1] = result.is_delivery;
      if (result.is_delivery) {
        res_daily_quantities[t - 1] = m_req.max_inventory - I;
        I = std::max<double>(0., m_req.max_inventory - m_req.scenario_infos[s].daily_demands[t]);
      } else {
        I = std::max<double>(0., I - m_req.scenario_infos[s].daily_demands[t]);
      }
      if (traces) {
        cout << "day: " << t << " I: " << prevI << " chooseDelivery: " << result.is_delivery << " resultInventory: " << I << " totalCost: " << result.total_cost << endl;
        cout << " inventory_cost: " << result.inventory_cost << ", stockout_cost: " << result.stockout_cost << ", delivery_cost: " << result.delivery_cost << ", supplier_minus_cost: " << result.supplier_minus_cost << ", capacity_penalty_cost: " << result.capacity_penalty_cost << endl;
        cout << " quantity: " << (result.is_delivery? m_req.max_inventory - prevI : 0.) << ":" << res_daily_quantities[t - 1] << " load: " << m_req.scenario_infos[s].daily_capa_penalty_costs[t - 1].threshold << endl;
      }
      t++;
    }
 // removerow,becausealreadyinloopexternalresize
    // m_resp.scenario_info_results.resize(m_req.nb_scenario);
    m_resp.scenario_info_results[s].daily_plans = res_daily_plans;
    m_resp.scenario_info_results[s].daily_quantities = res_daily_quantities;
    m_resp.scenario_info_results[s].total_cost = vec_day_res[s].total_cost;
  }

  if (traces) cout << "backtracking_ok!" << endl;
  map_memory_.clear();
  return;
}

DayRes OURemoteSolver::SolveDayInventoryScenario(int day, double start_inventory, int scenario_id) {
  // Dynamic programming recursive function,discuss cases and store results
  bool traces = false;
  if (traces) {
    cout << "(day, start_inventory, scenario_id) = (" << day << ", " << start_inventory << ", " << scenario_id << ")" << endl;
  }

 // UselockCheckcache
  {
    std::shared_lock<std::shared_mutex> lock(memory_mutex_);
    auto it = map_memory_.find({day, start_inventory, scenario_id});
    if (it != map_memory_.end()) {
      return it->second;
    }
  }
  if (day > m_req.nb_days) {
    if (traces) {
      cout << "reach the end of planning period" << endl;
    }
    DayRes zero;
    zero.total_cost = 0.0;
    zero.is_delivery = false;
    zero.end_inventory = start_inventory;
    return zero;
  }
  DayRes res;
  // recordNumber of days,Inventory,Demand
  uint64_t demand = m_req.scenario_infos[scenario_id].daily_demands[day];
  if (traces) {
    cout << "demand: " << demand << endl;
  }
  // No-delivery option
  DayRes no_delivery_res;
  double new_inventory = start_inventory - demand;
  double no_delivery_inventory_cost = m_req.inventory_cost * std::max<double>(0., new_inventory);
  double no_delivery_stockout_cost = m_req.stockout_cost * std::max<double>(0., -new_inventory);
  double no_delivery_end_inventory = std::max<double>(0., new_inventory);
  no_delivery_res.total_cost = no_delivery_inventory_cost + no_delivery_stockout_cost;
  if (traces) {
    cout << "no_delivery_res.total_cost: " << no_delivery_res.total_cost << endl;
    cout << " inventory_cost: " << no_delivery_inventory_cost << ", stockout_cost: " << no_delivery_stockout_cost << endl;
    cout << " no_delivery_end_inventory: " << no_delivery_end_inventory << endl;
  }
  // Delivery option
  DayRes with_delivery_res;
  double delivery_inventory = m_req.max_inventory - demand;
  double quantity = m_req.max_inventory - start_inventory;
  double with_delivery_inventory_cost = m_req.inventory_cost * std::max<double>(0., delivery_inventory);
  double with_delivery_stockout_cost = m_req.stockout_cost * std::max<double>(0., -delivery_inventory);
  double with_delivery_end_inventory = std::max<double>(0., delivery_inventory);
  double with_delivery_supplier_minus_cost = m_req.supplier_minus_cost * (double)(m_req.nb_days - day + 1) * quantity;
  double with_delivery_delivery_cost = m_req.scenario_infos[scenario_id].daily_delivery_costs[day];
  double with_delivery_capacity_penalty_cost = m_req.scenario_infos[scenario_id].daily_capa_penalty_costs[day].getCost(quantity);
  with_delivery_res.total_cost = with_delivery_inventory_cost + with_delivery_stockout_cost + with_delivery_supplier_minus_cost + with_delivery_delivery_cost + with_delivery_capacity_penalty_cost;
  if (traces) {
    cout << "with_delivery_res.total_cost: " << with_delivery_res.total_cost << endl;
    cout << " inventory_cost: " << with_delivery_inventory_cost << ", stockout_cost: " << with_delivery_stockout_cost << ", supplier_minus_cost: " << with_delivery_supplier_minus_cost << endl;
    cout << " delivery_cost: " << with_delivery_delivery_cost << ", capacity_penalty_cost: " << with_delivery_capacity_penalty_cost << endl;
    cout << " with_delivery_end_inventory: " << with_delivery_end_inventory << endl;
  }
 // Calculatefuturecost
  DayRes future_res;
  double next_inventory_no_delivery = std::max<double>(0., no_delivery_end_inventory);
  future_res = SolveDayInventoryScenario(day + 1, next_inventory_no_delivery, scenario_id);
  no_delivery_res.total_cost += future_res.total_cost;
  double next_inventory_with_delivery = std::max<double>(0., with_delivery_end_inventory);
  future_res = SolveDayInventoryScenario(day + 1, next_inventory_with_delivery, scenario_id);
  with_delivery_res.total_cost += future_res.total_cost;
  if (traces) {
    cout << "future_res.total_cost: " << future_res.total_cost << endl;
  }

  // compare,Choose lower cost option
  DayRes day_res;
  if (traces) {
    cout << "Comparing: no_delivery=" << no_delivery_res.total_cost << " vs with_delivery=" << with_delivery_res.total_cost << endl;
  }
  if (with_delivery_res.total_cost < no_delivery_res.total_cost) {
    day_res.total_cost = with_delivery_res.total_cost;
    day_res.is_delivery = true;
    if (traces) cout << " Choose: DELIVERY" << endl;
 // debugInfo
    day_res.inventory_cost = with_delivery_inventory_cost;
    day_res.stockout_cost = with_delivery_stockout_cost;
    day_res.delivery_cost = with_delivery_delivery_cost;
    day_res.supplier_minus_cost = with_delivery_supplier_minus_cost;
    day_res.capacity_penalty_cost = with_delivery_capacity_penalty_cost;
  } else {
    day_res.total_cost = no_delivery_res.total_cost;
    day_res.is_delivery = false;
    if (traces) cout << " Choose: NO DELIVERY" << endl;
 // debugInfo
    day_res.inventory_cost = no_delivery_inventory_cost;
    day_res.stockout_cost = no_delivery_stockout_cost;
    day_res.delivery_cost = 0.0;
    day_res.supplier_minus_cost = 0.0;
    day_res.capacity_penalty_cost = 0.0;
  }

  // UsewritelockProtectedWriteoperation
  {
    std::unique_lock<std::shared_mutex> lock(memory_mutex_);
    map_memory_[{day, start_inventory, scenario_id}] = day_res;
  }

  if (traces) {
    // cin.get();
  }
  return day_res;
}
