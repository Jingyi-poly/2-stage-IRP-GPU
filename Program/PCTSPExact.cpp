#include "PCTSPExact.h"
#include "gurobi_c++.h"
#include <chrono>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <queue>
#include <cmath>

PCTSPExact::PCTSPExact(const Params & params, double timeLimit, bool verbose)
    : params(params), timeLimit(timeLimit), verbose(verbose),
      n(params.nbClients + 1)
{}

// ──────────────────────────────────────────────────────────
// Split DP for a single scenario
// ──────────────────────────────────────────────────────────
double PCTSPExact::splitDP(const std::vector<int> & perm, int scenIdx) const
{
    int sz = (int)perm.size();
    if (sz == 0) return 0.0;

    const double cap = params.vehicleCapacity;
    const double penCap = params.penaltyCapacity;
    const double INF = 1e30;

    std::vector<double> cost(sz + 1, INF);
    cost[0] = 0.0;

    for (int j = 1; j <= sz; j++) {
        double load = 0.0;
        double routeDist = 0.0;
        for (int i = j; i >= 1; i--) {
            int cli = perm[i - 1];
            load += params.cli[cli].demands_scenarios[scenIdx];
            if (i == j)
                routeDist = params.timeCost[cli][0];
            else
                routeDist += params.timeCost[cli][perm[i]];
            double fullDist = params.timeCost[0][cli] + routeDist;
            double excess = std::max(0.0, load - cap);
            double segCost = fullDist + penCap * excess;
            if (cost[i - 1] + segCost < cost[j])
                cost[j] = cost[i - 1] + segCost;
        }
    }
    return cost[sz];
}

// ──────────────────────────────────────────────────────────
// Evaluate a tour stochastically
// ──────────────────────────────────────────────────────────
double PCTSPExact::evaluateTourStochastic(
    const std::vector<int> & tour,
    const std::vector<bool> & visited) const
{
    double skipCost = 0.0;
    for (int c = 1; c < n; c++)
        if (!visited[c])
            skipCost += params.cli[c].skipPenalty;

    if (tour.empty())
        return skipCost;

    double totalPenCost = 0.0;
    #pragma omp parallel for reduction(+:totalPenCost) schedule(static)
    for (int s = 0; s < params.n_scenarios; s++)
        totalPenCost += splitDP(tour, s);

    return totalPenCost / params.n_scenarios + skipCost;
}

// ──────────────────────────────────────────────────────────
// Subtour elimination callback
// ──────────────────────────────────────────────────────────
class SubtourCallback : public GRBCallback {
public:
    int n;
    GRBVar * xFlat;   // edge vars x[i*n+j] for i < j
    GRBVar * yVars;

    SubtourCallback(int n, GRBVar * xFlat, GRBVar * yVars)
        : n(n), xFlat(xFlat), yVars(yVars) {}

protected:
    void callback() override {
        if (where != GRB_CB_MIPSOL) return;

        std::vector<double> yv(n);
        for (int i = 0; i < n; i++)
            yv[i] = getSolution(yVars[i]);

        std::vector<int> selected;
        for (int i = 0; i < n; i++)
            if (yv[i] > 0.5)
                selected.push_back(i);

        // Build adjacency for selected nodes
        std::vector<std::vector<int>> adj(n);
        for (int a = 0; a < (int)selected.size(); a++) {
            int i = selected[a];
            for (int b = a + 1; b < (int)selected.size(); b++) {
                int j = selected[b];
                int lo = std::min(i, j), hi = std::max(i, j);
                double xv = getSolution(xFlat[lo * n + hi]);
                if (xv > 0.5) {
                    adj[i].push_back(j);
                    adj[j].push_back(i);
                }
            }
        }

        // BFS to find connected components
        std::vector<bool> vis(n, false);
        for (int s : selected) {
            if (vis[s]) continue;
            std::vector<int> comp;
            bool hasDepot = false;
            std::queue<int> q;
            q.push(s);
            vis[s] = true;
            while (!q.empty()) {
                int cur = q.front(); q.pop();
                comp.push_back(cur);
                if (cur == 0) hasDepot = true;
                for (int nb : adj[cur])
                    if (!vis[nb]) { vis[nb] = true; q.push(nb); }
            }
            if (!hasDepot && (int)comp.size() >= 2) {
                GRBLinExpr expr = 0;
                for (int a = 0; a < (int)comp.size(); a++)
                    for (int b = a + 1; b < (int)comp.size(); b++) {
                        int lo = std::min(comp[a], comp[b]);
                        int hi = std::max(comp[a], comp[b]);
                        expr += xFlat[lo * n + hi];
                    }
                addLazy(expr <= (int)comp.size() - 1);
            }
        }
    }
};

