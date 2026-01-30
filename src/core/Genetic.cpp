#include "irp/core/Genetic.h"
#include <algorithm> 
#include <random> // New
void Genetic::evolve(int maxIter, int maxIterNonProd, int nbRec)
{
	Individu *parent1;
	Individu *parent2;
	int place;
	double rangRelatif;
	nbIterNonProd = 0; // Initialize to 0
	nbIter = 0;
	int resultCross;
	int measure = 0;
	string temp;
	traces = false; // Disable tracing for OU Policy
	if (!traces) {
		cout << "Solver is running..." << endl;
	}

	// DEBUG: OutputTimecorrelationInfo
	cout << "=== Time Control Debug Info ===" << endl;
	auto now = std::chrono::steady_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - params->debut);
	cout << "Time limit (ticks) = " << ticks.count() << " seconds" << endl;
	cout << "Elapsed at start = " << elapsed.count() << " seconds" << endl;
	cout << "Using wall clock time (not CPU time)" << endl;
	cout << "================================" << endl;

	
	// Stop:MaximumIteration OR Iteration OR RunTime
	while (nbIter < maxIter && nbIterNonProd < maxIterNonProd &&
	    (std::chrono::steady_clock::now() - params->debut <= ticks))
	{
		//cout <<"nbIter: "<<nbIter<<endl;
		// on demande deux individus a la population wein populationtwoindividuals
		population->recopieIndividu(rejeton, population->getIndividuBinT(rangRelatif));
		population->recopieIndividu(rejeton2, population->getIndividuBinT(rangRelatif));
		if (true) {
			// cout << "copy individu" << nbIter << endl;
			// rejeton->printChromT();
			// cin.get();
			// rejeton2->printChromT();
			// cin.get();
		}
		if (traces) cout << "copy individu" << nbIter << endl;
		//cout <<"before: "<<endl;
		// on choisit le crossover en fonction du probleme webased onproblemSelectiondivide/part
		if (!params->isInventoryRouting)
			resultCross = crossOX();
		else
			resultCross = crossPOX_OU_2(); // OU Policy
		if (traces) cout << "cross" << nbIter << endl;
		muter();
		if (traces) cout << "muter" << nbIter << endl;
		// REPAIR IF NEEDED
		if (!rejeton->estValide)
		{
			//cout <<"repairs: "<<nbIter<<endl;
			place = population->addIndividu(rejeton);
			reparer();
		}
		if (traces) cout << "reparer" << nbIter << endl;
		//cout <<"out"<<endl;
		// ADD IN THE POPULATION IF IS FEASIBLE
		if (rejeton->estValide)
			place = population->addIndividu(rejeton);
		//cout <<"d: "<<nbIter<<endl;
		// Reset counter when new optimal solution found
		// canaccuratetracenoImprovementIteration
		if (place == 0 && rejeton->estValide)
			nbIterNonProd = 0; // Reset counter
		else
			nbIterNonProd++;
		if (traces) cout << "add individu" << nbIter << endl;
		//out <<"p: "<<nbIter<<endl;
		//cout <<"rejrton "<<rejeton->estValide<<" place "<<place<<endl;
		if (place == 0 && rejeton->estValide)
		{
			if (traces) {
				cout << "NEW BEST FEASIBLE ";
				cout << rejeton->coutSol.evaluation;	
				cout << " Cost : " << rejeton->coutSol.fitness
					<< " capacity Violation : " << rejeton->coutSol.capacityViol
					<< " length Violation : " << rejeton->coutSol.lengthViol;
				cout << endl;
				cout << endl;
			}
			// rejeton->measureGivenSol();
		}
		//cout <<"k: "<<nbIter<<endl;
		if (nbRec > 0 && nbIterNonProd % (maxIterNonProd / 5 + 1) == maxIterNonProd / 5)
		{
			cout << "Diversification" << endl;
			population->diversify();//
		}
	
		// MANAGEMENT OF PARAMETERS
		if (nbIter % 10 == 0) {
			gererPenalites();
			if (traces) cout << "gererPenalites" << nbIter << endl;
		}
			
		// TRACES
		if (nbIter % 1 == 0) {
			population->afficheEtat(nbIter);
			if (traces) cout << "afficheEtat" << nbIter << endl;
		}
		
		nbIter++;
		//cout <<"see"<<endl;
		//cout <<nbIter << maxIter << nbIterNonProd << maxIterNonProd << (clock() - params->debut) << ticks<<endl;
	}

	// DEBUG: OutputStoporiginalbecause
	auto end_time = std::chrono::steady_clock::now();
	auto total_elapsed = std::chrono::duration_cast<std::chrono::seconds>(end_time - params->debut);
	cout << "\n=== Evolution Stopped ===" << endl;
	cout << "Total iterations: " << nbIter << endl;
	cout << "Non-productive iterations: " << nbIterNonProd << endl;
	cout << "Elapsed time: " << total_elapsed.count() << " seconds" << endl;
	cout << "Time limit: " << ticks.count() << " seconds" << endl;
	cout << "Stop reason: ";
	if (nbIter >= maxIter) cout << "Max iterations reached (" << maxIter << ")" << endl;
	else if (nbIterNonProd >= maxIterNonProd) cout << "Early stopping (" << maxIterNonProd << " iterations without improvement)" << endl;
	else if (total_elapsed > ticks) cout << "Time limit exceeded" << endl;
	else cout << "Unknown" << endl;
	cout << "========================" << endl;

	//int a;cin>>a;
	// fin de l'algorithme , diverses informations affichees
	if (traces)
	{
		cout << "time passes : " << clock() << endl;
		cout << "number of iterations : " << nbIter << endl;
	}
}

// effectue la mutation
void Genetic::muter()
{
	rejeton->updateLS();
	//cout <<"run"<<endl;
	rejeton->RunSearchTotal(false);

	rejeton->updateIndiv();

	// +++++ First day control:Mutationafter/backsyncfirst +++++
	if (rejeton->control_day_1_ == 1) {
		rejeton->SyncDay1ToAllScenarios();
		rejeton->CheckSubDay1Consistency("After muter");
	}

	population->updateNbValides(rejeton);
}

