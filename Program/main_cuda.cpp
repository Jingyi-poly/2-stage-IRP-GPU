#include "Genetic.h"
#include "commandline.h"
#include "LocalSearch.h"
#include "Split.h"
#include "InstanceCVRPLIB.h"
#include <chrono>
#include <omp.h>
#include <SplitCUDA.h>

using namespace std;


void test(int n_test,CommandLine & commandline,std::vector<int> & best_permutation){
	std::cout<<"\n\n\n\n"<<best_permutation.size()<<"\n";
	for (int i = 0; i < best_permutation.size(); ++i){
		std::cout<<best_permutation[i]<<", ";
	}
	try
	{
		// Reading the arguments of the program
		int n_threads = commandline.ap.nthreads;
		int n_extra_senarios = n_test;
		int debug_frequency = commandline.ap.freqPrint;
		int timeLim = commandline.ap.timeLim;
		omp_set_num_threads(n_threads); 
		

		InstanceCVRPLIB cvrp(commandline.pathInstance, commandline.isRoundingInteger);
        // generate scenarios
		Params params(cvrp.x_coords,cvrp.y_coords,cvrp.dist_mtx,cvrp.service_time,cvrp.demands,
			          cvrp.vehicleCapacity,cvrp.durationLimit,commandline.nbVeh,cvrp.isDurationConstraint,commandline.verbose,commandline.ap);
        params.generate_scenario_demands_dev(n_extra_senarios);
		params.update_max_vehi();
		SplitCUDA split(params);
		std::cout<<"Finished generating scenarios\n";


		
		std::cout<<"  n clients: "<<params.nbClients<<"\n";



		double best_penalized_cost = 1.e20;
		long checked_scen = 0;

		double gpu1_scds = 0.;
		double gpu2_scds = 0.;
		double gpu3_scds = 0.;
		double gpu4_scds = 0.;

		int iter_count = 0;
		
			// create individual, then split
			Individual indiv(params);
			// need to assign the chromT
			indiv.chromT.clear();
			for (int i = 0; i < best_permutation.size(); i++){
				indiv.chromT.push_back(best_permutation[i]);
			}
			// do split
			// std::vector<std::vector < std::vector <int>>> current_chromR_res(params.n_scenarios);

			split.reset();
			split.preprocess(indiv, params.nbVehicles);
			split.generate_split();
			split.reconstruct_from_pred(indiv);
			indiv.evaluateCompleteCost(params);

			std::cout<<"TEST CASE";
				std::cout<<"EvalIndiv.penalizedCost   "<<indiv.eval.penalizedCost<<" / "<<best_penalized_cost<<"\n";
				// for (int s = 0; s < params.n_scenarios; ++s){
				// 	if (indiv.eval.capacityExcessScen[s] < 0.1 ) std::cout<<s<<"   :   "<<indiv.eval.penalizedCostScen[s]<<", ";
				// }
				// std::cout<<"\n";
				// std::cout<<"\n";
				std::cout<<"EvalIndiv.nbRoutes   "<<indiv.eval.nbRoutes<<"\n";
				std::cout<<"EvalIndiv.distance   "<<indiv.eval.distance<<"\n";
				std::cout<<"EvalIndiv.capacityExcess   "<<indiv.eval.capacityExcess<<"\n";
				std::cout<<"EvalIndiv.durationExcess   "<<indiv.eval.durationExcess<<"\n";
				std::cout<<"EvalIndiv.isFeasible   "<<indiv.eval.isFeasible<<"\n";
			
			iter_count+=1;
	}
	catch (const string& e) { std::cout << "EXCEPTION | " << e << std::endl; }
	catch (const std::exception& e) { std::cout << "EXCEPTION | " << e.what() << std::endl; }
	
}



