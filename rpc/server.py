#!/usr/bin/env python3
"""
gRPC serverimplement - containcompleteOURemoteSolver
"""

import grpc
import logging
from concurrent import futures
import time
import math
import json
from typing import Dict, List, Tuple, Any

import irp_pb2
import irp_pb2_grpc
from gpu_main import gpu_run

# ConfigurationLog
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


def protobuf_to_dict(obj):
  """willprotobufObjectConvertfordictionary"""
  if hasattr(obj, 'DESCRIPTOR'):
    result = {}
    for field in obj.DESCRIPTOR.fields:
      value = getattr(obj, field.name)
      if field.type == field.TYPE_MESSAGE:
        if field.label == field.LABEL_REPEATED:
          result[field.name] = [protobuf_to_dict(item) for item in value]
        else:
          result[field.name] = protobuf_to_dict(value) if value else None
      elif field.type == field.TYPE_ENUM:
        result[field.name] = value
      else:
 # Processscalarfield(Array)
        if field.label == field.LABEL_REPEATED:
          result[field.name] = list(value) # Convertfornormallist
        else:
          result[field.name] = value
    return result
  elif isinstance(obj, (list, tuple)):
    return [protobuf_to_dict(item) for item in obj]
  else:
    return obj


class DayRes:
  """correspondingC++in theDayResstructure"""
  def __init__(self):
    self.is_delivery = False
    self.total_cost = 0.0
    self.end_inventory = 0.0
    self.inventory_cost = 0.0
    self.stockout_cost = 0.0
    self.delivery_cost = 0.0
    self.supplier_minus_cost = 0.0
    self.capacity_penalty_cost = 0.0


