#include "PCTSPExact.h"
#include "gurobi_c++.h"
#include <chrono>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <iomanip>
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

        // When all visits forced, fix y[i] = 1 => reduces to TSP
        if (!params.ap.optionalVisit)
            for (int i = 1; i < n; i++)
                model.addConstr(yVars[i] == 1);

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
        model.set(GRB_DoubleParam_TimeLimit, std::min(timeLimit * 0.5, 300.0));

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
// ──────────────────────────────────────────────────────────
// Benders callback: evaluates candidate tours via Split DP
// across all scenarios, adds integer L-shaped optimality cuts.
//
// Master has only tour vars (a[i][j]) + one penalty variable θ.
// θ represents the average stochastic Split DP cost.
// When Gurobi proposes an integer tour, the callback:
//   1. Extracts the tour permutation
//   2. Evaluates SplitDP on all S scenarios (OpenMP parallel)
//   3. If θ < true cost, adds a lazy cut to tighten θ
// ──────────────────────────────────────────────────────────
class BendersSplitCallback : public GRBCallback {
public:
    const Params & params;
    int nc;
    GRBVar * a_flat;
    GRBVar   theta;
    int numCuts;
    double bestFoundCost;
    std::vector<int> bestPerm;
    bool verbose;

    BendersSplitCallback(const Params & p, int nc, GRBVar * a, GRBVar th, bool verb)
        : params(p), nc(nc), a_flat(a), theta(th),
          numCuts(0), bestFoundCost(1e30), verbose(verb) {}

protected:
    void callback() override {
        if (where != GRB_CB_MIPSOL) return;

        std::vector<int> succ(nc, -1);
        for (int i = 0; i < nc; i++)
            for (int j = 0; j < nc; j++) if (i != j)
                if (getSolution(a_flat[i * nc + j]) > 0.5)
                    succ[i] = j;

        std::vector<int> perm;
        perm.reserve(nc);
        int cur = 0;
        for (int step = 0; step < nc; step++) {
            perm.push_back(cur + 1);
            if (succ[cur] < 0) return;
            cur = succ[cur];
        }

        int S = params.n_scenarios;
        double totalCost = 0.0;
        #pragma omp parallel for reduction(+:totalCost) schedule(static)
        for (int s = 0; s < S; s++)
            totalCost += splitDPeval(perm, s);
        double avgCost = totalCost / S;

        if (avgCost < bestFoundCost) {
            bestFoundCost = avgCost;
            bestPerm = perm;
        }

        double thetaVal = getSolution(theta);
        if (thetaVal < avgCost - 1e-4) {
            GRBLinExpr tourSum = 0;
            int c = 0;
            for (int step = 0; step < nc; step++) {
                tourSum += a_flat[c * nc + succ[c]];
                c = succ[c];
            }
            addLazy(theta >= avgCost - avgCost * (nc - tourSum));
            numCuts++;

            if (verbose && numCuts % 50 == 1)
                std::cout << "  [CB] cut #" << numCuts
                          << "  θ_sol=" << thetaVal
                          << "  true=" << avgCost
                          << "  best=" << bestFoundCost << std::endl;
        }
    }

    double splitDPeval(const std::vector<int> & perm, int scenIdx) const {
        int sz = (int)perm.size();
        if (sz == 0) return 0.0;
        const double cap = params.vehicleCapacity;
        const double penCap = params.penaltyCapacity;
        std::vector<double> cost(sz + 1, 1e30);
        cost[0] = 0.0;
        for (int j = 1; j <= sz; j++) {
            double load = 0.0, routeDist = 0.0;
            for (int i = j; i >= 1; i--) {
                int cli = perm[i - 1];
                load += params.cli[cli].demands_scenarios[scenIdx];
                if (i == j) routeDist = params.timeCost[cli][0];
                else routeDist += params.timeCost[cli][perm[i]];
                double fullDist = params.timeCost[0][cli] + routeDist;
                double excess = std::max(0.0, load - cap);
                double segCost = fullDist + penCap * excess;
                if (cost[i - 1] + segCost < cost[j])
                    cost[j] = cost[i - 1] + segCost;
            }
        }
        return cost[sz];
    }
};

