#include "irp/core/Population.h"
#include <fstream>
#include <string>
#include <cmath>
#include <sstream>
// constructeur
Population::Population(Params *params) : params(params)
{
	Individu *randomIndiv;
	trainer = new Individu(params, 1.0); //Initialize an instance,including all arrays for split.
	delete trainer->localSearch;  		 //
	trainer->localSearch = new LocalSearch(params, trainer); //Initialize data structures for local search,and prepare for subsequent operations.

	double temp = params->penalityCapa;
	double temp2 = params->penalityLength;
	// SousPop structure for sub-population management
	valides = new SousPop(); 
	invalides = new SousPop();
	valides->nbIndiv = 0;
	invalides->nbIndiv = 0;
	valides->nbGenerations = 0;
	invalides->nbGenerations = 0;
	bool compter = true;

	// TODO -- for testing for now Generate2*muindividuals
	/*randomIndiv = new Individu (params,1.0);
	recopieIndividu(trainer,randomIndiv);
	trainer->generalSplit();
	trainer->updateLS();
	trainer->localSearch->runILS(false,100);
	return ;*/ 

	for (int i = 0; i < params->mu * 2; i++)
	{
		if (i == params->mu)
		{
			params->penalityCapa *= 50;
			params->penalityLength *= 50;
			compter = false;
		}
		randomIndiv = new Individu(params, 1.0); 
		// chromT is a 2D vector: [days][customer visit sequence]
		
		education(randomIndiv);
		if (compter)
			updateNbValides(randomIndiv);
		addIndividu(randomIndiv);
		delete randomIndiv;
	}

	// on initialise par defaut a 100, comme si tout etait valide au debut
	// mais c'est arbitraire
	for (int i = 0; i < 100; i++) 
 // Default initialization list,assuming100individualsinStartisValid
	{
		listeValiditeCharge.push_back(true);
		listeValiditeTemps.push_back(true);
	}

	params->penalityCapa = temp;
	params->penalityLength = temp2;
}

Population::Population(ParamsList* params_list) : params(params_list->general_params_), params_list_(params_list)
{
	bool trace = true;
	Individu *randomIndiv;
	trainer = new Individu(params_list_); //Initialize an instance,Useparams_list.TODO NewindividualsInstance

	// +++++ CRITICAL: Validate trainer was created successfully +++++
	if (!trainer) {
		throw std::runtime_error("FATAL: Failed to create trainer Individu in Population constructor");
	}
	if (trainer->sub_individus_.empty()) {
		throw std::runtime_error("FATAL: trainer->sub_individus_ is empty in Population constructor");
	}

	// Essentially determine first day delivery plan first then each scenario finds its own optimal solution Therefore neededlocalsearchcount matchesparams_listconsistent
	// +++++ CRITICAL: Add error handling for LocalSearch initialization +++++
	int ls_init_failures = 0;
	BatchRunFunc([&](int s) {
		try {
			// Validate sub_individu exists
			if (!trainer->sub_individus_[s]) {
				cerr << "ERROR: trainer->sub_individus_[" << s << "] is nullptr before LocalSearch init" << endl;
				ls_init_failures++;
				return;
			}

			// Clean up existing LocalSearch if present
			delete trainer->sub_individus_[s]->localSearch;
			trainer->sub_individus_[s]->localSearch = nullptr; // set tonullptravoid double free

			// Create new LocalSearch with error handling
			trainer->sub_individus_[s]->localSearch = new LocalSearch(params_list_->vec_params_[s], trainer->sub_individus_[s]);

			// Validate it was created successfully
			if (!trainer->sub_individus_[s]->localSearch) {
				cerr << "ERROR: Failed to create LocalSearch for scenario " << s << endl;
				ls_init_failures++;
			} else if (trace) {
				cout << "✓ LocalSearch initialized successfully for scenario " << s << endl;
			}
		} catch (const std::exception& e) {
			cerr << "ERROR: Exception creating LocalSearch for scenario " << s << ": " << e.what() << endl;
			trainer->sub_individus_[s]->localSearch = nullptr;
			ls_init_failures++;
		}
	}, trainer->sub_individus_.size());

	// +++++ Check if any LocalSearch initialization failed +++++
	if (ls_init_failures > 0) {
		throw std::runtime_error("FATAL: " + std::to_string(ls_init_failures) +
		            " LocalSearch initialization(s) failed out of " +
		            std::to_string(trainer->sub_individus_.size()) + " scenarios");
	}

	// Population does not need multiple scenarios

	// Save original penalty for adjustment during generation
	std::pair<double, double> original_penality = std::make_pair(params->penalityCapa, params->penalityLength);
	// SousPop structure for sub-population management
	valides = new SousPop(); 
	invalides = new SousPop();
	valides->nbIndiv = 0;
	invalides->nbIndiv = 0;
	valides->nbGenerations = 0;
	invalides->nbGenerations = 0;
	bool compter = true;

	// TODO -- for testing for now Generate2*muindividuals
	/*randomIndiv = new Individu (params,1.0);
	recopieIndividu(trainer,randomIndiv);
	trainer->generalSplit();
	trainer->updateLS();
	trainer->localSearch->runILS(false,100);
	return ;*/ 

	for (int i = 0; i < params->mu * 2; i++)
	{
		if (i == params->mu)
		{
			params->penalityCapa *= 50;
			params->penalityLength *= 50;
			compter = false;
		}
		randomIndiv = new Individu(params_list_); 
		// chromT is a 2D vector: [days][customer visit sequence]
		if (trace) cout << "new Individu" << endl;
		education(randomIndiv);
		if (trace) cout << "education" << endl;
		if (compter)
			updateNbValides(randomIndiv);
		if (trace) cout << "updateNbValides" << endl;
		addIndividu(randomIndiv);
		if (trace) cout << "addIndividu" << endl;
		delete randomIndiv;
	}

	// on initialise par defaut a 100, comme si tout etait valide au debut
	// mais c'est arbitraire
	for (int i = 0; i < 100; i++) 
 // Default initialization list,assuming100individualsinStartisValid
	{
		listeValiditeCharge.push_back(true);
		listeValiditeTemps.push_back(true);
	}

	params->penalityCapa = original_penality.first;
	params->penalityLength = original_penality.second;

}

