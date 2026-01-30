#!/usr/bin/env python3
"""
TestCustomer - forVerify gRPC serverfunction
"""

import grpc
import logging

import irp_pb2
import irp_pb2_grpc

# ConfigurationLog
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


def create_test_request():
  """CreateTestRequest"""
  # Create CapaCostFunc
  
  # Create ControlDayOne
  control_day_one = irp_pb2.ControlDayOne(
    day_1_delivery=True,
    day_1_cost=200.0
  )
  
  # Create OuReq
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
  """TestserverConnection"""
  try:
    # Create gRPC Channel
    with grpc.insecure_channel('localhost:50051') as channel:
 # Createstore/exist
      stub = irp_pb2_grpc.IrpServiceStub(channel)
      
      # CreateTestRequest
      request = create_test_request()
      
      logger.info("Send test request...")
      
      # callserver
      response = stub.ProcessOptimization(request)
      
 logger.info("toserverResponse:")
      logger.info(f"Number of scenario results: {len(response.scenario_info_results)}")
      
      for i, scenario in enumerate(response.scenario_info_results):
        logger.info(f"Scenario {i+1}:")
        logger.info(f" - ScenarioID: {scenario.scenario_id}")
        logger.info(f" - Total cost: {scenario.total_cost}")
        logger.info(f" - Daily plans: {scenario.daily_plans}")
        logger.info(f" - Daily quantities: {scenario.daily_quantities}")
      
      logger.info("Test completed successfully!")
      
  except grpc.RpcError as e:
    logger.error(f"gRPC Error: {e.code()}: {e.details()}")
  except Exception as e:
    logger.error(f"otherError: {e}")


if __name__ == '__main__':
  test_server() 