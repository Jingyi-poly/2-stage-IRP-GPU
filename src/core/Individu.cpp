#include "irp/core/Individu.h"
#include <chrono>
#include <thread>

// gRPCcorrelationFile
#include <grpcpp/grpcpp.h>
#include "irp.grpc.pb.h"

#include <random> // New
#include "irp/utils/ScenarioUtils.h" // addLogStatisticsfunction

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using irp::IrpService;
// Remove conflicting using declarations

bool g_trace = false;
int g_ms = 0;

// +++++ THREAD SAFETY: Initialize static mutex for gRPC channel creation +++++
std::mutex Individu::grpc_channel_mutex_;

// +++++ GPU STATISTICS: Initialize static counters +++++
std::atomic<int> Individu::total_gpu_calls_(0);
std::atomic<int> Individu::total_scenarios_processed_(0);
std::mutex Individu::gpu_stats_mutex_;

// simpleWaitFunction,Parameterforseconds
void wait(int milliseconds) {
  std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

// constructeur d'un Individu comme simple conteneur
Individu::Individu(Params *params) : params(params)
{
	vector<int> tempVect;
	pattern p;
	p.dep = 0;
	p.pat = 0;
	localSearch = new LocalSearch();
	// Initialize member variables
	estValide = true;
	isFitnessComputed = false;
	// Initialize general_individu_ to nullptr to avoid dangling pointers
	general_individu_ = nullptr;

	for (int i = 0; i <= params->nbDays; i++)
	{
		chromT.push_back(tempVect);
		chromR.push_back(tempVect);
		for (int j = 0; j < params->nombreVehicules[i]; j++)
			chromR[i].push_back(-1);
	}

	for (int i = 0; i < params->nbClients + params->nbDepots; i++)
		chromP.push_back(p);

	for (int i = 0; i < params->nbDepots + params->nbClients; i++)
	{
		suivants.push_back(tempVect);
		precedents.push_back(tempVect);
	}
}

// constructeur d'un individu aleatoire avec toutes les structures de recherche (split)
// Build an individual with all search structures
Individu::Individu(Params *params, double facteurSurete) : params(params)
{
	vector<int> tempVect;
	vector<vector<int>> tempVect2;
	localSearch = new LocalSearch();
	isFitnessComputed = false;
	// Initialize member variables
	estValide = true;
	coutSol.evaluation = 0;
	coutSol.fitness = 0;
	coutSol.capacityViol = 0;
	coutSol.lengthViol = 0;
	// Initialize general_individu_ to nullptr to avoid dangling pointers
	general_individu_ = nullptr;
	int j, temp;

	// IRP, this construction has to be fully changed.
	// There is no more concept of pattern
	// And the chromP is useless
	// So, for now let's simply assume that we serve all customers in one delivery on day 1 (we ignore the product availability at the supplier for now)
	// Later on we will arrange this construction
	chromT = vector<vector<int>> (params->nbDays + 1);
	chromL = vector<vector<double>> (params->nbDays + 1, vector<double>(params->nbClients + params->nbDepots, 0.));
	chromR = vector<vector<int>> (params->nbDays + 1, vector<int>(params->nombreVehicules[1], -1));

	bool force_delivery = !params->force_delivery_clients_.empty();
	std::vector<int> vec_force_delivery_clients = params->force_delivery_clients_;
	// OPTION 2 -- JUST IN TIME POLICY This code first considers initial inventory,then decides if service is needed,and when to provide.
	
	if (force_delivery) {
		for (int i = params->nbDepots; i < params->nbClients + params->nbDepots; i++) {
			if (std::find(vec_force_delivery_clients.begin(), vec_force_delivery_clients.end(), i) != vec_force_delivery_clients.end()) {
				chromL[1][i] = params->cli[i].maxInventory - params->cli[i].startingInventory;
			} else {
				chromL[1][i] = 0;
			}
		}
		for (int client : vec_force_delivery_clients) {
			chromT[1].push_back(client);
		}
		double startInventory;
		double dailyDemand;
		for (int i = params->nbDepots; i < params->nbClients + params->nbDepots; i++)
		{
			startInventory = chromL[1][i] > 0 ? params->cli[i].maxInventory : params->cli[i].startingInventory;
			startInventory -= params->cli[i].dailyDemand[1];
			for (int k = 2; k <= params->nbDays; k++)
			{
				if (startInventory >= params->cli[i].dailyDemand[k])
				{
					// enough initial inventory, no need to service
					startInventory -= params->cli[i].dailyDemand[k];
					chromL[k][i] = 0;
				}
				else
				{
					// not enough initial inventory, just in time policy for the initial solution
					dailyDemand = params->cli[i].maxInventory - startInventory; // CalculateReplenishment quantityalso need toaccording toOU PolicyCalculate,Default
					startInventory = params->cli[i].maxInventory;
					chromL[k][i] = dailyDemand;
					chromT[k].push_back(i);
				}
			}
		}

		// And shuffle the whole solution
		// Shuffle chromT to randomize customer sequence in route
		for (int k = 2; k <= params->nbDays; k++)
		{
			for (int i = 0; i <= (int)chromT[k].size() - 1; i++)
			{
				j = i + params->rng->genrand64_int64() % ((int)chromT[k].size() - i); //iandchromT[k].size() - 1betweenrandomintegerj.
				//swap i,j
				temp = chromT[k][i]; 
				chromT[k][i] = chromT[k][j]; 
				chromT[k][j] = temp;
			}
		}

	} else {
		double startInventory;
		double dailyDemand;
		for (int i = params->nbDepots; i < params->nbClients + params->nbDepots; i++)
		{
			startInventory = params->cli[i].startingInventory;
			for (int k = 1; k <= params->nbDays; k++)
			{
				if (startInventory >= params->cli[i].dailyDemand[k])
				{
					// enough initial inventory, no need to service
					startInventory -= params->cli[i].dailyDemand[k];
					chromL[k][i] = 0;
				}
				else
				{
					// not enough initial inventory, just in time policy for the initial solution
					dailyDemand = params->cli[i].maxInventory - startInventory; // CalculateReplenishment quantityalso need toaccording toOU PolicyCalculate,Default
					startInventory = params->cli[i].maxInventory;
					chromL[k][i] = dailyDemand;
					chromT[k].push_back(i);
				}
			}
		}

		// And shuffle the whole solution
		// Shuffle chromT to randomize customer sequence in route
		for (int k = 1; k <= params->nbDays; k++)
		{
			for (int i = 0; i <= (int)chromT[k].size() - 1; i++)
			{
				j = i + params->rng->genrand64_int64() % ((int)chromT[k].size() - i); //iandchromT[k].size() - 1betweenrandomintegerj.
				//swap i,j
				temp = chromT[k][i]; 
				chromT[k][i] = chromT[k][j]; 
				chromT[k][j] = temp;
			}
		}
	}

	// initialisation of the other structures
	//Initializeotherdatastructure: Initializepred2Dvector,appearswithorSorthaveclose/related.Initializepotentiels, suivants, andprecedentsvector.
	
	for (int k = 0; k <= params->nbDays; k++)
	{
		pred.push_back(tempVect2);
		for (int i = 0; i < params->nombreVehicules[k] + 1; i++)
		{
			// pred is a 3D integer vector storing predecessor nodes
			pred[k].push_back(tempVect); 
			pred[k][i].push_back(0);
			for (int j = 0; j < (int)params->nbClients + params->nbDepots + 1; j++)
				pred[k][i].push_back(0);
		}
	}
	// potentiels is a 2D float vector for split algorithm distances
 // potentiels[i+1] represents distance potential
	vector<double> potTemp;
	for (int i = 0; i <= params->nombreVehicules[1]; i++)
	{
		potentiels.push_back(potTemp);
		for (int j = 0; j < (int)params->nbClients + params->nbDepots + 1; j++)
			potentiels[i].push_back(1.e30);
	}
	potentiels[0][0] = 0;

	for (int i = 0; i < params->nbDepots + params->nbClients; i++)
	{
		suivants.push_back(tempVect);
		precedents.push_back(tempVect);
	}
}

Individu::Individu(ParamsList *params_list) : params(params_list->general_params_), control_day_1_(params_list->control_day_1_)
{
	is_scenario_ = true; // Multi-scenario mode
	// InitializeallMemberVariableInitializeError
	estValide = true;
	isFitnessComputed = false;
	localSearch = nullptr;

	// Initializesub_individus_vector
	// threadsafe:resizeafter/back,threadsWritedifferentIndexPosition,nodatarace
	// First usenullptrInitialize,ensure exception safety
	sub_individus_.resize(params_list->nb_scenarios_, nullptr);

	// ParallelCreateeachScenarioIndividuObject
	BatchRunFunc([&](int s) {
		sub_individus_[s] = new Individu(params_list->vec_params_[s], 1.0);
	}, sub_individus_.size());

	// VerifyallScenarioSuccessInitialize
	for (int s = 0; s < sub_individus_.size(); s++) {
		if (sub_individus_[s] == nullptr) {
			std::cerr << "Error: Failed to initialize scenario " << s << std::endl;
			throw std::runtime_error("Scenario initialization failed");
		}
	}

	// Create general_individu_ with scenario parameters
	general_individu_ = new Individu(params_list->general_params_, 1.0);

	// +++++ First day control:Initializeafter/backallScenariofirst +++++
	if (control_day_1_ == 1) {
		// UseScenario0firstas/makeforbenchmark,Sync to all other scenarios
		SyncDay1ToAllScenarios();

		// VerifysyncisnoSuccess
		try {
			CheckSubDay1Consistency("After initialization");
		} catch (const std::exception& e) {
			std::cerr << "Warning: Day 1 consistency check failed after initialization: "
			     << e.what() << std::endl;
		}
	}
}

// destructeur
Individu::~Individu()
{
	if (localSearch != nullptr) {
		delete localSearch;
		localSearch = nullptr;
	}
	for (int s = 0; s < sub_individus_.size(); s++)
	{
		if (sub_individus_[s] != nullptr) {
			delete sub_individus_[s];
			sub_individus_[s] = nullptr;
		}
	}
	// general_individu_ (Repairmemoryleak)
	if (general_individu_ != nullptr) {
		delete general_individu_;
		general_individu_ = nullptr;
	}
}

// The Split is done as follows, we test if it's possible to split without setting a fixed limit on the number of vehicles
// If the resulting solution satisfies the number of vehicle, it's perfectly fine, we return it
// Otherwise we call the Split version with limited fleet (which is a bit slower).
void Individu::generalSplit()
{
	if (is_scenario_) {
		if (g_trace) cout << "generalSplit_scenario" << endl;
		if (g_trace) cout << "UpdatePenalty_scenario" << endl;
		UpdatePenalty();
		if (g_trace) cout << "UpdatePenalty_scenario end" << endl;
		BatchRunFunc([&](int s) {
			sub_individus_[s]->generalSplit();
		}, sub_individus_.size());
		if (g_trace) cout << "generalSplit_scenario end" << endl;
		measureSol();
		if (g_trace) cout << "measureSol_scenario end" << endl;
		return;
	}
	
	//cout <<"begin split"<<endl;
	coutSol.evaluation = 0;

	// lancement de la procedure split pour chaque jour
	// on essaye deja le split simple, si c'est sans succes , le split LF
	for (int k = 1; k <= params->nbDays; k++)
		if (chromT[k].size() != 0)
		{
			int enoughVehicle = splitSimple(k);
			// cout << "Enough Vehicle: " << enoughVehicle << endl;
			if (enoughVehicle == 0)
			{
				splitLF(k);
			}
		}

	// After Split
	// we call a function that fills all other data structures and computes the cost
	//cout <<"after split";
	measureSol();
	//cout <<"after measure";
	isFitnessComputed = true;

	if (params->borneSplit > 1000)
		throw string("Erreur Split");

	if (coutSol.evaluation > 1.e20)
	{
		// if no feasible solution has been found,
		// then we relax the limit on the max capacity violation adn try again (which is initially set to 4*Q)
		// it's a very exceptional case (mostly for the PVRP, should not happen for the CVRP).
		cout << " Impossible de Split, augmentation de l'acceptation du split " << endl;
		params->borneSplit *= 1.1;
		generalSplit();
	}
	//cout <<"end split"<<endl;
}

// fonction split which does not consider a limit on the number of vehicles
// just uses the line "1" of the "potentiels" table.
int Individu::splitSimple(int k)
{
	if (is_scenario_) {
		throw std::runtime_error("Individu::splitSimple is not implemented for multi-scenario!");
	}
	
	// on va utiliser la ligne 1 des potentiels et structures pred
	double load, distance, time, time2, cost;
	int s0, s1, sb, j;
	potentiels[1][0] = 0;
	s0 = params->ordreVehicules[k][0].depotNumber;

	for (int i = 0; i < (int)chromT[k].size(); i++)
	{
		load = 0;
		distance = 0;
		time = 0; // needed to check the duration constraint for the PVRP, not needed for CVRP
		j = i;
		while (j < (int)chromT[k].size() && load <= params->ordreVehicules[k][0].vehicleCapacity * params->borneSplit)
		{
			s1 = chromT[k][j];
			load += chromL[k][s1];
			if (i == j)
			{
				distance = params->timeCost[s0][s1];
				time = params->timeCost[s0][s1];
			}
			else
			{
				sb = chromT[k][j - 1];
				distance += params->timeCost[sb][s1];
				time += params->cli[sb].serviceDuration + params->timeCost[sb][s1];
			}

			// computing the penalized cost
			cost = distance + params->timeCost[s1][s0];
			if (load > params->ordreVehicules[k][0].vehicleCapacity)
				cost += (load - params->ordreVehicules[k][0].vehicleCapacity) * params->penalityCapa;

			// needed to check the duration constraint for the PVRP, not needed for CVRP
			time2 = time + params->cli[s1].serviceDuration + params->timeCost[s1][s0];
			if (time2 > params->ordreVehicules[k][0].maxRouteTime)
				cost += (time2 - params->ordreVehicules[k][0].maxRouteTime) * params->penalityLength;

			if (potentiels[1][i] + cost < potentiels[1][j + 1]) // basic Bellman algorithm
			{
				potentiels[1][j + 1] = potentiels[1][i] + cost;
				pred[k][1][j + 1] = i;
			}
			j++;
		}
	}

	// testing if le number of vehicles is correct
	// in addition, the table pred is updated to keep track of everything
	j = (int)chromT[k].size();
	for (int jj = 0; jj < params->nombreVehicules[k]; jj++)
	{
		pred[k][params->nombreVehicules[k] - jj][j] = pred[k][1][j];
		j = pred[k][params->nombreVehicules[k] - jj][j];
	}

	coutSol.evaluation = -1.e30; // just for security, making sure this value is not used (as it does not contain the constants)
	initPot(k);
	if (j == 0)
		return 1;
	else
		return 0;
}

// fonction split pour problemes a flotte limitee
void Individu::splitLF(int k)
{
	if (is_scenario_) {
		throw std::runtime_error("Individu::splitLF is not implemented for multi-scenario!");
	}
	
	double load, distance, time, time2, cost;
	int sb, s0, s1, i, j;

	// pour chaque camion
	for (int cam = 0; cam < params->nombreVehicules[k]; cam++)
	{
		i = 0;
		s0 = params->ordreVehicules[k][cam].depotNumber;
		while (i < (int)chromT[k].size() && potentiels[cam][i] < 1.e29)
		{
			if (potentiels[cam][i] < potentiels[cam + 1][i])
			{
				potentiels[cam + 1][i] = potentiels[cam][i];
				pred[k][cam + 1][i] = i;
			}
			load = 0;
			distance = 0;
			time = 0;
			j = i;

			while (j < (int)chromT[k].size() && load <= params->ordreVehicules[k][cam].vehicleCapacity * params->borneSplit)
			{
				s1 = chromT[k][j];
				load += chromL[k][s1];
				if (i == j)
				{
					distance = params->timeCost[s0][s1];
					time = params->timeCost[s0][s1];
				}
				else
				{
					sb = chromT[k][j - 1];
					distance += params->timeCost[sb][s1];
					time += params->cli[sb].serviceDuration + params->timeCost[sb][s1];
				}

				// computing the penalized cost
				cost = distance + params->timeCost[s1][s0];
				if (load > params->ordreVehicules[k][cam].vehicleCapacity)
					cost += (load - params->ordreVehicules[k][cam].vehicleCapacity) * params->penalityCapa;

				// needed to check the duration constraint for the PVRP, not needed for CVRP
				time2 = time + params->cli[s1].serviceDuration + params->timeCost[s1][s0];
				if (time2 > params->ordreVehicules[k][cam].maxRouteTime)
					cost += (time2 - params->ordreVehicules[k][cam].maxRouteTime) * params->penalityLength;

				if (potentiels[cam][i] + cost < potentiels[cam + 1][j + 1]) // Basic Bellman iteration
				{
					potentiels[cam + 1][j + 1] = potentiels[cam][i] + cost;
					pred[k][cam + 1][j + 1] = i;
				}
				j++;
			}
			i++;
		}
	}

	// on ajoute le fitness du jour donne
	coutSol.evaluation = -1.e30; // just for security, making sure this value is not used (as it does not contain the constants)

	// on nettoye ce que l'on a deplace
	initPot(k);
}

void Individu::measureSol()
{
	if (is_scenario_) {
		if (g_trace) cout << "measureSol_scenario" << endl;
		std::vector<struct coutSol> vec_cout_sol(sub_individus_.size());
		std::vector<bool> vec_estValide(sub_individus_.size(), true);
		BatchRunFunc([&](int s) {
			sub_individus_[s]->measureSol();
			vec_cout_sol[s] = sub_individus_[s]->coutSol;
			vec_estValide[s] = sub_individus_[s]->estValide;
		}, sub_individus_.size());
		coutSol.fitness = 0;
		coutSol.capacityViol = 0;
		coutSol.lengthViol = 0;
		coutSol.evaluation = 0;
		estValide = true;
		// cout << "vec_cout_sol.size(): " << vec_cout_sol.size() << endl;
		for (int s = 0; s < vec_cout_sol.size(); s++) {
			coutSol.fitness += vec_cout_sol[s].fitness;
			coutSol.capacityViol += vec_cout_sol[s].capacityViol;
			coutSol.lengthViol += vec_cout_sol[s].lengthViol;
			coutSol.evaluation += vec_cout_sol[s].evaluation;
			estValide = estValide && vec_estValide[s];
			if (g_trace) cout << "s: " << s << " estValide: " << vec_estValide[s] << endl;
		}
		return;
	}
	
	int depot;
	int i, j;
	double distance, load, time;
	coutSol.fitness = 0;
	coutSol.capacityViol = 0;
	coutSol.lengthViol = 0;
	int nbServices = 0;

	// for (int k = 0; k < (int)chromT.size(); k++){
	// 	cout << "day " << k << " : ";
	// 	for (int i = 0; i < (int)chromT[k].size(); i++){
	// 		cout << chromT[k][i] << " ";
	// 	}
	// 	cout << endl;
	// }
	// for (int k = 0; k < (int)chromL.size(); k++){
	// 	cout << "day " << k << " : ";
	// 	for (int i = 0; i < (int)chromL[k].size(); i++){
	// 		cout << chromL[k][i] << " ";
	// 	}
	// 	cout << endl;
	// }
	// cin.get();

	// Add to the fitness the constants and the inventory cost
	if(params->isstockout){
		vector<vector<double>> I_end(params->nbDays+2, vector<double>(params->nbDepots + params->nbClients));
		for (int i = params->nbDepots; i < params->nbDepots + params->nbClients; i++){
				I_end[0][i] = params->cli[i].startingInventory;
				// cout << "client " << i-params->nbDepots+1 << " inventory cost " << params->cli[i].inventoryCost << ", stockout cost " << params->cli[i].stockoutCost << endl;
		}


		for (int k = 1; k <= params->nbDays; k++){
			// cout << "day " << k << " : " << endl;
			for (int cus = params->nbDepots; cus < params->nbDepots + params->nbClients; cus++){
				if (chromL[k][cus] != params->cli[cus].maxInventory - I_end[k-1][cus] && chromL[k][cus] != 0){
					// cout << "customer " << cus << " I " << I_end[k-1][cus] << ", L " << chromL[k][cus] << " maxInventory " << params->cli[cus].maxInventory << ", ";
					// cin.get();
					chromL[k][cus] = params->cli[cus].maxInventory - I_end[k-1][cus];
				}
				coutSol.fitness += params->cli[cus].inventoryCost* std::max<double>(0,I_end[k-1][cus]+chromL[k][cus]-params->cli[cus].dailyDemand[k]);
				coutSol.fitness += params->cli[cus].stockoutCost* std::max<double>(0,params->cli[cus].dailyDemand[k]-I_end[k-1][cus]-chromL[k][cus]);

				coutSol.fitness-= chromL[k][cus] * (params->ancienNbDays + 1 - k) * params->inventoryCostSupplier;
				I_end[k][cus] = std::max<double>(0,I_end[k-1][cus] + chromL[k][cus] - params->cli[cus].dailyDemand[k]);
				// cout << "customer " << cus << " I " << I_end[k-1][cus] << ", L " << chromL[k][cus] << ", ";
			}	
		}
	}
	
	else {
		for (int k = 1; k <= params->ancienNbDays; k++){
			for (int i = params->nbDepots; i < params->nbDepots + params->nbClients; i++)
				coutSol.fitness += chromL[k][i] * (params->ancienNbDays + 1 - k) * (params->cli[i].inventoryCost - params->inventoryCostSupplier);
		}
	}

	double inventoryCost = coutSol.fitness;

	for (int kk = 1; kk <= params->nbDays; kk++)
	{
		// on parcourt les sommets grace aux resultats de split pour
		// remplir les structures
		j = (int)chromT[kk].size();

		for (int jj = 0; jj < params->nombreVehicules[kk]; jj++)
		{
			depot = params->ordreVehicules[kk][params->nombreVehicules[kk] - jj - 1].depotNumber;
			distance = 0;
			load = 0;
			i = (int)pred[kk][params->nombreVehicules[kk] - jj][j];
			chromR[kk][params->nombreVehicules[kk] - jj - 1] = i;

			if (j == i)
			{
				distance = 0;
				load = 0;
			}
			// case where there is only one delivery in the route
			else if (j == i + 1)
			{
				distance = params->timeCost[depot][chromT[kk][i]] + params->timeCost[chromT[kk][i]][depot];
				load = chromL[kk][chromT[kk][i]];
				time = params->timeCost[depot][chromT[kk][i]];
				time += params->cli[chromT[kk][i]].serviceDuration + params->timeCost[chromT[kk][i]][depot];
				if (time > params->ordreVehicules[kk][params->nombreVehicules[kk] - jj - 1].maxRouteTime)
					coutSol.lengthViol += time - params->ordreVehicules[kk][params->nombreVehicules[kk] - jj - 1].maxRouteTime;
				nbServices++;
			}
			else
			{
				distance = params->timeCost[depot][chromT[kk][i]];
				load = 0;
				time = params->timeCost[depot][chromT[kk][i]];

				// infos sommets milieu
				for (int k = i; k <= j - 2; k++)
				{
					time += params->cli[chromT[kk][k]].serviceDuration + params->timeCost[chromT[kk][k]][chromT[kk][k + 1]];
					distance += params->timeCost[chromT[kk][k]][chromT[kk][k + 1]];
					load += chromL[kk][chromT[kk][k]];
					nbServices++;
				}

				// infos sommet fin
				time += params->cli[chromT[kk][j - 1]].serviceDuration + params->timeCost[chromT[kk][j - 1]][depot];
				distance += params->timeCost[chromT[kk][j - 1]][depot];
				load += chromL[kk][chromT[kk][j - 1]];
				if (time > params->ordreVehicules[kk][params->nombreVehicules[kk] - jj - 1].maxRouteTime)
					coutSol.lengthViol += time - params->ordreVehicules[kk][params->nombreVehicules[kk] - jj - 1].maxRouteTime;
				nbServices++;
			}

			coutSol.fitness += distance;
			if (load > params->ordreVehicules[kk][params->nombreVehicules[kk] - jj - 1].vehicleCapacity)
				coutSol.capacityViol += load - params->ordreVehicules[kk][params->nombreVehicules[kk] - jj - 1].vehicleCapacity;

			j = i;
		}
	}

	double routeCost = coutSol.fitness - inventoryCost;

	// And the necessary constants
	if(params->isstockout)coutSol.fitness += params->objectiveConstant_stockout;
	else coutSol.fitness += params->objectiveConstant;

	if (coutSol.capacityViol < 0.0001 && coutSol.lengthViol < 0.0001)
		estValide = true;
	else
		estValide = false;

	double constant = coutSol.fitness - routeCost - inventoryCost;

	coutSol.evaluation = params->penalityCapa * coutSol.capacityViol + params->penalityLength * coutSol.lengthViol + coutSol.fitness;
}

void Individu::measureGivenSol()
{
	if (is_scenario_) {
		throw std::runtime_error("Individu::measureGivenSol is not implemented for multi-scenario!");
	}
	
	int depot;
	int i, j;
	double distance, load, time;
	double fitness = 0;
	double capacityViol = 0;
	double lengthViol = 0;
	int nbServices = 0;
	double evaluation = 0;
	vector<vector<double>> chromL_given = vector<vector<double>>(params->nbDays+1, vector<double>(params->nbDepots + params->nbClients, 0));
	vector<vector<int>> chromT_given = vector<vector<int>>(params->nbDays+1, vector<int>());
	vector<string> chromT_given_srting(params->nbDays+1, "");
	// scenario 0
	// chromT_given_srting[0] = "";
	// chromT_given_srting[1] = "";
	// chromT_given_srting[2] = "3415";
	// chromT_given_srting[3] = "52";
	// chromT_given_srting[4] = "";
	// chromT_given_srting[5] = "315";
	// chromT_given_srting[6] = "";

	// scenario 3
	// chromT_given_srting[0] = "";
	// chromT_given_srting[1] = "";
	// chromT_given_srting[2] = "5143";
	// chromT_given_srting[3] = "25";
	// chromT_given_srting[4] = "";
	// chromT_given_srting[5] = "5143";
	// chromT_given_srting[6] = "";

	// scenario 4
	// chromT_given_srting[0] = "";
	// chromT_given_srting[1] = "21";
	// chromT_given_srting[2] = "543";
	// chromT_given_srting[3] = "251";
	// chromT_given_srting[4] = "";
	// chromT_given_srting[5] = "3415";
	// chromT_given_srting[6] = "";

	// scenario 6
	// chromT_given_srting[0] = "";
	// chromT_given_srting[1] = "5143";
	// chromT_given_srting[2] = "25";
	// chromT_given_srting[3] = "";
	// chromT_given_srting[4] = "3415";
	// chromT_given_srting[5] = "25";
	// chromT_given_srting[6] = "";

	// scenario 10
	chromT_given_srting[0] = "";
	chromT_given_srting[1] = "512";
	chromT_given_srting[2] = "25";
	chromT_given_srting[3] = "";
	chromT_given_srting[4] = "3415";
	chromT_given_srting[5] = "25";
	chromT_given_srting[6] = "";

	// willstringConvertforintegerList
	for(int k = 0; k <= params->nbDays; k++) {
		chromT_given[k].clear(); // ClearCurrentdateList
		chromL_given[k] = vector<double>(params->nbDepots + params->nbClients, 0);
		
		// ifwhenstringnotforEmpty
		if(chromT_given_srting[k] != "") {
			// stringin theeachcharacter
			for(char c : chromT_given_srting[k]) {
				// Convert character to integer and add to list
				// becausestringnumber inisfrom1Start,soneedminus/reduceto'0'toactualintegerValue
				int num = c - '0';
				if(num > 0) {
					chromT_given[k].push_back(num);
					chromL_given[k][num] = 1;
				}
			}
		}
	}

	// Add to the fitness the constants and the inventory cost
	if(params->isstockout){
		vector<vector<double>> I_end(params->nbDays+2, vector<double>(params->nbDepots + params->nbClients));
		for (int i = params->nbDepots; i < params->nbDepots + params->nbClients; i++){
				I_end[0][i] = params->cli[i].startingInventory;
				// cout << "client " << i-params->nbDepots+1 << " inventory cost " << params->cli[i].inventoryCost << ", stockout cost " << params->cli[i].stockoutCost << endl;
		}


		for (int k = 1; k <= params->nbDays; k++){
			// cout << "day " << k << " : " << endl;
			for (int cus = params->nbDepots; cus < params->nbDepots + params->nbClients; cus++){
				if (chromL_given[k][cus] != params->cli[cus].maxInventory - I_end[k-1][cus] && chromL_given[k][cus] != 0){
					// cout << "customer " << cus << " I " << I_end[k-1][cus] << ", L " << chromL[k][cus] << " maxInventory " << params->cli[cus].maxInventory << ", ";
					// cin.get();
					chromL_given[k][cus] = params->cli[cus].maxInventory - I_end[k-1][cus];
				}
				fitness += params->cli[cus].inventoryCost* std::max<double>(0,I_end[k-1][cus]+chromL_given[k][cus]-params->cli[cus].dailyDemand[k]);
				fitness += params->cli[cus].stockoutCost* std::max<double>(0,params->cli[cus].dailyDemand[k]-I_end[k-1][cus]-chromL_given[k][cus]);

				fitness-= chromL_given[k][cus] * (params->ancienNbDays + 1 - k) * params->inventoryCostSupplier;
				I_end[k][cus] = std::max<double>(0,I_end[k-1][cus] + chromL_given[k][cus] - params->cli[cus].dailyDemand[k]);
				// cout << "customer " << cus << " I " << I_end[k-1][cus] << ", L " << chromL[k][cus] << ", ";
			}	
		}
	}
	
	else {
		for (int k = 1; k <= params->ancienNbDays; k++){
			for (int i = params->nbDepots; i < params->nbDepots + params->nbClients; i++)
				fitness += chromL_given[k][i] * (params->ancienNbDays + 1 - k) * (params->cli[i].inventoryCost - params->inventoryCostSupplier);
		}
	}

	double inventoryCost = fitness;

	for (int k = 0; k < (int)chromT_given.size(); k++){
		cout << "day " << k << " : ";
		for (int i = 0; i < (int)chromT_given[k].size(); i++){
			cout << chromT_given[k][i] << " ";
		}
		cout << endl;
	}
	for (int k = 0; k < (int)chromL_given.size(); k++){
		cout << "day " << k << " : ";
		for (int i = 0; i < (int)chromL_given[k].size(); i++){
			cout << chromL_given[k][i] << " ";
		}
		cout << endl;
	}

	for (int kk = 1; kk <= params->nbDays; kk++)
	{
		// on parcourt les sommets grace aux resultats de split pour
		// remplir les structures
		j = (int)chromT_given[kk].size();

		for (int jj = 0; jj < params->nombreVehicules[kk]; jj++)
		{
			depot = params->ordreVehicules[kk][params->nombreVehicules[kk] - jj - 1].depotNumber;
			distance = 0;
			load = 0;
			i = (int)pred[kk][params->nombreVehicules[kk] - jj][j];

			if (j == i)
			{
				distance = 0;
				load = 0;
			}
			// case where there is only one delivery in the route
			else if (j == i + 1)
			{
				distance = params->timeCost[depot][chromT_given[kk][i]] + params->timeCost[chromT_given[kk][i]][depot];
				load = chromL_given[kk][chromT_given[kk][i]];
				time = params->timeCost[depot][chromT_given[kk][i]];
				time += params->cli[chromT_given[kk][i]].serviceDuration + params->timeCost[chromT_given[kk][i]][depot];
				if (time > params->ordreVehicules[kk][params->nombreVehicules[kk] - jj - 1].maxRouteTime)
					lengthViol += time - params->ordreVehicules[kk][params->nombreVehicules[kk] - jj - 1].maxRouteTime;
				nbServices++;
			}
			else
			{
				distance = params->timeCost[depot][chromT_given[kk][i]];
				load = 0;
				time = params->timeCost[depot][chromT_given[kk][i]];

				// infos sommets milieu
				for (int k = i; k <= j - 2; k++)
				{
					time += params->cli[chromT_given[kk][k]].serviceDuration + params->timeCost[chromT_given[kk][k]][chromT_given[kk][k + 1]];
					distance += params->timeCost[chromT_given[kk][k]][chromT_given[kk][k + 1]];
					load += chromL_given[kk][chromT_given[kk][k]];
					nbServices++;
				}

				// infos sommet fin
				time += params->cli[chromT_given[kk][j - 1]].serviceDuration + params->timeCost[chromT_given[kk][j - 1]][depot];
				distance += params->timeCost[chromT_given[kk][j - 1]][depot];
				load += chromL_given[kk][chromT_given[kk][j - 1]];
				if (time > params->ordreVehicules[kk][params->nombreVehicules[kk] - jj - 1].maxRouteTime)
					lengthViol += time - params->ordreVehicules[kk][params->nombreVehicules[kk] - jj - 1].maxRouteTime;
				nbServices++;
			}

			fitness += distance;
			if (load > params->ordreVehicules[kk][params->nombreVehicules[kk] - jj - 1].vehicleCapacity)
				capacityViol += load - params->ordreVehicules[kk][params->nombreVehicules[kk] - jj - 1].vehicleCapacity;

			j = i;
		}
	}

	double routeCost = fitness - inventoryCost;

	// And the necessary constants
	if(params->isstockout)fitness += params->objectiveConstant_stockout;
	else fitness += params->objectiveConstant;

	if (capacityViol < 0.000001 && lengthViol < 0.000001) {
		estValide = true;
	}
	else {
		estValide = false;
		cout << "capacityViol: " << capacityViol << " lengthViol: " << lengthViol << endl;
	}

	double constant = fitness - routeCost - inventoryCost;

	evaluation = params->penalityCapa * capacityViol + params->penalityLength * lengthViol + fitness;
	cout << "evaluation: " << evaluation << " fitness: " << fitness << endl;
	cout << "routeCost: " << routeCost << endl;
	cout << "inventoryCost: " << inventoryCost << endl;
	cout << "constant: " << constant << endl;
	// cout << "fitness: " << coutSol.fitness << endl;
}

// initialisation du vecteur potentiels
void Individu::initPot(int day)
{
	if (is_scenario_) {
		throw std::runtime_error("Individu::initPot is not implemented for multi-scenario!");
	}
	
	for (int i = 0; i < params->nombreVehicules[day] + 1; i++)
	{
		for (size_t j = 0; j <= chromT[day].size() + 1; j++)
		{
			potentiels[i][j] = 1.e30;
		}
	}
	potentiels[0][0] = 0;
}
// mise a jour de l'objet localSearch, attention, split doit avoir ete calcule avant
// Update localSearch Object,butRequiredbeforehandCalculateoutPartitionValue
void Individu::updateLS()
{
	if (is_scenario_) {
		if (g_trace) cout << "updateLS_scenario" << endl;
		UpdatePenalty();
		BatchRunFunc([&](int s) {
			sub_individus_[s]->updateLS();
		}, sub_individus_.size());
		// Print localSearch->demandPerDay
		// for (int s = 0; s < sub_individus_.size(); s++) {
		// 	cout << "Scenario-localSearch " << s << ": " << endl;
		// 	for (int k = 1; k <= params->nbDays; k++) {	
		// 		cout << "Day " << k << ": ";
		// 		for (int i = 0; i < (int)sub_individus_[s]->localSearch->demandPerDay[k].size(); i++) {
		// 			cout << sub_individus_[s]->localSearch->demandPerDay[k][i] << " ";
		// 		}
		// 		cout << endl;
		// 	}
		// 	cin.get();
		// }	
		return;
	}
	
	int depot;
	int i, j;
	Noeud *myDepot;
	Noeud *myDepotFin;
	Noeud *myClient;
	Route *myRoute;

	// We copy the amount of delivery per day
	// (more clean to make sure that LocalSearch can work totally independently of the Individu structure)
	localSearch->demandPerDay = chromL;

	for (int kk = 1; kk <= params->nbDays; kk++)
	{
		// on reinitialise l'ordreParcours
		localSearch->ordreParcours[kk].clear();

		// on replace les champs estPresent a false
		for (i = params->nbDepots; i < (int)localSearch->clients[kk].size(); i++)
		{
			localSearch->clients[kk][i]->estPresent = false;
		}

		// on parcourt les sommets grace aux resultats de split pour
		// remplir les structures
		j = (int)chromT[kk].size();

		for (int jj = 0; jj < params->nombreVehicules[kk]; jj++)
		{
			depot = params->ordreVehicules[kk][params->nombreVehicules[kk] - jj - 1].depotNumber;
			i = (int)pred[kk][params->nombreVehicules[kk] - jj][j];

			myDepot = localSearch->depots[kk][params->nombreVehicules[kk] - jj - 1];
			myDepotFin = localSearch->depotsFin[kk][params->nombreVehicules[kk] - jj - 1];
			myRoute = localSearch->routes[kk][params->nombreVehicules[kk] - jj - 1];

			myDepot->suiv = myDepotFin;
			myDepot->pred = myDepotFin;
			myDepotFin->suiv = myDepot;
			myDepotFin->pred = myDepot;

			// Case when there is only one vertex in the cycle
			if (j == i + 1)
			{
				myClient = localSearch->clients[kk][chromT[kk][i]];
				myClient->pred = myDepot;
				myClient->suiv = myDepotFin;
				myClient->route = myRoute;
				myClient->estPresent = true;
				myDepot->suiv = myClient;
				myDepotFin->pred = myClient;
				localSearch->ordreParcours[kk].push_back(myClient->cour);
			}
			else if (j > i + 1)
			{
				// on a au moins 2 sommets
				// infos sommet debut
				myClient = localSearch->clients[kk][chromT[kk][i]];
				myClient->pred = myDepot;
				myClient->suiv = localSearch->clients[kk][chromT[kk][i + 1]];
				myClient->route = myRoute;
				myClient->estPresent = true;
				myDepot->suiv = myClient;
				localSearch->ordreParcours[kk].push_back(myClient->cour);

				// infos sommet fin
				myClient = localSearch->clients[kk][chromT[kk][j - 1]];
				myClient->pred = localSearch->clients[kk][chromT[kk][j - 2]];
				myClient->suiv = myDepotFin;
				myClient->route = myRoute;
				myClient->estPresent = true;
				myDepotFin->pred = myClient;
				localSearch->ordreParcours[kk].push_back(myClient->cour);

				// infos sommets milieu
				for (int k = (int)i + 1; k <= j - 2; k++)
				{
					myClient = localSearch->clients[kk][chromT[kk][k]];
					myClient->pred = localSearch->clients[kk][chromT[kk][k - 1]];
					myClient->suiv = localSearch->clients[kk][chromT[kk][k + 1]];
					myClient->route = myRoute;
					myClient->estPresent = true;
					localSearch->ordreParcours[kk].push_back(myClient->cour);
				}
			}
			j = i;
		}
		// pour chaque route on met les charges partielles a jour
		for (i = 0; i < (int)localSearch->routes[kk].size(); i++)
			localSearch->routes[kk][i]->updateRouteData();
	}
}
int partition(std::vector<Route*>& arr, int low, int high)
{
  Route* pivot = arr[high]; 
  int i = (low - 1);
	
  for (int j = low; j <= high - 1; j++)
  {
    if (arr[j]->centroidAngle <= pivot->centroidAngle)
    {
      i++;
      std::swap(arr[i], arr[j]);
    }
  }
  std::swap(arr[i + 1], arr[high]);
  return (i + 1);
}

int Individu::randomizedPartition(std::vector<Route*>& arr, int low, int high)
{
  //int random = low + rand() % (high - low); 
	int random = low + params->rng->genrand64_int64() % (high - low);
  std::swap(arr[random], arr[high]); 
  return partition(arr, low, high); 
}

void Individu::randomizedQuickSort(std::vector<Route*>& arr, int low, int high) 
{ 
  if (low < high) 
  { 
    int pi = randomizedPartition(arr, low, high); 
    randomizedQuickSort(arr, low, pi - 1); 
    randomizedQuickSort(arr, pi + 1, high); 
  } 
}


// mise a jour du chromT suite aux modification de localSearch,
//kkrouteSort,NewchromT[kk]
void Individu::updateIndiv()
{
	if (is_scenario_) {
		if (g_trace) cout << "updateIndiv_scenario" << endl;
		// wait(g_ms);
		BatchRunFunc([&](int s) {
			sub_individus_[s]->updateIndiv();
		}, sub_individus_.size());
		// for (int s = 0; s < sub_individus_.size(); s++) {
		// 	cout << "Scenario-chromL " << s << ": " << endl;
		// 	for (int k = 1; k <= params->nbDays; k++) {	
		// 		cout << "Day " << k << ": ";
		// 		for (int i = 0; i < (int)sub_individus_[s]->chromL[k].size(); i++) {
		// 			cout << sub_individus_[s]->chromL[k][i] << " ";
		// 		}
		// 		cout << endl;
		// 	}
		// 	cin.get();
		// }	
		return;
	}
	
	// Don't forget to copy back the load delivered to each customer on each day
	chromL = localSearch->demandPerDay;

	vector<Route *> ordreRoutesAngle;
	Route *temp;
	Noeud *node;

	for (int kk = 1; kk <= params->nbDays; kk++)
	{
		ordreRoutesAngle = localSearch->routes[kk];

		
		for (int r = 0; r < (int)ordreRoutesAngle.size(); r++)
			ordreRoutesAngle[r]->updateCentroidCoord();

		/*/ on trie les routes dans l'ordre des centroidesbycenter pointSequenceSort
		if (params->triCentroides)
			for (int r = 0; r < (int)ordreRoutesAngle.size(); r++)
				for (int rr = 1; rr < (int)ordreRoutesAngle.size() - r - 1; rr++)
					if (ordreRoutesAngle[rr]->centroidAngle > ordreRoutesAngle[rr + 1]->centroidAngle)
					{
						temp = ordreRoutesAngle[rr + 1];
						ordreRoutesAngle[rr + 1] = ordreRoutesAngle[rr];
						ordreRoutesAngle[rr] = temp;
					}
		//static int callCount = 0; 
		//callCount++;
		//out << "Function has been called " << callCount << " times." << std::endl;
		
		//******************jingyi: on calcule les angles des centroidesCalculateof center pointangle***************
	
		*/
		if (params->triCentroides)
			randomizedQuickSort(ordreRoutesAngle, 0, ordreRoutesAngle.size() - 1);
		
		// on recopie les noeuds dans le chromosome
		
		chromT[kk].clear();
		for (int r = 0; r < (int)ordreRoutesAngle.size(); r++)
		{
			node = ordreRoutesAngle[r]->depot->suiv;
			while (!node->estUnDepot)
			{
				chromT[kk].push_back(node->cour);
				node = node->suiv;
			}
		}
	}

	generalSplit();
}

// Computes the maximum amount of load that can be delivered to client on a day k without exceeding the
// customer maximum inventory
double Individu::maxFeasibleDeliveryQuantity(int day, int client)
{
	if (is_scenario_) {
		throw std::runtime_error("Individu::maxFeasibleDeliveryQuantity is not implemented for multi-scenario!");
	}
	
	// Printing customer inventory and computing customer inventory cost
	double inventoryClient;
	double minSlack = 1.e30;
	//cout <<"start = "<<params->cli[client].startingInventory<<endl;
	inventoryClient = params->cli[client].startingInventory;
	for (int k = 1; k <= params->nbDays; k++)
	{
		// here level in the morning
		inventoryClient += chromL[k][client];
		//cout <<"chromL["<<k<<"]["<<client<<" ] = "<<chromL[k][client]<<endl;
		//cout<<inventoryClient<<endl;
		// level after receiving inventory

		// updating the residual capacity if k is greater than the day
		if (k >= day && params->cli[client].maxInventory - inventoryClient < minSlack)
			minSlack = params->cli[client].maxInventory - inventoryClient;

		if (minSlack < -0.0001)
		{
			cout << "ISSUE : negative slack capacit during crossover, this should not happen" << endl;
			throw("ISSUE : negative slack capacit during crossover, this should not happen");
		}

		inventoryClient -= params->cli[client].dailyDemand[k];
		// level after consumption
	}

	if (minSlack < 0.0001) // avoiding rounding issues
		return 0.;
	else
		return minSlack;
}

// distance generale
// il faut que les suivants de chaque individu aient ete calcules
double Individu::distance(Individu *indiv2)
{
	if (is_scenario_) {
		if (g_trace) cout << "distance_scenario" << endl;
		if (indiv2 == nullptr || indiv2->sub_individus_.size() != sub_individus_.size()) {
			throw std::runtime_error("Distance Invalid Individu Parameters!");
		}
		std::vector<double> vec_distance(sub_individus_.size());
		BatchRunFunc([&](int s) {
			auto sub_individu2 = indiv2->sub_individus_[s];
			vec_distance[s] = sub_individus_[s]->distance(sub_individu2);
		}, sub_individus_.size());
		double ret = 0;
		for (int s = 0; s < vec_distance.size(); s++) {
			ret += vec_distance[s];
		}
		// Calculateaverage distance
		ret /= vec_distance.size();	
		return ret;
	}
	
	int note = 0;
	bool isIdentical;

	// Inventory Routing
	// distance based on number of customers which have different service days
	if (params->isInventoryRouting)
	{
		for (int j = params->nbDepots; j < params->nbClients + params->nbDepots; j++)
		{
			isIdentical = true;
			for (int k = 1; k <= params->nbDays; k++)
				if ((chromL[k][j] < 0.0001 && indiv2->chromL[k][j] > 0.0001) || (indiv2->chromL[k][j] < 0.0001 && chromL[k][j] > 0.0001))
					isIdentical = false;
			if (isIdentical == false)
				note++;
		}
	}
	// CVRP case, broken-pairs distance
	else
	{
		for (int j = params->nbDepots; j < params->nbClients + params->nbDepots; j++)
		{
			isIdentical = true;
			for (int s = 0; s < (int)suivants[j].size(); s++)
				if ((suivants[j][s] != indiv2->suivants[j][s] || precedents[j][s] != indiv2->precedents[j][s]) && (precedents[j][s] != indiv2->suivants[j][s] || suivants[j][s] != indiv2->precedents[j][s]))
					isIdentical = false;
			if (!isIdentical)
				note++;
		}
	}

	return ((double)note / (double)(params->nbClients));
}

// calcul des suivants
void Individu::computeSuivants()
{
	if (is_scenario_) {
		throw std::runtime_error("Individu::computeSuivants is not implemented for multi-scenario!");
	}
	
	vector<int> vide;
	int s, jj;
	for (int i = 0; i < params->nbDepots + params->nbClients; i++)
	{
		suivants[i] = vide;
		precedents[i] = vide;
	}

	// on va noter pour chaque individu et chaque client
	// la liste des clients places juste apres lui dans le giant tour pour chaque jour
	for (int k = 1; k <= params->nbDays; k++)
		if (chromT[k].size() != 0)
		{
			for (int i = 0; i < (int)chromT[k].size() - 1; i++)
				suivants[chromT[k][i]].push_back(chromT[k][i + 1]);

			for (int i = 1; i < (int)chromT[k].size(); i++)
				precedents[chromT[k][i]].push_back(chromT[k][i - 1]);

			suivants[chromT[k][chromT[k].size() - 1]].push_back(chromT[k][0]);
			precedents[chromT[k][0]].push_back(chromT[k][chromT[k].size() - 1]);

			// on enleve ceux qui sont des fins de routes
			s = (int)chromT[k].size();
			jj = 0;
			while (s != 0 && jj < params->nombreVehicules[k])
			{
				suivants[chromT[k][s - 1]].pop_back();
				suivants[chromT[k][s - 1]].push_back(params->ordreVehicules[k][params->nombreVehicules[k] - jj - 1].depotNumber);
				s = pred[k][params->nombreVehicules[k] - jj][s];
				precedents[chromT[k][s]].pop_back();
				precedents[chromT[k][s]].push_back(params->ordreVehicules[k][params->nombreVehicules[k] - jj - 1].depotNumber);
				jj++;
			}
		}
}

// ajoute un element proche dans les structures de proximite
void Individu::addProche(Individu *indiv)
{
	// if (is_scenario_) {
	// 	// if (g_trace) cout << "addProche_scenario" << endl;
	// 	if (indiv == nullptr || indiv->sub_individus_.size() != sub_individus_.size()) {
	// 		throw std::runtime_error("AddProche Invalid Individu Parameters!");
	// 	}
	// 	wait(g_ms);
	// 	BatchRunFunc([&](int s) {
	// 		auto sub_individu = indiv->sub_individus_[s];
	// 		sub_individus_[s]->addProche(sub_individu);
	// 	}, sub_individus_.size());
	// 	return;
	// }
	
	list<proxData>::iterator it;
	proxData data;
	data.indiv = indiv;
	data.dist = distance(indiv);

	if (plusProches.empty())
		plusProches.push_back(data);
	else
	{
		it = plusProches.begin();
		while (it != plusProches.end() && it->dist < data.dist)
			++it;
		plusProches.insert(it, data);
	}
}

// enleve un element dans les structures de proximite
void Individu::removeProche(Individu *indiv)
{
	if (is_scenario_) {
		if (g_trace) cout << "removeProche_scenario" << endl;
		if (indiv == nullptr || indiv->sub_individus_.size() != sub_individus_.size()) {
			throw std::runtime_error("RemoveProche Invalid Individu Parameters!");
		}
		BatchRunFunc([&](int s) {
			auto sub_individu = indiv->sub_individus_[s];
			sub_individus_[s]->removeProche(sub_individu);
		}, sub_individus_.size());
		return;
	}
	
	list<proxData>::iterator last = plusProches.end();
	for (list<proxData>::iterator first = plusProches.begin(); first != last;)
		if (first->indiv == indiv)
			first = plusProches.erase(first);
		else
			++first;
}

// distance moyenne avec les n individus les plus proches
double Individu::distPlusProche(int n)
{
	if (is_scenario_) {
		// if (g_trace) cout << "distPlusProche_scenario" << endl;
		std::vector<double> vec_dist_plus_proche(sub_individus_.size());
		BatchRunFunc([&](int s) {
			vec_dist_plus_proche[s] = sub_individus_[s]->distPlusProche(n);
		}, sub_individus_.size());
		double ret = 0;
		for (int s = 0; s < vec_dist_plus_proche.size(); s++) {
			ret += vec_dist_plus_proche[s];
		}
		// Calculateaverage distance
		ret /= vec_dist_plus_proche.size();
		return ret;
	}
	
	double result = 0;
	double compte = 0;
	list<proxData>::iterator it = plusProches.begin();

	for (int i = 0; i < n && it != plusProches.end(); i++)
	{
		result += it->dist;
		compte += 1.0;
		++it;
	}
	return result / compte;
}

void Individu::RunSearchTotal(bool is_repair) {
	if (is_scenario_) {
		if (g_trace) cout << "RunSearchTotal_scenario" << endl;

		// +++++ byOptimize(RiOptimize)- supportParalleltransform +++++
		// priorityUsebyOptimizededicatedThread pool(gRPCmode),elseUsegeneralThread pool
		ThreadPool* routing_pool = g_routing_thread_pool ? g_routing_thread_pool : g_thread_pool;

		if (routing_pool != nullptr) {
			// UseThread poolParallelExecutebyOptimize
			if (g_trace) cout << "Running route optimization in parallel with thread pool" << endl;
			std::vector<std::future<void>> futures;
			for (int s = 0; s < sub_individus_.size(); s++) {
				futures.push_back(routing_pool->enqueue([this, s]() {
					sub_individus_[s]->localSearch->runSearchRi();
				}));
			}
			// WaitallbyOptimizetaskComplete
			for (auto& f : futures) {
				f.wait();
			}
			if (g_trace) cout << "Route optimization completed" << endl;
		} else {
			// SequenceExecute(compatibleOld,whentwoThread poolInitialize)
			if (g_trace) cout << "Running route optimization sequentially" << endl;
			for (int s = 0; s < sub_individus_.size(); s++) {
				sub_individus_[s]->localSearch->runSearchRi();
			}
		}

		// +++++ InventoryPolicyOptimize(PiOptimize)+++++
		// based on g_thread_pool if nullautomaticSelectiongRPCormode
		RunSearchPi(0);

		return;
	}
	localSearch->runSearchTotal(is_repair);
}

void Individu::RunSearchPi(int rep_times) {
	// recordFunctioncallStart
	auto start_time = logFunctionStart("Individu::RunSearchPi");

	bool traces = false; //
	if (!is_scenario_) {
		logFunctionEnd("Individu::RunSearchPi", start_time);
		return;
	}

	// CheckisnoUsegRPCmode (threads=0,g_thread_poolfornullptr)
	if (g_thread_pool == nullptr) {
		if (traces) std::cout << "Using gRPC mode for Pi search" << std::endl;
		RunSearchPi_gRPC(rep_times);
		logFunctionEnd("Individu::RunSearchPi", start_time);
		return;
	}

	// UseCalculatemode (threads>0)
	if (traces) std::cout << "Using local computation mode for Pi search" << std::endl;
	RunSearchPi_Local(rep_times);
	logFunctionEnd("Individu::RunSearchPi", start_time);
}

void Individu::RunSearchPi_Local(int rep_times) {
	// Calculatemode:UseThread poolParallelExecute
	bool traces = false;
	if (traces) std::cout << "RunSearchPi_Local: starting inventory optimization" << std::endl;

	// +++++ InventoryPolicyOptimize(PiOptimize) +++++
	// Note: Route optimization done in RunSearchTotal, only inventory optimization here
	// implementtwo-phase random optimization: first day decision needs expected cost from all scenarios

	if (control_day_1_ == 1) {
		// +++++ twophaseOptimizemode +++++
		// For each customer, optimize first day decisions across all scenarios
		if (traces) std::cout << "RunSearchPi_Local: using two-stage optimization for control_day_1" << std::endl;

		int nb_scenarios = sub_individus_.size();
		int nb_clients = params->nbClients;

		// CustomerListandrandom(withgRPCmodeconsistent)
		vector<int> vec_rand_clients;
		for (int c = params->nbDepots; c < nb_clients + params->nbDepots; c++) {
			vec_rand_clients.push_back(c);
		}
		std::mt19937 rng(params->seed);
		std::shuffle(vec_rand_clients.begin(), vec_rand_clients.end(), rng);

		// eachCustomer
		for (int i = 0; i < vec_rand_clients.size(); i++) {
			int c = vec_rand_clients[i];
			if (traces) std::cout << " Optimizing client " << c << "..." << std::endl;

			// "Skipfirst"inallScenariounderCost(UseThread poolParalleltransform)
			double total_cost_skip = 0.0;
			std::vector<OUPolicyResult> results_skip(nb_scenarios);
			BatchRunFunc([&](int s) {
				results_skip[s] = sub_individus_[s]->localSearch->EvaluateClient(c, DAY1_FORCE_SKIP);
			}, nb_scenarios);
			// CalculateTotal cost
			for (int s = 0; s < nb_scenarios; s++) {
				total_cost_skip += results_skip[s].totalCost;
			}

			// "firstDelivery"inallScenariounderCost(UseThread poolParalleltransform)
			double total_cost_delivery = 0.0;
			std::vector<OUPolicyResult> results_delivery(nb_scenarios);
			BatchRunFunc([&](int s) {
				results_delivery[s] = sub_individus_[s]->localSearch->EvaluateClient(c, DAY1_FORCE_DELIVERY);
			}, nb_scenarios);
			// CalculateTotal cost
			for (int s = 0; s < nb_scenarios; s++) {
				total_cost_delivery += results_delivery[s].totalCost;
			}

			// SelectionTotal costlowandApplytoallScenario
			if (total_cost_delivery < total_cost_skip) {
				if (traces) std::cout << "  Client " << c << ": choosing DELIVERY (cost: " << total_cost_delivery << " < " << total_cost_skip << ")" << std::endl;
				for (int s = 0; s < nb_scenarios; s++) {
					sub_individus_[s]->localSearch->ApplyClientSolution(c, results_delivery[s]);
				}
			} else {
				if (traces) std::cout << "  Client " << c << ": choosing SKIP (cost: " << total_cost_skip << " <= " << total_cost_delivery << ")" << std::endl;
				for (int s = 0; s < nb_scenarios; s++) {
					sub_individus_[s]->localSearch->ApplyClientSolution(c, results_skip[s]);
				}
			}
		}

		if (traces) std::cout << "RunSearchPi_Local: two-stage inventory optimization completed" << std::endl;
	} else {
		// +++++ originalhavemode:eachScenarioOptimize +++++
		for (int s = 0; s < sub_individus_.size(); s++) {
			sub_individus_[s]->localSearch->mutationDifferentDay();
		}

		if (traces) std::cout << "RunSearchPi_Local: inventory optimization completed" << std::endl;
	}
}

void Individu::RunSearchPi_gRPC(int rep_times) {
	// gRPCCalculatemode:UseserverCalculate
	bool traces = false;
	vector<int> vec_rand_clients;
	for (int c = params->nbDepots; c < params->nbClients + params->nbDepots; c++) {
		vec_rand_clients.push_back(c);
	}
	// params->seedas/makeforstd::mt19937Seed
	std::mt19937 rng(params->seed);
	std::shuffle(vec_rand_clients.begin(), vec_rand_clients.end(), rng);
	if (traces) cout << "random_shuffle vec_rand_clients" << endl;
	// cin.get();
	bool is_improved = false;
	std::vector<double> vec_objective(sub_individus_.size());
	for (int c = 0; c < vec_rand_clients.size(); c++) {
		// gRPCRequest
		irp::OuReq req;
		irp::OuResp resp;
		// fill req
		req.set_client_id(vec_rand_clients[c]);
		req.set_nb_days(params->nbDays);
		req.set_nb_scenario(sub_individus_.size());
		req.set_max_inventory(params->cli[vec_rand_clients[c]].maxInventory);
		req.set_start_inventory(params->cli[vec_rand_clients[c]].startingInventory);
		req.set_inventory_cost(params->cli[vec_rand_clients[c]].inventoryCost);
		req.set_stockout_cost(params->cli[vec_rand_clients[c]].stockoutCost);
		// +++++ FIX: Pass actual supplier inventory cost instead of hardcoded 0 +++++
		// This value is used by GPU solver to calculate supplier discount:
		// supplier_discount = supplier_minus_cost * (remaining_days) * quantity
		req.set_supplier_minus_cost(params->inventoryCostSupplier);
		// std::vector<std::vector<DeliveryPlan>> vec_delivery_plans(sub_individus_.size());
		for (int s = 0; s < sub_individus_.size(); s++) {
			// std::vector<DeliveryPlan> vec_delivery_plans;
			sub_individus_[s]->localSearch->PreComputeDeliveryPlan(vec_rand_clients[c]);
			irp::ScenarioInfo* info = req.add_scenario_infos();
			info->set_scenario_id(s);
			
			// Setdaily_demands allindex=0Start convenientproblem
			for (int day = 1; day <= params->nbDays; day++) {
				info->add_daily_demands(sub_individus_[s]->params->cli[vec_rand_clients[c]].dailyDemand[day]);
			}
			// CRITICAL FIX: Only iterate over valid elements in vec_delivery_plans_
			// vec_delivery_plans_ has size=nbDays+1 but only indices 0 to nbDays-1 are used
			// (corresponding to Day 1 through Day nbDays)
			for (int day_idx = 0; day_idx < params->nbDays; day_idx++) {
				auto& delivery_plan = sub_individus_[s]->localSearch->vec_delivery_plans_[day_idx];
				irp::CapaCostFunc* capa_cost_func = info->add_daily_capa_penalty_costs();
				capa_cost_func->set_threshold(delivery_plan.load);
				capa_cost_func->set_cost(0);
				capa_cost_func->set_slope(params->penalityCapa);
				capa_cost_func->set_intercept(0 - delivery_plan.load * params->penalityCapa); // below/undercompare
				info->add_daily_delivery_costs(delivery_plan.detour); // allindex=0Start convenientproblem
			}
		}
		req.set_control_day_1(control_day_1_);
		if (control_day_1_ != 1) {
			throw std::runtime_error("control_day_1_ not supported");
		}

		// +++++ First day control:removeConstraint,GPUserverbased onCostcompareautomaticdecision +++++
		// originalhaveGenerationfromInitializechoose_deliveryfieldReadValue(isfalse)
		// inPython RPCserverthroughCostcomparetoSelectionfirstdecision
		/*
		if (control_day_1_ == 1 && sub_individus_.size() > 0) {
			// UseScenario0firstDeliverydecision asforallScenarioConstraint
			auto* day1_info = req.mutable_control_day_1_info();

			// fromScenario0GetfirstDeliverydecision
			auto& delivery_plans_0 = sub_individus_[0]->localSearch->vec_delivery_plans_;
			if (delivery_plans_0.size() > 0) {
				// delivery_plans_[0] corresponding1
				day1_info->set_day_1_delivery(delivery_plans_0[0].choose_delivery);
				day1_info->set_day_1_cost(delivery_plans_0[0].detour);

				if (traces) {
					cout << "Control Day 1 Info: delivery=" << delivery_plans_0[0].choose_delivery
					   << ", cost=" << delivery_plans_0[0].detour << endl;
				}
			}
		}
		*/

		if (traces) cout << "fill req" << endl;

		// callgRPCCustomer
		try {
			// +++++ THREAD SAFETY: Protect gRPC channel creation with mutex +++++
			{
				std::lock_guard<std::mutex> lock(grpc_channel_mutex_);

				// CreateorgRPCConnection(RequestCreateNewConnection)
				if (!grpc_channel_) {
					if (traces) cout << "Creating new gRPC channel..." << endl;
					std::string target_address = "localhost:50051";
					grpc_channel_ = grpc::CreateChannel(target_address, grpc::InsecureChannelCredentials());

					// +++++ Validate channel was created successfully +++++
					if (!grpc_channel_) {
						cerr << "ERROR: Failed to create gRPC channel to " << target_address << endl;
						continue;
					}

					if (traces) cout << "✓ gRPC channel created successfully" << endl;
				} else {
					if (traces) cout << "Reusing existing gRPC channel" << endl;
				}

				// +++++ CRITICAL: Validate channel state before use +++++
				auto state = grpc_channel_->GetState(false);
				if (traces) {
					cout << "gRPC channel state: " << state << " (0=IDLE, 1=CONNECTING, 2=READY, 3=TRANSIENT_FAILURE, 4=SHUTDOWN)" << endl;
				}

				// If channel is in bad state, try to reset it
				if (state == 4) { // SHUTDOWN
					cerr << "WARNING: gRPC channel is SHUTDOWN, recreating..." << endl;
					grpc_channel_ = grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials());
					if (!grpc_channel_) {
						cerr << "ERROR: Failed to recreate gRPC channel" << endl;
						continue;
					}
				}
			} // Mutex released here

			// +++++ Create stub and make RPC call (outside mutex for better concurrency) +++++
			if (!grpc_channel_) {
				cerr << "ERROR: grpc_channel_ is nullptr before creating stub" << endl;
				continue;
			}

			auto stub = irp::IrpService::NewStub(grpc_channel_);
			if (!stub) {
				cerr << "ERROR: Failed to create gRPC stub" << endl;
				continue;
			}

			if (traces) cout << "Created gRPC stub, sending request..." << endl;

			// sendRequest
			grpc::ClientContext context;
			// Set timeout to prevent hanging
			std::chrono::system_clock::time_point deadline = std::chrono::system_clock::now() + std::chrono::seconds(30);
			context.set_deadline(deadline);

			grpc::Status status = stub->ProcessOptimization(&context, req, &resp);

			if (!status.ok()) {
				cerr << "ERROR: gRPC call failed [" << status.error_code() << "]: " << status.error_message() << endl;
				cerr << "    Details: " << status.error_details() << endl;

				// If connection failed, invalidate channel so it gets recreated next time
				if (status.error_code() == grpc::StatusCode::UNAVAILABLE ||
				  status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED) {
					std::lock_guard<std::mutex> lock(grpc_channel_mutex_);
					grpc_channel_.reset(); // Force recreation on next attempt
					cerr << "    Channel reset due to connection failure" << endl;
				}
				continue;
			}

			if (traces) cout << "✓ gRPC call succeeded" << endl;
		} catch (const std::exception& e) {
			cerr << "ERROR: Exception in gRPC call: " << e.what() << endl;
			// Reset channel on exception to force reconnection
			{
				std::lock_guard<std::mutex> lock(grpc_channel_mutex_);
				grpc_channel_.reset();
			}
			continue;
		}

		// +++++ GPU STATISTICS: Record successful GPU call +++++
		total_gpu_calls_++;
		int scenarios_in_response = resp.scenario_info_results_size();
		total_scenarios_processed_ += scenarios_in_response;
		if (traces) {
			cout << "GPU call #" << total_gpu_calls_.load()
			   << " processed " << scenarios_in_response << " scenarios"
			   << " (total: " << total_scenarios_processed_.load() << ")" << endl;
		}

		// +++++ CRITICAL: Validate response before processing +++++
		int expected_scenarios = sub_individus_.size();
		int received_scenarios = resp.scenario_info_results_size();

		if (received_scenarios != expected_scenarios) {
			cerr << "ERROR: Expected " << expected_scenarios
			   << " scenarios but received " << received_scenarios << endl;
			if (received_scenarios == 0) {
				cerr << "    No scenarios in response! Skipping this client." << endl;
				continue;
			}
			cerr << "    Will process only " << std::min(expected_scenarios, received_scenarios) << " scenarios." << endl;
		}

		if (traces) {
			cout << "Expected scenarios: " << expected_scenarios << endl;
			cout << "Received scenarios: " << received_scenarios << endl;
		}

		// +++++ CRITICAL: Validate all sub_individus and LocalSearch pointers +++++
		bool validation_failed = false;
		for (int s = 0; s < sub_individus_.size(); s++) {
			if (!sub_individus_[s]) {
				cerr << "ERROR: sub_individus_[" << s << "] is nullptr in gRPC response processing" << endl;
				validation_failed = true;
				break;
			}
			if (!sub_individus_[s]->localSearch) {
				cerr << "ERROR: sub_individus_[" << s << "]->localSearch is nullptr in gRPC response processing" << endl;
				validation_failed = true;
				break;
			}
		}

		if (validation_failed) {
			cerr << "    Skipping client " << vec_rand_clients[c] << " due to validation failure" << endl;
			continue;
		}

		double total_current_cost = 0, total_objective = 0;
		int scenarios_processed = 0;

		for (int s = 0; s < sub_individus_.size(); s++) {
			// Bounds check
			if (s >= resp.scenario_info_results_size()) {
				cerr << "ERROR: scenario index " << s << " out of bounds for scenario_info_results (size="
				   << resp.scenario_info_results_size() << ")" << endl;
				continue;
			}

			const irp::ScenarioInfo& scenario_result = resp.scenario_info_results(s);

			// Validate scenario result has expected data
			if (scenario_result.daily_plans_size() == 0) {
				cerr << "WARNING: Scenario " << s << " has no daily_plans in response" << endl;
				continue;
			}

			for (int i = 1; i <= scenario_result.daily_plans_size(); i++) {
				// Bounds check
				if (i - 1 >= sub_individus_[s]->localSearch->vec_delivery_plans_.size()) {
					cerr << "ERROR: delivery plan index " << (i-1) << " out of bounds for vec_delivery_plans_ (size="
					   << sub_individus_[s]->localSearch->vec_delivery_plans_.size() << ")" << endl;
					continue;
				}
				if (i - 1 >= scenario_result.daily_plans_size()) {
					cerr << "ERROR: daily_plans index " << (i-1) << " out of bounds (size="
					   << scenario_result.daily_plans_size() << ")" << endl;
					continue;
				}
				if (i - 1 >= scenario_result.daily_quantities_size()) {
					cerr << "ERROR: daily_quantities index " << (i-1) << " out of bounds (size="
					   << scenario_result.daily_quantities_size() << ")" << endl;
					continue;
				}

				sub_individus_[s]->localSearch->vec_delivery_plans_[i - 1].choose_delivery = scenario_result.daily_plans(i-1);
				sub_individus_[s]->localSearch->vec_delivery_plans_[i - 1].quantity = scenario_result.daily_quantities(i-1);
			}

		// take/getGPUactualUsefirstDeliverydecision
		sub_individus_[s]->localSearch->gpu_day_1_delivery_ = scenario_result.actual_day_1_delivery();

			total_current_cost += sub_individus_[s]->localSearch->current_cost_;
			vec_objective[s] = scenario_result.total_cost();
			total_objective += vec_objective[s];
			scenarios_processed++;
		}

		// Only apply improvements if we successfully processed all scenarios
		if (scenarios_processed != sub_individus_.size()) {
			cerr << "WARNING: Only processed " << scenarios_processed << "/" << sub_individus_.size()
			   << " scenarios successfully. Skipping improvement application." << endl;
			continue;
		}

		// CheckResultisnotis
		if (total_current_cost > total_objective) {
			is_improved = true;
			if (traces) cout << "Improvement found! Applying delivery plan..." << endl;
			for (int s = 0; s < sub_individus_.size(); s++) {
				sub_individus_[s]->localSearch->ApplyDeliveryPlan(vec_rand_clients[c], vec_objective[s]);
			}
		} else if (traces) {
			cout << "No improvement: current=" << total_current_cost << " vs objective=" << total_objective << endl;
		}
		if (traces) cout << "fill resp" << endl;
		// cin.get();
		// Check if scenario first day results are consistent
		// for (int s = 1; s < sub_individus_.size(); s++) {
		// 	if (resp.scenario_info_results(s).daily_plans(0) != resp.scenario_info_results(0).daily_plans(0)) {
		// 		throw std::runtime_error("daily_plans(0) not consistent");
		// 	}
		// 	if (resp.scenario_info_results(s).daily_quantities(0) != resp.scenario_info_results(0).daily_quantities(0)) {
		// 		throw std::runtime_error("daily_quantities(0) not consistent");
		// 	}
		// }
		// // Check eachsub_individulocalsearch->demandperday[1]isnoconsistent
		// cout << "c: " << vec_rand_clients[c] << endl;
		// for (int s1 = 0; s1 < sub_individus_.size(); s1++) {
		// 	for (int i = 0; i < sub_individus_[s1]->localSearch->demandPerDay[1].size(); i++) {
		// 		cout << sub_individus_[s1]->localSearch->demandPerDay[1][i] << " ";
		// 	}
		// 	cout << endl;
		// }
		// cout << endl;
		// for (int s = 0; s < sub_individus_.size(); s++) {
		// 	if (sub_individus_[s]->localSearch->demandPerDay[1][vec_rand_clients[c]] != sub_individus_[0]->localSearch->demandPerDay[1][vec_rand_clients[c]]) {
		// 		cout << "demandperday[1] not consistent" << endl;
		// 		cout << "c: " << vec_rand_clients[c] << endl;
		// 		for (int s1 = 0; s1 < sub_individus_.size(); s1++) {
		// 			for (int i = 0; i < sub_individus_[s1]->localSearch->demandPerDay[1].size(); i++) {
		// 				cout << sub_individus_[s1]->localSearch->demandPerDay[1][i] << " ";
		// 			}
		// 			cout << endl;
		// 		}
		// 		cout << endl;
		// 		throw std::runtime_error("demandperday[1] not consistent");
		// 	}
		// }
	}
	if (traces) cout << "is_improved: " << is_improved << endl;
	// if (is_improved && rep_times < 5) {
	// 	RunSearchPi_gRPC(rep_times + 1);
	// }

	// +++++ First day control:gRPCOptimizeafter/backforcesyncfirst +++++
	// GPUSolverswillforeachScenarioOptimize,possiblyfirstnotconsistent
	// forceUseScenario0firstDeliverydecision
	if (control_day_1_ == 1) {
		if (traces) cout << "Syncing day 1 after gRPC optimization..." << endl;

		// SaveScenario0firstDeliverydecision
		std::vector<int> day1_clients;
		std::vector<double> day1_quantities;

		if (sub_individus_[0]->localSearch != nullptr) {
			auto& local_search = sub_individus_[0]->localSearch;
			// take/getfirstDeliveryInfo
			if (local_search->routes.size() > 1 && local_search->routes[1].size() > 0) {
				for (auto* route : local_search->routes[1]) {
					auto* node = route->depot->suiv;
					while (!node->estUnDepot) {
						day1_clients.push_back(node->cour);
						day1_quantities.push_back(local_search->demandPerDay[1][node->cour]);
						node = node->suiv;
					}
				}
			}

			// ApplytoallotherScenario
			for (int s = 1; s < sub_individus_.size(); s++) {
				if (sub_individus_[s]->localSearch != nullptr) {
					auto& ls = sub_individus_[s]->localSearch;
					// ResetfirstDeliveryforScenario0decision
					for (size_t i = 0; i < day1_clients.size(); i++) {
						int client_id = day1_clients[i];
						double quantity = day1_quantities[i];
						ls->demandPerDay[1][client_id] = quantity;
					}
				}
			}
		}

		if (traces) cout << "Day 1 sync completed after gRPC" << endl;
	}
}

