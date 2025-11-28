#include "Individual.h" 

// void Individual::evaluateCompleteCost(const Params & params)
// {
// 	oeveral_eval = EvalIndiv();
// 	for (int s = 0; s < params.n_scenarios; ++s){
// 		eval = EvalIndiv();
// 		for (int r = 0; r < params.nbVehicles; r++)
// 		{
// 			if (!chromR[r].empty())
// 			{
// 				double distance = params.timeCost[0][chromR[r][0]];
// 				double service = params.cli[chromR[r][0]].serviceDuration;
// 				// double load = params.cli[chromR[r][0]].demand;
// 				std::vector<double> load;
// 				load.push_back(params.cli[chromR[r][0]].demands_scenarios[s]);
// 				predecessors[chromR[r][0]] = 0;
// 				for (int i = 1; i < (int)chromR[r].size(); i++)
// 				{
// 					distance += params.timeCost[chromR[r][i-1]][chromR[r][i]];
// 					// load += params.cli[chromR[r][i]].demand;
// 					load[s] += params.cli[chromR[r][i]].demands_scenarios[s];
// 					service += params.cli[chromR[r][i]].serviceDuration;
// 					predecessors[chromR[r][i]] = chromR[r][i-1];
// 					successors[chromR[r][i-1]] = chromR[r][i];
// 				}
// 				successors[chromR[r][chromR[r].size()-1]] = 0;
// 				distance += params.timeCost[chromR[r][chromR[r].size()-1]][0];
// 				eval.distance += distance;
// 				eval.nbRoutes++;
// 				if (load[s] > params.vehicleCapacity) eval.capacityExcess += load[s] - params.vehicleCapacity;
// 				if (distance + service > params.durationLimit) eval.durationExcess += distance + service - params.durationLimit;
// 			}
// 		}
// 		eval.penalizedCost = eval.distance + eval.capacityExcess*params.penaltyCapacity + eval.durationExcess*params.penaltyDuration;
// 		eval.isFeasible = (eval.capacityExcess < MY_EPSILON && eval.durationExcess < MY_EPSILON);

// 		oeveral_eval.penalizedCost += eval.penalizedCost;
// 		oeveral_eval.distance += eval.distance;
// 		oeveral_eval.capacityExcess += eval.capacityExcess;
// 		oeveral_eval.durationExcess += eval.durationExcess;
// 		oeveral_eval.isFeasible = oeveral_eval.isFeasible && eval.isFeasible;
// 		if (eval.nbRoutes > oeveral_eval.nbRoutes){
// 			oeveral_eval.nbRoutes = eval.nbRoutes;
// 		}
// 	}

// }
void Individual::resetEval(const Params & params){
	eval.penalizedCostScen = std::vector <double>(params.n_scenarios);
	eval.nbRoutesScen = std::vector <int>(params.n_scenarios);
	eval.distanceScen = std::vector <double>(params.n_scenarios);
	eval.capacityExcessScen = std::vector <double>(params.n_scenarios);
	eval.durationExcessScen = std::vector <double>(params.n_scenarios);
	eval.isFeasibleScen = std::vector <bool>(params.n_scenarios, true);
}

