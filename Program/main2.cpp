#include "Genetic.h"
#include "commandline.h"
#include "LocalSearch.h"
#include "Split.h"
#include "InstanceCVRPLIB.h"
#include <chrono>
#include <omp.h>

using namespace std;


int main(int argc, char *argv[])
{
	bool debug = false;

	ofstream myfile;
	try
	{
		// Reading the arguments of the program
		CommandLine commandline(argc, argv);
		int n_threads = commandline.ap.nthreads;
		int n_extra_senarios = commandline.ap.n_extra_senarios;
		int debug_frequency = commandline.ap.freqPrint;
		int timeLim = commandline.ap.timeLim;
		omp_set_num_threads(n_threads); 
	std::string logfile = "../anpy/logs/cpu"+std::to_string(n_threads)+".log";
  myfile.open (logfile);

		// Print all algorithm parameter values
		if (commandline.verbose) print_algorithm_parameters(commandline.ap);

		// Reading the data file and initializing some data structures
		if (commandline.verbose) std::cout << "----- READING INSTANCE: " << commandline.pathInstance << std::endl;
		InstanceCVRPLIB cvrp(commandline.pathInstance, commandline.isRoundingInteger);


        // generate scenarios

		Params params(cvrp.x_coords,cvrp.y_coords,cvrp.dist_mtx,cvrp.service_time,cvrp.demands,
			          cvrp.vehicleCapacity,cvrp.durationLimit,commandline.nbVeh,cvrp.isDurationConstraint,commandline.verbose,commandline.ap);
        params.generate_scenario_demands(n_extra_senarios);
		params.update_max_vehi();
		std::cout<<"Finished generating scenarios\n";


		
		std::cout<<"  n clients: "<<params.nbClients<<"\n";
		std::vector<int> client_ids;
		for (int i = 1; i < params.nbClients + 1; i++)
		{
			client_ids.push_back(i);
		}

		std::vector<int> best_permutation;
		double best_penalized_cost = 1.e20;
		long checked_scen = 0;

		int iter_count = 0;
		double split_total_time = 0;

	std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
		do{
			bool debug_local = debug;
			checked_scen += 1;
			if (checked_scen % debug_frequency == 0){
				debug_local = true;
			}
			if (debug_local) std::cout<<"\n\n  Starting "<<checked_scen<<"th permutation\n";
			// create individual, then split
			Individual indiv(params);
			// need to assign the chromT
			indiv.chromT.clear();
			for (int i = 0; i < client_ids.size(); i++){
				indiv.chromT.push_back(client_ids[i]);
			}
			// do split
			// std::vector<std::vector < std::vector <int>>> current_chromR_res(params.n_scenarios);
			// std::vector<Split> splits(params.n_scenarios,params);
			std::vector<double> split_time(params.n_scenarios,true);
			if (n_threads > 1){
			std::chrono::steady_clock::time_point split_begin = std::chrono::steady_clock::now();
				#pragma omp parallel for
				for (int s = 0; s < params.n_scenarios; s++){
					Split split(params);
					// Split split = splits[s];
					// current_chromR_res[s] = split.generalSplitReturn(indiv, params.nbVehicles, s);
					split.generalSplit(indiv, params.nbVehicles, s);
			std::chrono::steady_clock::time_point split_end = std::chrono::steady_clock::now();
			split_time[s] = std::chrono::duration_cast<std::chrono::nanoseconds>(split_end - split_begin).count()/1.e9;


					bool res1 = split.generateChromR(indiv, s) == 0;
					if (!res1){
						std::cout<<"!\n!\n!\n"<<res1<<"  need to use another split\n";
					}
				}
				double this_split_time = 0;
				for (int s = 0; s < params.n_scenarios; ++s){
					if (this_split_time < split_time[s]){
						this_split_time = split_time[s];
					}
				}
				split_total_time += this_split_time;
			}else{
				for (int s = 0; s < params.n_scenarios; s++){
	std::chrono::steady_clock::time_point split_begin = std::chrono::steady_clock::now();
					Split split(params);
					// Split split = splits[s];
					// current_chromR_res[s] = split.generalSplitReturn(indiv, params.nbVehicles, s);
					split.generalSplit(indiv, params.nbVehicles, s);
					std::chrono::steady_clock::time_point split_end = std::chrono::steady_clock::now();
					split_total_time += std::chrono::duration_cast<std::chrono::nanoseconds>(split_end - split_begin).count()/1.e9;
					// split.generateChromR(indiv, s);
					bool res1 = split.generateChromR(indiv, s) == 0;
					if (!res1){
						std::cout<<"!\n!\n!\n"<<res1<<"  need to use another split\n";
					}
				}
			}
			// indiv.syn_single_scen_chromR();
			indiv.evaluateCompleteCost(params);

			if (debug_local) {
				std::cout<<"EvalIndiv.penalizedCost   "<<indiv.eval.penalizedCost<<" / "<<best_penalized_cost<<"\n";
				// for (int s = 0; s < params.n_scenarios; ++s){
				// 	std::cout<<indiv.eval.penalizedCostScen[s]<<", ";
				// }
				// std::cout<<"\n";
				std::cout<<"EvalIndiv.nbRoutes   "<<indiv.eval.nbRoutes<<"\n";
				// for (int s = 0; s < params.n_scenarios; ++s){
				// 	std::cout<<indiv.eval.nbRoutesScen[s]<<", ";
				// }
				// std::cout<<"\n";
				std::cout<<"EvalIndiv.distance   "<<indiv.eval.distance<<"\n";
				// for (int s = 0; s < params.n_scenarios; ++s){
				// 	std::cout<<indiv.eval.distanceScen[s]<<", ";
				// }
				// std::cout<<"\n";
				std::cout<<"EvalIndiv.capacityExcess   "<<indiv.eval.capacityExcess<<"\n";
				// for (int s = 0; s < params.n_scenarios; ++s){
				// 	std::cout<<indiv.eval.capacityExcessScen[s]<<", ";
				// }
				// std::cout<<"\n";
				std::cout<<"EvalIndiv.durationExcess   "<<indiv.eval.durationExcess<<"\n";
				// for (int s = 0; s < params.n_scenarios; ++s){
				// 	std::cout<<indiv.eval.durationExcessScen[s]<<", ";
				// }
				// std::cout<<"\n";
				std::cout<<"EvalIndiv.isFeasible   "<<indiv.eval.isFeasible<<"\n";
				// for (int s = 0; s < params.n_scenarios; ++s){
				// 	std::cout<<indiv.eval.isFeasibleScen[s]<<", ";
				// }
				// std::cout<<"\n";
			}

			if (best_penalized_cost > indiv.eval.penalizedCost){
				best_penalized_cost = indiv.eval.penalizedCost;
				if (debug_local) std::cout<<"*****  New best solution found with cost: "<<best_penalized_cost<<"  *****\n";
				for (int i = 0; i < indiv.chromT.size(); i++)
				{
					best_permutation.push_back(indiv.chromT[i]);
				}
			}

			myfile<<split_total_time<<" "<<best_penalized_cost<<"\n";
			
			if (debug_local) {
				std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
				double scds = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count()/1000.0;
				std::cout<<"Elapsed time(T"<<n_threads<<"): "<<scds<<"s\n";
				std::cout<<"    Avg time(T"<<n_threads<<"): "<<split_total_time*1000 / checked_scen<<"s/kperm\n";
				if (timeLim > 0 && timeLim <=scds){
					std::cout<<"Time out\n";
					break;
				}
			}
			iter_count += 1;
		// } while (std::next_permutation(client_ids.begin(), client_ids.end()));
		} while (std::next_permutation(client_ids.begin(), client_ids.end()) && iter_count < commandline.ap.iterLim);
	
	


		

		// Running HGS
		// Genetic solver(params);
		// solver.run();
		
		// // Exporting the best solution
		// if (solver.population.getBestFound() != NULL)
		// {
		// 	if (params.verbose) std::cout << "----- WRITING BEST SOLUTION IN : " << commandline.pathSolution << std::endl;
		// 	solver.population.exportCVRPLibFormat(*solver.population.getBestFound(),commandline.pathSolution);
		// 	solver.population.exportSearchProgress(commandline.pathSolution + ".PG.csv", commandline.pathInstance);
		// }
		myfile.close();
	}
	catch (const string& e) { std::cout << "EXCEPTION | " << e << std::endl; }
	catch (const std::exception& e) { std::cout << "EXCEPTION | " << e.what() << std::endl; }
	return 0;
}
