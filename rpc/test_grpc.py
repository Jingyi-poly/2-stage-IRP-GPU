#!/usr/bin/env python3
"""
简单的gRPC测试脚本
"""

import grpc
import irp_pb2
import irp_pb2_grpc
import logging

# 配置日志
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


def test_grpc_connection():
    """测试gRPC连接"""
    try:
        # 创建channel
        channel = grpc.insecure_channel('localhost:50051')
        stub = irp_pb2_grpc.IrpServiceStub(channel)
        
        # 创建测试请求
        request = irp_pb2.OuReq(
            client_id=123,
            nb_days=3,
            nb_scenario=2,
            max_inventory=1000,
            start_inventory=100,
            inventory_cost=5.0,
            stockout_cost=50.0,
            supplier_minus_cost=10.0,
            control_day_1=0
        )
        
        # 添加场景信息
        for s in range(2):
            scenario = irp_pb2.ScenarioInfo(
                scenario_id=s,
                daily_demands=[50.0, 60.0, 70.0],
                daily_delivery_costs=[10.0, 12.0, 15.0],
                daily_capa_penalty_costs=[
                    irp_pb2.CapaCostFunc(threshold=100.0, cost=50.0, slope=0.5, intercept=10.0),
                    irp_pb2.CapaCostFunc(threshold=100.0, cost=50.0, slope=0.5, intercept=10.0),
                    irp_pb2.CapaCostFunc(threshold=100.0, cost=50.0, slope=0.5, intercept=10.0)
                ],
                day_1_cost=200.0,
                total_cost=500.0,
                daily_plans=[True, False, True],
                daily_quantities=[55.0, 0.0, 75.0]
            )
            request.scenario_infos.append(scenario)
        
        logger.info("发送测试请求...")
        response = stub.ProcessOptimization(request)
        
        logger.info("收到响应:")
        logger.info(f"场景结果数量: {len(response.scenario_info_results)}")
        
        for i, scenario in enumerate(response.scenario_info_results):
            logger.info(f"场景 {i+1}:")
            logger.info(f"  - 场景ID: {scenario.scenario_id}")
            logger.info(f"  - 总成本: {scenario.total_cost}")
            logger.info(f"  - 每日计划: {scenario.daily_plans}")
            logger.info(f"  - 每日数量: {scenario.daily_quantities}")
        
        logger.info("测试成功完成！")
        return True
        
    except grpc.RpcError as e:
        logger.error(f"gRPC错误: {e.code()}: {e.details()}")
        return False
    except Exception as e:
        logger.error(f"其他错误: {e}")
        return False


if __name__ == '__main__':
    test_grpc_connection() 