#include "ParamsList.h"
#include "commandline.h"
#include <iostream>
#include <random>
#include <cmath>

ParamsList::ParamsList(commandline& c) : commandline_(c) {
    // 从commandline获取场景数量
    nb_scenarios_ = c.get_nb_scenario();
    cout << "nb_scenarios_: " << nb_scenarios_ << endl;
    // 初始化vec_params向量，大小为nb_scenarios_，所有元素初始化为nullptr
    init_params_list();
    init_demand_matrix();
    set_params_demand_matrix();
}

ParamsList::~ParamsList() {
    // 清理所有Params对象
    for (auto* params : vec_params_) {
        if (params != nullptr) {
            delete params;
        }
    }
    delete general_params_;
    vec_params_.clear();
}

void ParamsList::init_params_list() {
    vec_params_.resize(nb_scenarios_, nullptr);
    general_params_ = new Params(
        commandline_.get_path_to_instance(), commandline_.get_path_to_solution(), commandline_.get_type(),
        commandline_.get_nbVeh(), commandline_.get_path_to_BKS(), commandline_.get_seed(), commandline_.get_rou(), commandline_.get_stockout());

    for (int i = 0; i < nb_scenarios_; i++) {
        vec_params_[i] = new Params(
            commandline_.get_path_to_instance(), commandline_.get_path_to_solution(), commandline_.get_type(),
            commandline_.get_nbVeh(), commandline_.get_path_to_BKS(), commandline_.get_seed(), commandline_.get_rou(), commandline_.get_stockout());
    }

    control_day_1_ = 1;

    set_general_params();
}

void ParamsList::set_general_params() {
    // 设置所有场景的通用参数
    if (vec_params_.size() == 0) {
        return;
    }
    general_params_->is_scenario_ = true;
    general_params_->demand_seed = commandline_.get_demand_seed();
    nb_days_ = general_params_->nbDays;
    nb_clients_ = general_params_->nbClients;
    nb_depots_ = general_params_->nbDepots;
    nb_vehicles_ = general_params_->nbVehiculesPerDep;  // 修正变量名
    seed_ = general_params_->seed;
    demand_seed_ = general_params_->demand_seed;
    // cout << "demand_seed_: " << demand_seed_ << endl;
    // cin.get();

    for (int c = 0; c < nb_clients_; c++) {
        vec_client_max_inventory_.push_back(general_params_->cli[c+nb_depots_].maxInventory);
    }
}

void ParamsList::init_demand_matrix() {
    bool trace = true;
	if (nb_scenarios_ == 0) {
		nb_scenarios_ = 1;
	}
	// 先根据seed 伪随机生成一个随机数
	mt19937 rng(demand_seed_);
	
	// 创建简化的分布生成器
	normal_distribution<double> normal_dist(0.0, 1.0);
	uniform_real_distribution<double> uniform_dist(0.0, 1.0);
	
	// 初始化scenario_2_daily_demand_矩阵
	// 维度：nb_scenario_ x nbDays
	vec_scenario_daily_client_demand_.clear();
	vec_scenario_daily_client_demand_.resize(nb_scenarios_);
	for (int s = 0; s < nb_scenarios_; s++) {
		vec_scenario_daily_client_demand_[s].resize(nb_days_ + 1);
		for (int d = 1; d <= nb_days_; d++) {
			vec_scenario_daily_client_demand_[s][d].resize(nb_clients_, 0.0); // +1 因为天数从1开始索引
		}
	}
	
	for (int s = 0; s < nb_scenarios_; s++) {
		for (int d = 1; d <= nb_days_; d++) {
			for (int c = 0; c < nb_clients_; c++) {
				// // 基础正态分布随机数
				// double random_value = normal_dist(rng);
				// // 限制范围在[-2, 2]之间(95%的置信度)
				// if (random_value < -2.0) random_value = -2.0;
				// if (random_value > 2.0) random_value = 2.0;
				
				// 基础需求率 + 随机噪声 + 随机乘数
				double base_demand_rate = 0.45;  // 基础需求率（0.2-0.7的中间值）
				double noise = (uniform_dist(rng) - 0.5) * 0.3;  // 随机噪声：-0.15到0.15
				double random_multiplier = 0.8 + uniform_dist(rng) * 0.4;  // 0.8-1.2的随机乘数
				
				// 组合随机性
				double demand_rate = (base_demand_rate + noise) * random_multiplier;
				
				// 确保需求率在0.2-0.7范围内
				if (demand_rate < 0.2) demand_rate = 0.2;
				if (demand_rate > 0.7) demand_rate = 0.7;
				
                // cout << "demand_rate: " << demand_rate << endl;
				// 根据max_inventory 和 demand_rate 计算出需求 并取整数
				vec_scenario_daily_client_demand_[s][d][c] = round(vec_client_max_inventory_[c] * demand_rate);
			}
		}
	}
    // cin.get();
	
	// 打印情报
	// if (trace) {
	// 	print_demand_matrix();
	// 	cin.get();
	// }
}

void ParamsList::print_demand_matrix() {
    cout << "初始化需求矩阵完成" << endl;
    // 打印每一个client最多前5个场景的每天的需求
    for (int c = 0; c < nb_clients_; c++) {
        cout << "client " << c << " 最大库存:" << vec_client_max_inventory_[c] << " 最多前5个场景的每天的需求:" << endl;
        for (int s = 0; s < nb_scenarios_; s++) {
            cout << "场景 " << s << " 每天的需求:";
            for (int d = 1; d <= nb_days_; d++) {
                cout << vec_scenario_daily_client_demand_[s][d][c] << " ";
            }
            cout << endl;
            if (s == 4) {
                break;
            }
        }
        cout << "--------------------------------" << endl;
    }
    cout << endl;
}

void ParamsList::set_params_demand_matrix() {
    // 设置每个场景的client的daily_demand
    for (int s = 0; s < nb_scenarios_; s++) {
        for (int c = 0; c < nb_clients_; c++) {
            // 获取当前场景和客户的需求数据
            // 注意：客户索引需要加上仓库数量，因为cli向量中前nb_depots_个是仓库
            int client_index = c + nb_depots_;
            
            // 清空原有的dailyDemand并重新设置
            vec_params_[s]->cli[client_index].dailyDemand.clear();
            vec_params_[s]->cli[client_index].dailyDemand.push_back(0.0); // 第0天设为0
            
            // 设置每一天的需求
            for (int d = 1; d <= nb_days_; d++) {
                double demand = vec_scenario_daily_client_demand_[s][d][c];
                vec_params_[s]->cli[client_index].dailyDemand.push_back(demand);
            }
        }
    }
    
    cout << "已为所有场景设置客户需求矩阵" << endl;
}