// ──────────────────────────────────────────────────────────
// Full stochastic MILP via Benders decomposition:
//   Master:  directed TSP tour (a[i][j], u[i]) + θ penalty
//   Sub:     Split DP evaluation per scenario (in callback)
//
// Model size is independent of S — only ~nc² variables.
// Scenario costs are computed on-the-fly and fed back as
// integer optimality cuts (lazy constraints).
// ──────────────────────────────────────────────────────────
PCTSPResult PCTSPExact::solveFullStochasticMILP()
{
    using std::cout; using std::endl; using std::fixed; using std::setprecision;
    auto tStart = std::chrono::steady_clock::now();
    PCTSPResult result;
    result.optimal = false;
    result.objValue = 1e30;

    const int nc = n - 1;
    const int S  = params.n_scenarios;

    cout << "\n===== Benders Stochastic CVRP =====" << endl;
    cout << "  Clients:    " << nc << endl;
    cout << "  Scenarios:  " << S << endl;
    cout << "  Master vars (binary):     " << nc * (nc - 1) << "  (tour arcs)" << endl;
    cout << "  Master vars (continuous):  " << nc + 1 << "  (MTZ pos + θ)" << endl;
    cout << "  Master constraints:        ~" << nc * (nc - 1) + 2 * nc + 1 << endl;
    cout << "  Subproblem: SplitDP O(n²) × " << S << " scenarios per callback" << endl;
    cout << "==================================" << endl;

    try {
        GRBEnv env(true);
        if (!verbose) env.set(GRB_IntParam_OutputFlag, 0);
        env.set(GRB_IntParam_LazyConstraints, 1);
        env.start();
        GRBModel model(env);
        model.set(GRB_DoubleParam_TimeLimit, timeLimit);

        /* ── 1. Directed tour arcs ── */
        std::vector<GRBVar> a_var(nc * nc);
        for (int i = 0; i < nc; i++)
            for (int j = 0; j < nc; j++)
                if (i != j)
                    a_var[i * nc + j] = model.addVar(0, 1, 0, GRB_BINARY);

        for (int i = 0; i < nc; i++) {
            GRBLinExpr outD = 0, inD = 0;
            for (int j = 0; j < nc; j++) if (j != i) {
                outD += a_var[i * nc + j];
                inD  += a_var[j * nc + i];
            }
            model.addConstr(outD == 1);
            model.addConstr(inD  == 1);
        }

        /* ── 2. MTZ subtour elimination ── */
        std::vector<GRBVar> u_var(nc);
        for (int i = 0; i < nc; i++)
            u_var[i] = model.addVar(1, nc, 0, GRB_CONTINUOUS);
        model.addConstr(u_var[0] == 1);
        for (int i = 0; i < nc; i++)
            for (int j = 1; j < nc; j++) if (i != j)
                model.addConstr(u_var[j] >= u_var[i] + 1 - nc * (1 - a_var[i * nc + j]));

        /* ── 3. θ penalty variable: average stochastic Split cost ── */
        GRBVar theta = model.addVar(0, GRB_INFINITY, 1.0, GRB_CONTINUOUS, "theta");
        model.setObjective(GRBLinExpr(theta), GRB_MINIMIZE);

        model.update();

        cout << "\nMaster model:" << endl;
        cout << "  Vars:        " << model.get(GRB_IntAttr_NumVars) << endl;
        cout << "  Binary:      " << model.get(GRB_IntAttr_NumBinVars) << endl;
        cout << "  Constraints: " << model.get(GRB_IntAttr_NumConstrs) << endl;
        cout << "  Non-zeros:   " << model.get(GRB_IntAttr_NumNZs) << endl;
        cout << "Solving with Benders callbacks..." << endl;

        BendersSplitCallback cb(params, nc, a_var.data(), theta, verbose);
        model.setCallback(&cb);
        model.optimize();

        auto tEnd = std::chrono::steady_clock::now();
        result.solveTime = std::chrono::duration<double>(tEnd - tStart).count();

        int status = model.get(GRB_IntAttr_Status);
        cout << "\n  Benders cuts added: " << cb.numCuts << endl;
        cout << "  Gurobi status:      " << status << endl;

        if (cb.bestPerm.empty()) {
            cout << "No feasible tour evaluated." << endl;
            return result;
        }

        double bestBound = model.get(GRB_DoubleAttr_ObjBound);
        result.objValue = cb.bestFoundCost;
        result.optimal  = (status == GRB_OPTIMAL &&
                           std::abs(cb.bestFoundCost - bestBound) < 1e-4);

        result.tour.clear();
        result.tour.push_back(0);
        for (int c : cb.bestPerm) result.tour.push_back(c);
        result.tour.push_back(0);

        result.selectedNodes.push_back(0);
        for (int c = 1; c <= nc; c++)
            result.selectedNodes.push_back(c);

        result.totalDistance = 0;
        for (int i = 0; i + 1 < (int)result.tour.size(); i++)
            result.totalDistance += params.timeCost[result.tour[i]][result.tour[i + 1]];

        double totalDist = 0, totalExcess = 0;
        const std::vector<int> & perm = cb.bestPerm;
        #pragma omp parallel for reduction(+:totalDist,totalExcess) schedule(static)
        for (int s = 0; s < S; s++) {
            int sz = (int)perm.size();
            std::vector<double> dp(sz + 1, 1e30);
            std::vector<double> dpDist(sz + 1, 0), dpExcess(sz + 1, 0);
            dp[0] = 0;
            for (int j = 1; j <= sz; j++) {
                double load = 0, rDist = 0;
                for (int i = j; i >= 1; i--) {
                    int cli = perm[i - 1];
                    load += params.cli[cli].demands_scenarios[s];
                    if (i == j) rDist = params.timeCost[cli][0];
                    else rDist += params.timeCost[cli][perm[i]];
                    double fDist = params.timeCost[0][cli] + rDist;
                    double exc = std::max(0.0, load - params.vehicleCapacity);
                    double seg = fDist + params.penaltyCapacity * exc;
                    if (dp[i - 1] + seg < dp[j]) {
                        dp[j] = dp[i - 1] + seg;
                        dpDist[j] = dpDist[i - 1] + fDist;
                        dpExcess[j] = dpExcess[i - 1] + exc;
                    }
                }
            }
            totalDist += dpDist[sz];
            totalExcess += dpExcess[sz];
        }
        double avgDist = totalDist / S;
        double avgExcess = totalExcess / S;

        cout << "\n===== Benders Result =====" << endl;
        cout << fixed << setprecision(2);
        cout << "  Stochastic cost:  " << result.objValue << endl;
        cout << "  avg distance:     " << avgDist << endl;
        cout << "  avg capExcess:    " << avgExcess << endl;
        cout << "  tourDist(TSP):    " << result.totalDistance << endl;
        cout << "  Best bound:       " << bestBound << endl;
        cout << "  Gap:              " << 100.0 * (result.objValue - bestBound) / (std::abs(result.objValue) + 1e-10) << "%" << endl;
        cout << "  Cuts generated:   " << cb.numCuts << endl;
        cout << "  Solve time:       " << result.solveTime << "s" << endl;

    } catch (GRBException & e) {
        std::cerr << "Gurobi error " << e.getErrorCode()
                  << ": " << e.getMessage() << std::endl;
    }
    return result;
}