// eventuellement effectue une reparation de la solutionhave,Repairsolve
void Genetic::reparer()
{
	double temp, temp2;
	bool continuer = false;

	temp = params->penalityCapa;
	temp2 = params->penalityLength;

	/*First tentativetryRepair(firsttry):

first,CapacityandLengthdivide/part,make it becomefororiginalto10times.
ifa/anrandomless thana certainThreshold params->pRep(possiblyisperformRepairtryProbability),thentryUpdateandRepairsolve.
RunLocalSearch,trytoa/an,possiblyissolve.
UpdaterejetontoLocalSearchResult.*/
	params->penalityCapa *= 10;
	params->penalityLength *= 10;
	if (params->rng->genrand64_real1() < params->pRep)
	{
		rejeton->UpdatePenalty();
		rejeton->updateLS();
		rejeton->RunSearchTotal(true);
		rejeton->updateIndiv();

		// +++++ First day control:firstRepairafter/backsyncfirst +++++
		if (rejeton->control_day_1_ == 1) {
			rejeton->SyncDay1ToAllScenarios();
			rejeton->CheckSubDay1Consistency("After reparer");
		}

		/* Second tentativetryRepair(secondtry):
ifrejetonstillnot,thenperformsecondRepairtry.
,CapacityandLengthdivide/part500times,willadd/plussolvenotaspects.
againRunLocalSearchandUpdatesolve.
Restoreoriginaldivide/partValue:*/
		if (!rejeton->estValide)
		{
			//cout <<"second"<<endl;
			params->penalityCapa *= 500;
			params->penalityLength *= 500;
			rejeton->UpdatePenalty();
			rejeton->generalSplit();
			//cout <<"general";
			rejeton->updateLS();
			//cout <<"localse";
			rejeton->RunSearchTotal(true);
			rejeton->updateIndiv();

			// +++++ First day control:secondRepairafter/backsyncfirst +++++
			if (rejeton->control_day_1_ == 1) {
				rejeton->SyncDay1ToAllScenarios();
				rejeton->CheckSubDay1Consistency("After reparer 2");
			}
		}
	}
	params->penalityCapa = temp;
	params->penalityLength = temp2;
}

// gestion des penalites
void Genetic::gererPenalites()
{
	double fractionCharge = population->fractionValidesCharge();
	double fractionTemps = population->fractionValidesTemps();
	// cout << "fractionCharge: " << fractionCharge << endl;
	// cout << "fractionTemps: " << fractionTemps << endl;
	// cin.get();
	if (fractionCharge < params->minValides && params->penalityCapa < 1000)
		params->penalityCapa = params->penalityCapa * 1.2;
	else if (fractionCharge > params->maxValides && params->penalityCapa > 0.01)
		params->penalityCapa = params->penalityCapa * 0.85;

	if (fractionTemps < params->minValides && params->penalityLength < 1000)
		params->penalityLength = params->penalityLength * 1.2;
	else if (fractionTemps > params->maxValides && params->penalityLength > 0.01)
		params->penalityLength = params->penalityLength * 0.85;

	population->validatePen();
}
/*
is`Genetic`ClassConstructordefine,maindidbelowseveral things:
1. **ParameterInitialize**:throughConstructorInitializeList,willpassed inParameterValuerespectivelyClassMemberVariable.
for example,passed in`params`ParameterValueClassMemberVariable`params`.mainisforInitializeMemberVariable.

2. **LocalVariableInitialize**:InitializeseveralTemporaryVariable`tempVect`(a/anintegerTypevector)
and`chromTRand`(a/anintegervectorvector)and`chromPRand`(a/an`pattern`Typevector)and`tempList`(a/anintegerTypeList).

3. **`pattern`ObjectInitialize**:Createa/anNew`pattern`Object`p`,andwill itMember`dep`and`pat`Initializefor0.

4. **Initialize`chromTRand`**:Use`tempVect`for`chromTRand`add`params->nbDays + 1`Emptyintegervector.

*/
Genetic::Genetic(Params *params, Population *population, std::chrono::seconds ticks, bool traces, bool writeProgress) : params(params), population(population), ticks(ticks), traces(traces), writeProgress(writeProgress)
{
	
	vector<int> tempVect;
	vector<vector<int>> chromTRand;
	vector<pattern> chromPRand;
	list<int> tempList;
	pattern p;
	p.dep = 0;
	p.pat = 0;

	//4. **Initialize`chromTRand`**:Use`tempVect`for`chromTRand`add`params->nbDays + 1`Emptyintegervector.
	for (int i = 0; i <= params->nbDays; i++)
		chromTRand.push_back(tempVect); 

	// Initialize freqClient and chromPRand vectors
	// Add frequency to freqClient and pattern to chromPRand

	for (int i = 0; i < params->nbClients + params->nbDepots; i++)
	{
		freqClient.push_back(params->cli[i].freq);
		chromPRand.push_back(p);
	}
	

// **Initialize`rejeton`and`rejeton2`**:for`rejeton`and`rejeton2`AssignmentNew`Individu`Object.

/// Configure localSearch for rejeton

	rejeton = new Individu(params, 1.0);
	rejeton2 = new Individu(params, 1.0);
	delete rejeton->localSearch;
	rejeton->localSearch = new LocalSearch(params, rejeton);
}

Genetic::Genetic(ParamsList *params_list, Population *population, std::chrono::seconds ticks, bool traces, bool writeProgress) : params(params_list->general_params_), population(population), ticks(ticks), traces(traces), writeProgress(writeProgress)
{
	vector<int> tempVect;
	vector<vector<int>> chromTRand;
	vector<pattern> chromPRand;
	list<int> tempList;
	pattern p;
	p.dep = 0;
	p.pat = 0;

	//4. **Initialize`chromTRand`**:Use`tempVect`for`chromTRand`add`params->nbDays + 1`Emptyintegervector.
	for (int i = 0; i <= params->nbDays; i++)
		chromTRand.push_back(tempVect); 

	// Initialize freqClient and chromPRand vectors
	// Add frequency to freqClient and pattern to chromPRand

	for (int i = 0; i < params->nbClients + params->nbDepots; i++)
	{
		freqClient.push_back(params->cli[i].freq);
		chromPRand.push_back(p);
	}
	

// **Initialize`rejeton`and`rejeton2`**:for`rejeton`and`rejeton2`AssignmentNew`Individu`Object.

/// Configure localSearch for rejeton

	rejeton = new Individu(params_list);
	rejeton2 = new Individu(params_list);
	BatchRunFunc([&](int s) {
		delete rejeton->sub_individus_[s]->localSearch;
		rejeton->sub_individus_[s]->localSearch = nullptr; // set tonullptravoid double free
		rejeton->sub_individus_[s]->localSearch = new LocalSearch(params_list->vec_params_[s], rejeton->sub_individus_[s]);
	}, rejeton->sub_individus_.size());
}

