#!/usr/bin/env python3
"""
gRPC 服务器实现 - 包含完整的OURemoteSolver逻辑
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

# 配置日志
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


def protobuf_to_dict(obj):
    """将protobuf对象转换为字典"""
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
                # 处理重复的标量字段（如数组）
                if field.label == field.LABEL_REPEATED:
                    result[field.name] = list(value)  # 转换为普通list
                else:
                    result[field.name] = value
        return result
    elif isinstance(obj, (list, tuple)):
        return [protobuf_to_dict(item) for item in obj]
    else:
        return obj


class DayRes:
    """对应C++中的DayRes结构"""
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
    """Python版本的OURemoteSolver实现"""
    
    def __init__(self, req: irp_pb2.OuReq, resp: irp_pb2.OuResp):
        self.m_req = req
        self.m_resp = resp
        self.map_memory = {}  # 对应C++中的map_memory_
    
    def get_cost(self, capa_cost_func: irp_pb2.CapaCostFunc, quantity: float) -> float:
        """计算容量惩罚成本"""
        if quantity <= capa_cost_func.threshold:
            return 0.0  # 不超出容量，无惩罚
        else:
            # 超出容量的惩罚 = slope * (quantity - threshold)
            return capa_cost_func.slope * (quantity - capa_cost_func.threshold)
    
    def solve_day_inventory_scenario(self, day: int, start_inventory: float, scenario_id: int) -> DayRes:
        """对应C++中的SolveDayInventoryScenario函数"""
        traces = False
        
        # 检查是否已经计算过
        key = (day, start_inventory, scenario_id)
        if key in self.map_memory:
            return self.map_memory[key]
        
        # 如果超过规划期，返回零成本
        if day > self.m_req.nb_days:
            if traces:
                print("reach the end of planning period")
            zero = DayRes()
            zero.total_cost = 0.0
            zero.is_delivery = False
            zero.end_inventory = start_inventory
            return zero
        
        res = DayRes()
        # 边界保护
        demands = self.m_req.scenario_infos[scenario_id].daily_demands
        delivery_costs = self.m_req.scenario_infos[scenario_id].daily_delivery_costs
        capa_penalty_costs = self.m_req.scenario_infos[scenario_id].daily_capa_penalty_costs
        if day > len(demands):
            logger.error(f"day={day} 超出 daily_demands 长度={len(demands)}，scenario_id={scenario_id}")
            return DayRes()
        if day > len(delivery_costs):
            logger.error(f"day={day} 超出 daily_delivery_costs 长度={len(delivery_costs)}，scenario_id={scenario_id}")
            return DayRes()
        if day > len(capa_penalty_costs):
            logger.error(f"day={day} 超出 daily_capa_penalty_costs 长度={len(capa_penalty_costs)}，scenario_id={scenario_id}")
            return DayRes()
        demand = demands[day - 1]  # 从索引 1 开始，所以减 1
        
        if traces:
            print(f"demand: {demand}")
        
        # 不配送选项
        no_delivery_res = DayRes()
        new_inventory = start_inventory - demand
        no_delivery_inventory_cost = self.m_req.inventory_cost * max(0.0, new_inventory)
        no_delivery_stockout_cost = self.m_req.stockout_cost * max(0.0, -new_inventory)
        no_delivery_end_inventory = max(0.0, new_inventory)
        no_delivery_res.total_cost = no_delivery_inventory_cost + no_delivery_stockout_cost
        
        if traces:
            print(f"no_delivery_res.total_cost: {no_delivery_res.total_cost}")
            print(f"  inventory_cost: {no_delivery_inventory_cost}, stockout_cost: {no_delivery_stockout_cost}")
            print(f"  no_delivery_end_inventory: {no_delivery_end_inventory}")
        
        # 配送选项
        with_delivery_res = DayRes()
        delivery_inventory = self.m_req.max_inventory - demand
        quantity = self.m_req.max_inventory - start_inventory
        with_delivery_inventory_cost = self.m_req.inventory_cost * max(0.0, delivery_inventory)
        with_delivery_stockout_cost = self.m_req.stockout_cost * max(0.0, -delivery_inventory)
        with_delivery_end_inventory = max(0.0, delivery_inventory)
        with_delivery_supplier_minus_cost = self.m_req.supplier_minus_cost * (self.m_req.nb_days - day + 1) * quantity
        with_delivery_delivery_cost = delivery_costs[day - 1] # 使用 day 作为索引，因为 C++ 端使用 1-based 索引
        with_delivery_capacity_penalty_cost = self.get_cost(capa_penalty_costs[day - 1], quantity) # 使用 day 作为索引，因为 C++ 端使用 1-based 索引
        
        with_delivery_res.total_cost = (with_delivery_inventory_cost + with_delivery_stockout_cost + 
                                      with_delivery_supplier_minus_cost + with_delivery_delivery_cost + 
                                      with_delivery_capacity_penalty_cost)
        
        if traces:
            print(f"with_delivery_res.total_cost: {with_delivery_res.total_cost}")
            print(f"  inventory_cost: {with_delivery_inventory_cost}, stockout_cost: {with_delivery_stockout_cost}")
            print(f"  supplier_minus_cost: {with_delivery_supplier_minus_cost}")
            print(f"  delivery_cost: {with_delivery_delivery_cost}, capacity_penalty_cost: {with_delivery_capacity_penalty_cost}")
            print(f"  with_delivery_end_inventory: {with_delivery_end_inventory}")
        
        # 计算未来成本
        next_inventory_no_delivery = max(0.0, no_delivery_end_inventory)
        future_res = self.solve_day_inventory_scenario(day + 1, next_inventory_no_delivery, scenario_id)
        no_delivery_res.total_cost += future_res.total_cost
        
        next_inventory_with_delivery = max(0.0, with_delivery_end_inventory)
        future_res = self.solve_day_inventory_scenario(day + 1, next_inventory_with_delivery, scenario_id)
        with_delivery_res.total_cost += future_res.total_cost
        
        if traces:
            print(f"future_res.total_cost: {future_res.total_cost}")
        
        # 比较，选择成本更低的选项
        if traces:
            print(f"Comparing: no_delivery={no_delivery_res.total_cost} vs with_delivery={with_delivery_res.total_cost}")
        
        if with_delivery_res.total_cost < no_delivery_res.total_cost:
            res.total_cost = with_delivery_res.total_cost
            res.is_delivery = True
            if traces:
                print("  Choose: DELIVERY")
            res.inventory_cost = with_delivery_inventory_cost
            res.stockout_cost = with_delivery_stockout_cost
            res.delivery_cost = with_delivery_delivery_cost
            res.supplier_minus_cost = with_delivery_supplier_minus_cost
            res.capacity_penalty_cost = with_delivery_capacity_penalty_cost
        else:
            res.total_cost = no_delivery_res.total_cost
            res.is_delivery = False
            if traces:
                print("  Choose: NO DELIVERY")
            res.inventory_cost = no_delivery_inventory_cost
            res.stockout_cost = no_delivery_stockout_cost
            res.delivery_cost = 0.0
            res.supplier_minus_cost = 0.0
            res.capacity_penalty_cost = 0.0
        
        self.map_memory[key] = res
        return res
    
    def solve(self):
        """对应C++中的Solve函数"""
        traces = False

        # 强制配送plan
        # 用法 只要clientid在vec_day_1_force_delivery中，那么第一天就强制配送，否则绝对不配送
        vec_day_1_force_delivery = [2, 5, 3] # [1,2,3] 表示clientid为1,2,3的场景第一天强制配送，其他场景第一天不配送
        force_delivery = False

        vec_day_res = [DayRes() for _ in range(self.m_req.nb_scenario)]
        
        # 根据control_day_1的不同情况处理
        if self.m_req.control_day_1 == 0:
            # 不控制第一天
            for s in range(self.m_req.nb_scenario):
                # 如果传了start_inventory，则使用start_inventory，否则使用start_inventory
                if self.m_req.scenario_infos[s].HasField("start_inventory"):
                    vec_day_res[s] = self.solve_day_inventory_scenario(1, float(self.m_req.scenario_infos[s].start_inventory), s)
                else:
                    vec_day_res[s] = self.solve_day_inventory_scenario(1, float(self.m_req.start_inventory), s)
        
            if traces:
                print("dp_ok!")
        
            # backtracking
            for s in range(self.m_req.nb_scenario):
                # 边界保护
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
                    # 重新赋值，确保每次循环都有值
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
                        I = max(0.0, self.m_req.max_inventory - demands[t - 1])  # 从索引 1 开始，所以减 1
                    else:
                        I = max(0.0, I - demands[t - 1])  # 从索引 1 开始，所以减 1
                    
                    if traces:
                        print(f"day: {t} I: {prevI} chooseDelivery: {result.is_delivery} resultInventory: {I} totalCost: {result.total_cost}")
                        print(f"  inventory_cost: {result.inventory_cost}, stockout_cost: {result.stockout_cost}")
                        print(f"  delivery_cost: {result.delivery_cost}, supplier_minus_cost: {result.supplier_minus_cost}")
                        print(f"  capacity_penalty_cost: {result.capacity_penalty_cost}")
                        print(f"  quantity: {res_daily_quantities[t - 1]}")
                    
                    t += 1
                
                # 创建结果场景信息
                result_scenario = irp_pb2.ScenarioInfo(
                    scenario_id=s,
                    daily_demands=self.m_req.scenario_infos[s].daily_demands,
                    daily_delivery_costs=self.m_req.scenario_infos[s].daily_delivery_costs,
                    daily_capa_penalty_costs=self.m_req.scenario_infos[s].daily_capa_penalty_costs,
                    day_1_cost=0.0,  # 可以根据需要设置
                    total_cost=vec_day_res[s].total_cost,
                    daily_plans=res_daily_plans,
                    daily_quantities=res_daily_quantities,
                    start_inventory=int(I)
                )
                
                self.m_resp.scenario_info_results.append(result_scenario)
        
        elif self.m_req.control_day_1 == 1:
            # 半控第一天
            vec_day_1_no_delivery_res = [DayRes() for _ in range(self.m_req.nb_scenario)]
            vec_day_1_with_delivery_res = [DayRes() for _ in range(self.m_req.nb_scenario)]
            vec_day_1_no_delivery_cost = [0.0] * self.m_req.nb_scenario
            vec_day_1_with_delivery_cost = [0.0] * self.m_req.nb_scenario
            
            for s in range(self.m_req.nb_scenario):
                # 边界保护
                demands = self.m_req.scenario_infos[s].daily_demands
                delivery_costs = self.m_req.scenario_infos[s].daily_delivery_costs
                capa_penalty_costs = self.m_req.scenario_infos[s].daily_capa_penalty_costs
                # 第一天不配送
                day_1_end_inventory_no_delivery = self.m_req.start_inventory - demands[0]  # 从索引 1 开始，所以减 1
                day_1_cost_no_delivery = (self.m_req.inventory_cost * max(0.0, day_1_end_inventory_no_delivery) 
                                        if day_1_end_inventory_no_delivery > 0.0 
                                        else self.m_req.stockout_cost * -day_1_end_inventory_no_delivery)
                day_1_end_inventory_no_delivery = max(0.0, day_1_end_inventory_no_delivery)
                
                vec_day_1_no_delivery_res[s] = self.solve_day_inventory_scenario(2, day_1_end_inventory_no_delivery, s)
                vec_day_1_no_delivery_res[s].total_cost += day_1_cost_no_delivery
                vec_day_1_no_delivery_cost[s] = day_1_cost_no_delivery
                
                # 边界保护
                demands = self.m_req.scenario_infos[s].daily_demands
                delivery_costs = self.m_req.scenario_infos[s].daily_delivery_costs
                capa_penalty_costs = self.m_req.scenario_infos[s].daily_capa_penalty_costs

                # 第一天配送
                day_1_end_inventory_with_delivery = self.m_req.max_inventory - demands[0]  # 从索引 1 开始，所以减 1
                day_1_cost_with_delivery = (delivery_costs[1-1] +  # 使用 day 作为索引，因为 C++ 端使用 1-based 索引
                                          self.get_cost(capa_penalty_costs[1-1],  # 使用 day 作为索引，因为 C++ 端使用 1-based 索引
                                                      self.m_req.max_inventory - self.m_req.start_inventory))
                day_1_cost_with_delivery += self.m_req.inventory_cost * day_1_end_inventory_with_delivery
                
                vec_day_1_with_delivery_res[s] = self.solve_day_inventory_scenario(2, day_1_end_inventory_with_delivery, s)
                vec_day_1_with_delivery_res[s].total_cost += day_1_cost_with_delivery
                vec_day_1_with_delivery_cost[s] = day_1_cost_with_delivery
            
            # 选择成本更低的方案
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
                vec_day_res = vec_day_1_no_delivery_res # 这个day res不包含day1的cost 策略也是第二天的
                vec_day_1_cost = vec_day_1_no_delivery_cost
            else:
                vec_day_res = vec_day_1_with_delivery_res # 这个day res不包含day1的cost
                vec_day_1_cost = vec_day_1_with_delivery_cost
            
            # 保存第一天结果
            for s in range(self.m_req.nb_scenario):
                self.map_memory[(1, self.m_req.start_inventory, s)] = vec_day_res[s]
        
            if traces:
                print("dp_ok!")
        
            # backtracking
            for s in range(self.m_req.nb_scenario):
                # 边界保护
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
                    # 重新赋值，确保每次循环都有值
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
                        I = max(0.0, self.m_req.max_inventory - demands[t - 1])  # 从索引 1 开始，所以减 1
                    else:
                        I = max(0.0, I - demands[t - 1])  # 从索引 1 开始，所以减 1
                    
                    if traces:
                        print(f"day: {t} I: {prevI} chooseDelivery: {result.is_delivery} resultInventory: {I} totalCost: {result.total_cost}")
                        print(f"  inventory_cost: {result.inventory_cost}, stockout_cost: {result.stockout_cost}")
                        print(f"  delivery_cost: {result.delivery_cost}, supplier_minus_cost: {result.supplier_minus_cost}")
                        print(f"  capacity_penalty_cost: {result.capacity_penalty_cost}")
                        print(f"  quantity: {res_daily_quantities[t - 1]}")
                    
                    t += 1
                
                # 创建结果场景信息
                result_scenario = irp_pb2.ScenarioInfo(
                    scenario_id=s,
                    daily_demands=self.m_req.scenario_infos[s].daily_demands,
                    daily_delivery_costs=self.m_req.scenario_infos[s].daily_delivery_costs,
                    daily_capa_penalty_costs=self.m_req.scenario_infos[s].daily_capa_penalty_costs,
                    day_1_cost=day_1_cost,  # 可以根据需要设置
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
        # gpu 不控制第一天，第一天由server控制 gpu只专注实现 求解多个场景的目前最优解
        # 首先 把req中所有第一天相关的信息拿出来，然后把vec[0]都干掉，相当于gpu只去求解2-k天
        gpu_req = irp_pb2.OuReq()
        gpu_req.client_id = self.m_req.client_id
        gpu_req.nb_days = self.m_req.nb_days - 1
        gpu_req.nb_scenario = self.m_req.nb_scenario
        gpu_req.max_inventory = self.m_req.max_inventory
        gpu_req.inventory_cost = self.m_req.inventory_cost
        gpu_req.stockout_cost = self.m_req.stockout_cost
        gpu_req.supplier_minus_cost = self.m_req.supplier_minus_cost

        # 处理每个场景，只保留2~k天的数据
        for scenario in self.m_req.scenario_infos:
            new_scenario = irp_pb2.ScenarioInfo()
            new_scenario.scenario_id = scenario.scenario_id
            new_scenario.daily_demands.extend(scenario.daily_demands[1:])  # 去掉第0天
            new_scenario.daily_delivery_costs.extend(scenario.daily_delivery_costs[1:])
            new_scenario.daily_capa_penalty_costs.extend(scenario.daily_capa_penalty_costs[1:])
            gpu_req.scenario_infos.append(new_scenario)

        # 第一天送/不送 - (创建两个独立的请求-废弃） 把两个请求合成一个 只是scenario数量翻倍 前一半是配送 后一半是不配送
        # gpu_day_1_with_delivery_req = irp_pb2.OuReq()
        # gpu_day_1_with_delivery_req.CopyFrom(gpu_req)
        
        # gpu_day_1_no_delivery_req = irp_pb2.OuReq()
        # gpu_day_1_no_delivery_req.CopyFrom(gpu_req)
        temp_scenario_infos1 = []
        temp_scenario_infos2 = []
        
        # 处理第一天送货的情况
        for scenario_id, scenario_info in enumerate(gpu_req.scenario_infos):
            # 处理第一天送货的情况
            temp_scenario_info1 = irp_pb2.ScenarioInfo()
            temp_scenario_info1.CopyFrom(scenario_info)
            temp_scenario_info1.start_inventory = int(self.m_req.max_inventory - self.m_req.scenario_infos[scenario_id].daily_demands[0])
            temp_scenario_info1.daily_plans.append(True)
            temp_scenario_info1.daily_quantities.append(self.m_req.max_inventory - self.m_req.start_inventory)
            temp_scenario_info1.day_1_cost = 0.0
            temp_scenario_info1.day_1_cost += self.m_req.inventory_cost * temp_scenario_info1.start_inventory # 第1天库存成本
            temp_scenario_info1.day_1_cost += self.m_req.scenario_infos[scenario_id].daily_delivery_costs[0] # 第1天配送成本
            temp_scenario_info1.day_1_cost += self.get_cost(self.m_req.scenario_infos[scenario_id].daily_capa_penalty_costs[0], temp_scenario_info1.daily_quantities[0]) # 第1天容量惩罚成本
            temp_scenario_info1.total_cost = temp_scenario_info1.day_1_cost
            temp_scenario_infos1.append(temp_scenario_info1)
            
            # 处理第一天不送货的情况
            temp_scenario_info2 = irp_pb2.ScenarioInfo()
            temp_scenario_info2.CopyFrom(scenario_info)
            temp_scenario_id = scenario_id + self.m_req.nb_scenario # 区分
            temp_scenario_info2.start_inventory = int(max(0.0, self.m_req.start_inventory - self.m_req.scenario_infos[scenario_id].daily_demands[0]))
            temp_scenario_info2.daily_plans.append(False)
            temp_scenario_info2.daily_quantities.append(0.0)
            temp_scenario_info2.day_1_cost = 0.0
            temp_scenario_info2.day_1_cost += self.m_req.inventory_cost * temp_scenario_info2.start_inventory # 第1天库存成本
            temp_scenario_info2.day_1_cost += self.m_req.stockout_cost * max(0.0, self.m_req.scenario_infos[scenario_id].daily_demands[0] - self.m_req.start_inventory) # 第1天缺货成本
            temp_scenario_info2.total_cost = temp_scenario_info2.day_1_cost
            temp_scenario_infos2.append(temp_scenario_info2)
        
        # 把gpu的替换为temp_scenario_infos1和temp_scenario_infos2 scenario数量翻倍
        gpu_req.scenario_infos.clear()
        gpu_req.scenario_infos.extend(temp_scenario_infos1)
        gpu_req.scenario_infos.extend(temp_scenario_infos2)
        gpu_req.control_day_1 = 0
        gpu_req.nb_scenario = len(gpu_req.scenario_infos) # double
        
        # 把两个请求合成一个 只是scenario数量翻倍 前一半是配送 后一半是不配送
        # ------------------------------
        # 在这里接入gpu
        # 把pb转成dict 
        gpu_req_dict = protobuf_to_dict(gpu_req)
        # 这里调用gpu函数 注意 每一个scenario的start_inventory不一样 要进去拿; 传入的场景数量是double的但gpu不需要感知 求解就行
        total_cost_after_day_1 = [0.0] * gpu_req.nb_scenario # gpu_req.nb_scenario是两倍的nb_scenario 前一半是配送 后一半是不配送
        daily_plans_after_day_1 = [[]] * gpu_req.nb_scenario
        daily_quantities_after_day_1 = [[]] * gpu_req.nb_scenario
        # ------------------------------
        # 不需要使用path 我直接把dict作为参数传进去 run(path) 改为 gpu_run(gpu_req_dict) 里面那一步读数据的操作就不用了
        # gpu_run(gpu_req_dict)
        # return costs, flags, quantities, num 
        # 后面写入三个结果 前一半是第一天配送 后一半是第一天不配送
        # 我理解 用起来大概是这样
        # ------------------------------
        total_cost_after_day_1, daily_plans_after_day_1, daily_quantities_after_day_1, _ = gpu_run(gpu_req_dict)
        # ------------------------------
        # 完成接入gpu 下面开始写入结果

        # 选择成本更低的方案
        day_1_with_delivery_total_cost = [0.0] * self.m_req.nb_scenario
        day_1_no_delivery_total_cost = [0.0] * self.m_req.nb_scenario
        # 同时提取送/不配送的信息 并更新totalcost
        for scenario_id in range(self.m_req.nb_scenario):
            day_1_with_delivery_total_cost[scenario_id] = total_cost_after_day_1[scenario_id]
            day_1_with_delivery_total_cost[scenario_id] += gpu_req.scenario_infos[scenario_id].day_1_cost
            day_1_no_delivery_total_cost[scenario_id] = total_cost_after_day_1[scenario_id + self.m_req.nb_scenario]
            day_1_no_delivery_total_cost[scenario_id] += gpu_req.scenario_infos[scenario_id + self.m_req.nb_scenario].day_1_cost

        # 合并结果
        result_resp = irp_pb2.OuResp()
        # 先选择总成本小的 再把第一天的plan和quantity先写入 再写剩下的 并更新totalcost
        if sum(day_1_with_delivery_total_cost) < sum(day_1_no_delivery_total_cost):
            # 复制场景信息
            for s in range(self.m_req.nb_scenario):
                new_scenario = irp_pb2.ScenarioInfo()
                new_scenario.CopyFrom(temp_scenario_infos1[s])
                result_resp.scenario_info_results.append(new_scenario)
            
            for scenario_id in range(self.m_req.nb_scenario):
                # 不要重复添加，因为求解器返回的结果已经包含了第1天的数据
                result_resp.scenario_info_results[scenario_id].daily_plans.extend(daily_plans_after_day_1[scenario_id])
                result_resp.scenario_info_results[scenario_id].daily_quantities.extend(daily_quantities_after_day_1[scenario_id])
                result_resp.scenario_info_results[scenario_id].total_cost = day_1_with_delivery_total_cost[scenario_id]
        else:
            # 复制场景信息
            for s in range(self.m_req.nb_scenario):
                new_scenario = irp_pb2.ScenarioInfo()
                new_scenario.CopyFrom(temp_scenario_infos2[s])
                new_scenario.scenario_id = s
                result_resp.scenario_info_results.append(new_scenario)
            
            for scenario_id in range(self.m_req.nb_scenario):
                # 不要重复添加，因为求解器返回的结果已经包含了第1天的数据
                result_resp.scenario_info_results[scenario_id].daily_plans.extend(daily_plans_after_day_1[scenario_id + self.m_req.nb_scenario])
                result_resp.scenario_info_results[scenario_id].daily_quantities.extend(daily_quantities_after_day_1[scenario_id + self.m_req.nb_scenario])
                result_resp.scenario_info_results[scenario_id].total_cost = day_1_no_delivery_total_cost[scenario_id]

        # 复制结果到响应
        for scenario_info in result_resp.scenario_info_results:
            new_scenario = irp_pb2.ScenarioInfo()
            new_scenario.CopyFrom(scenario_info)
            self.m_resp.scenario_info_results.append(new_scenario)

        return


class IrpServiceServicer(irp_pb2_grpc.IrpServiceServicer):
    """实现 IrpService 的服务器端逻辑"""
    
    def ProcessOptimization(self, request, context):
        """处理优化请求"""
        use_gpu = True
        traces = False
        if traces:
            logger.info(f"收到优化请求: client_id={request.client_id}, nb_days={request.nb_days}, nb_scenario={request.nb_scenario}")
            logger.info(f"收到的 scenario_infos 数量: {len(request.scenario_infos)}")
        
        try:
            # 打印请求的JSON格式
            if traces:
                logger.info("=== 请求数据 (JSON格式) ===")
                request_dict = protobuf_to_dict(request)
                logger.info(json.dumps(request_dict, indent=2, ensure_ascii=False))
                print(json.dumps(request_dict, indent=2, ensure_ascii=False))
            # input()
            
            # 创建响应对象
            response = irp_pb2.OuResp()
            
            # 创建OURemoteSolver并求解
            if use_gpu:
                solver = OURemoteSolver(request, response)
                solver.solve_with_gpu()
            else:
                solver = OURemoteSolver(request, response)
                solver.solve()
            
            if traces:
                logger.info(f"处理完成，返回 {len(response.scenario_info_results)} 个场景结果")
            
            # 打印响应的JSON格式
            if traces:
                logger.info("=== 响应数据 (JSON格式) ===")
                response_dict = protobuf_to_dict(response)
                logger.info(json.dumps(response_dict, indent=2, ensure_ascii=False))
            # input()
            # 打印一些调试信息
            for i, scenario in enumerate(response.scenario_info_results):
                if traces:
                    logger.info(f"场景 {i+1}: total_cost={scenario.total_cost}, "
                              f"daily_plans={scenario.daily_plans}, "
                              f"daily_quantities={scenario.daily_quantities}")
            
            return response
            
        except Exception as e:
            logger.error(f"处理请求时发生错误: {e}")
            context.set_code(grpc.StatusCode.INTERNAL)
            context.set_details(f"服务器内部错误: {str(e)}")
            return irp_pb2.OuResp()


def serve():
    """启动 gRPC 服务器"""
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    irp_pb2_grpc.add_IrpServiceServicer_to_server(IrpServiceServicer(), server)
    
    # 监听端口
    listen_addr = '[::]:50051'
    server.add_insecure_port(listen_addr)
    
    logger.info(f"启动 gRPC 服务器，监听地址: {listen_addr}")
    server.start()
    
    try:
        # 保持服务器运行
        while True:
            time.sleep(86400)  # 24小时
    except KeyboardInterrupt:
        logger.info("收到中断信号，正在关闭服务器...")
        server.stop(0)
        logger.info("服务器已关闭")


if __name__ == '__main__':
    serve() 