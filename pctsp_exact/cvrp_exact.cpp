// Two-Stage Stochastic CVRP Exact Solver
//
// First stage:  Determine client visiting order (Hamiltonian path, MTZ)
// Second stage: Per-scenario route splitting (binary "cut" variables)
//
// The permutation is decided once; route partitioning adapts per scenario.
// This directly models what Split DP does, inside the MILP.

#include "InstanceCVRPLIB.h"
#include "Params.h"
#include "Split.h"
#include "Individual.h"
#include "gurobi_c++.h"
#include <chrono>
#include <climits>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numeric>
#include <vector>
#include <algorithm>
#include <omp.h>

using namespace std;

struct Result {
    vector<int> chromT;
    double mipObj;
    double mipGap;
    double solveTime;
    bool optimal;
};

Result solveTwoStage(const Params & params, int milpScen, double timeLimit, bool verbose)
{
    Result res;
    res.mipObj = 1e30;
    res.optimal = false;

    const int n = params.nbClients;            // clients 1..n
    const int S = min(milpScen, params.n_scenarios);
    const double Q = params.vehicleCapacity;
    const double penCap = params.penaltyCapacity;
    const int maxV = params.nbVehicles;
    const int E = n + 1;                       // virtual end-depot node

    if (verbose)
        cout << "Two-Stage MILP: " << n << " clients, " << S << " scen, "
             << "Q=" << Q << ", maxV=" << maxV << ", penCap=" << penCap << endl;

    auto tStart = chrono::steady_clock::now();

    // Big-M for load constraints (max total demand in any single scenario)
    double bigM = 0;
    for (int s = 0; s < S; s++) {
        double td = 0;
        for (int c = 1; c <= n; c++) td += params.cli[c].demands_scenarios[s];
        bigM = max(bigM, td);
    }

    // Bounds on delta_net[i] = sum_{j!=i,j=1..n} x[i][j]*(d(0,j)-d(i,j))
    // Used in McCormick linearisation of  q[i][s] = cut[i][s] * delta_net[i]
    // Include 0 in range because delta_net=0 when i is the last client (x[i][E]=1)
    vector<double> dLB(n + 1, 0.0), dUB(n + 1, 0.0);
    for (int i = 1; i <= n; i++) {
        double lb = 0, ub = 0;
        for (int j = 1; j <= n; j++) {
            if (j == i) continue;
            double v = params.timeCost[0][j] - params.timeCost[i][j];
            lb = min(lb, v);
            ub = max(ub, v);
        }
        dLB[i] = lb;
        dUB[i] = ub;
    }

    try {
        GRBEnv env(true);
        if (!verbose) env.set(GRB_IntParam_OutputFlag, 0);
        env.start();
        GRBModel model(env);
        model.set(GRB_DoubleParam_TimeLimit, timeLimit);
        model.set(GRB_IntAttr_ModelSense, GRB_MINIMIZE);

        // ════════════════════════════════════════════
        //  First stage variables – Hamiltonian path
        // ════════════════════════════════════════════
        // Nodes:  0 = start-depot,  1..n = clients,  E = end-depot
        // x[i][j] = 1 iff j immediately follows i in the giant tour
        // Objective coeff = arc distance (first-stage routing cost, paid once)

        // 2-D indexing helpers (row i, col j)  i in 0..n, j in 1..E
        auto XI = [&](int i, int j) -> int { return i * (E + 1) + j; };
        const int xSize = (n + 1) * (E + 1);
        vector<GRBVar> xv(xSize);

        for (int j = 1; j <= n; j++)                             // depot -> client
            xv[XI(0, j)] = model.addVar(0, 1, params.timeCost[0][j], GRB_BINARY);
        for (int i = 1; i <= n; i++)
            for (int j = 1; j <= n; j++) if (i != j)            // client -> client
                xv[XI(i, j)] = model.addVar(0, 1, params.timeCost[i][j], GRB_BINARY);
        for (int i = 1; i <= n; i++)                             // client -> end-depot
            xv[XI(i, E)] = model.addVar(0, 1, params.timeCost[i][0], GRB_BINARY);

        // MTZ position (continuous, 1..n)
        vector<GRBVar> pos(n + 1);
        for (int i = 1; i <= n; i++)
            pos[i] = model.addVar(1, n, 0, GRB_CONTINUOUS);

        // ════════════════════════════════════════════
        //  Second stage variables – per-scenario
        // ════════════════════════════════════════════
        // cut[i][s]  : 1 iff route breaks after client i in scenario s
        //   obj coeff = d(i,0)/S  (depot-return part of detour cost)
        // load[i][s] : cumulative demand in client i's current route
        // exc[i][s]  : max(0, route_load - Q)  at route ends
        //   obj coeff = penCap/S
        // delta[i]   : aux variable = sum_j x[i][j]*(d(0,j)-d(i,j))
        // q[i][s]    : McCormick of cut[i][s]*delta[i]
        //   obj coeff = 1/S  (depot-departure minus saved-direct part)

        auto IDX = [&](int i, int s) -> int { return i * S + s; };

        vector<GRBVar> cutv(  (n + 1) * S);
        vector<GRBVar> loadv( (n + 1) * S);
        vector<GRBVar> excv(  (n + 1) * S);
        vector<GRBVar> qv(    (n + 1) * S);
        vector<GRBVar> deltav(n + 1);

        for (int i = 1; i <= n; i++) {
            deltav[i] = model.addVar(dLB[i], dUB[i], 0, GRB_CONTINUOUS);
            for (int s = 0; s < S; s++) {
                cutv [IDX(i,s)] = model.addVar(0, 1,          params.timeCost[i][0] / S, GRB_BINARY);
                loadv[IDX(i,s)] = model.addVar(0, GRB_INFINITY, 0,                       GRB_CONTINUOUS);
                excv [IDX(i,s)] = model.addVar(0, GRB_INFINITY, penCap / S,              GRB_CONTINUOUS);
                qv   [IDX(i,s)] = model.addVar(-GRB_INFINITY, GRB_INFINITY, 1.0 / S,    GRB_CONTINUOUS);
            }
        }
        model.update();

        // ════════════════════════════════════════════
        //  Constraints
        // ════════════════════════════════════════════

        // ── Hamiltonian-path flow ──

        { // out-degree of start-depot = 1
            GRBLinExpr e = 0;
            for (int j = 1; j <= n; j++) e += xv[XI(0,j)];
            model.addConstr(e == 1);
        }
        { // in-degree of end-depot = 1
            GRBLinExpr e = 0;
            for (int i = 1; i <= n; i++) e += xv[XI(i,E)];
            model.addConstr(e == 1);
        }
        for (int j = 1; j <= n; j++) { // each client: in-degree = 1
            GRBLinExpr e = xv[XI(0,j)];
            for (int i = 1; i <= n; i++) if (i != j) e += xv[XI(i,j)];
            model.addConstr(e == 1);
        }
        for (int i = 1; i <= n; i++) { // each client: out-degree = 1
            GRBLinExpr e = xv[XI(i,E)];
            for (int j = 1; j <= n; j++) if (j != i) e += xv[XI(i,j)];
            model.addConstr(e == 1);
        }

        // ── MTZ subtour elimination ──
        for (int i = 1; i <= n; i++)
            for (int j = 1; j <= n; j++) if (i != j)
                model.addConstr(pos[j] >= pos[i] + 1 - n * (1 - xv[XI(i,j)]));

        // ── Cut feasibility: can't cut at last client ──
        for (int i = 1; i <= n; i++)
            for (int s = 0; s < S; s++)
                model.addConstr(cutv[IDX(i,s)] + xv[XI(i,E)] <= 1);

        // ── Vehicle limit per scenario ──
        for (int s = 0; s < S; s++) {
            GRBLinExpr e = 0;
            for (int i = 1; i <= n; i++) e += cutv[IDX(i,s)];
            model.addConstr(e <= maxV - 1);
        }

        // ── Load tracking (lower-bound constraints, tightened by optimiser) ──
        if (verbose) cout << "Adding load + excess constraints (" << S << " scen)..." << endl;
        for (int s = 0; s < S; s++) {
            for (int j = 1; j <= n; j++) {
                double dj = params.cli[j].demands_scenarios[s];
                model.addConstr(loadv[IDX(j,s)] >= dj);              // base bound
                for (int i = 1; i <= n; i++) {
                    if (i == j) continue;
                    // load accumulates when arc is used and no cut
                    model.addConstr(loadv[IDX(j,s)] >= loadv[IDX(i,s)] + dj
                                    - bigM * (1 - xv[XI(i,j)])
                                    - bigM * cutv[IDX(i,s)]);
                }
            }
            // Capacity excess at route ends
            for (int i = 1; i <= n; i++)
                model.addConstr(excv[IDX(i,s)] >= loadv[IDX(i,s)] - Q
                                - bigM * (1 - cutv[IDX(i,s)] - xv[XI(i,E)]));
        }

        // ── Delta definition + McCormick ──
        if (verbose) cout << "Adding McCormick constraints..." << endl;
        for (int i = 1; i <= n; i++) {
            GRBLinExpr dexpr = 0;
            for (int j = 1; j <= n; j++) {
                if (j == i) continue;
                dexpr += xv[XI(i,j)] * (params.timeCost[0][j] - params.timeCost[i][j]);
            }
            model.addConstr(deltav[i] == dexpr);

            double lb = dLB[i], ub = dUB[i];
            for (int s = 0; s < S; s++) {
                GRBVar & c = cutv[IDX(i,s)];
                GRBVar & q = qv[IDX(i,s)];
                model.addConstr(q >= lb * c);
                model.addConstr(q <= ub * c);
                model.addConstr(q >= deltav[i] - ub * (1 - c));
                model.addConstr(q <= deltav[i] - lb * (1 - c));
            }
        }

        model.update();

        if (verbose) {
            int nV = model.get(GRB_IntAttr_NumVars);
            int nC = model.get(GRB_IntAttr_NumConstrs);
            int nB = model.get(GRB_IntAttr_NumBinVars);
            cout << "Model: " << nB << " bin, " << (nV - nB) << " cont, "
                 << nC << " constrs" << endl;
        }

        // ── Solve ──
        if (verbose) cout << "Solving..." << endl;
        model.optimize();

        auto tEnd = chrono::steady_clock::now();
        res.solveTime = chrono::duration<double>(tEnd - tStart).count();

        if (model.get(GRB_IntAttr_SolCount) == 0) {
            if (verbose) cout << "No feasible solution found." << endl;
            return res;
        }

        res.mipObj  = model.get(GRB_DoubleAttr_ObjVal);
        res.mipGap  = model.get(GRB_DoubleAttr_MIPGap);
        res.optimal = (model.get(GRB_IntAttr_Status) == GRB_OPTIMAL);

        // ── Extract chromT via successor chain ──
        int cur = -1;
        for (int j = 1; j <= n; j++)
            if (xv[XI(0,j)].get(GRB_DoubleAttr_X) > 0.5) { cur = j; break; }

        while (cur >= 1 && cur <= n) {
            res.chromT.push_back(cur);
            int nxt = -1;
            for (int j = 1; j <= n; j++)
                if (j != cur && xv[XI(cur,j)].get(GRB_DoubleAttr_X) > 0.5)
                    { nxt = j; break; }
            if (nxt < 0) break;          // cur -> E (end-depot)
            cur = nxt;
        }

        if (verbose) {
            cout << "Solved in " << res.solveTime << "s" << endl;
            cout << "  ObjVal=" << res.mipObj << "  Gap=" << res.mipGap * 100 << "%"
                 << "  Optimal=" << (res.optimal ? "Y" : "N") << endl;
            cout << "  chromT (" << res.chromT.size() << "):";
            for (int c : res.chromT) cout << " " << c;
            cout << endl;

            // Print routes for scenario 0
            if (S > 0) {
                cout << "\n  Routes (scenario 0):" << endl;
                int ri = 0;
                vector<int> route;
                for (int c : res.chromT) {
                    route.push_back(c);
                    bool isCut  = cutv[IDX(c,0)].get(GRB_DoubleAttr_X) > 0.5;
                    bool isLast = xv[XI(c,E)].get(GRB_DoubleAttr_X) > 0.5;
                    if (isCut || isLast) {
                        cout << "    R" << ri++ << ": 0";
                        double d = params.timeCost[0][route[0]];
                        for (int k = 0; k < (int)route.size(); k++) {
                            cout << " -> " << route[k];
                            if (k + 1 < (int)route.size())
                                d += params.timeCost[route[k]][route[k + 1]];
                        }
                        d += params.timeCost[route.back()][0];
                        cout << " -> 0  (d=" << d << ")" << endl;
                        route.clear();
                    }
                }
            }
        }

    } catch (GRBException & ex) {
        cerr << "Gurobi error " << ex.getErrorCode() << ": " << ex.getMessage() << endl;
    }
    return res;
}