// destructeur
Population::~Population()
{
	for (int i = 0; i < (int)valides->individus.size(); i++)
		delete valides->individus[i];

	for (int i = 0; i < (int)invalides->individus.size(); i++)
		delete invalides->individus[i];

	delete trainer;
}
// Update sub-population individuals
void Population::evalExtFit(SousPop *pop)
{
	int temp;
	vector<int> classement;
	vector<double> distances;

	for (int i = 0; i < pop->nbIndiv; i++)
	{
		classement.push_back(i);
		distances.push_back(pop->individus[i]->distPlusProche(params->nbCountDistMeasure));
	}

	// classement des individus en fonction de leur note de distance
	for (int n = 0; n < pop->nbIndiv; n++)
		for (int i = 0; i < pop->nbIndiv - n - 1; i++)
			if (distances[classement[i]] < distances[classement[i + 1]] - 0.000001)
			{
				temp = classement[i + 1];
				classement[i + 1] = classement[i];
				classement[i] = temp;
			}

	for (int i = 0; i < pop->nbIndiv; i++)
	{
		pop->individus[classement[i]]->divRank = (float)i / (float)(pop->nbIndiv - 1);
		pop->individus[classement[i]]->fitRank = (float)classement[i] / (float)(pop->nbIndiv - 1);
		pop->individus[classement[i]]->fitnessEtendu = pop->individus[classement[i]]->fitRank + ((float)1.0 - (float)params->el / (float)pop->nbIndiv) * pop->individus[classement[i]]->divRank;
	}
}
// Update sub-population individuals
int Population::addIndividu(Individu *indiv)
{
	SousPop *souspop;
	int k, result;

	if (indiv->estValide)
		souspop = valides;
	else
		souspop = invalides;

	result = placeIndividu(souspop, indiv);
	// il faut eventuellement enlever la moitie de la pop weneedremovehalf of pop.
	if (result != -1 && souspop->nbIndiv > params->mu + params->lambda)
	{

		while (souspop->nbIndiv > params->mu)
		{
			k = selectCompromis(souspop);
			removeIndividu(souspop, k);
		}

		souspop->nbGenerations++;
	}

	return result;
}

// met a jour les individus les plus proches d'une population
// en fonction de l'arrivant

void Population::updateProximity(SousPop *pop, Individu *indiv)
{
	for (int k = 0; k < pop->nbIndiv; k++)
		if (pop->individus[k] != indiv)
		{
			pop->individus[k]->addProche(indiv);
			indiv->addProche(pop->individus[k]);
		}
}

// fonction booleenne verifiant si le fitness n'existe pas deja

