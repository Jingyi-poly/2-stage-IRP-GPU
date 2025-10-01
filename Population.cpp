#include "Population.h"
#include <fstream>
#include <string>
#include <cmath>
#include <sstream>
// constructeur
Population::Population(Params *params) : params(params)
{
	Individu *randomIndiv;
	trainer = new Individu(params, 1.0);  //初始化一个实例，包括所有split要用到的数组。
	delete trainer->localSearch;   		 //
	trainer->localSearch = new LocalSearch(params, trainer); //为本地搜索过程初始化所需的数据结构，并为后续的操作做好准备。

	double temp = params->penalityCapa;
	double temp2 = params->penalityLength;
	//SousPop结构为基于种群的优化方法提供了一个简单的子种群表示，其中包含了子种群中的个体、子种群的大小和子种群经历的世代数量。
	valides = new SousPop(); 
	invalides = new SousPop();
	valides->nbIndiv = 0;
	invalides->nbIndiv = 0;
	valides->nbGenerations = 0;
	invalides->nbGenerations = 0;
	bool compter = true;

	// TODO -- for testing for now  生成2*mu的个体
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
		//chromT是一个二维向量，它的外部维度是天数（nbDays），内部维度是在每一天要访问的客户的顺序。这个客户顺序最初是基于“Just In Time”策略确定的，然后被随机化。
		
		education(randomIndiv);
		if (compter)
			updateNbValides(randomIndiv);
		addIndividu(randomIndiv);
		delete randomIndiv;
	}

	// on initialise par d�faut � 100, comme si tout �tait valide au d�but
	// mais c'est arbitraire
	for (int i = 0; i < 100; i++) 
    // 默认初始化列表，假设100个体在开始时都是有效的
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
	trainer = new Individu(params_list_);  //初始化一个实例，这里使用params_list。TODO 新个体实例
	// 本质上是先确定第一天的配送方案 后面所有场景各走各的最优解 因此需要的localsearch数量和params_list一致
	BatchRunFunc([&](int s) {
		delete trainer->sub_individus_[s]->localSearch;
		trainer->sub_individus_[s]->localSearch = nullptr; // 设置为nullptr避免重复释放
		trainer->sub_individus_[s]->localSearch = new LocalSearch(params_list_->vec_params_[s], trainer->sub_individus_[s]);
	}, trainer->sub_individus_.size());

	// population不需要感知多场景

	// 由于在生成子代的过程中 需要对penality进行调整 因此需要保存原始的penality
	std::pair<double, double> original_penality = std::make_pair(params->penalityCapa, params->penalityLength);
	//SousPop结构为基于种群的优化方法提供了一个简单的子种群表示，其中包含了子种群中的个体、子种群的大小和子种群经历的世代数量。
	valides = new SousPop(); 
	invalides = new SousPop();
	valides->nbIndiv = 0;
	invalides->nbIndiv = 0;
	valides->nbGenerations = 0;
	invalides->nbGenerations = 0;
	bool compter = true;

	// TODO -- for testing for now  生成2*mu的个体
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
		//chromT是一个二维向量，它的外部维度是天数（nbDays），内部维度是在每一天要访问的客户的顺序。这个客户顺序最初是基于“Just In Time”策略确定的，然后被随机化。
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

	// on initialise par d�faut � 100, comme si tout �tait valide au d�but
	// mais c'est arbitraire
	for (int i = 0; i < 100; i++) 
    // 默认初始化列表，假设100个体在开始时都是有效的
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
//此函数的目的是更新给定子种群中所有个体与新个体的邻近度或相似度。
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
//此函数的目的是更新给定子种群中所有个体与新个体的邻近度或相似度。
int Population::addIndividu(Individu *indiv)
{
	SousPop *souspop;
	int k, result;

	if (indiv->estValide)
		souspop = valides;
	else
		souspop = invalides;

	result = placeIndividu(souspop, indiv);
	// il faut �ventuellement enlever la moiti� de la pop 我们需要最终移除一半的 pop。
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

// met � jour les individus les plus proches d'une population
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

// fonction booleenne verifiant si le fitness n'existe pas d�ja

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
	/*移除低适应度的解决方案:

对于有效的解决方案，如果它们的数量超过了预定的阈值(params->rho * params->mu)，则从列表的尾部删除它们。这里，尾部适应度较低的解决方案。
对于无效的解决方案，执行相同的操作*/
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
		// 多场景初始化
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
	// 保险起见 没问题可以删掉试试
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

	// on place notre individu � la fin
	for (int i = p + 1; i < (int)pop->individus.size(); i++)
		pop->individus[i - 1] = pop->individus[i];

	// on l'enleve de la population
	pop->individus.pop_back();
	pop->nbIndiv--;

	// on l'enleve des structures de proximit�
	for (int i = 0; i < pop->nbIndiv; i++)
		pop->individus[i]->removeProche(partant);

	// et on supprime le partant
	delete partant;
}
// recalcule l'evaluation des individus a partir des violation
// puis effectue un tri � bulles de la population
// operateur de tri peu efficace mais fonction appell�e tr�s rarement
void Population::validatePen()
{
	Individu *indiv;
	// on met � jour les evaluations
	for (int i = 0; i < valides->nbIndiv; i++) {
		valides->individus[i]->UpdatePenalty();
	}
	for (int i = 0; i < invalides->nbIndiv; i++)
	{
		invalides->individus[i]->UpdatePenalty(); // 更新penalty，只在此之前会永久性调整penalty（in genetic）
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
// ATTENTION !!! ne recopie que le chromP, chromT et les attributs du fitness只能复制 chromP、chromT 和健康属性
void Population::recopieIndividu(Individu *destination, Individu *source)
{
	if (source->is_scenario_)
	{
		destination->is_scenario_ = source->is_scenario_;
		// destination->params = source->params; // TODO 这里需要修改?
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
	
	// exporte les solutions actuelles des individus dans un dossier exports current individual solutions to a folder 将当前的解决方案导出到文件夹中
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
		// le trainer a gard� les infos des routes de bestValide
		params->penalityCapa = temp;
		params->penalityLength = temp2;
		// We will update the local search structure for paths.
		// We are obliged to set very strong parameters so that the splitting does not produce a from the best valid solution
		// so that the splitting does not produce a from the best valid solution
		for (int s = 0; s < trainer->sub_individus_.size(); s++) {
			auto sub_indiv = trainer->sub_individus_[s];
			if (sub_indiv == nullptr) {
				throw std::runtime_error("sub_indiv is nullptr");
			}
			
			// 获取当前场景的 localSearch
			auto local_search = sub_indiv->localSearch;
			if (local_search == nullptr) {
				cout << "Error: local_search is nullptr for scenario " << s << endl;
				continue;
			}
			
			myfile.precision(10);
			cout.precision(10);
			ofstream myfile;
			if (add) myfile.open(nomFichier.data(), std::ios::app);//add on previous
			else myfile.open(nomFichier.data()); // 

			if (s == 0) {
				myfile << "Total Cost: " << trainer->coutSol.evaluation << endl;
			}
				
			cout << "writing the best solution: fitness " << sub_indiv->coutSol.evaluation << " in : " << nomFichier.c_str() << endl;
			
			myfile<<endl<<endl;
			local_search->printInventoryLevels(myfile,false);
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
			myfile <<"PITime: ";sprintf(myBuff, "%d", (int)(params->debut / 1000000));
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

			// // 打印 每一个client的 daily demand
			// myfile << "Daily Demand: " << endl;
			// // 打印表头
			// myfile << "Client\\Day";
			// for (int k = 1; k <= params->nbDays; k++) {
			// 	myfile << "\t" << k;
			// }
			// myfile << endl;
			
			// // 按客户为行，天数为列打印
			// for (int i = params->nbDepots; i < params->nbClients + params->nbDepots; i++) {
			// 	myfile << "Client " << i << ":";
			// 	for (int k = 1; k <= params->nbDays; k++) {
			// 		// 检查索引是否有效
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
		//cout <<"  fot ===   "<<fit<<endl;
		//cout<<"pri > tim"<<pri <<" "<< tim<<endl;
		//cout <<"   this "<<getIndividuBestValide()->coutSol.evaluation ;
		if (getIndividuBestValide() != NULL && getIndividuBestValide()->coutSol.evaluation < fit - 0.01)
		{
			cout << "!!! new BKS !!! : " << getIndividuBestValide()->coutSol.evaluation << endl;
			ExportPop(nomFichier,false);
		}
		else if (getIndividuBestValide() != NULL && std::fabs(getIndividuBestValide()->coutSol.evaluation - fit) < 0.01 &&  tim > (int)(timeBest / 1000000))
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
	recopieIndividu(trainer, indiv);
	if (trace) cout << "recopieIndividu" << endl;
	trainer->generalSplit();
	if (trace) cout << "generalSplit" << endl;
	trainer->updateLS();
	if (trace) cout << "updateLS" << endl;
	trainer->RunSearchTotal(false);
	if (trace) cout << "RunSearchTotal" << endl;
	trainer->updateIndiv();
	if (trace) cout << "updateIndiv" << endl;
	// trainer->CheckSubDay1Consistency("After education");
	recopieIndividu(indiv, trainer);
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
	cout.precision(6); // 修改精度

	cout << "It " << nbIter << " | Sol " << endl;
	// cin.get();
	if (params->is_scenario_) {
		// 如果是多场景 最多打印前10个sub_individus_的信息
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
