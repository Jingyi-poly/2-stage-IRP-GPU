#include "OUPolicySolver.h"

std::pair<double, std::pair<double, Noeud*>> OUPolicySolver::getInsertionInfo(Noeud* client, int day, vector<Noeud*> noeudTravails, double quantity) {
    // 从预先计算的插入信息中，选择成本最低的插入信息（插入信息中包含cost、load、place）
    double cost = INT_MAX;
    double load = 0;
    Noeud* place = nullptr;
    Noeud* nodetravail = noeudTravails[day];
    for(int i = 0; i < nodetravail->allInsertions.size(); i++) {
        auto& insertion = nodetravail->allInsertions[i];

        double pre_load = - insertion.load + quantity;
        double post_load = - insertion.load;
        if(eq(pre_load,0)) pre_load = 0;
        if(eq(post_load,0)) post_load = 0;
        // if (pre_load > 0 || post_load > 0) {
        //     cout << "pre_load: " << pre_load << " post_load: " << post_load << endl;
        //     cout << "penalityCapa: " << params->penalityCapa << endl;
        //     cin.get();
        // }
        double capacityPenaltyCost = params->penalityCapa * (std::max<double>(0., pre_load) + std::max<double>(0., post_load));

        double totalCost = insertion.detour + capacityPenaltyCost;

        if(totalCost < cost) {
            cost = totalCost;
            load = insertion.load;
            place = insertion.place;
        }
    }
    return make_pair(cost, make_pair(load, place));
}

OUPolicyResult OUPolicySolver::solve() {
    // 解决子问题，调用OUPolicyDP，用memory存储结果
    memory.clear();
    bool traces = false;

    DayResult result = OUPolicyDP(1, params->cli[clientId].startingInventory);
    double resultCost = result.totalCost;

    //追溯整个流程
    vector<bool> resultPlans(params->ancienNbDays + 1, false);
    vector<double> resultQuantities(params->ancienNbDays + 1, 0);
    vector<Noeud*> resultPlaces(params->ancienNbDays + 1, nullptr);

    int t = 1;
    double I = params->cli[clientId].startingInventory;
    double prevI = I;
    while (t <= params->ancienNbDays) {
        auto result = memory[{t, I}];
        prevI = I;
        resultPlans[t - 1] = result.chooseDelivery;
        if (result.chooseDelivery) {
            resultQuantities[t - 1] = params->cli[clientId].maxInventory - I;
            resultPlaces[t - 1] = result.insertPlace;
            I = std::max<double>(0., params->cli[clientId].maxInventory - params->cli[clientId].dailyDemand[t]);
        } else {
            I = std::max<double>(0., I - params->cli[clientId].dailyDemand[t]);
        }
        t++;
        if (traces) {
            cout << "day: " << t << " I: " << I << " chooseDelivery: " << result.chooseDelivery << " resultInventory: " << I << " totalCost: " << result.totalCost << endl;
        }
    }
    memory.clear();
    return {resultCost, resultPlans, resultQuantities, resultPlaces};
}

DayResult OUPolicySolver::OUPolicyDP(int t, int I) {
    // 动态规划的递归函数，分情况讨论并储存结果
    bool traces = false;
    if (traces) {
        cout << "(t, I) = (" << t << ", " << I << ")" << endl;
    }
    if (memory.find({t, I}) != memory.end()) {
        if (traces) {
            cout << "use memory" << endl;
        }
        return memory[{t, I}];
    }

    auto& clientParams = params->cli[clientId];

    if (t > params->ancienNbDays) {
        if (traces) {
            cout << "reach the end of planning period" << endl;
        }
        DayResult emptyResult;
        emptyResult.totalCost = 0.0;
        emptyResult.insertPlace = nullptr;
        emptyResult.chooseDelivery = false;
        return emptyResult;
    }
    DayInfo todayInfo;
    // 记录天数、库存、需求
    todayInfo.currentInventory = I;
    todayInfo.demand = clientParams.dailyDemand[t];
    todayInfo.totalCost = INT_MAX;

    // 不配送选项
    double new_inventory = I - clientParams.dailyDemand[t];

    // adding inventory cost
    todayInfo.noDelivery.inventoryCost = clientParams.inventoryCost * std::max<double>(0., new_inventory);
    // stockout cost
    todayInfo.noDelivery.shortageCost = clientParams.stockoutCost * std::max<double>(0., -new_inventory);
    new_inventory = std::max<double>(0., new_inventory);
    // supplier minus cost = 0
    todayInfo.noDelivery.supplierMinusCost = 0;
    // delivery cost = 0
    todayInfo.noDelivery.deliveryCost = 0;
    // capacity penalty cost = 0
    todayInfo.noDelivery.capacityPenaltyCost = 0;
    // total先不计算future的cost，通过剪枝筛选后，再加上future的cost
    todayInfo.noDelivery.totalCost = todayInfo.noDelivery.inventoryCost + todayInfo.noDelivery.shortageCost;
    todayInfo.noDelivery.resultInventory = new_inventory;
    
    // ================================================
    // 配送选项，计算配送成本以及插入位置
    new_inventory = clientParams.maxInventory - clientParams.dailyDemand[t];
    double quantity = clientParams.maxInventory - I;
    todayInfo.withDelivery.quantity = quantity;

    // adding inventory cost
    todayInfo.withDelivery.inventoryCost = clientParams.inventoryCost * std::max<double>(0., new_inventory);
    // stockout cost
    todayInfo.withDelivery.shortageCost = clientParams.stockoutCost * std::max<double>(0., -new_inventory);
    new_inventory = std::max<double>(0., new_inventory);
    // supplier minus cost
    todayInfo.withDelivery.supplierMinusCost = params->inventoryCostSupplier * (double)(params->ancienNbDays - t + 1) * quantity;
    // delivery cost 返回值为两层pair，第一层为成本，第二层为place和load
    auto delivery_plan = getInsertionInfo(client, t, noeudTravails, quantity);
    todayInfo.withDelivery.deliveryCost = delivery_plan.first;
    todayInfo.withDelivery.load = delivery_plan.second.first;
    todayInfo.withDelivery.insertPlace = delivery_plan.second.second;
    todayInfo.withDelivery.totalCost = todayInfo.withDelivery.deliveryCost + 
                                        todayInfo.withDelivery.inventoryCost + 
                                        todayInfo.withDelivery.shortageCost - 
                                        todayInfo.withDelivery.supplierMinusCost;
    // total先不计算future的cost，通过剪枝筛选后，再加上future的cost
    todayInfo.withDelivery.resultInventory = new_inventory;

    // 计算future的cost
    DayResult futureResult;
    futureResult = OUPolicyDP(t + 1, std::max<double>(0., todayInfo.noDelivery.resultInventory));
    todayInfo.noDelivery.totalCost += futureResult.totalCost;
    futureResult = OUPolicyDP(t + 1, std::max<double>(0., todayInfo.withDelivery.resultInventory));
    todayInfo.withDelivery.totalCost += futureResult.totalCost;


    // 比较，选择成本更低的选项
    DayResult result;
    if (todayInfo.withDelivery.totalCost < todayInfo.noDelivery.totalCost) {
        result.totalCost = todayInfo.withDelivery.totalCost;
        result.insertPlace = todayInfo.withDelivery.insertPlace;
        result.chooseDelivery = true;
    } else {
        result.totalCost = todayInfo.noDelivery.totalCost;
        result.insertPlace = nullptr;
        result.chooseDelivery = false;
    }
    
    memory[{t, I}] = result;
    return result;
}