int Genetic::crossOX()
{
	// on tire au hasard un debut et une fin pour la zone de crossover
	//forCrossoverdomain/fieldrandomand
	int debut = params->rng->genrand64_int64() % params->nbClients;
	int fin = params->rng->genrand64_int64() % params->nbClients;
	while (fin == debut)
		fin = params->rng->genrand64_int64() % params->nbClients;

	// on initialise le tableau de frequence signifiant si l'individu a ete place ou non
	for (int i = params->nbDepots; i < params->nbClients + params->nbDepots; i++)
		freqClient[i] = 1;

	int j = debut;
	// on garde les elements de debut a fin
	while ((j % params->nbClients) != ((fin + 1) % params->nbClients))
	{
		freqClient[rejeton->chromT[1][j % params->nbClients]] = 0;
		j++;
	}

	int temp;
	// on remplit les autres elements non deja places dans l'ordre du
	// deuxieme chromosome
	for (int i = 1; i <= params->nbClients; i++)
	{
		temp = rejeton2->chromT[1][(fin + i) % params->nbClients];
		if (freqClient[temp] == 1)
		{
			rejeton->chromT[1][j % params->nbClients] = temp;
			j++;
		}
	}

	// on calcule le fitness
	rejeton->generalSplit();
	return 0;
}

int Genetic::crossPOX2()
{
	vector<int> vide, garder, joursPerturb, tableauFin, tableauEtat;
	vector<double> charge;
	int debut, fin, day;
	int j1, j2;
	double quantity;

	// Keeping track of the chromL of the parent
	vector<vector<double>> chromLParent1 = rejeton->chromL;

	// Reinitializing the chromL of the rejeton (will become the child)
	// Keeping for each day and each customer the total sum of delivered load and initial inventory
	// (when inserting a customer, need to make sure that we are not exceeding this)
	for (int k = 1; k <= params->nbDays; k++)
		for (int i = params->nbDepots; i < params->nbDepots + params->nbClients; i++)
			rejeton->chromL[k][i] = 0.;

	// Keeping a vector to remember if a delivery has alrady been inserted for on day k for customer i
	vector<vector<bool>> hasBeenInserted = vector<vector<bool>>(params->nbDays + 1, vector<bool>(params->nbClients + params->nbDepots, false));

	// choosing the type of inheritance for each day (nothing, all, or mixed) based onrandomSequencedeterminewhichwill becompletelyinheritance,whichwillinheritancepartially,andwhichnotwillinheritanceanycontent.
	for (int k = 1; k <= params->nbDays; k++)
		joursPerturb.push_back(k);

	std::mt19937 rng1(params->seed);
	std::shuffle(joursPerturb.begin(), joursPerturb.end(), rng1);

	// Picking j1 et j2
	j1 = params->rng->genrand64_int64() % params->nbDays;
	j2 = params->rng->genrand64_int64() % params->nbDays;
	if (j1 > j2)
	{
		int temp = j2;
		j2 = j1;
		j1 = temp;
	}

	// Inheriting the data from rejeton1.
	// For each day, we will keep a sequence going from debut to fin
	for (int k = 0; k < params->nbDays; k++)
	{
		day = joursPerturb[k];
		// Copy a segment
		if (k < j1 && !rejeton->chromT[day].empty())
		{
			debut = (int)(params->rng->genrand64_int64() % rejeton->chromT[day].size()); // Random segment start
			fin = (int)(params->rng->genrand64_int64() % rejeton->chromT[day].size());
			tableauFin.push_back(fin);
			int j = debut;
			garder.clear();
			while (j != ((fin + 1) % rejeton->chromT[day].size()))
			{
				int ii = rejeton->chromT[day][j]; // getting the index to be inherited
				garder.push_back(ii);
				rejeton->chromL[day][ii] = chromLParent1[day][ii];
				//rejeton->chromL[day][ii] = std::min<double>(rejeton->maxFeasibleDeliveryQuantity(day, ii), chromLParent1[day][ii]);
				hasBeenInserted[day][ii] = true;
				j = (j + 1) % rejeton->chromT[day].size();
			}
			rejeton->chromT[day].clear();
			for (int g = 0; g < (int)garder.size(); g++)
				rejeton->chromT[day].push_back(garder[g]);
		}
		else if (k < j2) // on recopie rien
		{
			rejeton->chromT[day].clear();
			tableauFin.push_back(-1);
		}
		else // on recopie tout
		{
			tableauFin.push_back(0);
			for (int j = 0; j < (int)rejeton->chromT[day].size(); j++)
			{
				int ii = rejeton->chromT[day][j]; // getting the index to be inherited
				garder.push_back(ii);
				rejeton->chromL[day][ii] = chromLParent1[day][ii];
				//rejeton->chromL[day][ii] = std::min<double>(rejeton->maxFeasibleDeliveryQuantity(day, ii), 
														//				chromLParent1[day][ii]);
				hasBeenInserted[day][ii] = true;
				j = (j + 1) % rejeton->chromT[day].size();
			}
		}
	}
	
	// completing with rejeton 2
	for (int k = 0; k < params->nbDays; k++)
	{
		day = joursPerturb[k];
		fin = tableauFin[k];
		if (k < j2)
		{
			for (int i = 0; i < (int)rejeton2->chromT[day].size(); i++)
			{
				int ii = rejeton2->chromT[day][(i + fin + 1) % (int)rejeton2->chromT[day].size()];
				if (!hasBeenInserted[day][ii]) // it has not been inserted yet
				{
					// computing maximum possible delivery quantity
					quantity = rejeton2->chromL[day][ii];
					if (quantity > 0.0001)
					{
						rejeton->chromT[day].push_back(ii);
						rejeton->chromL[day][ii] = quantity;
						hasBeenInserted[day][ii] = true;
					}
				}
			}
		}
	}
	
	vector<vector<double>> I_end(params->nbDays+2, vector<double>(params->nbDepots + params->nbClients));
	//vector<double> sum(params->nbDepots + params->nbClients);
	//for(int i = params->nbDepots; i < params->nbDepots + params->nbClients; i++) sum[i] = params->cli[i].startingInventory;

	for (int i = params->nbDepots; i < params->nbDepots + params->nbClients; i++){
		I_end[0][i] = params->cli[i].startingInventory;
		//if(i == 157)cout<<"day0 cus = "<<i<<" "<<I_end[0][i]<<endl;	
	}

	for (int k = 1; k <= params->nbDays; k++){
		for (int cus = params->nbDepots; cus < params->nbDepots + params->nbClients; cus++){
			//if(cus == 157)cout<<"cus = "<<cus<<endl;
			//if(sum[cus]+rejeton->chromL[k][cus] >params->cli[cus].maxInventory) rejeton->chromL[k][cus]=params->cli[cus].maxInventory-sum[cus];

			rejeton->chromL[k][cus] = std::min<double>(rejeton->chromL[k][cus],params->cli[cus].maxInventory-I_end[k-1][cus]);
			
			//if(cus == 157)cout<<"yesterday "<<I_end[k-1][cus]<<" today "<<rejeton->chromL[k][cus]<<" maxIn "<<params->cli[cus].maxInventory<<endl;

			I_end[k][cus] = std::max<double>(0,I_end[k-1][cus] + rejeton->chromL[k][cus] - params->cli[cus].dailyDemand[k]);
			
			//if(cus == 157) cout<<"Iend "<<I_end[k][cus]<<endl;
		}	
	}

	
	/*
	for (int k = 1; k <= params->nbDays; k++){
		cout <<"rejeton->chromT["<<k<<"]: (";
		for (int i = 0; i < (int)rejeton->chromT[k].size(); i++){
				cout<<rejeton->chromT[k][i]<<" ";
		}
		cout<<")"<<endl;
	}
		*/
	return 0;
}

