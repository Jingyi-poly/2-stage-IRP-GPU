#include "MatrixSolver.h"

MatrixSolver::MatrixSolver(Params* params, int clientId, vector<Noeud*> noeudTravails) 
    : params(params), clientId(clientId), noeudTravails(noeudTravails) {
    client = params->cli[clientId]; // 获取client，即当前dp处理的client，仅此一个
    T = params->ancienNbDays; // 获取T，即总天数
    d = client .dailyDemand; // 获取dailyDemand，即每天的需求量，是一个长度为T的vector
    i_max = client.maxInventory; // 获取maxInventory，即最大库存量，是一个常数，不随着scenario变化
}

// 获取insertionInfo，即插入信息，其中cost包含了detour和capacityPenalty
// 其中detour是插入点到配送点的距离，capacityPenalty是插入点容量超过车辆承载的惩罚
// 可以把这个函数当作提前计算好的函数，只要输入第几天、配送量，就可以返回成本（只有成本是dp计算需要的，其他是为了外部框架）
// 我还写了一个提前计算的版本，在每一次solver创建的时候，会先提前计算每一天每一个quantity对应cost，但是写了之后严重影响速度（做了太多无用计算）（GPU可能不怕）
InsertionRes MatrixSolver::getInsertionInfo(
    Client client, int day, vector<Noeud*> noeudTravails, double quantity) {
    
    double cost = INFINITY;
    double load = 0;
    Noeud* place = nullptr;
    Noeud* nodetravail = noeudTravails[day];
    for(int i = 0; i < nodetravail->allInsertions.size(); i++) {
        auto& insertion = nodetravail->allInsertions[i];
        double pre_load = - insertion.load + quantity;
        double post_load = - insertion.load;
        if(eq(pre_load,0)) pre_load = 0;
        if(eq(post_load,0)) post_load = 0;
        double capacityPenaltyCost = params->penalityCapa * (std::max<double>(0., pre_load) + std::max<double>(0., post_load));
        double totalCost = insertion.detour + capacityPenaltyCost;
        if(totalCost < cost) {
            cost = totalCost;
            load = insertion.load;
            place = insertion.place;
        }
    }
    return {cost, quantity, place};
}