class OURemoteSolver:
 """PythonVersionOURemoteSolverimplement"""
  
  def __init__(self, req: irp_pb2.OuReq, resp: irp_pb2.OuResp):
    self.m_req = req
    self.m_resp = resp
    self.map_memory = {} # correspondingC++in themap_memory_
  
  def get_cost(self, capa_cost_func: irp_pb2.CapaCostFunc, quantity: float) -> float:
    """CalculateCapacityPenaltyCost"""
    if quantity <= capa_cost_func.threshold:
 return 0.0 # notoutCapacity,noPenalty
    else:
 # outCapacityPenalty = slope * (quantity - threshold)
      return capa_cost_func.slope * (quantity - capa_cost_func.threshold)
  
  def solve_day_inventory_scenario(self, day: int, start_inventory: float, scenario_id: int) -> DayRes:
    """correspondingC++in theSolveDayInventoryScenarioFunction"""
    traces = False
    
    # Check if already computed
    key = (day, start_inventory, scenario_id)
    if key in self.map_memory:
      return self.map_memory[key]
    
    # If beyond planning horizon,return zero cost
    if day > self.m_req.nb_days:
      if traces:
        print("reach the end of planning period")
      zero = DayRes()
      zero.total_cost = 0.0
      zero.is_delivery = False
      zero.end_inventory = start_inventory
      return zero
    
    res = DayRes()
    # Boundary protection
    demands = self.m_req.scenario_infos[scenario_id].daily_demands
    delivery_costs = self.m_req.scenario_infos[scenario_id].daily_delivery_costs
    capa_penalty_costs = self.m_req.scenario_infos[scenario_id].daily_capa_penalty_costs
    if day > len(demands):
 logger.error(f"day={day} out daily_demands Length={len(demands)},scenario_id={scenario_id}")
      return DayRes()
    if day > len(delivery_costs):
 logger.error(f"day={day} out daily_delivery_costs Length={len(delivery_costs)},scenario_id={scenario_id}")
      return DayRes()
    if day > len(capa_penalty_costs):
 logger.error(f"day={day} out daily_capa_penalty_costs Length={len(capa_penalty_costs)},scenario_id={scenario_id}")
      return DayRes()
    demand = demands[day - 1] # fromIndex 1 Start,sominus/reduce 1
    
    if traces:
      print(f"demand: {demand}")
    
    # No-delivery option
    no_delivery_res = DayRes()
    new_inventory = start_inventory - demand
    no_delivery_inventory_cost = self.m_req.inventory_cost * max(0.0, new_inventory)
    no_delivery_stockout_cost = self.m_req.stockout_cost * max(0.0, -new_inventory)
    no_delivery_end_inventory = max(0.0, new_inventory)
    no_delivery_res.total_cost = no_delivery_inventory_cost + no_delivery_stockout_cost
    
    if traces:
      print(f"no_delivery_res.total_cost: {no_delivery_res.total_cost}")
      print(f" inventory_cost: {no_delivery_inventory_cost}, stockout_cost: {no_delivery_stockout_cost}")
      print(f" no_delivery_end_inventory: {no_delivery_end_inventory}")
    
    # Delivery option
    with_delivery_res = DayRes()
    delivery_inventory = self.m_req.max_inventory - demand
    quantity = self.m_req.max_inventory - start_inventory
    with_delivery_inventory_cost = self.m_req.inventory_cost * max(0.0, delivery_inventory)
    with_delivery_stockout_cost = self.m_req.stockout_cost * max(0.0, -delivery_inventory)
    with_delivery_end_inventory = max(0.0, delivery_inventory)
    with_delivery_supplier_minus_cost = self.m_req.supplier_minus_cost * (self.m_req.nb_days - day + 1) * quantity
 with_delivery_delivery_cost = delivery_costs[day - 1] # Use day as/makeforIndex,because C++ Use 1-based Index
 with_delivery_capacity_penalty_cost = self.get_cost(capa_penalty_costs[day - 1], quantity) # Use day as/makeforIndex,because C++ Use 1-based Index
    
    # FIX: Supplier cost should REDUCE total cost (it's a discount), not increase it
    # This must match the CPU calculation: myCost -= inventoryCostSupplier * ...
    with_delivery_res.total_cost = (with_delivery_inventory_cost + with_delivery_stockout_cost -
                   with_delivery_supplier_minus_cost + with_delivery_delivery_cost +
                   with_delivery_capacity_penalty_cost)
    
    if traces:
      print(f"with_delivery_res.total_cost: {with_delivery_res.total_cost}")
      print(f" inventory_cost: {with_delivery_inventory_cost}, stockout_cost: {with_delivery_stockout_cost}")
      print(f" supplier_minus_cost: {with_delivery_supplier_minus_cost}")
      print(f" delivery_cost: {with_delivery_delivery_cost}, capacity_penalty_cost: {with_delivery_capacity_penalty_cost}")
      print(f" with_delivery_end_inventory: {with_delivery_end_inventory}")
    
 # CalculatetoCost
    next_inventory_no_delivery = max(0.0, no_delivery_end_inventory)
    future_res = self.solve_day_inventory_scenario(day + 1, next_inventory_no_delivery, scenario_id)
    no_delivery_res.total_cost += future_res.total_cost
    
    next_inventory_with_delivery = max(0.0, with_delivery_end_inventory)
    future_res = self.solve_day_inventory_scenario(day + 1, next_inventory_with_delivery, scenario_id)
    with_delivery_res.total_cost += future_res.total_cost
    
    if traces:
      print(f"future_res.total_cost: {future_res.total_cost}")
    
    # compare,Choose lower cost option
    if traces:
      print(f"Comparing: no_delivery={no_delivery_res.total_cost} vs with_delivery={with_delivery_res.total_cost}")
    
    if with_delivery_res.total_cost < no_delivery_res.total_cost:
      res.total_cost = with_delivery_res.total_cost
      res.is_delivery = True
      if traces:
        print(" Choose: DELIVERY")
      res.inventory_cost = with_delivery_inventory_cost
      res.stockout_cost = with_delivery_stockout_cost
      res.delivery_cost = with_delivery_delivery_cost
      res.supplier_minus_cost = with_delivery_supplier_minus_cost
      res.capacity_penalty_cost = with_delivery_capacity_penalty_cost
    else:
      res.total_cost = no_delivery_res.total_cost
      res.is_delivery = False
      if traces:
        print(" Choose: NO DELIVERY")
      res.inventory_cost = no_delivery_inventory_cost
      res.stockout_cost = no_delivery_stockout_cost
      res.delivery_cost = 0.0
      res.supplier_minus_cost = 0.0
      res.capacity_penalty_cost = 0.0
    
    self.map_memory[key] = res
    return res
  
  def solve(self):
    """correspondingC++in theSolveFunction"""
    traces = False

    # forceDeliveryplan
 # onlyclientidinvec_day_1_force_deliveryin/middle,thenfirstthen/justforceDelivery,elsepair/forno delivery
 vec_day_1_force_delivery = [2, 5, 3] # [1,2,3] represent/indicateclientidfor1,2,3ScenariofirstforceDelivery,otherScenariofirst day no delivery
    force_delivery = False

    vec_day_res = [DayRes() for _ in range(self.m_req.nb_scenario)]
    
 # based oncontrol_day_1differentProcess
    if self.m_req.control_day_1 == 0:
      # no first day control
      for s in range(self.m_req.nb_scenario):
        # ifpassedstart_inventory,thenUsestart_inventory,elseUsestart_inventory
        if self.m_req.scenario_infos[s].HasField("start_inventory"):
          vec_day_res[s] = self.solve_day_inventory_scenario(1, float(self.m_req.scenario_infos[s].start_inventory), s)
        else:
          vec_day_res[s] = self.solve_day_inventory_scenario(1, float(self.m_req.start_inventory), s)
    
      if traces:
        print("dp_ok!")
    
      # backtracking
      for s in range(self.m_req.nb_scenario):
        # Boundary protection
        demands = self.m_req.scenario_infos[s].daily_demands
        delivery_costs = self.m_req.scenario_infos[s].daily_delivery_costs
        capa_penalty_costs = self.m_req.scenario_infos[s].daily_capa_penalty_costs
        res_daily_plans = [False] * (self.m_req.nb_days + 1)
        res_daily_quantities = [0.0] * (self.m_req.nb_days + 1)
        t = 1
        if self.m_req.scenario_infos[s].HasField("start_inventory"):
          I = float(self.m_req.scenario_infos[s].start_inventory)
        else:
          I = float(self.m_req.start_inventory)
        prevI = I
        
        while t <= self.m_req.nb_days:
 # NewValue,loophaveValue
          demands = self.m_req.scenario_infos[s].daily_demands
          delivery_costs = self.m_req.scenario_infos[s].daily_delivery_costs
          capa_penalty_costs = self.m_req.scenario_infos[s].daily_capa_penalty_costs
          key = (t, I, s)
          if key not in self.map_memory:
            print(f"error: map_memory not found: (t, I, s) = ({t}, {I}, {s})")
            break
          
          result = self.map_memory[key]
          prevI = I
          res_daily_plans[t - 1] = result.is_delivery
          
          if result.is_delivery:
            res_daily_quantities[t - 1] = self.m_req.max_inventory - I
            I = max(0.0, self.m_req.max_inventory - demands[t - 1]) # fromIndex 1 Start,sominus/reduce 1
          else:
            I = max(0.0, I - demands[t - 1]) # fromIndex 1 Start,sominus/reduce 1
          
          if traces:
            print(f"day: {t} I: {prevI} chooseDelivery: {result.is_delivery} resultInventory: {I} totalCost: {result.total_cost}")
            print(f" inventory_cost: {result.inventory_cost}, stockout_cost: {result.stockout_cost}")
            print(f" delivery_cost: {result.delivery_cost}, supplier_minus_cost: {result.supplier_minus_cost}")
            print(f" capacity_penalty_cost: {result.capacity_penalty_cost}")
            print(f" quantity: {res_daily_quantities[t - 1]}")
          
          t += 1
        
        # CreateResultScenarioInfo
        result_scenario = irp_pb2.ScenarioInfo(
          scenario_id=s,
          daily_demands=self.m_req.scenario_infos[s].daily_demands,
          daily_delivery_costs=self.m_req.scenario_infos[s].daily_delivery_costs,
          daily_capa_penalty_costs=self.m_req.scenario_infos[s].daily_capa_penalty_costs,
          day_1_cost=0.0, # canbased onneedSet
          total_cost=vec_day_res[s].total_cost,
          daily_plans=res_daily_plans,
          daily_quantities=res_daily_quantities,
          start_inventory=int(I)
        )
        
        self.m_resp.scenario_info_results.append(result_scenario)
    
    elif self.m_req.control_day_1 == 1:
      # Semi-control first day
      vec_day_1_no_delivery_res = [DayRes() for _ in range(self.m_req.nb_scenario)]
      vec_day_1_with_delivery_res = [DayRes() for _ in range(self.m_req.nb_scenario)]
      vec_day_1_no_delivery_cost = [0.0] * self.m_req.nb_scenario
      vec_day_1_with_delivery_cost = [0.0] * self.m_req.nb_scenario
      
      for s in range(self.m_req.nb_scenario):
        # Boundary protection
        demands = self.m_req.scenario_infos[s].daily_demands
        delivery_costs = self.m_req.scenario_infos[s].daily_delivery_costs
        capa_penalty_costs = self.m_req.scenario_infos[s].daily_capa_penalty_costs
 # first day no delivery
        day_1_end_inventory_no_delivery = self.m_req.start_inventory - demands[0] # fromIndex 1 Start,sominus/reduce 1
        day_1_cost_no_delivery = (self.m_req.inventory_cost * max(0.0, day_1_end_inventory_no_delivery) 
                    if day_1_end_inventory_no_delivery > 0.0 
                    else self.m_req.stockout_cost * -day_1_end_inventory_no_delivery)
        day_1_end_inventory_no_delivery = max(0.0, day_1_end_inventory_no_delivery)
        
        vec_day_1_no_delivery_res[s] = self.solve_day_inventory_scenario(2, day_1_end_inventory_no_delivery, s)
        vec_day_1_no_delivery_res[s].total_cost += day_1_cost_no_delivery
        vec_day_1_no_delivery_cost[s] = day_1_cost_no_delivery
        
        # Boundary protection
        demands = self.m_req.scenario_infos[s].daily_demands
        delivery_costs = self.m_req.scenario_infos[s].daily_delivery_costs
        capa_penalty_costs = self.m_req.scenario_infos[s].daily_capa_penalty_costs

 # first delivery
        day_1_end_inventory_with_delivery = self.m_req.max_inventory - demands[0] # fromIndex 1 Start,sominus/reduce 1
        quantity = self.m_req.max_inventory - self.m_req.start_inventory
 day_1_cost_with_delivery = (delivery_costs[1-1] + # Use day as/makeforIndex,because C++ Use 1-based Index
 self.get_cost(capa_penalty_costs[1-1], quantity)) # Use day as/makeforIndex,because C++ Use 1-based Index

        # FIX: Correctly handle both holding cost (inventory > 0) and stockout cost (inventory < 0)
        # This must match CPU calculation in OUPolicySolver.cpp:285-291
        if day_1_end_inventory_with_delivery > 0.0:
          day_1_cost_with_delivery += self.m_req.inventory_cost * day_1_end_inventory_with_delivery
        else:
          day_1_cost_with_delivery += self.m_req.stockout_cost * (-day_1_end_inventory_with_delivery)

        # FIX: Subtract supplier cost (matches CPU calculation in OUPolicySolver.cpp:299)
        day_1_cost_with_delivery -= self.m_req.supplier_minus_cost * self.m_req.nb_days * quantity
        
        vec_day_1_with_delivery_res[s] = self.solve_day_inventory_scenario(2, day_1_end_inventory_with_delivery, s)
        vec_day_1_with_delivery_res[s].total_cost += day_1_cost_with_delivery
        vec_day_1_with_delivery_cost[s] = day_1_cost_with_delivery
      
      # Choose lower cost option
      no_delivery_cost = sum(res.total_cost for res in vec_day_1_no_delivery_res)
      with_delivery_cost = sum(res.total_cost for res in vec_day_1_with_delivery_res)
      
      vec_day_1_cost = [0.0] * self.m_req.nb_scenario
      day_1_delivery = no_delivery_cost > with_delivery_cost

      if force_delivery:
        # print(self.m_req.client_id)
        # print(vec_day_1_force_delivery)
        # input()
        if self.m_req.client_id in vec_day_1_force_delivery:
          day_1_delivery = True
          # input("force delivery:"+str(self.m_req.client_id))
        else:
          day_1_delivery = False 


      if not day_1_delivery:
 vec_day_res = vec_day_1_no_delivery_res # thisday resnotcontainday1cost Policyalsoissecond
        vec_day_1_cost = vec_day_1_no_delivery_cost
      else:
 vec_day_res = vec_day_1_with_delivery_res # thisday resnotcontainday1cost
        vec_day_1_cost = vec_day_1_with_delivery_cost
      
 # SavefirstResult
      for s in range(self.m_req.nb_scenario):
        self.map_memory[(1, self.m_req.start_inventory, s)] = vec_day_res[s]
    
      if traces:
        print("dp_ok!")
    
      # backtracking
      for s in range(self.m_req.nb_scenario):
        # Boundary protection
        demands = self.m_req.scenario_infos[s].daily_demands
        delivery_costs = self.m_req.scenario_infos[s].daily_delivery_costs
        capa_penalty_costs = self.m_req.scenario_infos[s].daily_capa_penalty_costs
        res_daily_plans = [False] * (self.m_req.nb_days + 1)
        res_daily_quantities = [0.0] * (self.m_req.nb_days + 1)
        day_1_res_inventory = 0
        if not day_1_delivery:
          res_daily_plans[0] = False
          res_daily_quantities[0] = 0.0
          day_1_res_inventory = max(0.0, self.m_req.start_inventory - demands[0])
        else:
          res_daily_plans[0] = True
          res_daily_quantities[0] = self.m_req.max_inventory - self.m_req.start_inventory
          day_1_res_inventory = max(0.0, self.m_req.max_inventory - demands[0])
        
        t = 2
        I = day_1_res_inventory
        day_1_cost = vec_day_1_cost[s]
        prevI = I
        
        while t <= self.m_req.nb_days:
 # NewValue,loophaveValue
          demands = self.m_req.scenario_infos[s].daily_demands
          delivery_costs = self.m_req.scenario_infos[s].daily_delivery_costs
          capa_penalty_costs = self.m_req.scenario_infos[s].daily_capa_penalty_costs
          key = (t, I, s)
          if key not in self.map_memory:
            print(f"error: map_memory not found: (t, I, s) = ({t}, {I}, {s})")
            break
          
          result = self.map_memory[key]
          prevI = I
          res_daily_plans[t - 1] = result.is_delivery
          
          if result.is_delivery:
            res_daily_quantities[t - 1] = self.m_req.max_inventory - I
            I = max(0.0, self.m_req.max_inventory - demands[t - 1]) # fromIndex 1 Start,sominus/reduce 1
          else:
            I = max(0.0, I - demands[t - 1]) # fromIndex 1 Start,sominus/reduce 1
          
          if traces:
            print(f"day: {t} I: {prevI} chooseDelivery: {result.is_delivery} resultInventory: {I} totalCost: {result.total_cost}")
            print(f" inventory_cost: {result.inventory_cost}, stockout_cost: {result.stockout_cost}")
            print(f" delivery_cost: {result.delivery_cost}, supplier_minus_cost: {result.supplier_minus_cost}")
            print(f" capacity_penalty_cost: {result.capacity_penalty_cost}")
            print(f" quantity: {res_daily_quantities[t - 1]}")
          
          t += 1
        
        # CreateResultScenarioInfo
        result_scenario = irp_pb2.ScenarioInfo(
          scenario_id=s,
          daily_demands=self.m_req.scenario_infos[s].daily_demands,
          daily_delivery_costs=self.m_req.scenario_infos[s].daily_delivery_costs,
          daily_capa_penalty_costs=self.m_req.scenario_infos[s].daily_capa_penalty_costs,
          day_1_cost=day_1_cost, # canbased onneedSet
          total_cost=vec_day_res[s].total_cost,
          daily_plans=res_daily_plans,
          daily_quantities=res_daily_quantities
        )
        
        self.m_resp.scenario_info_results.append(result_scenario)
    
    if traces:
      print("backtracking_ok!")
    
    self.map_memory.clear()
    return

  def solve_with_gpu(self):
 # gpu no first day control,firstbyserver gpuonlyimplement SolvemultipleScenariobefore/frontOptimal solution
 # first reqin/middleallfirstcorrelationInfooutto,thenvec[0],whenat/togpuonlytoSolve2-k

 # +++++ Checkisnoprovidepredeterminedfirst day decisionConstraint +++++
    has_day_1_constraint = (self.m_req.HasField("control_day_1_info") and
                self.m_req.control_day_1_info.ByteSize() > 0)
    forced_day_1_delivery = None
    if has_day_1_constraint:
      forced_day_1_delivery = self.m_req.control_day_1_info.day_1_delivery
      logger.info(f"[Day1 Control] Using pre-determined day 1 decision: delivery={forced_day_1_delivery}")

    gpu_req = irp_pb2.OuReq()
    gpu_req.client_id = self.m_req.client_id
    gpu_req.nb_days = self.m_req.nb_days - 1
    gpu_req.nb_scenario = self.m_req.nb_scenario
    gpu_req.max_inventory = self.m_req.max_inventory
    gpu_req.inventory_cost = self.m_req.inventory_cost
    gpu_req.stockout_cost = self.m_req.stockout_cost
    gpu_req.supplier_minus_cost = self.m_req.supplier_minus_cost

 # ProcesseachScenario,onlyretain2~kdata
    for scenario in self.m_req.scenario_infos:
      new_scenario = irp_pb2.ScenarioInfo()
      new_scenario.scenario_id = scenario.scenario_id
 new_scenario.daily_demands.extend(scenario.daily_demands[1:]) # to0
      new_scenario.daily_delivery_costs.extend(scenario.daily_delivery_costs[1:])
      new_scenario.daily_capa_penalty_costs.extend(scenario.daily_capa_penalty_costs[1:])
      gpu_req.scenario_infos.append(new_scenario)

 # first/not - (CreatetwoRequest-) twoRequesta/an onlyisscenarioCounttimes before/frontisDelivery after/backisno delivery
    # gpu_day_1_with_delivery_req = irp_pb2.OuReq()
    # gpu_day_1_with_delivery_req.CopyFrom(gpu_req)

    # gpu_day_1_no_delivery_req = irp_pb2.OuReq()
    # gpu_day_1_no_delivery_req.CopyFrom(gpu_req)
    temp_scenario_infos1 = []
    temp_scenario_infos2 = []
    
 # Processfirst
    for scenario_id, scenario_info in enumerate(gpu_req.scenario_infos):
 # Processfirst
      temp_scenario_info1 = irp_pb2.ScenarioInfo()
      temp_scenario_info1.CopyFrom(scenario_info)
      temp_scenario_info1.start_inventory = int(self.m_req.max_inventory - self.m_req.scenario_infos[scenario_id].daily_demands[0])
      temp_scenario_info1.daily_plans.append(True)
      temp_scenario_info1.daily_quantities.append(self.m_req.max_inventory - self.m_req.start_inventory)
      temp_scenario_info1.day_1_cost = 0.0
 temp_scenario_info1.day_1_cost += self.m_req.inventory_cost * temp_scenario_info1.start_inventory # 1InventoryCost
 temp_scenario_info1.day_1_cost += self.m_req.scenario_infos[scenario_id].daily_delivery_costs[0] # 1DeliveryCost
 temp_scenario_info1.day_1_cost += self.get_cost(self.m_req.scenario_infos[scenario_id].daily_capa_penalty_costs[0], temp_scenario_info1.daily_quantities[0]) # 1CapacityPenaltyCost
      # REFACTOR: Subtract supplier cost for Day 1 delivery (matches CPU in OUPolicySolver.cpp:299)
      quantity_day1 = self.m_req.max_inventory - self.m_req.start_inventory
      temp_scenario_info1.day_1_cost -= self.m_req.supplier_minus_cost * self.m_req.nb_days * quantity_day1
      temp_scenario_info1.total_cost = temp_scenario_info1.day_1_cost
      temp_scenario_infos1.append(temp_scenario_info1)
      
 # Processfirstnot
      temp_scenario_info2 = irp_pb2.ScenarioInfo()
      temp_scenario_info2.CopyFrom(scenario_info)
      temp_scenario_id = scenario_id + self.m_req.nb_scenario # distinguish
      temp_scenario_info2.start_inventory = int(max(0.0, self.m_req.start_inventory - self.m_req.scenario_infos[scenario_id].daily_demands[0]))
      temp_scenario_info2.daily_plans.append(False)
      temp_scenario_info2.daily_quantities.append(0.0)
      temp_scenario_info2.day_1_cost = 0.0
 temp_scenario_info2.day_1_cost += self.m_req.inventory_cost * temp_scenario_info2.start_inventory # 1InventoryCost
 temp_scenario_info2.day_1_cost += self.m_req.stockout_cost * max(0.0, self.m_req.scenario_infos[scenario_id].daily_demands[0] - self.m_req.start_inventory) # 1StockoutCost
      temp_scenario_info2.total_cost = temp_scenario_info2.day_1_cost
      temp_scenario_infos2.append(temp_scenario_info2)
    
 # gpuReplacementfortemp_scenario_infos1andtemp_scenario_infos2 scenarioCounttimes
    gpu_req.scenario_infos.clear()
    gpu_req.scenario_infos.extend(temp_scenario_infos1)
    gpu_req.scenario_infos.extend(temp_scenario_infos2)
    gpu_req.control_day_1 = 0
    gpu_req.nb_scenario = len(gpu_req.scenario_infos) # double
    
 # twoRequesta/an onlyisscenarioCounttimes before/frontisDelivery after/backisno delivery
    # ------------------------------
 # ingpu
 # pbdict 
    gpu_req_dict = protobuf_to_dict(gpu_req)
 # callgpuFunction Note a/anscenariostart_inventorynotsame to; passed inScenarioCountisdoublebutgpunot need Solvethen/justrow
 total_cost_after_day_1 = [0.0] * gpu_req.nb_scenario # gpu_req.nb_scenarioisdoublenb_scenario before/frontisDelivery after/backisno delivery
    daily_plans_after_day_1 = [[]] * gpu_req.nb_scenario
    daily_quantities_after_day_1 = [[]] * gpu_req.nb_scenario
    # ------------------------------
 # not needUsepath directdictas/makeforParameterpass intoto run(path) for gpu_run(gpu_req_dict) dataoperationthen/justnot
    # gpu_run(gpu_req_dict)
    # return costs, flags, quantities, num 
 # behindWritethreeResult before/frontisfirst delivery after/backisfirst day no delivery
 # toroughlyis
    # ------------------------------
    total_cost_after_day_1, daily_plans_after_day_1, daily_quantities_after_day_1, _ = gpu_run(gpu_req_dict)
    # ------------------------------
 # Completegpu belowStartWriteResult

    # Choose lower cost option
    day_1_with_delivery_total_cost = [0.0] * self.m_req.nb_scenario
    day_1_no_delivery_total_cost = [0.0] * self.m_req.nb_scenario
 # simultaneouslytake/get/no deliveryInfo andUpdatetotalcost
    for scenario_id in range(self.m_req.nb_scenario):
      day_1_with_delivery_total_cost[scenario_id] = total_cost_after_day_1[scenario_id]
      day_1_with_delivery_total_cost[scenario_id] += gpu_req.scenario_infos[scenario_id].day_1_cost
      day_1_no_delivery_total_cost[scenario_id] = total_cost_after_day_1[scenario_id + self.m_req.nb_scenario]
      day_1_no_delivery_total_cost[scenario_id] += gpu_req.scenario_infos[scenario_id + self.m_req.nb_scenario].day_1_cost

    # MergeResult
    result_resp = irp_pb2.OuResp()

 # +++++ decidefirstisnoDelivery +++++
 # ifprovidepredeterminedfirst day decision,Use;else,based onCostcomparetoSelection
    if has_day_1_constraint:
 # Usefirst day decision
      use_day_1_delivery = forced_day_1_delivery
      logger.info(f"[Day1 Control] Using forced day 1 decision: {use_day_1_delivery}")
    else:
 # based onCostcompareSelectionfirst day decision
      use_day_1_delivery = sum(day_1_with_delivery_total_cost) < sum(day_1_no_delivery_total_cost)
      logger.info(f"[Day1 Control] Cost comparison: with_delivery={sum(day_1_with_delivery_total_cost):.2f}, "
            f"no_delivery={sum(day_1_no_delivery_total_cost):.2f}, chosen: {use_day_1_delivery}")

 # first selection total costsmall then putfirstplanandquantityfirst write then write the rest andUpdatetotalcost
    if use_day_1_delivery:
      # CopyScenarioInfo
      for s in range(self.m_req.nb_scenario):
        new_scenario = irp_pb2.ScenarioInfo()
        new_scenario.CopyFrom(temp_scenario_infos1[s])
 new_scenario.actual_day_1_delivery = True # GPUSelectionfirst delivery
        result_resp.scenario_info_results.append(new_scenario)

      for scenario_id in range(self.m_req.nb_scenario):
 # notadd,becauseSolversReturnResultalreadycontainthe Nth1data
        result_resp.scenario_info_results[scenario_id].daily_plans.extend(daily_plans_after_day_1[scenario_id])
        result_resp.scenario_info_results[scenario_id].daily_quantities.extend(daily_quantities_after_day_1[scenario_id])
        result_resp.scenario_info_results[scenario_id].total_cost = day_1_with_delivery_total_cost[scenario_id]
    else:
      # CopyScenarioInfo
      for s in range(self.m_req.nb_scenario):
        new_scenario = irp_pb2.ScenarioInfo()
        new_scenario.CopyFrom(temp_scenario_infos2[s])
        new_scenario.scenario_id = s
 new_scenario.actual_day_1_delivery = False # GPUSelectionfirst day no delivery
        result_resp.scenario_info_results.append(new_scenario)

      for scenario_id in range(self.m_req.nb_scenario):
 # notadd,becauseSolversReturnResultalreadycontainthe Nth1data
        result_resp.scenario_info_results[scenario_id].daily_plans.extend(daily_plans_after_day_1[scenario_id + self.m_req.nb_scenario])
        result_resp.scenario_info_results[scenario_id].daily_quantities.extend(daily_quantities_after_day_1[scenario_id + self.m_req.nb_scenario])
        result_resp.scenario_info_results[scenario_id].total_cost = day_1_no_delivery_total_cost[scenario_id]

    # CopyResulttoResponse
    for scenario_info in result_resp.scenario_info_results:
      new_scenario = irp_pb2.ScenarioInfo()
      new_scenario.CopyFrom(scenario_info)
      self.m_resp.scenario_info_results.append(new_scenario)

    return


