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

#endif // OUPOLICYSOLVER_H