// ════════════════════════════════════════════
//  Stochastic evaluation helpers
// ════════════════════════════════════════════

struct ScenarioDetail {
    double totalCost;    // penalised cost = dist + penCap * excess
    double routeDist;    // pure routing distance
    double capExcess;    // total capacity excess (sum over routes)
    int    nRoutes;      // number of routes
};

// O(N^2) exact Split DP returning per-scenario details
vector<ScenarioDetail> evaluateDetailed(const Params & params,
                                        const vector<int> & chromT, int nthreads)
{
    int S = params.n_scenarios;
    double Q = params.vehicleCapacity;
    double penCap = params.penaltyCapacity;
    int sz = (int)chromT.size();
    vector<ScenarioDetail> details(S);

    #pragma omp parallel for num_threads(nthreads)
    for (int s = 0; s < S; s++) {
        vector<double> dp(sz + 1, 1e30);
        vector<int>    split_at(sz + 1, 0);   // best split predecessor
        dp[0] = 0.0;
        for (int j = 1; j <= sz; j++) {
            double load = 0.0, routeDist = 0.0;
            for (int i = j; i >= 1; i--) {
                int cli = chromT[i - 1];
                load += params.cli[cli].demands_scenarios[s];
                if (i == j) routeDist = params.timeCost[cli][0];
                else routeDist += params.timeCost[cli][chromT[i]];
                double fullDist = params.timeCost[0][cli] + routeDist;
                double excess = max(0.0, load - Q);
                double segCost = fullDist + penCap * excess;
                if (dp[i - 1] + segCost < dp[j]) {
                    dp[j] = dp[i - 1] + segCost;
                    split_at[j] = i - 1;
                }
            }
        }

        // Trace back to get route details
        double totalDist = 0, totalExcess = 0;
        int nRoutes = 0;
        int pos = sz;
        while (pos > 0) {
            int start = split_at[pos] + 1;
            nRoutes++;
            double load = 0, rd = 0;
            for (int k = start; k <= pos; k++) {
                int cli = chromT[k - 1];
                load += params.cli[cli].demands_scenarios[s];
                if (k == start) rd = params.timeCost[0][cli];
                else rd += params.timeCost[chromT[k - 2]][cli];
            }
            rd += params.timeCost[chromT[pos - 1]][0];
            totalDist += rd;
            totalExcess += max(0.0, load - Q);
            pos = split_at[pos];
        }
        details[s] = {dp[sz], totalDist, totalExcess, nRoutes};
    }
    return details;
}