// ──────────────────────────────────────────────────────────
// Solve deterministic PCTSP with Gurobi
// ──────────────────────────────────────────────────────────
PCTSPResult PCTSPExact::solveDeterministic()
{
    PCTSPResult result;
    result.optimal = false;
    auto tStart = std::chrono::steady_clock::now();

    try {
        GRBEnv env(true);
        if (!verbose) env.set(GRB_IntParam_OutputFlag, 0);
        env.set(GRB_IntParam_LazyConstraints, 1);
        env.start();

        GRBModel model(env);
        model.set(GRB_DoubleParam_TimeLimit, timeLimit);

        // Edge variables x[i,j] for i < j, stored in flat array x[i*n + j]
        std::vector<GRBVar> xVars(n * n);
        for (int i = 0; i < n; i++)
            for (int j = i + 1; j < n; j++)
                xVars[i * n + j] = model.addVar(
                    0, 1, -params.timeCost[i][j], GRB_BINARY);

        // Node selection variables y[i]
        std::vector<GRBVar> yVars(n);
        for (int i = 0; i < n; i++) {
            double prize = params.cli[i].skipPenalty;
            yVars[i] = model.addVar(0, 1, prize, GRB_BINARY);
        }

        model.set(GRB_IntAttr_ModelSense, GRB_MAXIMIZE);
        model.update();

        // Depot must be visited
        model.addConstr(yVars[0] == 1, "depot");

        // Degree constraints: Σ x_ij = 2*y_i
        for (int i = 0; i < n; i++) {
            GRBLinExpr expr = 0;
            for (int j = 0; j < n; j++) {
                if (j == i) continue;
                int lo = std::min(i, j), hi = std::max(i, j);
                expr += xVars[lo * n + hi];
            }
            model.addConstr(expr == 2.0 * yVars[i]);
        }

        model.update();

        SubtourCallback cb(n, xVars.data(), yVars.data());
        model.setCallback(&cb);
        model.optimize();

        auto tEnd = std::chrono::steady_clock::now();
        result.solveTime = std::chrono::duration<double>(tEnd - tStart).count();

        if (model.get(GRB_IntAttr_SolCount) == 0) {
            if (verbose) std::cout << "PCTSP: No solution found." << std::endl;
            return result;
        }

        result.objValue = model.get(GRB_DoubleAttr_ObjVal);
        result.optimal = (model.get(GRB_IntAttr_Status) == GRB_OPTIMAL);

        for (int i = 0; i < n; i++)
            if (yVars[i].get(GRB_DoubleAttr_X) > 0.5)
                result.selectedNodes.push_back(i);

        result.totalPrize = 0;
        for (int nd : result.selectedNodes)
            result.totalPrize += params.cli[nd].skipPenalty;

        // Build tour from edges
        std::vector<std::vector<int>> adj(n);
        for (int i : result.selectedNodes)
            for (int j : result.selectedNodes)
                if (i < j && xVars[i * n + j].get(GRB_DoubleAttr_X) > 0.5) {
                    adj[i].push_back(j);
                    adj[j].push_back(i);
                }

        result.tour.push_back(0);
        std::vector<bool> vis(n, false);
        vis[0] = true;
        int cur = 0;
        while (true) {
            int nxt = -1;
            for (int nb : adj[cur])
                if (!vis[nb]) { nxt = nb; break; }
            if (nxt < 0) break;
            result.tour.push_back(nxt);
            vis[nxt] = true;
            cur = nxt;
        }

        result.totalDistance = 0;
        for (int i = 0; i + 1 < (int)result.tour.size(); i++)
            result.totalDistance += params.timeCost[result.tour[i]][result.tour[i + 1]];
        result.totalDistance += params.timeCost[result.tour.back()][0];
        result.tour.push_back(0);

    } catch (GRBException & e) {
        std::cerr << "Gurobi error " << e.getErrorCode()
                  << ": " << e.getMessage() << std::endl;
    }
    return result;
}