// ==================OU Policy=====================//
int Genetic::crossPOX_OU() {
  vector<int> garder, joursPerturb;
  int debut, fin, day;

  // randomSelectionNumber of daysSequence
  for (int k = 1; k <= params->nbDays; k++) joursPerturb.push_back(k);
  std::mt19937 rng4(params->seed);
  std::shuffle(joursPerturb.begin(), joursPerturb.end(), rng4);

 // randomSelectioncompletelyinheritanceandpartiallyinheritanceTime
  int j1 = params->rng->genrand64_int64() % params->nbDays;
  int j2 = params->rng->genrand64_int64() % params->nbDays;
  if (j1 > j2) std::swap(j1, j2);

  // Initializebooleanvectorchild/subGeneration
  vector<vector<bool>> childDelivery(params->nbDays + 1, vector<bool>(params->nbClients + params->nbDepots, false));

 // based on parent1 and parent2 booleanvector
  vector<vector<bool>> parent1Bool(params->nbDays + 1, vector<bool>(params->nbClients + params->nbDepots, false));
  vector<vector<bool>> parent2Bool(params->nbDays + 1, vector<bool>(params->nbClients + params->nbDepots, false));

  // Initializebooleanvector
  for (int k = 1; k <= params->nbDays; k++) {
    for (int cus = params->nbDepots; cus < params->nbDepots + params->nbClients; cus++) {
      parent1Bool[k][cus] = rejeton->chromL[k][cus] > 0;
      parent2Bool[k][cus] = rejeton2->chromL[k][cus] > 0;
    }
  }

 // booleanvectorCrossover
  for (int k = 0; k < params->nbDays; k++) {
    day = joursPerturb[k];
    if (k < j1) { // inheritance parent1
      for (int cus = params->nbDepots; cus < params->nbDepots + params->nbClients; cus++) {
        childDelivery[day][cus] = parent1Bool[day][cus];
      }
    } else if (k < j2) { // notinheritanceany
      for (int cus = params->nbDepots; cus < params->nbDepots + params->nbClients; cus++) {
        childDelivery[day][cus] = false;
      }
    } else { // inheritance parent2
      for (int cus = params->nbDepots; cus < params->nbDepots + params->nbClients; cus++) {
        childDelivery[day][cus] = parent2Bool[day][cus];
      }
    }
  }

 // InventoryandDelivery
  vector<vector<double>> I_end(params->nbDays + 1, vector<double>(params->nbClients + params->nbDepots, 0));
  for (int i = params->nbDepots; i < params->nbDepots + params->nbClients; i++) {
    I_end[0][i] = params->cli[i].startingInventory;
  }

  for (int k = 1; k <= params->nbDays; k++) {
    for (int cus = params->nbDepots; cus < params->nbDepots + params->nbClients; cus++) {
 if (childDelivery[k][cus]) { // booleanvectorfor true,FullInventory
        rejeton->chromL[k][cus] = params->cli[cus].maxInventory - I_end[k - 1][cus];
        I_end[k][cus] = params->cli[cus].maxInventory - params->cli[cus].dailyDemand[k];
 } else { // booleanvectorfor false,only consumeInventory
        rejeton->chromL[k][cus] = 0;
        I_end[k][cus] = std::max(0.0, I_end[k - 1][cus] - params->cli[cus].dailyDemand[k]);
      }
    }
  }

 // child/subGenerationCustomerVisitSequence chromT
  for (int k = 1; k <= params->nbDays; k++) {
    rejeton->chromT[k].clear();
    for (int cus = params->nbDepots; cus < params->nbDepots + params->nbClients; cus++) {
      if (childDelivery[k][cus]) rejeton->chromT[k].push_back(cus);
    }
  }

  return 0;
}
// ==================OU Policy=====================//

