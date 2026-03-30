#ifndef PCTSP_EXACT_H
#define PCTSP_EXACT_H

#include "Params.h"
#include <vector>
#include <string>

struct PCTSPResult {
    double objValue;
    double totalPrize;
    double totalDistance;
    std::vector<int> selectedNodes;
    std::vector<int> tour;
    double solveTime;
    bool optimal;
};

class PCTSPExact {
public:
    PCTSPExact(const Params & params, double timeLimit = 600.0, bool verbose = true);

    // Deterministic PCTSP: maximize Σ prize_j * y_j - Σ c_ij * x_ij
    // prizes[i] = skipPenalty[i] for clients, 0 for depot
    PCTSPResult solveDeterministic();

    // Stochastic PCTSP: for each candidate subset, evaluate expected Split cost
    // over all scenarios. Returns the best subset + tour evaluated stochastically.
    PCTSPResult solveStochastic();

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