bool Population::fitExist(SousPop *pop, Individu *indiv)
{
	double fitness = indiv->coutSol.evaluation;
	for (int i = 0; i < (int)pop->nbIndiv; i++)
	{
		if (indiv != pop->individus[i] && pop->individus[i]->coutSol.evaluation >= (fitness - params->delta) && pop->individus[i]->coutSol.evaluation <= (fitness + params->delta))
			return true;
	}
	return false;
}
// procede de redemarrage avec remplacement d'une partie de la population
// modele tres simplifie
// on remplace la moitie individus de fitness situes sous la moyenne par des individus aleatoires
void Population::diversify()
{
	Individu *randomIndiv;
	double temp = params->penalityCapa;
	double temp2 = params->penalityLength;

	while (valides->nbIndiv > (int)(params->rho * (double)params->mu))
	{
		delete valides->individus[valides->nbIndiv - 1];
		valides->individus.pop_back();
		valides->nbIndiv--;
	}
	/*removelowFitnesssolve:

forValidsolve,ifCountpredeterminedThreshold(params->rho * params->mu),thenfromListDelete.,Fitnesslowsolve.
forInvalidsolve,Executesameoperation*/
	while (invalides->nbIndiv > (int)(params->rho * (double)params->mu))
	{
		delete invalides->individus[invalides->nbIndiv - 1];
		invalides->individus.pop_back();
		invalides->nbIndiv--;
	}

	for (int i = 0; i < params->mu * 2; i++)
	{
		if (i == params->mu)
		{
			params->penalityCapa *= 50;
			params->penalityLength *= 50;
		}
		// Multi-scenario initialization
		if (params->is_scenario_) {
			randomIndiv = new Individu(params_list_);
		} else {
			randomIndiv = new Individu(params, 1.0);
		}
		education(randomIndiv);
		addIndividu(randomIndiv);
		delete randomIndiv;
	}

	params->penalityCapa = temp;
	params->penalityLength = temp2;
}

int Population::placeIndividu(SousPop *pop, Individu *indiv)
{
	Individu *monIndiv;
	// to be safe problemcan
	if (params->is_scenario_) {
		monIndiv = new Individu(params_list_);
	} else {
		monIndiv = new Individu(params);
	}
	recopieIndividu(monIndiv, indiv);

	// regarde si son fitness est suffisamment espace
	bool placed = false;
	int i = (int)pop->individus.size() - 1;

	pop->individus.push_back(monIndiv);
	while (i >= 0 && !placed)
	{
		if (pop->individus[i]->coutSol.evaluation >= indiv->coutSol.evaluation + 0.001)
		{
			pop->individus[i + 1] = pop->individus[i];
			i--;
		}
		else
		{
			pop->individus[i + 1] = monIndiv;
			placed = true;
			pop->nbIndiv++;
			updateProximity(pop, pop->individus[i + 1]);
			return i + 1; // reussite
		}
	}
	if (!placed)
	{
		pop->individus[0] = monIndiv;
		placed = true;
		pop->nbIndiv++;
		updateProximity(pop, pop->individus[0]);
		if (pop == valides)
			timeBest = clock();
		return 0; // reussite
	}
	cout << "erreur placeIndividu" << endl;
	return -3;
}

void Population::removeIndividu(SousPop *pop, int p)
{
	Individu *partant = pop->individus[p];

	// on place notre individu a la fin
	for (int i = p + 1; i < (int)pop->individus.size(); i++)
		pop->individus[i - 1] = pop->individus[i];

	// on l'enleve de la population
	pop->individus.pop_back();
	pop->nbIndiv--;

	// on l'enleve des structures de proximite
	for (int i = 0; i < pop->nbIndiv; i++)
		pop->individus[i]->removeProche(partant);

	// et on supprime le partant
	delete partant;
}
// recalcule l'evaluation des individus a partir des violation
// puis effectue un tri a bulles de la population
// operateur de tri peu efficace mais fonction appellee tres rarement
void Population::validatePen()
{
	Individu *indiv;
	// on met a jour les evaluations
	for (int i = 0; i < valides->nbIndiv; i++) {
		valides->individus[i]->UpdatePenalty();
	}
	for (int i = 0; i < invalides->nbIndiv; i++)
	{
		invalides->individus[i]->UpdatePenalty(); // Updatepenalty,onlyinbeforewillPermanentAdjustedpenalty(in genetic)
		invalides->individus[i]->measureSol();
	}

	for (int i = 0; i < invalides->nbIndiv; i++)
		for (int j = 0; j < invalides->nbIndiv - i - 1; j++)
		{
			if (invalides->individus[j]->coutSol.evaluation > invalides->individus[j + 1]->coutSol.evaluation)
			{
				indiv = invalides->individus[j];
				invalides->individus[j] = invalides->individus[j + 1];
				invalides->individus[j + 1] = indiv;
			}
		}
}

