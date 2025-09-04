#include "Params.h"

// The universal constructor for both executable and shared library
// When the executable is run from the commandline,
// it will first generate an CVRPLIB instance from .vrp file, then supply necessary information.
Params::Params(
	const std::vector<double>& x_coords,
	const std::vector<double>& y_coords,
	const std::vector<std::vector<double>>& dist_mtx,
	const std::vector<double>& service_time,
	const std::vector<double>& demands,
	double vehicleCapacity,
	double durationLimit,
	int nbVeh,
	bool isDurationConstraint,
	bool verbose,
	const AlgorithmParameters& ap
)
	: ap(ap), isDurationConstraint(isDurationConstraint), nbVehicles(nbVeh), durationLimit(durationLimit),
	  vehicleCapacity(vehicleCapacity), timeCost(dist_mtx), verbose(verbose)
{
	// This marks the starting time of the algorithm
	startTime = clock();
	int maxClient = ap.maxClient;

	nbClients = (int)demands.size() - 1; // Need to substract the depot from the number of nodes
	if (maxClient > 0 && nbClients > maxClient ){
		nbClients = maxClient;
	}
	totalDemand = 0.;
	maxDemand = 0.;

	// Initialize RNG
	// ran.seed(ap.seed);
	seed = ap.seed;
	

	// check if valid coordinates are provided
	areCoordinatesProvided = (demands.size() == x_coords.size()) && (demands.size() == y_coords.size());

	cli = std::vector<Client>(nbClients + 1);
	for (int i = 0; i <= nbClients; i++)
	{
		// If useSwapStar==false, x_coords and y_coords may be empty.
		if (ap.useSwapStar == 1 && areCoordinatesProvided)
		{
			cli[i].coordX = x_coords[i];
			cli[i].coordY = y_coords[i];
			cli[i].polarAngle = CircleSector::positive_mod(
				32768. * atan2(cli[i].coordY - cli[0].coordY, cli[i].coordX - cli[0].coordX) / PI);
		}
		else
		{
			cli[i].coordX = 0.0;
			cli[i].coordY = 0.0;
			cli[i].polarAngle = 0.0;
		}

		cli[i].serviceDuration = service_time[i];
		cli[i].demand = demands[i];
		if (cli[i].demand > maxDemand) maxDemand = cli[i].demand;
		totalDemand += cli[i].demand;
	}

	if (verbose && ap.useSwapStar == 1 && !areCoordinatesProvided)
		std::cout << "----- NO COORDINATES HAVE BEEN PROVIDED, SWAP* NEIGHBORHOOD WILL BE DEACTIVATED BY DEFAULT" << std::endl;

	// Default initialization if the number of vehicles has not been provided by the user
	if (nbVehicles == INT_MAX)
	{
		nbVehicles = (int)std::ceil(1.3*totalDemand/vehicleCapacity) + 3;  // Safety margin: 30% + 3 more vehicles than the trivial bin packing LB
		if (verbose) 
			std::cout << "----- FLEET SIZE WAS NOT SPECIFIED: DEFAULT INITIALIZATION TO " << nbVehicles << " VEHICLES" << std::endl;
	}
	else
	{
		if (verbose)
			std::cout << "----- FLEET SIZE SPECIFIED: SET TO " << nbVehicles << " VEHICLES" << std::endl;
	}

	// Calculation of the maximum distance
	maxDist = 0.;
	for (int i = 0; i <= nbClients; i++)
		for (int j = 0; j <= nbClients; j++)
			if (timeCost[i][j] > maxDist) maxDist = timeCost[i][j];

	// Calculation of the correlated vertices for each customer (for the granular restriction)
	correlatedVertices = std::vector<std::vector<int> >(nbClients + 1);
	std::vector<std::set<int> > setCorrelatedVertices = std::vector<std::set<int> >(nbClients + 1);
	std::vector<std::pair<double, int> > orderProximity;
	for (int i = 1; i <= nbClients; i++)
	{
		orderProximity.clear();
		for (int j = 1; j <= nbClients; j++)
			if (i != j) orderProximity.emplace_back(timeCost[i][j], j);
		std::sort(orderProximity.begin(), orderProximity.end());

		for (int j = 0; j < std::min<int>(ap.nbGranular, nbClients - 1); j++)
		{
			// If i is correlated with j, then j should be correlated with i
			setCorrelatedVertices[i].insert(orderProximity[j].second);
			setCorrelatedVertices[orderProximity[j].second].insert(i);
		}
	}

	// Filling the vector of correlated vertices
	for (int i = 1; i <= nbClients; i++)
		for (int x : setCorrelatedVertices[i])
			correlatedVertices[i].push_back(x);

	// Safeguards to avoid possible numerical instability in case of instances containing arbitrarily small or large numerical values
	if (maxDist < 0.1 || maxDist > 100000)
		throw std::string(
			"The distances are of very small or large scale. This could impact numerical stability. Please rescale the dataset and run again.");
	if (maxDemand < 0.1 || maxDemand > 100000)
		throw std::string(
			"The demand quantities are of very small or large scale. This could impact numerical stability. Please rescale the dataset and run again.");
	if (nbVehicles < std::ceil(totalDemand / vehicleCapacity))
		throw std::string("Fleet size is insufficient to service the considered clients.");

	// A reasonable scale for the initial values of the penalties
	penaltyDuration = 1;
	penaltyCapacity = std::max<double>(0.1, std::min<double>(1000., maxDist / maxDemand));

	if (verbose)
		std::cout << "----- INSTANCE SUCCESSFULLY LOADED WITH " << nbClients << " CLIENTS AND " << nbVehicles << " VEHICLES" << std::endl;
}

