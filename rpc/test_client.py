#!/usr/bin/env python3
"""
测试客户端 - 用于验证 gRPC 服务器功能
"""

import grpc
import logging

import irp_pb2
import irp_pb2_grpc

# 配置日志
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


def create_test_request():
    """创建测试请求"""
    # 创建 CapaCostFunc
    
    # 创建 ControlDayOne
    control_day_one = irp_pb2.ControlDayOne(
        day_1_delivery=True,
        day_1_cost=200.0
    )
    
    # 创建 OuReq
    request = irp_pb2.OuReq(
        client_id=123,
        nb_days=5,
        nb_scenario=1,
        max_inventory=1000,
        start_inventory=100,
        inventory_cost=5.0,
        stockout_cost=50.0,
        supplier_minus_cost=10.0,
        control_day_1=1,
        control_day_1_info=control_day_one,
        scenario_infos=[irp_pb2.ScenarioInfo(
            scenario_id=1,
            daily_demands=[50.0, 60.0, 70.0, 80.0, 90.0],
            daily_delivery_costs=[10.0, 12.0, 15.0, 18.0, 20.0],
            daily_capa_penalty_costs=[irp_pb2.CapaCostFunc(
                threshold=100.0,
                cost=50.0,
                slope=0.5,
                intercept=10.0
            ) for _ in range(5)],
            day_1_cost=200.0,
            total_cost=500.0,
            daily_plans=[True, False, True, False, True],
            daily_quantities=[55.0, 0.0, 75.0, 0.0, 95.0]
        ) for _ in range(5)]
    )
    
    return request


def test_server():
    """测试服务器连接"""
    try:
        # 创建 gRPC 通道
        with grpc.insecure_channel('localhost:50051') as channel:
            # 创建存根
            stub = irp_pb2_grpc.IrpServiceStub(channel)
            
            # 创建测试请求
            request = create_test_request()
            
            logger.info("发送测试请求...")
            
            # 调用服务器
            response = stub.ProcessOptimization(request)
            
            logger.info("收到服务器响应:")
            logger.info(f"场景结果数量: {len(response.scenario_info_results)}")
            
            for i, scenario in enumerate(response.scenario_info_results):
                logger.info(f"场景 {i+1}:")
                logger.info(f"  - 场景ID: {scenario.scenario_id}")
                logger.info(f"  - 总成本: {scenario.total_cost}")
                logger.info(f"  - 每日计划: {scenario.daily_plans}")
                logger.info(f"  - 每日数量: {scenario.daily_quantities}")
            
            logger.info("测试成功完成！")
            
    except grpc.RpcError as e:
        logger.error(f"gRPC 错误: {e.code()}: {e.details()}")
    except Exception as e:
        logger.error(f"其他错误: {e}")


if __name__ == '__main__':
    test_server() 