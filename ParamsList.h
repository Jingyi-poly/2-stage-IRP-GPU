#ifndef PARAMSLIST_H
#define PARAMSLIST_H

#include "Params.h"
#include "commandline.h"
#include <vector>

class ParamsList {
public:
    std::vector<Params*> vec_params_;  // 存储nb_scenario个数的Params类
    commandline& commandline_;
    Params* general_params_;
    uint64_t nb_days_;
    uint64_t nb_clients_;
    uint64_t nb_depots_;
    uint64_t nb_vehicles_;
    uint64_t nb_scenarios_;
    uint64_t seed_;
    uint64_t demand_seed_;
    std::vector<double> vec_client_max_inventory_;

    // 一个三维数据 用来存储每个场景的 每一天的 每个客户的 需求
    std::vector<std::vector<std::vector<double>>> vec_scenario_daily_client_demand_;

    // 其他参数
    int control_day_1_;

public:
    // 构造函数
    ParamsList(commandline& c);
    // 析构函数
    ~ParamsList();

private:
    void init_params_list();
    void init_demand_matrix();
    void set_general_params();
    void print_demand_matrix();
    void set_params_demand_matrix();
};

#endif // PARAMSLIST_H 