Individu *Population::getIndividuBinT(double &rangRelatif)
{
	Individu *individu1;
	Individu *individu2;
	int place1, place2;
	double rangRelatif1, rangRelatif2;

	place1 = params->rng->genrand64_int64() % (valides->nbIndiv + invalides->nbIndiv);
	if (place1 >= valides->nbIndiv)
	{
		individu1 = invalides->individus[place1 - valides->nbIndiv];
		rangRelatif1 = (double)(place1 - valides->nbIndiv) / (double)invalides->nbIndiv;
	}
	else
	{
		individu1 = valides->individus[place1];
		rangRelatif1 = (double)place1 / (double)valides->nbIndiv;
	}

	place2 = params->rng->genrand64_int64() % (valides->nbIndiv + invalides->nbIndiv);
	if (place2 >= valides->nbIndiv)
	{
		individu2 = invalides->individus[place2 - valides->nbIndiv];
		rangRelatif2 = (double)(place2 - valides->nbIndiv) / (double)invalides->nbIndiv;
	}
	else
	{
		individu2 = valides->individus[place2];
		rangRelatif2 = (double)place2 / (double)valides->nbIndiv;
	}

	evalExtFit(valides);
	evalExtFit(invalides);

	if (individu1->fitnessEtendu < individu2->fitnessEtendu)
	{
		rangRelatif = rangRelatif1;
		return individu1;
	}
	else
	{
		rangRelatif = rangRelatif2;
		return individu2;
	}
}

// rank donne le rang de l'individu choisi
Individu *Population::getIndividuRand(double &rangRelatif)
{
	Individu *individu1;

	int place1 = params->rng->genrand64_int64() % (valides->nbIndiv + invalides->nbIndiv);
	if (place1 >= valides->nbIndiv)
	{
		individu1 = invalides->individus[place1 - valides->nbIndiv];
		rangRelatif = (double)(place1 - valides->nbIndiv) / (double)invalides->nbIndiv;
	}
	else
	{
		individu1 = valides->individus[place1];
		rangRelatif = (double)place1 / (double)valides->nbIndiv;
	}
	return valides->individus[place1];
}
// getter de 1 individu par selection au hasard dans un certain % des meilleurs meilleurs
// rank donne le rang de l'individu choisi
Individu *Population::getIndividuPourc(int pourcentage, double &rangRelatif)
{
	int place;
	if ((valides->nbIndiv * pourcentage) / 100 != 0)
	{
		place = params->rng->genrand64_int64() % ((valides->nbIndiv * pourcentage) / 100);
		rangRelatif = (double)place / (double)params->mu;
		return valides->individus[place];
	}
	else
		return getIndividuBinT(rangRelatif);
}
Individu *Population::getIndividuBestValide()
{
	if (valides->nbIndiv != 0)
		return valides->individus[0];
	else
		return NULL;
}

Individu *Population::getIndividuBestInvalide()
{
	// for (int i = 0; i < invalides->nbIndiv; i++) {
	// 	cout << "invalides->individus[" << i << "]->coutSol.evaluation: " << invalides->individus[i]->coutSol.evaluation << endl;
	// }
	// cin.get();
	if (invalides->nbIndiv != 0)
		return invalides->individus[0];
	else
		return NULL;
}

// getter simple d'un individu
Individu *Population::getIndividu(int p)
{
	return valides->individus[p];
}
// recopie un Individu dans un autre
// ATTENTION !!! ne recopie que le chromP, chromT et les attributs du fitnessonlyCopy chromP,chromT andhealthyProperty
void Population::recopieIndividu(Individu *destination, Individu *source)
{
	if (source->is_scenario_)
	{
		destination->is_scenario_ = source->is_scenario_;
		// destination->params = source->params; // TODO needModify?
		destination->sub_individus_.resize(source->sub_individus_.size());
		BatchRunFunc([&](int s) {
			recopieIndividu(destination->sub_individus_[s], source->sub_individus_[s]);
		}, source->sub_individus_.size());
		destination->coutSol = source->coutSol;
		return;
	}
	destination->chromT = source->chromT;
	destination->chromL = source->chromL;
	destination->chromR = source->chromR;
	destination->coutSol = source->coutSol;
	destination->isFitnessComputed = source->isFitnessComputed;
	destination->estValide = source->estValide;
	destination->suivants = source->suivants;
	destination->precedents = source->precedents;
}