// ──────────────────────────────────────────────────────────
// Solve TSP on a subset of clients (returns ordered client IDs)
// ──────────────────────────────────────────────────────────
std::vector<int> PCTSPExact::solveTSP(const std::vector<int> & clients) const
{
    int m = (int)clients.size();
    if (m <= 1) return clients;

    try {
        GRBEnv env(true);
        env.set(GRB_IntParam_OutputFlag, 0);
        env.set(GRB_IntParam_LazyConstraints, 1);
        env.start();

        GRBModel model(env);
        model.set(GRB_DoubleParam_TimeLimit, std::min(timeLimit * 0.5, 60.0));

        std::vector<int> nodes = {0};
        nodes.insert(nodes.end(), clients.begin(), clients.end());
        int tn = (int)nodes.size();

        std::vector<GRBVar> xFlat(tn * tn);
        for (int a = 0; a < tn; a++)
            for (int b = a + 1; b < tn; b++) {
                double d = params.timeCost[nodes[a]][nodes[b]];
                xFlat[a * tn + b] = model.addVar(0, 1, d, GRB_BINARY);
            }
        model.set(GRB_IntAttr_ModelSense, GRB_MINIMIZE);
        model.update();

        for (int a = 0; a < tn; a++) {
            GRBLinExpr expr = 0;
            for (int b = 0; b < tn; b++) {
                if (b == a) continue;
                int lo = std::min(a, b), hi = std::max(a, b);
                expr += xFlat[lo * tn + hi];
            }
            model.addConstr(expr == 2);
        }
        model.update();

        struct TSPCb : public GRBCallback {
            int tn; GRBVar * xFlat;
            TSPCb(int tn, GRBVar * xF) : tn(tn), xFlat(xF) {}
            void callback() override {
                if (where != GRB_CB_MIPSOL) return;
                std::vector<std::vector<int>> adj(tn);
                for (int a = 0; a < tn; a++)
                    for (int b = a + 1; b < tn; b++)
                        if (getSolution(xFlat[a * tn + b]) > 0.5) {
                            adj[a].push_back(b); adj[b].push_back(a);
                        }
                std::vector<bool> vis(tn, false);
                for (int s = 0; s < tn; s++) {
                    if (vis[s]) continue;
                    std::vector<int> comp;
                    std::queue<int> q; q.push(s); vis[s] = true;
                    while (!q.empty()) {
                        int c = q.front(); q.pop(); comp.push_back(c);
                        for (int nb : adj[c]) if (!vis[nb]) { vis[nb] = true; q.push(nb); }
                    }
                    if ((int)comp.size() < tn) {
                        GRBLinExpr expr = 0;
                        for (int a = 0; a < (int)comp.size(); a++)
                            for (int b = a + 1; b < (int)comp.size(); b++) {
                                int lo = std::min(comp[a], comp[b]), hi = std::max(comp[a], comp[b]);
                                expr += xFlat[lo * tn + hi];
                            }
                        addLazy(expr <= (int)comp.size() - 1);
                    }
                }
            }
        };

        TSPCb cb(tn, xFlat.data());
        model.setCallback(&cb);
        model.optimize();

        if (model.get(GRB_IntAttr_SolCount) == 0) return clients;

        std::vector<std::vector<int>> adj(tn);
        for (int a = 0; a < tn; a++)
            for (int b = a + 1; b < tn; b++)
                if (xFlat[a * tn + b].get(GRB_DoubleAttr_X) > 0.5) {
                    adj[a].push_back(b); adj[b].push_back(a);
                }

        std::vector<int> result;
        std::vector<bool> vis(tn, false);
        vis[0] = true;
        int cur = adj[0].empty() ? 0 : adj[0][0];
        while (true) {
            if (cur == 0 && !result.empty()) break;
            vis[cur] = true;
            if (cur != 0) result.push_back(nodes[cur]);
            int nxt = -1;
            for (int nb : adj[cur]) if (!vis[nb]) { nxt = nb; break; }
            if (nxt < 0) break;
            cur = nxt;
        }
        return result;

    } catch (GRBException & e) {
        std::cerr << "TSP Gurobi error: " << e.getMessage() << std::endl;
        return clients;
    }
}

