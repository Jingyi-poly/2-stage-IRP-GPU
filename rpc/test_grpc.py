#!/usr/bin/env python3
"""
simplegRPCTestscript
"""

import grpc
import irp_pb2
import irp_pb2_grpc
import logging

# ConfigurationLog
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


def test_grpc_connection():
  """TestgRPCConnection"""
  try:
    # Createchannel
    channel = grpc.insecure_channel('localhost:50051')
    stub = irp_pb2_grpc.IrpServiceStub(channel)
    
    # CreateTestRequest
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
    
    # addScenarioInfo
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
    
    logger.info("Send test request...")
    response = stub.ProcessOptimization(request)
    
    logger.info("Received response:")
    logger.info(f"Number of scenario results: {len(response.scenario_info_results)}")
    
    for i, scenario in enumerate(response.scenario_info_results):
      logger.info(f"Scenario {i+1}:")
      logger.info(f" - ScenarioID: {scenario.scenario_id}")
      logger.info(f" - Total cost: {scenario.total_cost}")
      logger.info(f" - Daily plans: {scenario.daily_plans}")
      logger.info(f" - Daily quantities: {scenario.daily_quantities}")
    
    logger.info("Test completed successfully!")
    return True
    
  except grpc.RpcError as e:
    logger.error(f"gRPCError: {e.code()}: {e.details()}")
    return False
  except Exception as e:
    logger.error(f"otherError: {e}")
    return False


if __name__ == '__main__':
  test_grpc_connection() 