// ==================OU Policy=====================//
int Genetic::crossPOX_OU_2() {
	if (params->is_scenario_) {
		if (rejeton->sub_individus_.size() != rejeton2->sub_individus_.size()) {
			throw std::runtime_error("crossPOX_OU_2: rejeton and rejeton2 have different number of sub-individus");
		}
		// cout << "cross day 1" << endl;
		// crossPOX_OU_day_1(rejeton, rejeton2);
		// cout << "cross day 1 done" << endl;
		BatchRunFunc([&](int s) {
			crossPOX_OU_ptr(rejeton->sub_individus_[s], rejeton2->sub_individus_[s], rejeton->general_individu_);
		}, rejeton->sub_individus_.size());
		// cout << "cross sub done" << endl;
		rejeton->generalSplit();
		// cout << "general split done" << endl;

		// +++++ First day control:Crossoverafter/backsyncfirst +++++
		if (rejeton->control_day_1_ == 1) {
			rejeton->SyncDay1ToAllScenarios();
			rejeton->CheckSubDay1Consistency("After crossPOX_OU_2");
		}
		// cout << "check sub day 1 done" << endl;
		return 0;
	}
  vector<int> vide, garder, tableauFin, tableauEtat;
  vector<double> charge;
  int debut, fin, day;
  int j1, j2;
  double quantity;

	// Keeping track of the chromL of the parent
	vector<vector<double>> chromLParent1 = rejeton->chromL;

	// Reinitializing the chromL of the rejeton (will become the child)
	// Keeping for each day and each customer the total sum of delivered load and initial inventory
	// (when inserting a customer, need to make sure that we are not exceeding this)
	for (int k = 1; k <= params->nbDays; k++)
		for (int i = params->nbDepots; i < params->nbDepots + params->nbClients; i++)
			rejeton->chromL[k][i] = 0.;

	// Keeping a vector to remember if a delivery has alrady been inserted for on day k for customer i
	vector<vector<bool>> hasBeenInserted = vector<vector<bool>>(params->nbDays + 1, vector<bool>(params->nbClients + params->nbDepots, false));

	// choosing the type of inheritance for each day (nothing, all, or mixed) based onrandomSequencedeterminewhichwill becompletelyinheritance,whichwillinheritancepartially,andwhichnotwillinheritanceanycontent.
	vector<int> joursPerturb = vector<int>(params->nbDays, 0);
	for (int k = 1; k <= params->nbDays; k++)
		joursPerturb[k-1] = k;

	std::mt19937 rng3(params->seed);
	std::shuffle(joursPerturb.begin(), joursPerturb.end(), rng3);

	// Picking j1 et j2
	j1 = params->rng->genrand64_int64() % params->nbDays;
	j2 = params->rng->genrand64_int64() % params->nbDays;
	if (j1 > j2) std::swap(j1, j2);

	// Inheriting the data from rejeton1.
	// For each day, we will keep a sequence going from debut to fin
	for (int k = 0; k < params->nbDays; k++)
	{
		day = joursPerturb[k];
		// Copy a segment
		if (k < j1 && !rejeton->chromT[day].empty())
		{
			debut = (int)(params->rng->genrand64_int64() % rejeton->chromT[day].size()); // Random segment start
			fin = (int)(params->rng->genrand64_int64() % rejeton->chromT[day].size());
			tableauFin.push_back(fin);
			int j = debut;
			garder.clear();
			// fromdebuttofinCopydata
			while (j != ((fin + 1) % rejeton->chromT[day].size()))
			{
				int ii = rejeton->chromT[day][j]; // getting the index to be inherited
				if(chromLParent1[day][ii] < 0.1) continue;
				garder.push_back(ii);
				rejeton->chromL[day][ii] = 1; //CopyGeneration,ifGenerationfor0,thenchild/subGenerationalsofor0,elsefor1
				hasBeenInserted[day][ii] = true;
				j = (j + 1) % rejeton->chromT[day].size();
			}
			rejeton->chromT[day].clear();
			for (int g = 0; g < (int)garder.size(); g++)
				rejeton->chromT[day].push_back(garder[g]);
		}
		else if (k < j2) // on recopie rien
		{
			rejeton->chromT[day].clear();
			tableauFin.push_back(-1);
		}
		else // on recopie tout
		{
			tableauFin.push_back(0);
			for (int j = 0; j < (int)rejeton->chromT[day].size(); j++)
			{
				int ii = rejeton->chromT[day][j]; // getting the index to be inherited
				if(chromLParent1[day][ii] < 0.1) continue;
				garder.push_back(ii);
				rejeton->chromL[day][ii] = 1;
				hasBeenInserted[day][ii] = true;
				// j = (j + 1) % rejeton->chromT[day].size();
			}
		}
	}
	
	// completing with rejeton 2
	for (int k = 0; k < params->nbDays; k++)
	{
		day = joursPerturb[k];
		fin = tableauFin[k];
		if (k < j1)
		{
			for (int i = 0; i < (int)rejeton2->chromT[day].size(); i++)
			{
				// int ii = rejeton2->chromT[day][(i + fin + 1) % (int)rejeton2->chromT[day].size()];
				int ii = rejeton2->chromT[day][(i + fin + 1) % (int)rejeton2->chromT[day].size()];
				if (!hasBeenInserted[day][ii]) // it has not been inserted yet
				{
					if (rejeton2->chromL[day][ii] > 0.1)
					{
						rejeton->chromT[day].push_back(ii);
						rejeton->chromL[day][ii] = 1.;
						hasBeenInserted[day][ii] = true;
					}
					
				}
			}
		}
	}
	
	vector<vector<double>> I_end(params->nbDays+2, vector<double>(params->nbDepots + params->nbClients));
	//vector<double> sum(params->nbDepots + params->nbClients);
	//for(int i = params->nbDepots; i < params->nbDepots + params->nbClients; i++) sum[i] = params->cli[i].startingInventory;

	for (int i = params->nbDepots; i < params->nbDepots + params->nbClients; i++){
		I_end[0][i] = params->cli[i].startingInventory;
		//if(i == 157)cout<<"day0 cus = "<<i<<" "<<I_end[0][i]<<endl;	
	}

	for (int k = 1; k <= params->nbDays; k++){
		for (int cus = params->nbDepots; cus < params->nbDepots + params->nbClients; cus++){
			if (rejeton->chromL[k][cus] == 1) {
				rejeton->chromL[k][cus] = params->cli[cus].maxInventory - I_end[k-1][cus];
				I_end[k][cus] = std::max<double>(0., params->cli[cus].maxInventory - params->cli[cus].dailyDemand[k]);
			} else {
				rejeton->chromL[k][cus] = 0.;
				I_end[k][cus] = std::max<double>(0., I_end[k-1][cus] - params->cli[cus].dailyDemand[k]);
			}
		}	
	}

	rejeton->generalSplit();

	return 0;
}