class IrpServiceServicer(irp_pb2_grpc.IrpServiceServicer):
 """implement IrpService server"""
  
  def ProcessOptimization(self, request, context):
    """ProcessOptimizeRequest"""
    use_gpu = True
    traces = False
    if traces:
 logger.info(f"toOptimizeRequest: client_id={request.client_id}, nb_days={request.nb_days}, nb_scenario={request.nb_scenario}")
 logger.info(f"to scenario_infos Count: {len(request.scenario_infos)}")
    
    try:
 # PrintRequestJSONformat
      if traces:
        logger.info("=== Requestdata (JSONformat) ===")
        request_dict = protobuf_to_dict(request)
        logger.info(json.dumps(request_dict, indent=2, ensure_ascii=False))
        print(json.dumps(request_dict, indent=2, ensure_ascii=False))
      # input()
      
      # CreateResponseObject
      response = irp_pb2.OuResp()
      
      # CreateOURemoteSolverandSolve
      if use_gpu:
        solver = OURemoteSolver(request, response)
        solver.solve_with_gpu()
      else:
        solver = OURemoteSolver(request, response)
        solver.solve()
      
      if traces:
        logger.info(f"ProcessComplete,Return {len(response.scenario_info_results)} ScenarioResult")
      
 # PrintResponseJSONformat
      if traces:
        logger.info("=== Responsedata (JSONformat) ===")
        response_dict = protobuf_to_dict(response)
        logger.info(json.dumps(response_dict, indent=2, ensure_ascii=False))
      # input()
      # PrintsomeDebugInfo
      for i, scenario in enumerate(response.scenario_info_results):
        if traces:
          logger.info(f"Scenario {i+1}: total_cost={scenario.total_cost}, "
               f"daily_plans={scenario.daily_plans}, "
               f"daily_quantities={scenario.daily_quantities}")
      
      return response
      
    except Exception as e:
 logger.error(f"ProcessRequestError: {e}")
      context.set_code(grpc.StatusCode.INTERNAL)
      context.set_details(f"serverinternalError: {str(e)}")
      return irp_pb2.OuResp()


def serve():
  """Start gRPC server"""
  server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
  irp_pb2_grpc.add_IrpServiceServicer_to_server(IrpServiceServicer(), server)
  
 # Port
  listen_addr = '[::]:50051'
  server.add_insecure_port(listen_addr)
  
  logger.info(f"Start gRPC server,listening address: {listen_addr}")
  server.start()
  
  try:
    # maintainserverRun
    while True:
      time.sleep(86400) # 24hours
  except KeyboardInterrupt:
 logger.info("tointerruptsignal,incloseserver...")
    server.stop(0)
    logger.info("serveralreadyclose")


if __name__ == '__main__':
  serve() 