double evaluateWithHGSSplit(Params & params, const vector<int> & chromT, int nthreads)
{
    Individual indiv(params, false);
    indiv.chromT = chromT;
    #pragma omp parallel for num_threads(nthreads)
    for (int s = 0; s < params.n_scenarios; s++) {
        Split split(params);
        split.generalSplit(indiv, params.nbVehicles, s);
        split.generateChromR(indiv, s);
    }
    indiv.evaluateCompleteCost(params);
    return indiv.eval.penalizedCost;
}

void printStats(const string & label, vector<double> & vals)
{
    if (vals.empty()) return;
    sort(vals.begin(), vals.end());
    int n = (int)vals.size();
    double sum = 0, sum2 = 0;
    for (double v : vals) { sum += v; sum2 += v * v; }
    double mean = sum / n;
    double stddev = sqrt(max(0.0, sum2 / n - mean * mean));
    auto pct = [&](double p) -> double {
        double idx = p * (n - 1);
        int lo = (int)idx;
        int hi = min(lo + 1, n - 1);
        return vals[lo] + (idx - lo) * (vals[hi] - vals[lo]);
    };
    cout << "  " << label << ":\n"
         << "    mean=" << mean << "  std=" << stddev << "\n"
         << "    min=" << vals[0]
         << "  p25=" << pct(0.25)
         << "  p50=" << pct(0.50)
         << "  p75=" << pct(0.75)
         << "  p95=" << pct(0.95)
         << "  max=" << vals[n - 1] << endl;
}