// Only for first day crossover
void Genetic::crossPOX_OU_day_1(Individu *child1, Individu *child2) {
	// cout << "=== crossPOX_OU_day_1 START ===" << endl;
	if (!child1->is_scenario_ || !child2->is_scenario_) {
		throw std::runtime_error("crossPOX_OU_day_1 is only supported for scenario");
	}
	// Randomly select from sub_individu based on seed
	int index1 = params->rng->genrand64_int64() % child1->sub_individus_.size();
	int index2 = params->rng->genrand64_int64() % child2->sub_individus_.size();
	auto sub_child_1 = child1->sub_individus_[index1];
	auto sub_child_2 = child2->sub_individus_[index2];
	cout << "sub_child_1->chromT[1].size(): " << sub_child_1->chromT[1].size() << endl;
	crossPOX_OU_ptr(sub_child_1, sub_child_2, nullptr);
	sub_child_1->generalSplit();
	cout << "crossPOX_OU_ptr done" << endl;
	// willin thesub-individualdatasynctogeneral_individu_
	if (child1->general_individu_ != nullptr) {
		child1->general_individu_->chromT = sub_child_1->chromT;
		child1->general_individu_->chromL = sub_child_1->chromL;
		cout << "child1->general_individu_->chromT[1].size(): " << child1->general_individu_->chromT[1].size() << endl;
	} else {
		cout << "WARNING: child1->general_individu_ is nullptr!" << endl;
	}
}

void Genetic::crossPOX_OU_ptr(Individu *child1, Individu *child2, Individu *general_individu = nullptr)
{
	if (child1->is_scenario_ || child2->is_scenario_) {
		throw std::runtime_error("crossPOX_OU_ptr is not supported for scenario");
	}
	
	vector<int> vide, garder, tableauFin, tableauEtat;
  vector<double> charge;
  int debut, fin, day;
  int j1, j2;
  double quantity;

	// Keeping track of the chromL of the parent
	vector<vector<double>> chromLParent1 = child1->chromL;

	// Reinitializing the chromL of the child1 (will become the child)
	// Keeping for each day and each customer the total sum of delivered load and initial inventory
	// (when inserting a customer, need to make sure that we are not exceeding this)
	for (int k = 1; k <= params->nbDays; k++)
		for (int i = params->nbDepots; i < params->nbDepots + params->nbClients; i++)
			child1->chromL[k][i] = 0.;

	// Keeping a vector to remember if a delivery has alrady been inserted for on day k for customer i
	vector<vector<bool>> hasBeenInserted = vector<vector<bool>>(params->nbDays + 1, vector<bool>(params->nbClients + params->nbDepots, false));

	// choosing the type of inheritance for each day (nothing, all, or mixed) based onrandomSequencedeterminewhichwill becompletelyinheritance,whichwillinheritancepartially,andwhichnotwillinheritanceanycontent.
	vector<int> joursPerturb = vector<int>(params->nbDays, 0);
	for (int k = 1; k <= params->nbDays; k++)
		joursPerturb[k-1] = k;

	std::mt19937 rng4(params->seed);
	std::shuffle(joursPerturb.begin(), joursPerturb.end(), rng4);

	// Picking j1 et j2
	j1 = params->rng->genrand64_int64() % params->nbDays;
	j2 = params->rng->genrand64_int64() % params->nbDays;
	if (j1 > j2) std::swap(j1, j2);

	// Inheriting the data from child1.
	// For each day, we will keep a sequence going from debut to fin
	for (int k = 0; k < params->nbDays; k++)
	{
		day = joursPerturb[k];
		// Copy a segment
		if (k < j1 && !child1->chromT[day].empty())
		{
			debut = (int)(params->rng->genrand64_int64() % child1->chromT[day].size()); // Random segment start
			fin = (int)(params->rng->genrand64_int64() % child1->chromT[day].size());
			tableauFin.push_back(fin);
			int j = debut;
			garder.clear();
			// fromdebuttofinCopydata
			while (j != ((fin + 1) % child1->chromT[day].size()))
			{
				int ii = child1->chromT[day][j]; // getting the index to be inherited
				if(chromLParent1[day][ii] < 0.1) continue;
				garder.push_back(ii);
				child1->chromL[day][ii] = 1; //CopyGeneration,ifGenerationfor0,thenchild/subGenerationalsofor0,elsefor1
				hasBeenInserted[day][ii] = true;
				j = (j + 1) % child1->chromT[day].size();
			}
			child1->chromT[day].clear();
			for (int g = 0; g < (int)garder.size(); g++)
				child1->chromT[day].push_back(garder[g]);
		}
		else if (k < j2) // on recopie rien
		{
			child1->chromT[day].clear();
			tableauFin.push_back(-1);
		}
		else // on recopie tout
		{
			tableauFin.push_back(0);
			for (int j = 0; j < (int)child1->chromT[day].size(); j++)
			{
				int ii = child1->chromT[day][j]; // getting the index to be inherited
				if(chromLParent1[day][ii] < 0.1) continue;
				garder.push_back(ii);
				child1->chromL[day][ii] = 1;
				hasBeenInserted[day][ii] = true;
				// j = (j + 1) % child1->chromT[day].size();
			}
		}
	}
	
	// completing with child2
	for (int k = 0; k < params->nbDays; k++)
	{
		day = joursPerturb[k];
		fin = tableauFin[k];
		if (k < j1)
		{
			for (int i = 0; i < (int)child2->chromT[day].size(); i++)
			{
				// int ii = child2->chromT[day][(i + fin + 1) % (int)child2->chromT[day].size()];
				int ii = child2->chromT[day][(i + fin + 1) % (int)child2->chromT[day].size()];
				if (!hasBeenInserted[day][ii]) // it has not been inserted yet
				{
					if (child2->chromL[day][ii] > 0.1)
					{
						child1->chromT[day].push_back(ii);
						child1->chromL[day][ii] = 1.;
						hasBeenInserted[day][ii] = true;
					}
					
				}
			}
		}
	}

	// if (general_individu != nullptr) {
	// 	// deep copyfirstdata
	// 	child1->chromT[1].clear();
	// 	child1->chromL[1].clear();
		
	// 	// deep copy chromT[1]
	// 	for (int i = 0; i < general_individu->chromT[1].size(); i++) {
	// 		child1->chromT[1].push_back(general_individu->chromT[1][i]);
	// 	}
		
	// 	// deep copy chromL[1]
	// 	for (int i = 0; i < general_individu->chromL[1].size(); i++) {
	// 		child1->chromL[1][i] = general_individu->chromL[1][i];
	// 	}

	// 	// firstdatasyncforgeneral_individu_
	// 	// cout << "alreadysyncgeneral_individu_tochild1" << endl;
	// 	// for (int i = 0; i < child1->chromT[1].size(); i++) {
	// 	// 	cout << child1->chromT[1][i] << " ";
	// 	// }
	// 	// cout << endl;
	// 	// for (int i = 0; i < general_individu->chromT[1].size(); i++) {
	// 	// 	cout << general_individu->chromT[1][i] << " ";
	// 	// }
	// 	// cout << endl;
	// }
	vector<vector<double>> I_end(params->nbDays+2, vector<double>(params->nbDepots + params->nbClients));
	//vector<double> sum(params->nbDepots + params->nbClients);
	//for(int i = params->nbDepots; i < params->nbDepots + params->nbClients; i++) sum[i] = params->cli[i].startingInventory;

	for (int i = params->nbDepots; i < params->nbDepots + params->nbClients; i++){
		I_end[0][i] = params->cli[i].startingInventory;
		//if(i == 157)cout<<"day0 cus = "<<i<<" "<<I_end[0][i]<<endl;	
	}

	for (int k = 1; k <= params->nbDays; k++){
		for (int cus = params->nbDepots; cus < params->nbDepots + params->nbClients; cus++){
			if (child1->chromL[k][cus] == 1) {
				child1->chromL[k][cus] = params->cli[cus].maxInventory - I_end[k-1][cus];
				I_end[k][cus] = std::max<double>(0., params->cli[cus].maxInventory - params->cli[cus].dailyDemand[k]);
			} else {
				child1->chromL[k][cus] = 0.;
				I_end[k][cus] = std::max<double>(0., I_end[k-1][cus] - params->cli[cus].dailyDemand[k]);
			}
		}	
	}

	child1->generalSplit();

	return;
}
// ==================OU Policy=====================//