void Individual::evaluateCompleteCost(const Params & params,bool debug)
{
	eval = EvalIndivMultiScen();
	resetEval(params);



	for (int s = 0; s < params.n_scenarios; ++s){

		EvalIndiv scen_eval = EvalIndiv();
		for (int r = 0; r < params.nbVehicles; r++)
		{

			if (!chromR_scen[s][r].empty())
			{
				double distance = params.timeCost[0][chromR_scen[s][r][0]];
				double service = params.cli[chromR_scen[s][r][0]].serviceDuration;
				// double load = params.cli[chromR_scen[s][r][0]].demand;
				double load = params.cli[chromR_scen[s][r][0]].demands_scenarios[s];
				// std::vector<double> load;
				// load.push_back(params.cli[chromR_scen[s][r][0]].demands_scenarios[s]);
				predecessors[chromR_scen[s][r][0]] = 0;
				
				for (int i = 1; i < (int)chromR_scen[s][r].size(); i++)
				{
					distance += params.timeCost[chromR_scen[s][r][i-1]][chromR_scen[s][r][i]];
					// load += params.cli[chromR_scen[s][r][i]].demand;
					load += params.cli[chromR_scen[s][r][i]].demands_scenarios[s];
					service += params.cli[chromR_scen[s][r][i]].serviceDuration;
					predecessors[chromR_scen[s][r][i]] = chromR_scen[s][r][i-1];
					successors[chromR_scen[s][r][i-1]] = chromR_scen[s][r][i];
				}
				successors[chromR_scen[s][r][chromR_scen[s][r].size()-1]] = 0;
				distance += params.timeCost[chromR_scen[s][r][chromR_scen[s][r].size()-1]][0];
				scen_eval.distance += distance;
				scen_eval.nbRoutes++;
				if (load > params.vehicleCapacity) {
					scen_eval.capacityExcess += load - params.vehicleCapacity;
					// std::cout<<"Scen "<<s<<" load penalty: "<< load - params.vehicleCapacity<<"\n";
				}
				if (distance + service > params.durationLimit) scen_eval.durationExcess += distance + service - params.durationLimit;

			}
		}
		scen_eval.penalizedCost = scen_eval.distance + scen_eval.capacityExcess*params.penaltyCapacity + scen_eval.durationExcess*params.penaltyDuration;
		scen_eval.isFeasible = (scen_eval.capacityExcess < MY_EPSILON && scen_eval.durationExcess < MY_EPSILON);

		eval.penalizedCost += scen_eval.penalizedCost;
		// eval.penalizedCost = std::max( eval.penalizedCost,scen_eval.penalizedCost);
		// eval.penalizedCost = std::min( eval.penalizedCost,scen_eval.penalizedCost);
		eval.distance += scen_eval.distance;
		eval.capacityExcess += scen_eval.capacityExcess;
		eval.durationExcess += scen_eval.durationExcess;
		eval.isFeasible = eval.isFeasible && scen_eval.isFeasible;
		if (scen_eval.nbRoutes > eval.nbRoutes){
			eval.nbRoutes = scen_eval.nbRoutes;
		}
		
		eval.penalizedCostScen[s] = scen_eval.penalizedCost;
		eval.nbRoutesScen[s] = scen_eval.nbRoutes;
		eval.distanceScen[s] = scen_eval.distance;
		eval.capacityExcessScen[s] = scen_eval.capacityExcess;
		eval.durationExcessScen[s] = scen_eval.durationExcess;
		eval.isFeasibleScen[s] = scen_eval.isFeasible;
	}
	eval.penalizedCost = eval.penalizedCost / params.n_scenarios;
}