void Population::ExportPop(string nomFichier,bool add)
{
	
	// Export current solutions to file
	vector<int> rout;
	vector<double> routTime;
	int compteur;
	Noeud *noeudActuel;
	LocalSearch *loc;
	ofstream myfile;
	double cost;
	double temp, temp2;
	char *myBuff;
	Individu *bestValide = getIndividuBestValide();
	//cout <<"in";
	//int q;cin>>q;

	if (bestValide != NULL)
	{
		temp = params->penalityCapa;
		temp2 = params->penalityLength;
		params->penalityCapa = 10000;
		params->penalityLength = 10000;
		//cout <<"edu begin";cin>>q;
		education(bestValide);
		//cout <<"edu";cin>>q;
		// le trainer a garda les infos des routes de bestValide
		params->penalityCapa = temp;
		params->penalityLength = temp2;
		// We will update the local search structure for paths.
		// We are obliged to set very strong parameters so that the splitting does not produce a from the best valid solution
		// so that the splitting does not produce a from the best valid solution
	// Only export requested scenario count
	int expected_num_scenarios = bestValide->sub_individus_.size();
	int actual_num_scenarios = trainer->sub_individus_.size();

	if (actual_num_scenarios != expected_num_scenarios) {
		cout << "⚠️ Warning in ExportPop: Expected " << expected_num_scenarios
		   << " scenarios but trainer has " << actual_num_scenarios << endl;
		cout << "  Exporting only the first " << expected_num_scenarios << " scenarios." << endl;
	}

	int num_scenarios_to_export = std::min(expected_num_scenarios, actual_num_scenarios);

	for (int s = 0; s < num_scenarios_to_export; s++) {
		cout << "  [ExportPop] Checking scenario " << s << " before writing" << endl;
			auto sub_indiv = trainer->sub_individus_[s];
			if (sub_indiv == nullptr) {
				throw std::runtime_error("sub_indiv is nullptr");
			}

			// GetCurrentScenario localSearch
			auto local_search = sub_indiv->localSearch;
			if (local_search == nullptr) {
				cout << "Error: local_search is nullptr for scenario " << s << endl;
				continue;
			}

			// +++++ CheckisnohaveValidRoute,ifnonethenSkipnotSave +++++
			compteur = 0;
			for (int k = 1; k <= params->nbDays; k++)
				for (int i = 0; i < (int)local_search->routes[k].size(); i++)
					if (!local_search->depots[k][i]->suiv->estUnDepot)
						compteur++;

			if (compteur == 0) {
				cout << "  [ExportPop] Scenario " << s << " has no valid routes, skipping file creation" << endl;
				continue;
			}

			cout << "  [ExportPop] Writing scenario " << s << " to file (add=" << add << "), routes=" << compteur << endl;

			myfile.precision(10);
			cout.precision(10);
			// +++++ u4feeu590duff1au7b2cu4e00u4e2au573au666fu6839u636eaddu53c2u6570uff0cu540eu7eedu573au666fu603bu662fu8ffdu52a0 +++++
			bool should_append = add || (s > 0);
			if (should_append) myfile.open(nomFichier.data(), std::ios::app);
			else myfile.open(nomFichier.data()); // Overwrite on first scenario if add=false

			if (s == 0) {
				myfile << "Total Cost: " << trainer->coutSol.evaluation << endl;

				// +++++ GPU STATISTICS: Output GPU call statistics +++++
				myfile << "GPU Calls: " << Individu::total_gpu_calls_.load() << endl;
				myfile << "GPU Scenarios Processed: " << Individu::total_scenarios_processed_.load() << endl;
				if (Individu::total_gpu_calls_.load() > 0) {
					double avg_scenarios = (double)Individu::total_scenarios_processed_.load()
					           / (double)Individu::total_gpu_calls_.load();
					myfile << "Avg Scenarios per GPU Call: " << avg_scenarios << endl;
				}

				// Output GPU timing statistics from LogStats
				if (g_log_stats.count("GPU_ProcessOptimization") > 0) {
					auto& gpu_stats = g_log_stats["GPU_ProcessOptimization"];
					myfile << "GPU Total Time (ms): " << gpu_stats.total_time_ms << endl;
					myfile << "GPU Avg Time (ms): " << gpu_stats.avg_time_ms << endl;
				}
			}
				
			cout << "writing the best solution: fitness " << sub_indiv->coutSol.evaluation << " in : " << nomFichier.c_str() << endl;
			
			myfile<<endl<<endl;
			// CommentInventoryOutput,onlyretainRouteandcostInfo
			// local_search->printInventoryLevels(myfile,false);
			// export cost
			myfile << sub_indiv->coutSol.evaluation << endl;

			// exporting the number of routes
			compteur = 0;
			for (int k = 1; k <= params->nbDays; k++)
				for (int i = 0; i < (int)local_search->routes[k].size(); i++)
					if (!local_search->depots[k][i]->suiv->estUnDepot)
						compteur++;
			myfile << compteur << endl;

			// exporting the total CPU time (ms)
			myBuff = new char[100];
			myfile <<"Total Time: ";sprintf(myBuff, "%d", (int)(clock() / 1000000));
			myfile << myBuff << endl;

			myBuff = new char[100];
			// CalculatefromStarttoinTime(seconds)
			auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
				std::chrono::steady_clock::now() - params->debut
			);
			myfile <<"PITime: ";sprintf(myBuff, "%d", (int)elapsed.count());
			myfile << myBuff << endl;

			// exporting the time to best solution
			myBuff = new char[100];
			myfile <<"Best Solution Time: ";sprintf(myBuff, "%d", (int)(timeBest / 1000000));
			//strncpy(myBuff, "%d", (int)(timeBest / 1000));
			myfile << myBuff << endl;

			for (int k = 1; k <= params->nbDays; k++)
			{
				for (int i = 0; i < (int)local_search->routes[k].size(); i++)
				{
					compteur = 1;
					if (!local_search->depots[k][i]->suiv->estUnDepot)
					{

						myfile << " " << local_search->routes[k][i]->depot->cour << " " << (k - 1) % params->ancienNbDays + 1 << " " << compteur << " " << local_search->routes[k][i]->temps << " "
							<< local_search->routes[k][i]->charge << " ";

						// on place la route dans un vecteur de noeuds clients
						noeudActuel = local_search->routes[k][i]->depot->suiv;
						cost = params->timeCost[local_search->routes[k][i]->depot->cour][noeudActuel->cour];

						rout.clear();
						routTime.clear();
						rout.push_back(local_search->routes[k][i]->depot->cour);
						routTime.push_back(0);
						rout.push_back(noeudActuel->cour);
						routTime.push_back(cost);

						while (!noeudActuel->estUnDepot)
						{
							cost += params->cli[noeudActuel->cour].serviceDuration + params->timeCost[noeudActuel->cour][noeudActuel->suiv->cour];
							noeudActuel = noeudActuel->suiv;
							rout.push_back(noeudActuel->cour);
							routTime.push_back(cost);
						}

						myfile << " " << (int)rout.size();

						for (int j = 0; j < (int)rout.size(); j++)
						{
							myfile << " " << rout[j];
						}
						myfile << endl;
						compteur++;
					}
				}
			}

			// Print client daily demand
			// myfile << "Daily Demand: " << endl;
			// // Print
			// myfile << "Client\\Day";
			// for (int k = 1; k <= params->nbDays; k++) {
			// 	myfile << "\t" << k;
			// }
			// myfile << endl;
			
			// // byCustomerforrow,Number of daysforcolumnPrint
			// for (int i = params->nbDepots; i < params->nbClients + params->nbDepots; i++) {
			// 	myfile << "Client " << i << ":";
			// 	for (int k = 1; k <= params->nbDays; k++) {
			// 		// CheckIndexisnoValid
			// 		if (k-1 < params_list_->vec_params_[s]->cli[i].dailyDemand.size()) {
			// 			myfile << "\t" << params_list_->vec_params_[s]->cli[i].dailyDemand[k-1];
			// 		} else {
			// 			myfile << "\tN/A";
			// 		}
			// 	}
			// 	myfile << endl;
			// }

			myfile.close();
		}
	}
	else
	{
		cout << " impossible to find a valid individual " << endl;
	}
}