// A depot fixs, amliorer une solution, dcomposition du problme par dpot
void Genetic::improve_decompRoutes(int maxIter, int maxIterNonProd, Individu *indiv, int grainSize, int decal, Population *pop, int nbRec)
{
	double place;
	bool addBestIndivFeatures = false;
	bool randomOrder = false;
	if (params->rng->genrand64_real1() < 1.0)
	{
		addBestIndivFeatures = true;
		cout << "taking features from the best individual" << endl;
	}
	if (params->rng->genrand64_real1() < 0.3)
	{
		randomOrder = true;
		cout << "using random order" << endl;
	}

	vector<int> orderVehicles;
	for (int i = 0; i < params->nbVehiculesPerDep; i++)
		orderVehicles.push_back(i);
	int temp, temp2;
	// on introduit du desordre
	for (int a1 = 0; a1 < (int)orderVehicles.size() - 1; a1++)
	{
		temp2 = a1 + params->rng->genrand64_int64() % ((int)orderVehicles.size() - a1);
		temp = orderVehicles[a1];
		orderVehicles[a1] = orderVehicles[temp2];
		orderVehicles[temp2] = temp;
	}

	double init = indiv->coutSol.evaluation;
	if (std::chrono::steady_clock::now() - params->debut > ticks)
		return;
	Params *sousProbleme;
	Population *sousPopulation;
	Genetic *sousSolver;
	Individu *initialIndiv;
	Individu *bestIndiv;
	Individu *bestIndiv2;
	Individu *bestIndiv3;
	int k1, myk;
	bool found;
	vector<int> route;
	vector<vector<int>> routes;
	vector<vector<int>> routes2;
	vector<vector<int>> routes3;
	for (int k = 0; k < params->nbVehiculesPerDep; k++)
	{
		routes.push_back(route);
		routes2.push_back(route);
		routes3.push_back(route);
	}
	SousPop *subpop;
	SousPop *subsubPop;
	if (pop->valides->nbIndiv != 0)
		subpop = pop->valides;
	else
		subpop = pop->invalides;

	vector<Vehicle *> serieVehiclesTemp;
	vector<int> serieNumeroRoutes;
	vector<int> serieVisitesTemp;
	Vehicle **serieVehicles;
	int *serieVisites;
	int fin;

	// normalement les routes sont deja organisees dans l'ordre des centroides, puisque le probleme de based etait un VRP
	// on va donc les accumuler jusque depasser "grainSize"
	// puis resoudre le probleme
	// et enfin remettre les choses a la bonne place.
	for (int k = 0; k <= params->nbVehiculesPerDep; k++)
	{
		if ((int)serieVisitesTemp.size() > grainSize || k == params->nbVehiculesPerDep)
		{
			// on va traiter le sous-probleme associe aux routes premierVeh...kk-1
			serieVehicles = new Vehicle *[(int)serieVehiclesTemp.size()];
			for (int ii = 0; ii < (int)serieVehiclesTemp.size(); ii++)
				serieVehicles[ii] = serieVehiclesTemp[ii];
			serieVisites = new int[(int)serieVisitesTemp.size()];
			for (int ii = 0; ii < (int)serieVisitesTemp.size(); ii++)
				serieVisites[ii] = serieVisitesTemp[ii];

			if ((int)serieVisitesTemp.size() > 0)
			{
				sousProbleme = new Params(params, 0, serieVisites, serieVehicles, NULL, NULL, 0, 0, (int)serieVisitesTemp.size(), (int)serieVehiclesTemp.size());
				sousPopulation = new Population(sousProbleme);

				// se servir des caracteristiques des individus originaux pour creer la population
				initialIndiv = new Individu(sousProbleme, 1.0);
				if (addBestIndivFeatures)
					for (int kl = 0; kl < subpop->nbIndiv; kl++)
					{
						initialIndiv->chromT[1].clear();
						for (int ii = 0; ii < (int)subpop->individus[kl]->chromT[1].size(); ii++)
							if (sousProbleme->correspondanceTable2[subpop->individus[kl]->chromT[1][ii]] != -1)
								initialIndiv->chromT[1].push_back(sousProbleme->correspondanceTable2[subpop->individus[kl]->chromT[1][ii]]);

						initialIndiv->generalSplit();
						sousPopulation->addIndividu(initialIndiv);
					}

				// on cree le solver (we don't solve for a problem with 1 customer only).
				sousSolver = new Genetic(sousProbleme, sousPopulation, std::chrono::seconds(10000000), true, true);
				if ((int)serieVisitesTemp.size() > 1)
					sousSolver->evolve(maxIter, maxIterNonProd, nbRec - 1);

				// on recupere les trois meilleurs differents
				if (sousPopulation->getIndividuBestValide() != NULL)
					subsubPop = sousPopulation->valides;
				else
					subsubPop = sousPopulation->invalides;

				bestIndiv = subsubPop->individus[0];
				k1 = 1;
				found = false;
				while (k1 < subsubPop->nbIndiv && !found)
				{
					if (subsubPop->individus[k1]->coutSol.evaluation >= bestIndiv->coutSol.evaluation + 0.1)
					{
						bestIndiv2 = subsubPop->individus[k1];
						found = true;
					}
					k1++;
				}
				if (!found)
					bestIndiv2 = subsubPop->individus[0];
				found = false;
				while (k1 < subsubPop->nbIndiv && !found)
				{
					if (subsubPop->individus[k1]->coutSol.evaluation >= bestIndiv2->coutSol.evaluation + 0.1)
					{
						bestIndiv3 = subsubPop->individus[k1];
						found = true;
					}
					k1++;
				}
				if (!found)
					bestIndiv3 = subsubPop->individus[0];

				// on transfere leurs caracteristiques
				for (int ii = 0; ii < (int)bestIndiv->chromR[1].size(); ii++)
				{
					if (ii == (int)bestIndiv->chromR[1].size() - 1)
						fin = (int)bestIndiv->chromT[1].size();
					else
						fin = bestIndiv->chromR[1][ii + 1];
					for (int j = bestIndiv->chromR[1][ii]; j < fin; j++)
						routes[serieNumeroRoutes[ii]].push_back(sousProbleme->correspondanceTable[bestIndiv->chromT[1][j]]);
				}
				for (int ii = 0; ii < (int)bestIndiv2->chromR[1].size(); ii++)
				{
					if (ii == (int)bestIndiv2->chromR[1].size() - 1)
						fin = (int)bestIndiv2->chromT[1].size();
					else
						fin = bestIndiv2->chromR[1][ii + 1];
					for (int j = bestIndiv2->chromR[1][ii]; j < fin; j++)
						routes2[serieNumeroRoutes[ii]].push_back(sousProbleme->correspondanceTable[bestIndiv2->chromT[1][j]]);
				}
				for (int ii = 0; ii < (int)bestIndiv3->chromR[1].size(); ii++)
				{
					if (ii == (int)bestIndiv3->chromR[1].size() - 1)
						fin = (int)bestIndiv3->chromT[1].size();
					else
						fin = bestIndiv3->chromR[1][ii + 1];
					for (int j = bestIndiv3->chromR[1][ii]; j < fin; j++)
						routes3[serieNumeroRoutes[ii]].push_back(sousProbleme->correspondanceTable[bestIndiv3->chromT[1][j]]);
				}

				delete initialIndiv;
				delete sousProbleme;
				delete sousPopulation;
				delete sousSolver;
			}

			delete[] serieVehicles;
			delete[] serieVisites;
			serieVehiclesTemp.clear();
			serieVisitesTemp.clear();
			serieNumeroRoutes.clear();
		}

		// si ce n'est pas la derniere iteration, on accumule les vehicules et clients de la route consideree
		// pour definir le prochain sous-probleme
		if (k != params->nbVehiculesPerDep)
		{
			if (!randomOrder)
				myk = (k + decal) % params->nbVehiculesPerDep;
			else
				(myk = orderVehicles[k]);

			serieVehiclesTemp.push_back(&params->ordreVehicules[1][myk]);
			serieNumeroRoutes.push_back(myk);
			if (myk == params->nbVehiculesPerDep - 1)
				fin = indiv->chromT[1].size();
			else
				fin = indiv->chromR[1][myk + 1];
			for (int ii = indiv->chromR[1][myk]; ii < fin; ii++)
				serieVisitesTemp.push_back(indiv->chromT[1][ii]);
		}
	}

	indiv->chromT[1].clear();
	for (int k = 0; k < params->nbVehiculesPerDep; k++)
		for (int kk = 0; kk < (int)routes[k].size(); kk++)
			indiv->chromT[1].push_back(routes[k][kk]);
	indiv->generalSplit();
	place = population->addIndividu(indiv);
	cout << "INTENSIFICATION PAR DECOMPOSITION GEOMETRIQUE: " << init << " --> " << indiv->coutSol.evaluation << endl;
	if (indiv->estValide && place == 0)
		nbIterNonProd = 1;
}

Genetic::~Genetic(void)
{
	delete rejeton;
	delete rejeton2;
}