// ──────────────────────────────────────────────────────────
// Evaluate a visited subset: solve TSP + stochastic Split DP
// ──────────────────────────────────────────────────────────
double PCTSPExact::evaluateSubset(const std::vector<bool> & visited) const
{
    std::vector<int> clients;
    for (int c = 1; c < n; c++)
        if (visited[c]) clients.push_back(c);

    std::vector<int> tour = solveTSP(clients);
    return evaluateTourStochastic(tour, visited);
}

// ──────────────────────────────────────────────────────────
// Solve stochastic PCTSP
// 1. Start from deterministic PCTSP solution
// 2. Local search: toggle each client, keep best improvement
// 3. Repeat until no improvement
// ──────────────────────────────────────────────────────────
PCTSPResult PCTSPExact::solveStochastic()
{
    auto tStart = std::chrono::steady_clock::now();
    PCTSPResult best;
    best.objValue = 1e30;
    best.optimal = false;

    // Step 1: Deterministic PCTSP for initial subset
    PCTSPResult detResult = solveDeterministic();
    if (detResult.selectedNodes.empty()) return best;

    if (verbose) {
        std::cout << "--- Deterministic PCTSP ---" << std::endl;
        std::cout << "  Visited: " << detResult.selectedNodes.size() - 1
                  << "/" << n - 1 << std::endl;
        std::cout << "  Det obj: " << detResult.objValue << std::endl;
    }

    // Step 2: Evaluate deterministic solution stochastically
    std::vector<bool> visited(n, false);
    visited[0] = true;
    for (int nd : detResult.selectedNodes)
        visited[nd] = true;

    double bestCost = evaluateSubset(visited);
    std::vector<bool> bestVisited = visited;

    if (verbose)
        std::cout << "  Initial stochastic cost: " << bestCost << std::endl;

    // Step 3: Local search - toggle each client
    bool improved = true;
    int iter = 0;
    while (improved) {
        improved = false;
        iter++;
        for (int c = 1; c < n; c++) {
            auto elapsed = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - tStart).count();
            if (elapsed > timeLimit * 0.8) break;

            std::vector<bool> trial = bestVisited;
            trial[c] = !trial[c];

            // At least 1 client must be visited
            int nVis = 0;
            for (int i = 1; i < n; i++) if (trial[i]) nVis++;
            if (nVis == 0) continue;

            double cost = evaluateSubset(trial);
            if (cost < bestCost - 1e-6) {
                bestCost = cost;
                bestVisited = trial;
                improved = true;
                if (verbose)
                    std::cout << "  Iter " << iter << ": toggle client " << c
                              << (trial[c] ? " ON" : " OFF")
                              << " -> cost " << bestCost
                              << " (visited " << nVis << ")" << std::endl;
            }
        }
    }

    // Build result
    std::vector<int> clients;
    for (int c = 1; c < n; c++)
        if (bestVisited[c]) clients.push_back(c);
    std::vector<int> tour = solveTSP(clients);

    best.tour.clear();
    best.tour.push_back(0);
    for (int c : tour) best.tour.push_back(c);
    best.tour.push_back(0);
    best.selectedNodes.push_back(0);
    for (int c : clients) best.selectedNodes.push_back(c);
    best.totalPrize = 0;
    for (int c : clients) best.totalPrize += params.cli[c].skipPenalty;
    best.objValue = bestCost;

    double tourDist = 0;
    for (int i = 0; i + 1 < (int)best.tour.size(); i++)
        tourDist += params.timeCost[best.tour[i]][best.tour[i + 1]];
    best.totalDistance = tourDist;

    auto tEnd = std::chrono::steady_clock::now();
    best.solveTime = std::chrono::duration<double>(tEnd - tStart).count();

    if (verbose) {
        std::cout << "--- Stochastic PCTSP Result ---" << std::endl;
        std::cout << "  Visited: " << clients.size() << "/" << n - 1 << std::endl;
        double skipCost = 0;
        for (int c = 1; c < n; c++)
            if (!bestVisited[c]) skipCost += params.cli[c].skipPenalty;
        std::cout << "  Skip cost: " << skipCost << std::endl;
        std::cout << "  Avg routing cost: " << (bestCost - skipCost) << std::endl;
        std::cout << "  Total stochastic cost: " << bestCost << std::endl;
        std::cout << "  Time: " << best.solveTime << "s" << std::endl;
    }

    return best;
}