// ──────────────────────────────────────────────────────────
// Solve stochastic PCTSP (original: TSP + Split DP)
// ──────────────────────────────────────────────────────────
PCTSPResult PCTSPExact::solveStochastic()
{
    auto tStart = std::chrono::steady_clock::now();
    PCTSPResult best;
    best.objValue = 1e30;
    best.optimal = false;

    // Forced all visits: solve TSP on all clients + stochastic evaluation only
    if (!params.ap.optionalVisit) {
        if (verbose)
            std::cout << "--- All visits forced: TSP + stochastic Split DP ---" << std::endl;

        std::vector<int> allClients;
        for (int c = 1; c < n; c++) allClients.push_back(c);
        std::vector<int> tour = solveTSP(allClients);
        std::vector<bool> visited(n, true);

        if (verbose)
            std::cout << "  TSP solved, tour size: " << tour.size() << std::endl;

        double cost = evaluateTourStochastic(tour, visited);

        best.tour.clear();
        best.tour.push_back(0);
        for (int c : tour) best.tour.push_back(c);
        best.tour.push_back(0);
        best.selectedNodes.push_back(0);
        for (int c : allClients) best.selectedNodes.push_back(c);
        best.objValue = cost;
        best.totalPrize = 0;
        best.totalDistance = 0;
        for (int i = 0; i + 1 < (int)best.tour.size(); i++)
            best.totalDistance += params.timeCost[best.tour[i]][best.tour[i + 1]];

        auto tEnd = std::chrono::steady_clock::now();
        best.solveTime = std::chrono::duration<double>(tEnd - tStart).count();

        if (verbose) {
            std::cout << "  Stochastic cost (avg penalized): " << cost << std::endl;
            std::cout << "  TSP tour distance: " << best.totalDistance << std::endl;
            std::cout << "  Time: " << best.solveTime << "s" << std::endl;
        }
        return best;
    }

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
