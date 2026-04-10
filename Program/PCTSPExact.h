#ifndef PCTSP_EXACT_H
#define PCTSP_EXACT_H

#include "Params.h"
#include <vector>
#include <string>
#include <functional>

struct PCTSPResult {
    double objValue;
    double totalPrize;
    double totalDistance;
    std::vector<int> selectedNodes;
    std::vector<int> tour;
    double solveTime;
    bool optimal;
};

using TourEvalFunc = std::function<double(const std::vector<int>&)>;

class PCTSPExact {
public:
    PCTSPExact(const Params & params, double timeLimit = 600.0, bool verbose = true);

    PCTSPResult solveDeterministic();
    PCTSPResult solveStochastic();

    // Benders stochastic MILP. If gpuEval is provided, uses it instead of
    // internal CPU Split DP to evaluate tours in the callback.
    // gpuEval signature: (const vector<int>& perm) -> avg stochastic cost
    PCTSPResult solveFullStochasticMILP(TourEvalFunc gpuEval = nullptr);

    // Evaluate a tour (visiting selected clients in tour order) across all scenarios
    double evaluateTourStochastic(const std::vector<int> & tour,
                                   const std::vector<bool> & visited) const;

    // Evaluate a visited subset: TSP + stochastic Split DP
    double evaluateSubset(const std::vector<bool> & visited) const;

private:
    const Params & params;
    double timeLimit;
    bool verbose;
    int n; // nbClients + 1 (including depot)

    // Split DP: partition a permutation into routes for a single scenario
    double splitDP(const std::vector<int> & perm, int scenIdx) const;

    // Solve TSP on a subset of clients using Gurobi DFJ lazy
    std::vector<int> solveTSP(const std::vector<int> & clients) const;
};

#endif
