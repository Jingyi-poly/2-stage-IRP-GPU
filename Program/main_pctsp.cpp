#include "PCTSPExact.h"
#include "commandline.h"
#include "InstanceCVRPLIB.h"
#include <chrono>
#include <iostream>
#include <iomanip>
#include <omp.h>

using namespace std;

int main(int argc, char *argv[])
{
    auto tAllStart = chrono::steady_clock::now();

    try
    {
        CommandLine commandline(argc, argv);
        omp_set_num_threads(commandline.ap.nthreads);

        if (commandline.verbose) print_algorithm_parameters(commandline.ap);
        cout << "----- READING INSTANCE: " << commandline.pathInstance << endl;

        InstanceCVRPLIB cvrp(commandline.pathInstance, commandline.isRoundingInteger);

        Params params(cvrp.x_coords, cvrp.y_coords, cvrp.dist_mtx,
                      cvrp.service_time, cvrp.demands,
                      cvrp.vehicleCapacity, cvrp.durationLimit,
                      commandline.nbVeh, cvrp.isDurationConstraint,
                      commandline.verbose, commandline.ap);

        params.generate_scenario_demands(commandline.ap.n_extra_senarios);
        params.update_max_vehi();
        params.generate_skip_penalties();

        cout << "Instance: " << params.nbClients << " clients, "
             << params.n_scenarios << " scenarios, cap=" << params.vehicleCapacity
             << ", penCap=" << fixed << setprecision(2) << params.penaltyCapacity << endl;

        double pctspTimeLimit = 600.0;
        if (commandline.ap.timeLim > 0)
            pctspTimeLimit = commandline.ap.timeLim;

        bool forceAllVisits = !commandline.ap.optionalVisit;
        cout << "Mode: " << (forceAllVisits ? "Full Stochastic MILP (all visits)" : "PCTSP (optional visits)") << endl;

        PCTSPExact solver(params, pctspTimeLimit, commandline.verbose);

        PCTSPResult stochResult;

        if (forceAllVisits) {
            // ── Full stochastic MILP: tour + per-scenario splits in one model ──
            stochResult = solver.solveFullStochasticMILP();
        } else {
            // ── 1. Deterministic PCTSP ──
            cout << "\n========================================" << endl;
            cout << " PHASE 1: Deterministic PCTSP (Gurobi)" << endl;
            cout << "========================================" << endl;
            PCTSPResult detResult = solver.solveDeterministic();

            if (!detResult.selectedNodes.empty()) {
                cout << fixed << setprecision(2);
                cout << "  Obj (prize - dist): " << detResult.objValue << endl;
                cout << "  Visited: " << detResult.selectedNodes.size() - 1
                     << "/" << params.nbClients << endl;
                cout << "  Tour distance: " << detResult.totalDistance << endl;
                cout << "  Optimal: " << (detResult.optimal ? "Yes" : "No") << endl;
                cout << "  Time: " << detResult.solveTime << "s" << endl;
            }

            // ── 2. Stochastic PCTSP (LS) ──
            cout << "\n========================================" << endl;
            cout << " Stochastic PCTSP (LS)" << endl;
            cout << "========================================" << endl;
            stochResult = solver.solveStochastic();
        }

        // ── Summary ──
        cout << "\n========================================" << endl;
        cout << " RESULT SUMMARY" << endl;
        cout << "========================================" << endl;
        cout << fixed << setprecision(2);
        cout << "  Stochastic cost:   " << stochResult.objValue << endl;
        cout << "  Tour distance:     " << stochResult.totalDistance << endl;
        cout << "  Visited:           " << stochResult.selectedNodes.size() - 1
             << "/" << params.nbClients << endl;
        cout << "  Time:              " << stochResult.solveTime << "s" << endl;

        cout << "  chromT:";
        for (int nd : stochResult.tour)
            if (nd != 0) cout << " " << nd;
        cout << endl;
    }
    catch (const string & e) { cout << "EXCEPTION | " << e << endl; }
    catch (const exception & e) { cout << "EXCEPTION | " << e.what() << endl; }

    auto tEnd = chrono::steady_clock::now();
    double tt = chrono::duration<double>(tEnd - tAllStart).count();
    cout << "\nTotal wall time: " << fixed << setprecision(2) << tt << "s" << endl;
    return 0;
}
