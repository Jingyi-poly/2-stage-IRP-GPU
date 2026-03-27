#include "GeneticHGS.h"
#include "commandline.h"
#include "InstanceCVRPLIB.h"
#include "SplitCUDA.h"
#include <chrono>
#include <omp.h>

using namespace std;

int main(int argc, char *argv[])
{
	auto tAllStart = std::chrono::steady_clock::now();

	try
	{
		CommandLine commandline(argc, argv);
		omp_set_num_threads(commandline.ap.nthreads);

		if (commandline.verbose) print_algorithm_parameters(commandline.ap);
		if (commandline.verbose)
			std::cout << "----- READING INSTANCE: " << commandline.pathInstance << std::endl;

		InstanceCVRPLIB cvrp(commandline.pathInstance, commandline.isRoundingInteger);

		Params params(cvrp.x_coords, cvrp.y_coords, cvrp.dist_mtx, cvrp.service_time, cvrp.demands,
					  cvrp.vehicleCapacity, cvrp.durationLimit, commandline.nbVeh,
					  cvrp.isDurationConstraint, commandline.verbose, commandline.ap);

		params.generate_scenario_demands(commandline.ap.n_extra_senarios);
		params.update_max_vehi();
		params.generate_skip_penalties();
		const int fullNbClients = params.nbClients;

		SplitCUDA splitCuda(params);
		std::cout << "Finished generating scenarios  (n_scen=" << params.n_scenarios
				  << "  n_cli=" << params.nbClients << ")  [GPU mode]" << std::endl;

		// GPU evaluation function
		auto gpuEval = [&splitCuda, &params, fullNbClients](Individual & indiv) {
			std::vector<int> fullChromT;
			bool useOptional = params.ap.optionalVisit && !indiv.clientVisited.empty();
			int nVisited = fullNbClients;

			if (useOptional)
			{
				fullChromT = indiv.chromT;
				indiv.chromT.clear();
				for (int c : fullChromT)
					if (indiv.clientVisited[c])
						indiv.chromT.push_back(c);
				nVisited = (int)indiv.chromT.size();
				if (nVisited == 0)
				{
					indiv.eval = EvalIndivMultiScen();
					indiv.resetEval(params);
					double skipCost = 0.0;
					for (int c = 1; c <= fullNbClients; c++)
						skipCost += params.cli[c].skipPenalty;
					indiv.eval.penalizedCost = skipCost;
					indiv.chromT = fullChromT;
					return;
				}
				params.nbClients = nVisited;
				splitCuda.setActiveClients(nVisited);
			}

			splitCuda.reset();
			splitCuda.preprocess(indiv, params.nbVehicles);
			splitCuda.generate_split();
			splitCuda.reconstruct_from_pred(indiv);
			indiv.evaluateCompleteCost(params);

			if (useOptional)
			{
				double skipCost = 0.0;
				for (int c = 1; c <= fullNbClients; c++)
					if (!indiv.clientVisited[c])
						skipCost += params.cli[c].skipPenalty;
				indiv.eval.penalizedCost += skipCost;
				params.nbClients = fullNbClients;
				indiv.chromT = fullChromT;
				splitCuda.setActiveClients(fullNbClients);
			}
		};

		std::string logfile = "../anpy/logs/" + std::to_string(params.n_scenarios) + "_"
							  + std::to_string(commandline.ap.maxClient) + ".log";
		std::ofstream myfile(logfile);

		GeneticHGS solver(params, gpuEval);
		solver.run(&myfile);
		myfile.close();

		const Individual * best = solver.getBestFound();
		if (best)
		{
			std::cout << "\n===== BEST SOLUTION =====" << std::endl;
			std::cout << "  penalizedCost: " << best->eval.penalizedCost << std::endl;
			std::cout << "  avg distance:  " << best->eval.distance / params.n_scenarios << std::endl;
			std::cout << "  avg capExcess: " << best->eval.capacityExcess / params.n_scenarios << std::endl;
			std::cout << "  isFeasible:    " << best->eval.isFeasible << std::endl;
			std::cout << "  nbRoutes:      " << best->eval.nbRoutes << std::endl;
			if (params.ap.optionalVisit && !best->clientVisited.empty())
			{
				int nVis = 0;
				for (int c = 1; c <= fullNbClients; c++)
					if (best->clientVisited[c]) nVis++;
				std::cout << "  visited/total: " << nVis << "/" << fullNbClients << std::endl;
			}
		}
	}
	catch (const string & e) { std::cout << "EXCEPTION | " << e << std::endl; }
	catch (const std::exception & e) { std::cout << "EXCEPTION | " << e.what() << std::endl; }

	auto tAllEnd = std::chrono::steady_clock::now();
	double totaltime = std::chrono::duration_cast<std::chrono::nanoseconds>(tAllEnd - tAllStart).count() / 1.e9;
	std::cout << "Total time: " << totaltime << "s" << std::endl;
	return 0;
}
