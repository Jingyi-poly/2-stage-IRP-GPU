#ifndef OUPOLICYSOLVER_H
#define OUPOLICYSOLVER_H

#include <vector>
#include <map>
#include "Params.h"
#include "Noeud.h"
#include "LinearPiece.h"

struct OUPolicyResult {
    double totalCost;
    std::vector<bool> plans;
    std::vector<double> quantities;
    std::vector<Noeud*> places;
};

struct DayResult {
    bool chooseDelivery; // true: 配送, false: 不配送
    Noeud* insertPlace;
    double totalCost;
};

struct OptionInfo {
    double resultInventory;   // 选择该选项后的库存
    double inventoryCost;  // 库存成本
    double shortageCost;   // 缺货成本
    double deliveryCost;   // 配送成本
    double supplierMinusCost; // 供应商减量成本
    double capacityPenaltyCost; // 超载惩罚成本
    double quantity;       // 补货量
    double totalCost;      // 总成本
    double load;
    Noeud* insertPlace;   // 插入位置
};

struct DayInfo {
    double currentInventory;  // 当前库存
    double demand;            // 需求量

    OptionInfo noDelivery; // 不配送选项
    OptionInfo withDelivery; // 配送选项

    bool chooseDelivery;   // 是否选择配送
    double totalCost;      // 最终成本
    Noeud* insertPlace;   // 插入位置
    double resultInventory;    // 最终库存
};

struct CapaCostFunc {
    // 在阈值之前是一个定值，在阈值之后是一个线性函数
    double threshold; // 阈值
    double cost; // 定值
    double slope; // 斜率
    double intercept; // 截距

    double getCost(double load) {
        if (load <= threshold) {
            return cost;
        } else {
            return cost + slope * load + intercept;
        }
    }
};

struct ControlDayOne {
    bool day_1_delivery; 
    double day_1_cost;
};

struct ScenarioInfo {
    int scenario_id; // 场景ID
    std::vector<double> daily_demands;  // 每日需求量
    std::vector<double> daily_delivery_costs; // 每日配送成本
    std::vector<CapaCostFunc> daily_capa_penalty_costs; // 每日超载惩罚成本
    double day_1_cost;
    double total_cost;
    std::vector<bool> daily_plans;
    std::vector<double> daily_quantities;
};

struct OuReq {
    int client_id; // 客户ID(index)
    int nb_days; // 天数
    int nb_scenario; // 场景数
    int max_inventory; // 最大库存
    int start_inventory; // 初始库存
    double inventory_cost; // 库存成本
    double stockout_cost; // 缺货成本
    double supplier_minus_cost; // 供应商减量成本
    int control_day_1; 
    ControlDayOne control_day_1_info; // 控制第一天

    std::vector<ScenarioInfo> scenario_infos;
};

struct OuResp {
    std::vector<ScenarioInfo> scenario_info_results;
};

class OUPolicySolver {
private:
    std::map<std::pair<int, int>, DayResult> memory;
    std::vector<Noeud*> deliveryPlaces;
    Params* params;
    Noeud* client;
    int clientId;
    vector<Noeud*> noeudTravails;

public:
    // 构造函数
    OUPolicySolver(Params* params, Noeud* client, int clientId, vector<Noeud*> noeudTravails) : params(params), client(client), clientId(clientId), noeudTravails(noeudTravails) {}

    // 计算特定日期的最小插入成本
    std::pair<double, std::pair<double, Noeud*>> getInsertionInfo(Noeud* client, int day, vector<Noeud*> noeudTravails, double quantity);

    // OU策略求解函数
    OUPolicyResult solve();

    // 动态规划核心函数
    DayResult OUPolicyDP(int t, int I);
};

struct DayRes {
    bool is_delivery;
    double total_cost;
    double end_inventory;

    // 用于debug
    double inventory_cost;
    double stockout_cost;
    double delivery_cost;
    double supplier_minus_cost;
    double capacity_penalty_cost;
};

class OURemoteSolver {
public:
    OURemoteSolver(const OuReq& req, OuResp& resp) : m_req(req), m_resp(resp) {}

public:
    OuReq m_req;
    OuResp m_resp;
    std::map<std::tuple<int, int, int>, DayRes> map_memory_;

public:
    void Process() {
        // 解析请求
        ParseParams();
        // 求解
        Solve();
        // 解析响应
        FillResp();
        // 返回响应
        return;
    }

    void ParseParams() {
        // 解析请求
    }

    void Solve();

    DayRes SolveDayInventoryScenario(int day, double start_inventory, int scenario_id);

    void FillResp() {
        // 填充响应
    }

    void CheckParams() {
        cout << "m_req.nb_days: " << m_req.nb_days << endl;
        cout << "m_req.nb_scenario: " << m_req.nb_scenario << endl;
        cout << "m_req.start_inventory: " << m_req.start_inventory << endl;
        cout << "m_req.max_inventory: " << m_req.max_inventory << endl;
        cout << "m_req.inventory_cost: " << m_req.inventory_cost << endl;
        cout << "m_req.stockout_cost: " << m_req.stockout_cost << endl;
        cout << "m_req.supplier_minus_cost: " << m_req.supplier_minus_cost << endl;
        cout << "m_req.control_day_1: " << m_req.control_day_1 << endl;
        cout << "m_req.control_day_1_info.day_1_delivery: " << m_req.control_day_1_info.day_1_delivery << endl;
        cout << "m_req.control_day_1_info.day_1_cost: " << m_req.control_day_1_info.day_1_cost << endl;
        cout << "m_req.scenario_infos.size(): " << m_req.scenario_infos.size() << endl;
        for (int s = 0; s < m_req.nb_scenario; ++s) {
            cout << "scenario_id: " << s << " daily_demands: " << m_req.scenario_infos[s].daily_demands.size() << endl;
            cout << "scenario_id: " << s << " daily_delivery_costs: " << m_req.scenario_infos[s].daily_delivery_costs.size() << endl;
            cout << "scenario_id: " << s << " daily_capa_penalty_costs: " << m_req.scenario_infos[s].daily_capa_penalty_costs.size() << endl;
            for (int i = 0; i < m_req.scenario_infos[s].daily_capa_penalty_costs.size(); ++i) {
                cout << "scenario_id: " << s << " daily_capa_penalty_costs[" << i << "].threshold: " << m_req.scenario_infos[s].daily_capa_penalty_costs[i].threshold << endl;
                cout << "scenario_id: " << s << " daily_capa_penalty_costs[" << i << "].cost: " << m_req.scenario_infos[s].daily_capa_penalty_costs[i].cost << endl;
                cout << "scenario_id: " << s << " daily_capa_penalty_costs[" << i << "].slope: " << m_req.scenario_infos[s].daily_capa_penalty_costs[i].slope << endl;
                cout << "scenario_id: " << s << " daily_capa_penalty_costs[" << i << "].intercept: " << m_req.scenario_infos[s].daily_capa_penalty_costs[i].intercept << endl;
            }
            cout << "scenario_id: " << s << " daily_quantities: " << m_req.scenario_infos[s].daily_quantities.size() << endl;
            cin.get();
        }
    }
};

#endif // OUPOLICYSOLVER_H
