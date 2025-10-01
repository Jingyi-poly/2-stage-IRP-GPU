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

    // 创建测试请求
    OuReq CreateTestRequest() {
        OuReq request;
        
        // 设置基本参数
        request.set_client_id(123);
        request.set_nb_days(5);
        request.set_nb_scenario(1);
        request.set_max_inventory(1000);
        request.set_start_inventory(100);
        request.set_inventory_cost(5.0);
        request.set_stockout_cost(50.0);
        request.set_supplier_minus_cost(10.0);
        request.set_control_day_1(1);
        
        // 设置 ControlDayOne
        ControlDayOne* control_day_one = request.mutable_control_day_1_info();
        control_day_one->set_day_1_delivery(true);
        control_day_one->set_day_1_cost(200.0);
        
        // 创建多个场景
        for (int i = 0; i < 5; ++i) {
            ScenarioInfo* scenario = request.add_scenario_infos();
            scenario->set_scenario_id(1);
            
            // 设置每日需求量
            scenario->add_daily_demands(50.0);
            scenario->add_daily_demands(60.0);
            scenario->add_daily_demands(70.0);
            scenario->add_daily_demands(80.0);
            scenario->add_daily_demands(90.0);
            
            // 设置每日配送成本
            scenario->add_daily_delivery_costs(10.0);
            scenario->add_daily_delivery_costs(12.0);
            scenario->add_daily_delivery_costs(15.0);
            scenario->add_daily_delivery_costs(18.0);
            scenario->add_daily_delivery_costs(20.0);
            
            // 设置每日超载惩罚成本
            for (int j = 0; j < 5; ++j) {
                CapaCostFunc* capa_cost = scenario->add_daily_capa_penalty_costs();
                capa_cost->set_threshold(100.0);
                capa_cost->set_cost(50.0);
                capa_cost->set_slope(0.5);
                capa_cost->set_intercept(10.0);
            }
            
            scenario->set_day_1_cost(200.0);
            scenario->set_total_cost(500.0);
            
            // 设置每日计划
            scenario->add_daily_plans(true);
            scenario->add_daily_plans(false);
            scenario->add_daily_plans(true);
            scenario->add_daily_plans(false);
            scenario->add_daily_plans(true);
            
            // 设置每日数量
            scenario->add_daily_quantities(55.0);
            scenario->add_daily_quantities(0.0);
            scenario->add_daily_quantities(75.0);
            scenario->add_daily_quantities(0.0);
            scenario->add_daily_quantities(95.0);
        }
        
        return request;
    }

    // 调用 ProcessOptimization
    void ProcessOptimization() {
        OuReq request = CreateTestRequest();
        OuResp response;
        ClientContext context;

        std::cout << "发送测试请求..." << std::endl;

        Status status = stub_->ProcessOptimization(&context, request, &response);

        if (status.ok()) {
            std::cout << "收到服务器响应:" << std::endl;
            std::cout << "场景结果数量: " << response.scenario_info_results_size() << std::endl;
            
            for (int i = 0; i < response.scenario_info_results_size(); ++i) {
                const ScenarioInfo& scenario = response.scenario_info_results(i);
                std::cout << "场景 " << (i + 1) << ":" << std::endl;
                std::cout << "  - 场景ID: " << scenario.scenario_id() << std::endl;
                std::cout << "  - 总成本: " << scenario.total_cost() << std::endl;
                
                std::cout << "  - 每日计划: [";
                for (int j = 0; j < scenario.daily_plans_size(); ++j) {
                    if (j > 0) std::cout << ", ";
                    std::cout << (scenario.daily_plans(j) ? "true" : "false");
                }
                std::cout << "]" << std::endl;
                
                std::cout << "  - 每日数量: [";
                for (int j = 0; j < scenario.daily_quantities_size(); ++j) {
                    if (j > 0) std::cout << ", ";
                    std::cout << scenario.daily_quantities(j);
                }
                std::cout << "]" << std::endl;
            }
            
            std::cout << "测试成功完成！" << std::endl;
        } else {
            std::cout << "gRPC 错误: " << status.error_code() << ": " << status.error_message() << std::endl;
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