void Population::ExportBKS(string nomFichier)
{
	double fit,tim,pri;
	ifstream fichier;
  std::string line;
	fichier.open(nomFichier.c_str());
	if (fichier.is_open())
	{
		while (getline(fichier, line)) {
      std::size_t found = line.find("COST SUMMARY : OVERALL");
      if (found != std::string::npos) {
				//cout<<"~!";
        std::stringstream ss(line.substr(found));
        std::string temp;
			
        ss >> temp >> temp>> temp >> temp>>fit;
        
      }
			found = line.find("Total Time:");
      if (found != std::string::npos) {
				//cout<<"~!";
        std::stringstream ss(line.substr(found));
        std::string temp;
			
        ss >> temp >> temp>>tim;
        break; 
      }
    }
		fichier.close();
		timeBest = clock();
		// "<<fit<<endl;
		//cout<<"pri > tim"<<pri <<" "<< tim<<endl;
		// this "<<getIndividuBestValide()->coutSol.evaluation ;
		if (getIndividuBestValide() != NULL && getIndividuBestValide()->coutSol.evaluation < fit - 0.01)
		{
			cout << "!!! new BKS !!! : " << getIndividuBestValide()->coutSol.evaluation << endl;
			ExportPop(nomFichier,false);
		}
		else if (getIndividuBestValide() != NULL && std::fabs(getIndividuBestValide()->coutSol.evaluation - fit) < 0.01 && tim > (int)(timeBest / 1000000))
		{
			cout << "!!! new time !!! : " << getIndividuBestValide()->coutSol.evaluation << endl;
			
			ExportPop(nomFichier,false);
		}
		
	}
	else
	{
		cout << " BKS file not found " << endl;
		ExportPop(nomFichier,false);
	}
}
// retourne la fraction d'individus valides en terme de charge
double Population::fractionValidesCharge()
{
	// if (params->is_scenario_) {
	// 	return valides->nbIndiv / (double)(valides->nbIndiv + invalides->nbIndiv);
	// }
	
	int count = 0;
	for (list<bool>::iterator it = listeValiditeCharge.begin(); it != listeValiditeCharge.end(); ++it)
		if (*it == true)
			count++;
	return double(count) / (double)(100);
}