Solution MatrixSolver::solve() {
    bool traces = false;
    int startInventory = client.startingInventory;
    // C_prev是前一天的cost，是一个长度为i_max+1的vector（index从0开始，库存为0； index=i_max,库存为i_max）
    // 目前为了方便阅读，vector的每一个元素是一个Cost结构体，包含fromC, fromL, fromF, pointer（这样的设计对dp求出最终cost没有帮助，只是为了在求出cost之后，倒推每一步的配送方案）
    // 在真正实现的时候，可以把元素为结构体（含四个数字）的矩阵替换为四个同样大小的元素为数字的矩阵（吗？）（有没有节约空间的方式）
    vector<Cost> C_prev(i_max+1, {INFINITY, INFINITY, INFINITY}); // 初始化C_prev，长度为i_max+1，初始值为无穷大，其中只有startInventory位置为0，代表只有初始库存的位置是可行的
    C_prev[startInventory] = {0., 0., 0.};

    // 以下两个vector和dp计算cost无关，只是为了在求出cost之后，倒推每一步的配送方案
    // 其中dayRes记录了每一天的C向量（或者说是f向量，在OU中其实是一个东西），dayPlan记录了每一天的如果配送的话，最佳的方案是哪个（也就是如果选择配送，一定选择这个方案，回溯时候用到）
    vector<vector<Cost>> dayRes;
    dayRes.push_back(vector<Cost>(2*i_max+1, {0., 0., 0.})); // 占位，第0个不用
    vector<InsertionRes> dayPlan(T+1, {0., 0., nullptr});

    // 遍历每一天，开始迭代咯
    for(int t = 1; t <= T; t++) {
        // f矩阵在OU policy中退化成向量：只有送或者不送，如果送的话一天结束时候的库存一定是确定的，即maxInventory-d[t]，因此对应点就是配送时候的成本；如果不送，一天结束时候库存量对应index有cost
        // （index从0开始，库存为-i_max；index=i_max,库存为0； index=2*i_max,库存为i_max）
        vector<Cost> f(2 * i_max+1, {INFINITY, INFINITY, INFINITY});
        int offset = i_max - d[t]; // 计算offset，计算C_prev[0]在f中的位置，C_prev向量从哪里开始，可以理解为偏移量（d[t]）(i_max其实是库存量刚好为0的index，0是库存量为-i_max的index)

        Cost f_delivery = {INFINITY, INFINITY, INFINITY}; // 计算配送的f点
        InsertionRes bestInsertion = {INFINITY, 0, nullptr}; // 存储配送情况下的最佳方案

        for(int i = 0; i <= i_max; i++) {
            if (C_prev[i].fromC == INFINITY) {
                continue; // 如果C_prev[i]是无穷大，说明这个库存量是不可能的，跳过，极大节约时间
            }
            f[offset + i].fromC = C_prev[i].fromC + C_prev[i].fromL + C_prev[i].fromF; // fromC表示前一天的总成本
            f[offset + i].fromL = client.inventoryCost * (i - d[t]); // fromL表示今天结束时库存导致的储存成本
            f[offset + i].fromF = 0.; // fromF表示今天配送的成本
            f[offset + i].pointer = i+i_max; // pointer表示前一天的库存量，也可以理解为前一天的index指针（回溯用到）

            // 计算如果配送 要配送多少 以及cost
            double quantity = i_max - i;
            if(quantity > 0.0001) {
                auto insertion = getInsertionInfo(client, t, noeudTravails, quantity);
                if (f[offset + i].fromC + insertion.cost < f_delivery.fromC + f_delivery.fromF) { // 由于配送之后当天结束的库存确定，不用比较fromL，相当于找到配送情况下的最小成本（对vector内部取min）
                    f_delivery = {f[offset + i].fromC, (i_max - d[t]) * client.inventoryCost, insertion.cost, f[offset + i].pointer};
                    bestInsertion = insertion;
                }
            }
        }

        // 计算配送的f点，库存是确定的，所以直接赋值
        f[2 * i_max - d[t]] = f_delivery;
        dayPlan[t] = bestInsertion;

        // 抵消计算stockout，前面为了方便对于库存为负的部分也是加上库存成本（但会是负数），所以这里进行抵消并加上缺货成本
        vector<double> stockout(2*i_max+1, 0.);
        for(int i = 0; i <= i_max; i++) {
            stockout[i] = (i-i_max) * (client.stockoutCost + client.inventoryCost);
        }
        for(int i = 0; i <= 2*i_max; i++) {
            f[i].fromL -= stockout[i];
        }

        // 计算小于零的最小值，对于所有缺货和库存为0的情况，我们找到最好的方案给库存=0的位置，小于零的不存在，直接扔掉
        Cost f_stockout = {INFINITY, INFINITY, INFINITY};
        for(int i = 0; i <= i_max; i++) {
            if(f[i].fromC + f[i].fromL + f[i].fromF < f_stockout.fromC + f_stockout.fromL + f_stockout.fromF) {
                f_stockout = f[i];
            }
        }
        // 找到最好的方案给库存=0的位置
        f[i_max] = f_stockout;

        // 迭代保存C_prev，只保留大于等于0的
        for(int i = 0; i <= i_max; i++) {
            C_prev[i] = f[i+i_max];
        }

        dayRes.push_back(f);

        if (traces) {
            // 打印f
            cout << "Day " << t << " f: ";
            for(int i = 0; i <= 2*i_max; i++) {
                if (i == i_max || i == 2*i_max-d[t]) {
                    cout << endl;
                }

                if (f[i].fromC != INFINITY) {
                    cout << "|" << f[i].fromC << " " << f[i].fromL << " " << f[i].fromF << "|";
                }
                else {
                    cout << ".";
                }
            }
            cout << endl;
        }
    }

    // 找到最优解
    Cost bestCost = {INFINITY, INFINITY, INFINITY};
    int best_index = -1;
    for(int i = 0; i <= i_max; i++) {
        if(C_prev[i].fromC + C_prev[i].fromL + C_prev[i].fromF < bestCost.fromC + bestCost.fromL + bestCost.fromF) {
            bestCost = C_prev[i];
            best_index = i;
        }
    }

    if (bestCost.fromC == INFINITY) {
        throw std::runtime_error("No solution found in last day! ");
    }

    // 回溯整个计划

    vector<bool> resPlans(T+1, false);
    vector<double> resQuantities(T+1, 0.);
    vector<Noeud*> resPlaces(T+1, nullptr);

    Cost prevCost = bestCost;
    int cur_index = best_index;
    for(int t = T; t >= 1; t--) {
        if (cur_index == 2*i_max-d[t] || prevCost.fromF > 0.0001) {
            resPlans[t] = true;
            resQuantities[t] = dayPlan[t].quantity;
            resPlaces[t] = dayPlan[t].place;
        }
        cur_index = prevCost.pointer;
        prevCost = dayRes[t-1][cur_index];
    }
    // 删除[0]
    resQuantities.erase(resQuantities.begin());
    resPlaces.erase(resPlaces.begin());
    resPlans.erase(resPlans.begin());

    return {bestCost.fromC + bestCost.fromL + bestCost.fromF, resPlans, resQuantities, resPlaces};
}