void Individu::UpdatePenalty()
{
	if (!is_scenario_) {
		return;
	}
	if (g_trace) cout << "UpdatePenalty_scenario" << endl;
	BatchRunFunc([&](int s) {
		sub_individus_[s]->params->penalityCapa = params->penalityCapa;
		sub_individus_[s]->params->penalityLength = params->penalityLength;
	}, sub_individus_.size());
}

void Individu::printChromT() {
	if (is_scenario_) {
		for (int s = 0; s < sub_individus_.size(); s++) {
			cout << "Scenario-Individu " << s << ": " << endl;
			sub_individus_[s]->printChromT();
		}
		return;
	}
	for (int k = 1; k <= params->nbDays; k++) {
		cout << "Day " << k << ": ";
		for (int i = 0; i < (int)chromT[k].size(); i++) {
			cout << chromT[k][i] << " ";
		}
		cout << endl;
	}
}

void Individu::CheckSubDay1Consistency(std::string msg) {
	// Check sub_individu chromT[1] and chromL[1] consistency
	auto temp_chromT = sub_individus_[0]->chromT[1];
	auto temp_chromL = sub_individus_[0]->chromL[1];
	
	// Sort sub_individu chromT[1] as benchmark
	std::sort(temp_chromT.begin(), temp_chromT.end());
	
	for (int s = 0; s < sub_individus_.size(); s++) {
		// Sort current sub_individu chromT[1] for comparison
		auto current_chromT = sub_individus_[s]->chromT[1];
		std::sort(current_chromT.begin(), current_chromT.end());
		
		if (current_chromT != temp_chromT) {
			for (int s1 = 0; s1 < sub_individus_.size(); s1++) {
				cout << s1 << ": ";
				for (int i = 0; i < sub_individus_[s1]->chromT[1].size(); i++) {
					cout << sub_individus_[s1]->chromT[1][i] << " ";
				}
				cout << endl;
			}
			throw std::runtime_error("chromT[1] not consistent: " + msg);
		}
	}
}
// fromspecifiedScenariosyncfirsttoallScenario
void Individu::SyncDay1FromScenario(int source_scenario_id) {
	if (!is_scenario_ || control_day_1_ != 1) {
		return; // Only execute in multi-scenario with first day control
	}

	if (source_scenario_id < 0 || source_scenario_id >= sub_individus_.size()) {
		throw std::runtime_error("Invalid source_scenario_id: " + std::to_string(source_scenario_id));
	}

	// +++++ CRITICAL: Validate sub_individus before accessing chromT/chromL +++++
	if (!sub_individus_[source_scenario_id]) {
		cerr << "ERROR: sub_individus_[" << source_scenario_id << "] is nullptr in SyncDay1FromScenario" << endl;
		throw std::runtime_error("Source scenario is nullptr");
	}

	// +++++ CRITICAL: Validate chromT and chromL have enough days +++++
	if (sub_individus_[source_scenario_id]->chromT.size() < 2) {
		cerr << "WARNING: chromT.size() = " << sub_individus_[source_scenario_id]->chromT.size()
		   << " in scenario " << source_scenario_id << ", skipping SyncDay1" << endl;
		return; // Not enough days to sync
	}
	if (sub_individus_[source_scenario_id]->chromL.size() < 2) {
		cerr << "WARNING: chromL.size() = " << sub_individus_[source_scenario_id]->chromL.size()
		   << " in scenario " << source_scenario_id << ", skipping SyncDay1" << endl;
		return; // Not enough days to sync
	}

	// GetScenariofirstNumber of days
	auto source_chromT_day1 = sub_individus_[source_scenario_id]->chromT[1];
	auto source_chromL_day1 = sub_individus_[source_scenario_id]->chromL[1];

	// Sync to all other scenarios
	for (int s = 0; s < sub_individus_.size(); s++) {
		if (s == source_scenario_id) continue;

		// +++++ CRITICAL: Validate each scenario before accessing +++++
		if (!sub_individus_[s]) {
			cerr << "ERROR: sub_individus_[" << s << "] is nullptr in SyncDay1FromScenario" << endl;
			continue; // Skip this scenario
		}

		// Validate arrays have enough size
		if (sub_individus_[s]->chromT.size() < 2 || sub_individus_[s]->chromL.size() < 2) {
			cerr << "WARNING: Scenario " << s << " arrays too small, skipping sync" << endl;
			continue;
		}

		sub_individus_[s]->chromT[1] = source_chromT_day1;
		sub_individus_[s]->chromL[1] = source_chromL_day1;

		// needNewCalculatefitness
		sub_individus_[s]->isFitnessComputed = false;
	}
}

// fromScenario0syncfirsttoallotherScenario(commonly used)
void Individu::SyncDay1ToAllScenarios() {
	SyncDay1FromScenario(0);
}

// Protected first day decisions in LocalSearch
void Individu::ProtectDay1InLocalSearch() {
	if (!is_scenario_ || control_day_1_ != 1) {
		return;
	}

	// thisFunctioninLocalSearchbefore/frontcall,SavefirstRoute
	// Actual protection implemented in LocalSearch
	// This function passes control_day_1 parameter correctly

	for (int s = 0; s < sub_individus_.size(); s++) {
		if (sub_individus_[s]->localSearch != nullptr) {
			// LocalSearchneedfirstProtected
			// LocalSearch class will be modified in subsequent steps
		}
	}
}
