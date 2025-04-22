#include "MatrixSolver.h"

MatrixSolver::MatrixSolver(Params* params, int clientId, vector<Noeud*> noeudTravails) 
    : params(params), clientId(clientId), noeudTravails(noeudTravails) {
    client = params->cli[clientId]; 
    T = params->ancienNbDays; 
    d = client.dailyDemand; 
    i_max = client.maxInventory; 
}


InsertPlan MatrixSolver::getInsertionInfo(
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
    
    vector<double> C_prev(2*i_max+1, INFINITY); 
    C_prev[startInventory+i_max] = 0.;

    
    vector<vector<double>> dailyCosts;
    dailyCosts.push_back(vector<double>(2*i_max+1, -1.)); 
    vector<vector<int>> dailyPointers;
    dailyPointers.push_back(vector<int>(2*i_max+1, 0));

    vector<InsertPlan> dailyPlans(T+1, {0., 0., nullptr});

    vector<double> C_inventory(2*i_max+1, 0.);
    for(int i = 0; i <= 2*i_max; i++) {
        C_inventory[i] = (i-i_max) * client.inventoryCost;
    }
    vector<double> C_stockout(2*i_max+1, 0.);
    for(int i = 0; i <= i_max; i++) {
        C_stockout[i] = -1 * (i-i_max) * (client.stockoutCost + client.inventoryCost);
    }

    for(int t = 1; t <= T; t++) {
        vector<double> C_cur(2 * i_max+1, INFINITY);
        vector<int> prevPointers(2 * i_max+1, -1);

        double C_delivery = INFINITY;
        int delivery_pointer = 2 * i_max - d[t];
        InsertPlan minCostDeliveryPlan = {INFINITY, 0, nullptr}; 

        int offset = i_max - d[t]; 
        for(int i = i_max; i <= 2 * i_max; i++) {
            if (C_prev[i] == INFINITY) {
                continue; 
            }
            C_cur[i-d[t]] = C_prev[i]; 
            prevPointers[i-d[t]] = i; 
            
            double quantity = 2*i_max - i;
            if(quantity > 0.0001) {
                auto plan = getInsertionInfo(client, t, noeudTravails, quantity);
                double C_detour = plan.cost;
                if (C_prev[i] + C_detour < C_delivery) { 
                    C_delivery = C_prev[i] + C_detour;
                    delivery_pointer = i;
                    minCostDeliveryPlan = plan;
                }
            }
        }

        C_cur[2 * i_max - d[t]] = C_delivery;
        prevPointers[2 * i_max - d[t]] = delivery_pointer;
        dailyPlans[t] = minCostDeliveryPlan;

        for(int i = 0; i <= 2*i_max; i++) {
            C_cur[i] += C_inventory[i]+C_stockout[i];
        }

        double C_zero = INFINITY;
        int zero_pointer = 0;
        for(int i = 0; i <= i_max; i++) {
            if(C_cur[i] < C_zero) {
                C_zero = C_cur[i];
                zero_pointer = prevPointers[i];
            }
        }
        C_cur[i_max] = C_zero;
        prevPointers[i_max] = zero_pointer;

        for(int i = 0; i <= 2*i_max; i++) {
            C_prev[i] = C_cur[i];
        }

        dailyCosts.push_back(C_cur);
        dailyPointers.push_back(prevPointers);

        if (traces) {
            cout << "Day " << t << " d: " << d[t] << " f: ";
            for(int i = 0; i <= 2*i_max; i++) {
                if (i == i_max || i == 2*i_max-d[t]) {
                    cout << endl;
                }

                if (C_cur[i] != INFINITY) {
                    cout << "|" << i-i_max << "|" << C_cur[i] << ", " << dailyCosts[t-1][prevPointers[i]] << "|";
                }
                else {
                    cout << ".";
                }
            }
            cout << endl;
        }
    }

    double bestCostLastDay = INFINITY;
    int best_index = -1;
    for(int i = i_max; i <= 2*i_max; i++) {
        if(C_prev[i] < bestCostLastDay) {
            bestCostLastDay = C_prev[i];
            best_index = i;
        }
    }

    if (bestCostLastDay == INFINITY) {
        throw std::runtime_error("No solution found in last day! ");
    }

    vector<bool> resPlans(T+1, false);
    vector<double> resQuantities(T+1, 0.);
    vector<Noeud*> resPlaces(T+1, nullptr);

    double prevCost = bestCostLastDay;
    int cur_index = best_index;
    for(int t = T; t >= 1; t--) {
        // cout << "Day " << t << " cur_index: " << cur_index << " prevCost: " << prevCost << endl;
        if (cur_index == 2*i_max-d[t]) {
            resPlans[t] = true;
            resQuantities[t] = dailyPlans[t].quantity;
            resPlaces[t] = dailyPlans[t].place;
        }
        cur_index = dailyPointers[t][cur_index];
        prevCost = dailyCosts[t-1][cur_index];
    }
    resQuantities.erase(resQuantities.begin());
    resPlaces.erase(resPlaces.begin());
    resPlans.erase(resPlans.begin());

    return {bestCostLastDay, resPlans, resQuantities, resPlaces};
}