void Individual::evaluateCompleteCost(const Params & params)
{
	eval = EvalIndivMultiScen();
	resetEval(params);

		// for (auto &c : params.cli){
		// 	for (int i = 0; i < params.n_scenarios;++i){
		// 		std::cout<<" "<<c.demands_scenarios[i];
		// 	}
		// 	std::cout<<"\n";
		// }
		// std::cout<<params.n_scenarios<<"\n";

	for (int s = 0; s < params.n_scenarios; ++s){
		EvalIndiv scen_eval = EvalIndiv();
		// for (int r = 0; r < params.nbVehicles; r++)
		// {
		// 	if (chromR_scen[s][r].empty()){
		// 		continue;
		// 	}
		// 	std::cout<<"Scenario "<<s<<"   Vehicle "<<r<<"\n     ";
		// 	for (int i = 0; i < (int)chromR_scen[s][r].size(); i++)
		// 	{
		// 		std::cout<<chromR_scen[s][r][i]<<" <- ";
		// 	}
		// 	std::cout<<"\n";
		// }
		for (int r = 0; r < params.nbVehicles; r++)
		{

			if (!chromR_scen[s][r].empty())
			{
				double distance = params.timeCost[0][chromR_scen[s][r][0]];
				double service = params.cli[chromR_scen[s][r][0]].serviceDuration;
				// double load = params.cli[chromR_scen[s][r][0]].demand;
				double load = params.cli[chromR_scen[s][r][0]].demands_scenarios[s];
				// std::vector<double> load;
				// load.push_back(params.cli[chromR_scen[s][r][0]].demands_scenarios[s]);
				predecessors[chromR_scen[s][r][0]] = 0;
				
				for (int i = 1; i < (int)chromR_scen[s][r].size(); i++)
				{
					distance += params.timeCost[chromR_scen[s][r][i-1]][chromR_scen[s][r][i]];
					// std::cout<<chromR_scen[s][r][i-1]<<" to "<<chromR_scen[s][r][i]<<"  dist: "<<params.timeCost[chromR_scen[s][r][i-1]][chromR_scen[s][r][i]]<<"\n";
					// load += params.cli[chromR_scen[s][r][i]].demand;
					load += params.cli[chromR_scen[s][r][i]].demands_scenarios[s];
					service += params.cli[chromR_scen[s][r][i]].serviceDuration;
					predecessors[chromR_scen[s][r][i]] = chromR_scen[s][r][i-1];
					successors[chromR_scen[s][r][i-1]] = chromR_scen[s][r][i];
				}
				successors[chromR_scen[s][r][chromR_scen[s][r].size()-1]] = 0;
				distance += params.timeCost[chromR_scen[s][r][chromR_scen[s][r].size()-1]][0];
				scen_eval.distance += distance;
				scen_eval.nbRoutes++;
				if (load > params.vehicleCapacity) {
					scen_eval.capacityExcess += load - params.vehicleCapacity;
					// std::cout<<"Scen "<<s<<" load penalty: "<< load - params.vehicleCapacity<<"\n";
				}
				if (distance + service > params.durationLimit) scen_eval.durationExcess += distance + service - params.durationLimit;
		// std::cout<<"Vehicle "<<r<<"   load: "<<load<<" / "<<params.vehicleCapacity<<"  excess: "<<scen_eval.capacityExcess<<"\n";
			}
		}
		// int j;std::cin>>j;
		scen_eval.penalizedCost = scen_eval.distance + scen_eval.capacityExcess*params.penaltyCapacity + scen_eval.durationExcess*params.penaltyDuration;
		scen_eval.isFeasible = (scen_eval.capacityExcess < MY_EPSILON && scen_eval.durationExcess < MY_EPSILON);

		eval.penalizedCost += scen_eval.penalizedCost;
		// eval.penalizedCost = std::max( eval.penalizedCost,scen_eval.penalizedCost);
		// eval.penalizedCost = std::min( eval.penalizedCost,scen_eval.penalizedCost);
		eval.distance += scen_eval.distance;
		eval.capacityExcess += scen_eval.capacityExcess;
		eval.durationExcess += scen_eval.durationExcess;
		eval.isFeasible = eval.isFeasible && scen_eval.isFeasible;
		if (scen_eval.nbRoutes > eval.nbRoutes){
			eval.nbRoutes = scen_eval.nbRoutes;
		}
		
		eval.penalizedCostScen[s] = scen_eval.penalizedCost;
		eval.nbRoutesScen[s] = scen_eval.nbRoutes;
		eval.distanceScen[s] = scen_eval.distance;
		eval.capacityExcessScen[s] = scen_eval.capacityExcess;
		eval.durationExcessScen[s] = scen_eval.durationExcess;
		eval.isFeasibleScen[s] = scen_eval.isFeasible;
	}
	eval.penalizedCost = eval.penalizedCost / params.n_scenarios;
	
}

Individual::Individual(Params & params)
{
	successors = std::vector <int>(params.nbClients + 1);
	predecessors = std::vector <int>(params.nbClients + 1);
	chromR = std::vector < std::vector <int> >(params.nbVehicles);
	chromR_scen = std::vector < std::vector < std::vector <int> > >(params.n_scenarios, std::vector < std::vector <int> >(params.nbVehicles));
	
	chromT = std::vector <int>(params.nbClients);
	for (int i = 0; i < params.nbClients; i++) chromT[i] = i + 1;
	std::shuffle(chromT.begin(), chromT.end(), params.ran);
	eval.penalizedCost = 1.e30;	
}

Individual::Individual(Params & params, std::string fileName) : Individual(params)
{
	double readCost;
	chromT.clear();
	std::ifstream inputFile(fileName);
	if (inputFile.is_open())
	{
		std::string inputString;
		inputFile >> inputString;
		// Loops in the input file as long as the first line keyword is "Route"
		for (int r = 0; inputString == "Route"; r++)
		{
			inputFile >> inputString;
			getline(inputFile, inputString);
			std::stringstream ss(inputString);
			int inputCustomer;
			while (ss >> inputCustomer) // Loops as long as there is an integer to read in this route
			{
				chromT.push_back(inputCustomer);
				chromR[r].push_back(inputCustomer);
			}
			inputFile >> inputString;
		}
		if (inputString == "Cost") inputFile >> readCost;
		else throw std::string("Unexpected token in input solution");

		// Some safety checks and printouts
		evaluateCompleteCost(params);
		if ((int)chromT.size() != params.nbClients) throw std::string("Input solution does not contain the correct number of clients");
		if (!eval.isFeasible) throw std::string("Input solution is infeasible");
		if (eval.penalizedCost != readCost)throw std::string("Input solution has a different cost than announced in the file");
		if (params.verbose) std::cout << "----- INPUT SOLUTION HAS BEEN SUCCESSFULLY READ WITH COST " << eval.penalizedCost << std::endl;
	}
	else 
		throw std::string("Impossible to open solution file provided in input in : " + fileName);
}