// retourne la fraction d'individus valides en terme de temps
double Population::fractionValidesTemps()
{
	int count = 0;
	for (list<bool>::iterator it = listeValiditeTemps.begin(); it != listeValiditeTemps.end(); ++it)
		if (*it == true)
			count++;

	return double(count) / (double)(100);
}

double Population::getDiversity(SousPop *pop)
{
	double total = 0;
	int count = 0;
	for (int i = 0; i < pop->nbIndiv / 2; i++)
		for (int j = i + 1; j < pop->nbIndiv / 2; j++)
		{
			total += pop->individus[i]->distance(pop->individus[j]);
			count++;
		}
	return total / (double)count;
}

double Population::getMoyenneValides()
{
	double moyenne = 0;
	for (int i = 0; i < valides->nbIndiv / 2; i++)
		moyenne += valides->individus[i]->coutSol.evaluation;
	return moyenne / (valides->nbIndiv / 2);
}
double Population::getMoyenneInvalides()
{
	double moyenne = 0;
	for (int i = 0; i < invalides->nbIndiv / 2; i++)
		moyenne += invalides->individus[i]->coutSol.evaluation;
	return moyenne / (invalides->nbIndiv / 2);
}

int Population::selectCompromis(SousPop *souspop)
{
	double pireFitnessEtendu = 0;
	int mauvais = -1;
	vector<int> classement;
	int temp, sortant;

	evalExtFit(souspop);

	// pour chaque individu on modifie le fitness etendu
	for (int i = 0; i < souspop->nbIndiv; i++)
	{
		classement.push_back(i);
		if (souspop->individus[i]->distPlusProche(1) < params->distMin)
			souspop->individus[i]->fitnessEtendu += 5;
		// for the CVRP instances, we need to allow duplicates with the same fitness since in the Golden instances
		// there is a lot of symmetry.
		if (fitExist(souspop, souspop->individus[i]))
			souspop->individus[i]->fitnessEtendu += 5;
	}

	// on classe les elements par fitness etendu et on prend le plus mauvais
	for (int n = 0; n < souspop->nbIndiv; n++)
		for (int i = 1; i < souspop->nbIndiv - n - 1; i++)
			if (souspop->individus[classement[i]]->fitnessEtendu > souspop->individus[classement[i + 1]]->fitnessEtendu)
			{
				temp = classement[i + 1];
				classement[i + 1] = classement[i];
				classement[i] = temp;
			}

	sortant = classement[souspop->nbIndiv - 1];

	if (params->rng->genrand64_real1() < -1)
		cout << souspop->individus[sortant]->fitRank << " "
			 << souspop->individus[sortant]->divRank << " "
			 << souspop->individus[sortant]->fitnessEtendu << endl;

	return sortant;
}

