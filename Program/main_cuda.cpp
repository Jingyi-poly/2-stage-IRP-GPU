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

		const int gpuBatchSize = commandline.ap.gpuBatchSize;
		SplitCUDA splitCuda(params, gpuBatchSize);
		std::cout << "Finished generating scenarios  (n_scen=" << params.n_scenarios
				  << "  n_cli=" << params.nbClients << ")  [GPU mode]" << std::endl;

		// GPU batch evaluation function
		auto gpuEval = [&splitCuda, &params, fullNbClients](std::vector<Individual*>& batch) {
			int B = (int)batch.size();
			bool useOptional = params.ap.optionalVisit;

			// Pre-process optional visit filtering per individual
			std::vector<std::vector<int>> savedChromT;
			std::vector<int> emptyIndices;
			if (useOptional)
			{
				savedChromT.resize(B);
				for (int b = 0; b < B; b++)
				{
					Individual & indiv = *batch[b];
					if (indiv.clientVisited.empty()) continue;
					savedChromT[b] = indiv.chromT;
					indiv.chromT.clear();
					for (int c : savedChromT[b])
						if (indiv.clientVisited[c])
							indiv.chromT.push_back(c);
					if (indiv.chromT.empty())
					{
						indiv.eval = EvalIndivMultiScen();
						indiv.resetEval(params);
						double skipCost = 0.0;
						for (int c = 1; c <= fullNbClients; c++)
							skipCost += params.cli[c].skipPenalty;
						indiv.eval.penalizedCost = skipCost;
						indiv.chromT = savedChromT[b];
						emptyIndices.push_back(b);
					}
				}
			}

			// Build the non-empty batch for GPU
			std::vector<Individual*> gpuBatch;
			gpuBatch.reserve(B);
			for (int b = 0; b < B; b++)
			{
				bool isEmpty = false;
				for (int idx : emptyIndices)
					if (idx == b) { isEmpty = true; break; }
				if (!isEmpty)
					gpuBatch.push_back(batch[b]);
			}

			if (!gpuBatch.empty())
			{
				int Bg = (int)gpuBatch.size();
				if (useOptional)
				{
					for (auto* indiv : gpuBatch)
					{
						int nVisited = (int)indiv->chromT.size();
						params.nbClients = nVisited;
						splitCuda.setActiveClients(nVisited);
						std::vector<Individual*> single = {indiv};
						splitCuda.reset_batch(1);
						splitCuda.preprocess_batch(single, params.nbVehicles);
						splitCuda.generate_split_batch(1);
						splitCuda.evaluateOnGPU_batch(single);
					}
				}
				else
				{
					splitCuda.reset_batch(Bg);
					splitCuda.preprocess_batch(gpuBatch, params.nbVehicles);
					splitCuda.generate_split_batch(Bg);
					splitCuda.evaluateOnGPU_batch(gpuBatch);
				}
			}

			if (useOptional)
			{
				for (int b = 0; b < B; b++)
				{
					Individual & indiv = *batch[b];
					if (!savedChromT[b].empty())
					{
						double skipCost = 0.0;
						for (int c = 1; c <= fullNbClients; c++)
							if (!indiv.clientVisited[c])
								skipCost += params.cli[c].skipPenalty;
						indiv.eval.penalizedCost += skipCost;
						indiv.chromT = savedChromT[b];
					}
				}
				params.nbClients = fullNbClients;
				splitCuda.setActiveClients(fullNbClients);
			}
		};

		std::string logfile = "../anpy/logs/" + std::to_string(params.n_scenarios) + "_"
							  + std::to_string(commandline.ap.maxClient) + ".log";
		std::ofstream myfile(logfile);

		GeneticHGS solver(params, gpuEval, gpuBatchSize, true);
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
			double tourDist = 0;
			if (!best->chromT.empty()) {
				tourDist += params.timeCost[0][best->chromT[0]];
				for (int i = 0; i + 1 < (int)best->chromT.size(); i++)
					tourDist += params.timeCost[best->chromT[i]][best->chromT[i+1]];
				tourDist += params.timeCost[best->chromT.back()][0];
			}
			std::cout << "  tourDist(TSP): " << tourDist << std::endl;
			std::cout << "  chromT:";
			for (int c : best->chromT) std::cout << " " << c;
			std::cout << std::endl;
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