int main(int argc, char *argv[])
{
	std::chrono::steady_clock::time_point all_begin = std::chrono::steady_clock::now();

	ofstream myfile;
	bool debug = false;
	try
	{
		// Reading the arguments of the program
		CommandLine commandline(argc, argv);
		int n_threads = commandline.ap.nthreads;
		int n_extra_senarios = commandline.ap.n_extra_senarios;
		int debug_frequency = commandline.ap.freqPrint;
		int timeLim = commandline.ap.timeLim;
		omp_set_num_threads(n_threads); 
		std::string logfile = "../anpy/logs/"+std::to_string(commandline.ap.n_extra_senarios+1)+"_"+std::to_string(commandline.ap.maxClient)+".log";

		

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
		SplitCUDA split(params);
			// for (auto & client : params.cli) {
			// 	for (int i = 0; i < params.n_scenarios; ++i){
			// 		std::cout<<client.demands_scenarios[i]<<",";
			// 	}
			// 	std::cout<<"\n";
			// }
		std::cout<<"Finished generating scenarios\n";


		
		std::cout<<"  n clients: "<<params.nbClients<<"\n";
		std::vector<int> client_ids;
		for (int i = 1; i < params.nbClients + 1; i++)
		{
			client_ids.push_back(i);
		}

		std::vector<int> best_permutation;
		std::vector < std::vector < std::vector <int> > > best_chromR_scen(n_extra_senarios+1, std::vector<std::vector<int>>(100));
		double best_penalized_cost = 1.e20;
		double best_distance = 1.e20;
		double best_violation = 1.e20;
		long checked_scen = 0;

		double gpu1_scds = 0.;
		double gpu2_scds = 0.;
		double gpu3_scds = 0.;
		double gpu4_scds = 0.;

		int iter_count = 0;
		
  		myfile.open (logfile);
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

	std::chrono::steady_clock::time_point inner_begin = std::chrono::steady_clock::now();
			split.reset();
	std::chrono::steady_clock::time_point inner_begin1 = std::chrono::steady_clock::now();
	gpu1_scds += std::chrono::duration_cast<std::chrono::nanoseconds>(inner_begin1 - inner_begin).count();
			split.preprocess(indiv, params.nbVehicles);
	std::chrono::steady_clock::time_point inner_begin2 = std::chrono::steady_clock::now();
	gpu2_scds += std::chrono::duration_cast<std::chrono::nanoseconds>(inner_begin2 - inner_begin1).count();
			split.generate_split();
	std::chrono::steady_clock::time_point inner_begin3 = std::chrono::steady_clock::now();
	gpu3_scds += std::chrono::duration_cast<std::chrono::nanoseconds>(inner_begin3 - inner_begin2).count();
			split.reconstruct_from_pred(indiv);
	std::chrono::steady_clock::time_point inner_begin4 = std::chrono::steady_clock::now();
	gpu4_scds += std::chrono::duration_cast<std::chrono::nanoseconds>(inner_begin4 - inner_begin3).count();
			// break;
			// split.generalSplit(indiv, params.nbVehicles);




			// indiv.syn_single_scen_chromR();
			indiv.evaluateCompleteCost(params);
			if (false && best_penalized_cost > indiv.eval.penalizedCost){
			// if (checked_scen == 1){
				best_penalized_cost = indiv.eval.penalizedCost;
				best_distance = indiv.eval.distance;
				best_violation = indiv.eval.capacityExcess;
				std::cout<<"*****  New best solution found with cost: "<<best_penalized_cost<<"   pen "<<indiv.eval.capacityExcess<<"  *****\n";
				best_permutation.clear();
				for (int i = 0; i < indiv.chromT.size(); i++)
				{
					best_permutation.push_back(indiv.chromT[i]);
				}
				best_chromR_scen.clear();
				best_chromR_scen.resize(indiv.chromR_scen.size());
				for (int s = 0; s < indiv.chromR_scen.size(); ++s){
					best_chromR_scen[s].resize(indiv.chromR_scen[s].size());
					for (int r = 0; r < indiv.chromR_scen[s].size(); ++r){
						if (indiv.chromR_scen[s][r].empty()){
							continue;
						}
						for (int i = 0; i < indiv.chromR_scen[s][r].size(); ++i){
							best_chromR_scen[s][r].push_back(indiv.chromR_scen[s][r][i]);
						}
					}
				}
			}

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

			myfile<<(gpu1_scds+gpu2_scds+gpu3_scds)/1.e9<<" "<<checked_scen<<" "<<best_penalized_cost<<"\n";
			myfile.flush();
			if (debug_local || true) {
				std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
				double scds = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count()/1000.0;
				std::cout<<"Elapsed time(T"<<n_threads<<"): "<<scds<<"s\n";
				std::cout<<"    GPU part 1 (T"<<n_threads<<"): "<<gpu1_scds/1.e9<<"s\n";
				std::cout<<"    GPU part 2 (T"<<n_threads<<"): "<<gpu2_scds/1.e9<<"s\n";
				std::cout<<"    GPU part 3 (T"<<n_threads<<"): "<<gpu3_scds/1.e9<<"s\n";
				std::cout<<"    GPU part 4 (T"<<n_threads<<"): "<<gpu4_scds/1.e9<<"s\n";
				std::cout<<"    Avg time(T"<<n_threads<<"): "<<(gpu1_scds+gpu2_scds+gpu3_scds)*1000/1.e9 / checked_scen<<"s/kperm\n";
				if (timeLim > 0 && timeLim <=scds){
					std::cout<<"Time out\n";
					break;
				}
			}
			iter_count+=1;
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
		// for (int s = 0; s < best_chromR_scen.size(); ++s){
		// 	std::cout<<"Scenario "<<s<<"\n";
		// 	for (int r = 0; r < best_chromR_scen[s].size(); ++r){
		// 		if (best_chromR_scen[s][r].empty()){
		// 			continue;
		// 		}
		// 		std::cout<<"  car "<<r<<": ";
		// 		int ld = 0;
		// 		for (int i = 0; i < best_chromR_scen[s][r].size(); ++i){
		// 			ld += params.cli[best_chromR_scen[s][r][i]].demands_scenarios[s];
		// 			std::cout<<best_chromR_scen[s][r][i]<<"("<<params.cli[best_chromR_scen[s][r][i]].demands_scenarios[s]<<")"<<" -> ";
		// 		}
		// 		std::cout<<"    "<<ld<<"\n";
		// 	}
		// }
		std::cout<<"Best Obj:   "<<best_penalized_cost<<"\n";
		std::cout<<"Distance:   "<<best_distance<<"\n";
		std::cout<<"     Vio:   "<<best_violation<<"\n";

		
	
  		myfile.close();
	}
	// test(999999, commandline, best_permutation);

	catch (const string& e) { std::cout << "EXCEPTION | " << e << std::endl; }
	catch (const std::exception& e) { std::cout << "EXCEPTION | " << e.what() << std::endl; }

	std::chrono::steady_clock::time_point all_end = std::chrono::steady_clock::now();
	double totaltime = std::chrono::duration_cast<std::chrono::nanoseconds>(all_end - all_begin).count()/1.e9;
	std::cout<<"Total time: "<<totaltime<<"s\n";
	return 0;
}