void Population::education(Individu *indiv)
{
	bool trace = false;
	if (trace) cout << "education" << endl;

	// +++++ CRITICAL: Validate trainer and sub_individus before use +++++
	if (!trainer) {
		cerr << "ERROR: trainer is nullptr in education()" << endl;
		return;
	}

	// +++++ CRITICAL: Validate all LocalSearch pointers before education +++++
	for (size_t s = 0; s < trainer->sub_individus_.size(); s++) {
		if (!trainer->sub_individus_[s]) {
			cerr << "ERROR: sub_individus_[" << s << "] is nullptr in education()" << endl;
			return;
		}
		if (!trainer->sub_individus_[s]->localSearch) {
			cerr << "ERROR: sub_individus_[" << s << "]->localSearch is nullptr in education()" << endl;
			cerr << "    This indicates LocalSearch initialization failed for scenario " << s << endl;
			return;
		}
	}

	try {
		if (trace) cout << "recopieIndividu" << endl;
		recopieIndividu(trainer, indiv);

		if (trace) cout << "generalSplit" << endl;
		trainer->generalSplit();

		if (trace) cout << "updateLS" << endl;
		trainer->updateLS();

		if (trace) cout << "RunSearchTotal" << endl;
		trainer->RunSearchTotal(false);

		if (trace) cout << "updateIndiv" << endl;
		trainer->updateIndiv();

		// +++++ First day control:educationafter/backsyncfirst +++++
		if (trainer->control_day_1_ == 1) {
			trainer->SyncDay1ToAllScenarios();
			trainer->CheckSubDay1Consistency("After education");
		}

		recopieIndividu(indiv, trainer);
	} catch (const std::exception& e) {
		cerr << "ERROR: Exception in education(): " << e.what() << endl;
		throw; // Re-throw to prevent silent failures
	}
}


// met a jour le compte des valides
void Population::updateNbValides(Individu *indiv)
{
	listeValiditeCharge.push_back(indiv->coutSol.capacityViol < 0.0001);
	listeValiditeCharge.pop_front();
	listeValiditeTemps.push_back(indiv->coutSol.lengthViol < 0.0001);
	listeValiditeTemps.pop_front();
}

void Population::afficheEtat(int nbIter)
{
	cout.precision(6); // Modify

	cout << "It " << nbIter << " | Sol " << endl;
	// cin.get();
	if (params->is_scenario_) {
		// Print first 10 sub_individus info for multi-scenario
		// cout << "scenario" << endl;
		auto best_valide = getIndividuBestValide();
		auto best_invalide = getIndividuBestInvalide();
		if (best_valide == nullptr) {
			cout << "NO-VALID ";
		} else {
			for (int s = 0; s < std::min(10, (int)best_valide->sub_individus_.size()); s++) {
				if (best_valide->sub_individus_[s] != nullptr) {
					cout << best_valide->sub_individus_[s]->coutSol.evaluation << " ";
				} else {
					cout << "NO-VALID ";
				}
			}
		}
		cout << " | " << endl;
		if (best_invalide == nullptr) {
			cout << "NO-INVALID ";
		} else {
			for (int s = 0; s < std::min(10, (int)best_invalide->sub_individus_.size()); s++) {
				if (best_invalide->sub_individus_[s] != nullptr) {
					cout << best_invalide->sub_individus_[s]->coutSol.evaluation << " ";
				} else {
					cout << "NO-INVALID ";
				}
			}
		}
	} else {
		if (getIndividuBestValide() != NULL)
			cout << getIndividuBestValide()->coutSol.evaluation << " ";
		else
			cout << "NO-VALID ";
		if (getIndividuBestInvalide() != NULL)
			cout << getIndividuBestInvalide()->coutSol.evaluation;
		else
			cout << "NO-INVALID";
	}
	cout << endl;

	cout.precision(8);

	cout << " | Moy " << getMoyenneValides() << " " << getMoyenneInvalides()
		 << " | Div " << getDiversity(valides) << " " << getDiversity(invalides)
		 << " | Val " << fractionValidesCharge() << " " << fractionValidesTemps()
		 << " | Pen " << params->penalityCapa << " " << params->penalityLength << " | Pop " << valides->nbIndiv << " " << invalides->nbIndiv << endl;
}