// ════════════════════════════════════════════
//  main
// ════════════════════════════════════════════

int main(int argc, char *argv[])
{
    if (argc < 2) {
        cerr << "Usage: cvrp_exact <instance> [-seed S] [-nextrascen N] "
                "[-timeLim T] [-nthreads T] [-milpScen S]" << endl;
        return 1;
    }

    string instancePath = argv[1];
    int seed = 1, nextra = 0, nthreads = 8, milpScen = -1;
    double timeLim = 600.0;
    string evalChromTFile;

    for (int i = 2; i < argc - 1; i++) {
        string a = argv[i];
        if      (a == "-seed")       seed     = atoi(argv[++i]);
        else if (a == "-nextrascen") nextra   = atoi(argv[++i]);
        else if (a == "-timeLim")    timeLim  = atof(argv[++i]);
        else if (a == "-nthreads")   nthreads = atoi(argv[++i]);
        else if (a == "-milpScen")   milpScen = atoi(argv[++i]);
        else if (a == "-evalChromT") evalChromTFile = argv[++i];
    }
    omp_set_num_threads(nthreads);

    AlgorithmParameters ap;
    ap.seed = seed;
    ap.n_extra_senarios = nextra;

    InstanceCVRPLIB cvrp(instancePath, true);
    Params params(cvrp.x_coords, cvrp.y_coords, cvrp.dist_mtx,
                  cvrp.service_time, cvrp.demands,
                  cvrp.vehicleCapacity, cvrp.durationLimit,
                  INT_MAX, cvrp.isDurationConstraint, true, ap);
    params.generate_scenario_demands(nextra);
    params.update_max_vehi();
    params.generate_skip_penalties();

    int totalScen = params.n_scenarios;
    if (milpScen < 0) milpScen = totalScen;
    milpScen = min(milpScen, totalScen);

    // ── Eval-only mode: load chromT from file ──
    vector<int> chromT_to_eval;
    if (!evalChromTFile.empty()) {
        ifstream fin(evalChromTFile);
        int c;
        while (fin >> c) chromT_to_eval.push_back(c);
        if ((int)chromT_to_eval.size() != params.nbClients) {
            cerr << "ERROR: chromT file has " << chromT_to_eval.size()
                 << " clients, expected " << params.nbClients << endl;
            return 1;
        }
        cout << "========================================\n"
             << " Eval-only mode: " << evalChromTFile << "\n"
             << "========================================\n"
             << "Instance: " << params.nbClients << " clients, "
             << totalScen << " scenarios\n"
             << "Capacity: " << params.vehicleCapacity
             << "  PenCap: " << params.penaltyCapacity << "\n"
             << "chromT:";
        for (int v : chromT_to_eval) cout << " " << v;
        cout << "\n" << endl;
        goto do_eval;
    }

    {
    cout << "========================================\n"
         << " Two-Stage Stochastic CVRP Exact (MILP)\n"
         << "========================================\n"
         << "Instance: " << params.nbClients << " clients, "
         << totalScen << " scenarios\n"
         << "MILP scenarios: " << milpScen << "\n"
         << "Capacity: " << params.vehicleCapacity
         << "  MaxV: " << params.nbVehicles
         << "  PenCap: " << params.penaltyCapacity << "\n"
         << "Time limit: " << timeLim << "s\n" << endl;

    Result res = solveTwoStage(params, milpScen, timeLim, true);

    if (res.chromT.empty()) { cerr << "No solution!\n"; return 1; }
    if ((int)res.chromT.size() != params.nbClients)
        cerr << "WARNING: chromT size " << res.chromT.size()
             << " != " << params.nbClients << endl;
    chromT_to_eval = res.chromT;

    } // end MILP block

    do_eval:
    // ── Detailed evaluation ──
    cout << "\n========================================\n"
         << " Full Stochastic Evaluation (" << totalScen << " scen)\n"
         << "========================================" << endl;

    auto t0 = chrono::steady_clock::now();
    vector<ScenarioDetail> details = evaluateDetailed(params, chromT_to_eval, nthreads);
    auto t1 = chrono::steady_clock::now();
    double c2 = evaluateWithHGSSplit(params, chromT_to_eval, nthreads);
    auto t2 = chrono::steady_clock::now();

    // Aggregate
    double sumCost = 0, sumDist = 0, sumExcess = 0;
    int sumRoutes = 0, feasibleCount = 0;
    vector<double> vCost(totalScen), vDist(totalScen), vExcess(totalScen), vRoutes(totalScen);
    for (int s = 0; s < totalScen; s++) {
        sumCost   += details[s].totalCost;
        sumDist   += details[s].routeDist;
        sumExcess += details[s].capExcess;
        sumRoutes += details[s].nRoutes;
        if (details[s].capExcess < 1e-9) feasibleCount++;
        vCost[s]   = details[s].totalCost;
        vDist[s]   = details[s].routeDist;
        vExcess[s] = details[s].capExcess;
        vRoutes[s] = details[s].nRoutes;
    }
    double avgCost   = sumCost / totalScen;
    double avgDist   = sumDist / totalScen;
    double avgExcess = sumExcess / totalScen;
    double avgRoutes = (double)sumRoutes / totalScen;

    cout << "\n  O(N^2) Split eval time: "
         << chrono::duration<double>(t1 - t0).count() << "s\n"
         << "  O(N)   HGS Split cost:  " << c2 << "  ("
         << chrono::duration<double>(t2 - t1).count() << "s)\n"
         << "  Diff (HGS - exact):     " << (c2 - avgCost) << "\n" << endl;

    cout << "  ── Averages over " << totalScen << " scenarios ──\n"
         << "  Penalised cost:    " << avgCost << "\n"
         << "  Route distance:    " << avgDist << "\n"
         << "  Capacity excess:   " << avgExcess << "\n"
         << "  Num routes:        " << avgRoutes << "\n"
         << "  Feasibility rate:  " << (100.0 * feasibleCount / totalScen) << "%  ("
         << feasibleCount << "/" << totalScen << ")\n" << endl;

    cout << "  ── Per-scenario distributions ──" << endl;
    printStats("Penalised cost", vCost);
    printStats("Route distance", vDist);
    printStats("Capacity excess", vExcess);
    printStats("Num routes", vRoutes);

    cout << "\n========================================\n"
         << " SUMMARY\n"
         << "========================================\n"
         << "  Avg penalised cost:        " << avgCost << "\n"
         << "  Avg route distance:        " << avgDist << "\n"
         << "  Avg capacity excess:       " << avgExcess << "\n"
         << "  Avg num routes:            " << avgRoutes << "\n"
         << "  Feasibility rate:          " << (100.0 * feasibleCount / totalScen) << "%" << endl;

    return 0;
}
