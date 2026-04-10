#include "commandline.h"
#include "InstanceCVRPLIB.h"
#include "Split.h"
#include "Individual.h"
#include <chrono>
#include <climits>
#include <cmath>
#include <fstream>
#include <iostream>
#include <omp.h>

using namespace std;

int main(int argc, char *argv[])
{
	if (argc < 3)
	{
		cerr << "Usage: cross_validate <instance> <chromT_file> [-nextrascen N] [-seed S] [-nthreads T]" << endl;
		return 1;
	}

	string instancePath = argv[1];
	string chromTPath = argv[2];
	int nextra = 99999;
	int seed = 1;
	int nthreads = 8;

	for (int i = 3; i < argc - 1; i++)
	{
		string arg = argv[i];
		if (arg == "-nextrascen") nextra = atoi(argv[++i]);
		else if (arg == "-seed") seed = atoi(argv[++i]);
		else if (arg == "-nthreads") nthreads = atoi(argv[++i]);
	}
	omp_set_num_threads(nthreads);

	AlgorithmParameters ap;
	ap.seed = seed;
	ap.n_extra_senarios = nextra;

	InstanceCVRPLIB cvrp(instancePath, true);
	Params params(cvrp.x_coords, cvrp.y_coords, cvrp.dist_mtx, cvrp.service_time, cvrp.demands,
				  cvrp.vehicleCapacity, cvrp.durationLimit, INT_MAX,
				  cvrp.isDurationConstraint, true, ap);

	params.generate_scenario_demands(nextra);
	params.update_max_vehi();
	params.generate_skip_penalties();

	cout << "Instance loaded: " << params.nbClients << " clients, "
		 << params.n_scenarios << " scenarios, "
		 << params.nbVehicles << " vehicles" << endl;

	ifstream fin(chromTPath);
	if (!fin.is_open())
	{
		cerr << "Cannot open chromT file: " << chromTPath << endl;
		return 1;
	}

	vector<int> chromT;
	int val;
	while (fin >> val)
		chromT.push_back(val);
	fin.close();

	cout << "chromT size: " << chromT.size() << " (expected: " << params.nbClients << ")" << endl;
	if ((int)chromT.size() != params.nbClients)
	{
		cerr << "ERROR: chromT size mismatch!" << endl;
		return 1;
	}

	Individual indiv(params, false);
	indiv.chromT = chromT;

	cout << "Evaluating with CPU Split (OpenMP " << nthreads << " threads, "
		 << params.n_scenarios << " scenarios)..." << endl;

	auto t0 = chrono::steady_clock::now();

	#pragma omp parallel for
	for (int s = 0; s < params.n_scenarios; s++)
	{
		Split split(params);
		split.generalSplit(indiv, params.nbVehicles, s);
		split.generateChromR(indiv, s);
	}
	indiv.evaluateCompleteCost(params);

	auto t1 = chrono::steady_clock::now();
	double evalTime = chrono::duration_cast<chrono::microseconds>(t1 - t0).count() / 1e6;

	cout << "\n===== CPU SPLIT EVALUATION =====" << endl;
	cout << "  penalizedCost: " << indiv.eval.penalizedCost << endl;
	cout << "  avg distance:  " << indiv.eval.distance / params.n_scenarios << endl;
	cout << "  avg capExcess: " << indiv.eval.capacityExcess / params.n_scenarios << endl;
	cout << "  avg durExcess: " << indiv.eval.durationExcess / params.n_scenarios << endl;
	cout << "  isFeasible:    " << indiv.eval.isFeasible << endl;
	cout << "  nbRoutes:      " << indiv.eval.nbRoutes << endl;
	cout << "  eval time:     " << evalTime << "s" << endl;

	// ── O(N²) exact Split DP (same as PCTSP) for comparison ──
	auto splitDP_exact = [&](const vector<int> & perm, int scenIdx) -> double {
		int sz = (int)perm.size();
		if (sz == 0) return 0.0;
		const double cap = params.vehicleCapacity;
		const double penCap = params.penaltyCapacity;
		vector<double> cost(sz + 1, 1e30);
		cost[0] = 0.0;
		for (int j = 1; j <= sz; j++) {
			double load = 0.0, routeDist = 0.0;
			for (int i = j; i >= 1; i--) {
				int cli = perm[i - 1];
				load += params.cli[cli].demands_scenarios[scenIdx];
				if (i == j) routeDist = params.timeCost[cli][0];
				else routeDist += params.timeCost[cli][perm[i]];
				double fullDist = params.timeCost[0][cli] + routeDist;
				double excess = max(0.0, load - cap);
				double segCost = fullDist + penCap * excess;
				if (cost[i - 1] + segCost < cost[j])
					cost[j] = cost[i - 1] + segCost;
			}
		}
		return cost[sz];
	};

	cout << "\nEvaluating with O(N^2) exact Split DP..." << endl;
	auto t2 = chrono::steady_clock::now();
	double totalExact = 0.0;
	#pragma omp parallel for reduction(+:totalExact)
	for (int s = 0; s < params.n_scenarios; s++)
		totalExact += splitDP_exact(chromT, s);
	double avgExact = totalExact / params.n_scenarios;
	auto t3 = chrono::steady_clock::now();
	double exactTime = chrono::duration_cast<chrono::microseconds>(t3 - t2).count() / 1e6;

	cout << "  O(N^2) penalizedCost: " << avgExact << endl;
	cout << "  O(N)   penalizedCost: " << indiv.eval.penalizedCost << endl;
	cout << "  Difference:           " << (indiv.eval.penalizedCost - avgExact)
		 << " (" << 100.0 * (indiv.eval.penalizedCost - avgExact) / avgExact << "%)" << endl;
	cout << "  O(N^2) eval time:     " << exactTime << "s" << endl;

	// Per-scenario comparison for first 10
	cout << "\nPer-scenario comparison (first 10):" << endl;
	cout << "  scen |  O(N) HGS  |  O(N^2) Exact | Diff" << endl;
	int nDiff = 0;
	for (int s = 0; s < min(10, params.n_scenarios); s++)
	{
		double hgsCost = indiv.eval.penalizedCostScen[s];
		double exactCost = splitDP_exact(chromT, s);
		double diff = hgsCost - exactCost;
		if (abs(diff) > 1e-6) nDiff++;
		cout << "  " << s << "    | " << hgsCost << " | " << exactCost
			 << " | " << diff << (abs(diff) > 1e-6 ? " ***" : "") << endl;
	}

	// Count total mismatches
	int totalDiff = 0;
	#pragma omp parallel for reduction(+:totalDiff)
	for (int s = 0; s < params.n_scenarios; s++)
	{
		double hgsCost = indiv.eval.penalizedCostScen[s];
		double exactCost = splitDP_exact(chromT, s);
		if (abs(hgsCost - exactCost) > 1e-6) totalDiff++;
	}
	cout << "\nScenarios with different Split results: " << totalDiff
		 << "/" << params.n_scenarios
		 << " (" << 100.0 * totalDiff / params.n_scenarios << "%)" << endl;

	return 0;
}
