#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>
#include "irp.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using irp::IrpService;
using irp::OuReq;
using irp::OuResp;
using irp::ScenarioInfo;
using irp::CapaCostFunc;
using irp::ControlDayOne;

class IrpClient {
public:
  IrpClient(std::shared_ptr<Channel> channel)
    : stub_(IrpService::NewStub(channel)) {}

  // CreateTestRequest
  OuReq CreateTestRequest() {
    OuReq request;
    
    // SetbasicParameter
    request.set_client_id(123);
    request.set_nb_days(5);
    request.set_nb_scenario(1);
    request.set_max_inventory(1000);
    request.set_start_inventory(100);
    request.set_inventory_cost(5.0);
    request.set_stockout_cost(50.0);
    request.set_supplier_minus_cost(10.0);
    request.set_control_day_1(1);
    
    // Set ControlDayOne
    ControlDayOne* control_day_one = request.mutable_control_day_1_info();
    control_day_one->set_day_1_delivery(true);
    control_day_one->set_day_1_cost(200.0);
    
    // CreatemultipleScenario
    for (int i = 0; i < 5; ++i) {
      ScenarioInfo* scenario = request.add_scenario_infos();
      scenario->set_scenario_id(1);
      
      // SetDaily demand quantity
      scenario->add_daily_demands(50.0);
      scenario->add_daily_demands(60.0);
      scenario->add_daily_demands(70.0);
      scenario->add_daily_demands(80.0);
      scenario->add_daily_demands(90.0);
      
      // SetDaily delivery cost
      scenario->add_daily_delivery_costs(10.0);
      scenario->add_daily_delivery_costs(12.0);
      scenario->add_daily_delivery_costs(15.0);
      scenario->add_daily_delivery_costs(18.0);
      scenario->add_daily_delivery_costs(20.0);
      
      // SetDaily capacity penalty cost
      for (int j = 0; j < 5; ++j) {
        CapaCostFunc* capa_cost = scenario->add_daily_capa_penalty_costs();
        capa_cost->set_threshold(100.0);
        capa_cost->set_cost(50.0);
        capa_cost->set_slope(0.5);
        capa_cost->set_intercept(10.0);
      }
      
      scenario->set_day_1_cost(200.0);
      scenario->set_total_cost(500.0);
      
      // SetDaily plans
      scenario->add_daily_plans(true);
      scenario->add_daily_plans(false);
      scenario->add_daily_plans(true);
      scenario->add_daily_plans(false);
      scenario->add_daily_plans(true);
      
      // SetDaily quantities
      scenario->add_daily_quantities(55.0);
      scenario->add_daily_quantities(0.0);
      scenario->add_daily_quantities(75.0);
      scenario->add_daily_quantities(0.0);
      scenario->add_daily_quantities(95.0);
    }
    
    return request;
  }

  // call ProcessOptimization
  void ProcessOptimization() {
    OuReq request = CreateTestRequest();
    OuResp response;
    ClientContext context;

    std::cout << "Send test request..." << std::endl;

    Status status = stub_->ProcessOptimization(&context, request, &response);

    if (status.ok()) {
 std::cout << "toserverResponse:" << std::endl;
      std::cout << "Number of scenario results: " << response.scenario_info_results_size() << std::endl;
      
      for (int i = 0; i < response.scenario_info_results_size(); ++i) {
        const ScenarioInfo& scenario = response.scenario_info_results(i);
        std::cout << "Scenario " << (i + 1) << ":" << std::endl;
        std::cout << " - ScenarioID: " << scenario.scenario_id() << std::endl;
        std::cout << " - Total cost: " << scenario.total_cost() << std::endl;
        
        std::cout << " - Daily plans: [";
        for (int j = 0; j < scenario.daily_plans_size(); ++j) {
          if (j > 0) std::cout << ", ";
          std::cout << (scenario.daily_plans(j) ? "true" : "false");
        }
        std::cout << "]" << std::endl;
        
        std::cout << " - Daily quantities: [";
        for (int j = 0; j < scenario.daily_quantities_size(); ++j) {
          if (j > 0) std::cout << ", ";
          std::cout << scenario.daily_quantities(j);
        }
        std::cout << "]" << std::endl;
      }
      
      std::cout << "Test completed successfully!" << std::endl;
    } else {
      std::cout << "gRPC Error: " << status.error_code() << ": " << status.error_message() << std::endl;
    }
  }

private:
  std::unique_ptr<IrpService::Stub> stub_;
};

int main(int argc, char** argv) {
  std::string target_address = "localhost:50051";
  IrpClient client(
    grpc::CreateChannel(target_address, grpc::InsecureChannelCredentials())
  );
  
  client.ProcessOptimization();
  
  return 0;
} 