void checkrandom(){
	std::mt19937 generator;
	generator = std::mt19937(14);
	std::uniform_int_distribution<int> distribution(1, 10000);
	std::cout<<distribution(generator)<<", "<<distribution(generator)<<", "<<distribution(generator)<<"\n";
	std::uniform_int_distribution<int> distribution2(1, 10000);
	generator = std::mt19937(14);
	std::cout<<distribution2(generator)<<", "<<distribution2(generator)<<", "<<distribution2(generator)<<"\n";

}


void Params::generate_scenario_demands(int n_extra_senarios){
	generator = std::mt19937(seed);
	n_scenarios = n_extra_senarios + 1;
	totalDemands = new double[n_scenarios];
	totalDemands[0] = totalDemand;
	std::cout<<"Generating "<<n_extra_senarios<<" extra scenarios\n";
	int max_demand = -1;
	int min_demand = 1e+8;
	std::vector< double > current_demands_sum(n_scenarios,nbVehicles * vehicleCapacity);
	std::cout<<"Max cap/veh "<<vehicleCapacity<<"  with "<<nbVehicles<<"cars\n";

	for (auto & client : cli) {
		// std::cout<<"Client with demand: "<<client.demand<<"\n";
		if (max_demand < client.demand){
			max_demand = client.demand;
		}
		if (min_demand > client.demand){
			min_demand = client.demand;
		}
	}
	if (min_demand <= 0){
		min_demand = 1.0;
	}
	std::cout<<"Max "<<max_demand<<"\nMin "<<min_demand<<"\n";
	int elap = (max_demand - min_demand);

	// std::uniform_int_distribution<int> distribution(min_demand, max_demand);
	// std::uniform_int_distribution<int> distribution(vehicleCapacity/3, vehicleCapacity);
	// std::uniform_int_distribution<int> distribution(1, vehicleCapacity/4);
	// std::uniform_int_distribution<int> distribution(vehicleCapacity*0.2, vehicleCapacity/2);
	std::uniform_int_distribution<int> distribution(1, vehicleCapacity);

	int remain_client = cli.size();
	// for (auto & client : cli) {
	// 	remain_client -= 1;
	// 	client.demands_scenarios.push_back(client.demand);
	// 	for (int i = 0; i < n_extra_senarios; ++i){
	for (int i = 0; i < n_extra_senarios; ++i){
		for (auto & client : cli) {
			// int randomNum = (rand() % elap) + min_demand;
			int randomNum = distribution(generator);
			// randomNum = vehicleCapacity/1.3;
			// if (current_demands_sum[i] - randomNum < remain_client){
			// 	randomNum = current_demands_sum[i] - remain_client;
			// }
			// if (cli.size() - remain_client <3  && i == 2){

			// std::cout<<"Client "<<cli.size() - remain_client<<" at Scenario "<<i<<"   load: "<<randomNum<<"  remain cap "<<current_demands_sum[i]<<"\n";
			// }
			// std::cout<<"Client "<<cli.size() - remain_client<<" at Scenario "<<i<<"   load: "<<randomNum<<"  remain cap "<<current_demands_sum[i]<<"\n";
			current_demands_sum[i] -= randomNum;
			client.demands_scenarios.push_back(randomNum);
			totalDemands[i+1] += randomNum;
		}
	}

}



void Params::generate_scenario_demands_dev(int n_extra_senarios){
	generator = std::mt19937(seed);
	n_scenarios = n_extra_senarios + 1;
	totalDemands = new double[n_scenarios];
	totalDemands[0] = totalDemand;
	std::cout<<"Generating "<<n_extra_senarios<<" extra scenarios\n";
	int max_demand = -1;
	int min_demand = 1e+8;
	std::vector< double > current_demands_sum(n_scenarios,nbVehicles * vehicleCapacity);
	std::cout<<"Max cap/veh "<<vehicleCapacity<<"  with "<<nbVehicles<<"cars\n";

	for (auto & client : cli) {
		// std::cout<<"Client with demand: "<<client.demand<<"\n";
		if (max_demand < client.demand){
			max_demand = client.demand;
		}
		if (min_demand > client.demand){
			min_demand = client.demand;
		}
	}
	if (min_demand <= 0){
		min_demand = 1.0;
	}
	std::cout<<"Max "<<max_demand<<"\nMin "<<min_demand<<"\n";
	// int elap = max_demand*1150.4;
	int elap = max_demand-min_demand;
	// std::uniform_int_distribution<int> distribution(min_demand, max_demand);
	// std::uniform_int_distribution<int> distribution(vehicleCapacity*0.2, vehicleCapacity/2);
	std::uniform_int_distribution<int> distribution(1, vehicleCapacity);

	int remain_client = cli.size();
	// for (auto & client : cli) {
	// 	remain_client -= 1;
	// 	client.demands_scenarios.push_back(client.demand);
	// 	for (int i = 0; i < n_extra_senarios; ++i){
	for (int i = 0; i < n_extra_senarios; ++i){
		for (auto & client : cli) {
			// int randomNum = (rand() % elap) + min_demand*0.2;
			// int randomNum = (rand() % elap) + min_demand;
			int randomNum = distribution(generator);
			// // randomNum = 1;
			// if (current_demands_sum[i] - randomNum < remain_client){
			// 	randomNum = current_demands_sum[i] - remain_client;
			// }
			// if (cli.size() - remain_client <3  && i == 2){

			// std::cout<<"Client "<<cli.size() - remain_client<<" at Scenario "<<i<<"   load: "<<randomNum<<"  remain cap "<<current_demands_sum[i]<<"\n";
			// }
			current_demands_sum[i] -= randomNum;
			client.demands_scenarios.push_back(randomNum);
			totalDemands[i+1] += randomNum;
		}
	}
	checkrandom();
}

