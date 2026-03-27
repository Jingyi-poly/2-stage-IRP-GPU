#include "GeneticHGS.h"
#include <iostream>
#include <algorithm>
#include <numeric>
#include <cmath>

GeneticHGS::GeneticHGS(Params & params, EvalFunc evaluator)
	: params(params), evaluator(std::move(evaluator)), bestSolution(params)
{
	bestSolution.eval.penalizedCost = 1.e30;
}

GeneticHGS::~GeneticHGS()
{
	for (auto * indiv : population)
		delete indiv;
}

void GeneticHGS::evaluateIndividual(Individual & indiv)
{
	evaluator(indiv);
}

// ───────────────────── Crossover ─────────────────────

void GeneticHGS::crossoverOX(Individual & result, const Individual & parent1, const Individual & parent2)
{
	std::vector<bool> freqClient(params.nbClients + 1, false);

	std::uniform_int_distribution<> distr(0, params.nbClients - 1);
	int start = distr(params.ran);
	int end = distr(params.ran);
	while (end == start) end = distr(params.ran);

	int j = start;
	while (j % params.nbClients != (end + 1) % params.nbClients)
	{
		result.chromT[j % params.nbClients] = parent1.chromT[j % params.nbClients];
		freqClient[result.chromT[j % params.nbClients]] = true;
		j++;
	}

	for (int i = 1; i <= params.nbClients; i++)
	{
		int temp = parent2.chromT[(end + i) % params.nbClients];
		if (!freqClient[temp])
		{
			result.chromT[j % params.nbClients] = temp;
			j++;
		}
	}

	if (params.ap.optionalVisit)
	{
		for (int c = 1; c <= params.nbClients; c++)
		{
			if (freqClient[c])
				result.clientVisited[c] = parent1.clientVisited[c];
			else
				result.clientVisited[c] = parent2.clientVisited[c];
		}
	}
}

// ───────────────────── Mutations ─────────────────────

void GeneticHGS::mutateSegmentReversal(Individual & indiv)
{
	int n = params.nbClients;
	std::uniform_int_distribution<> distr(0, n - 1);
	int i = distr(params.ran);
	int j = distr(params.ran);
	if (i > j) std::swap(i, j);
	std::reverse(indiv.chromT.begin() + i, indiv.chromT.begin() + j + 1);
}

void GeneticHGS::mutateSwap(Individual & indiv)
{
	int n = params.nbClients;
	std::uniform_int_distribution<> distr(0, n - 1);
	int i = distr(params.ran);
	int j = distr(params.ran);
	while (j == i) j = distr(params.ran);
	std::swap(indiv.chromT[i], indiv.chromT[j]);
}

void GeneticHGS::mutateOrOpt(Individual & indiv)
{
	int n = params.nbClients;
	if (n < 4) return;

	std::uniform_int_distribution<> posDist(0, n - 1);
	std::uniform_int_distribution<> lenDist(1, std::min(3, n - 1));

	int segStart = posDist(params.ran);
	int segLen = lenDist(params.ran);
	if (segStart + segLen > n) segLen = n - segStart;

	std::vector<int> segment(indiv.chromT.begin() + segStart,
							 indiv.chromT.begin() + segStart + segLen);
	indiv.chromT.erase(indiv.chromT.begin() + segStart,
					   indiv.chromT.begin() + segStart + segLen);

	std::uniform_int_distribution<> insDist(0, (int)indiv.chromT.size());
	int insertPos = insDist(params.ran);
	indiv.chromT.insert(indiv.chromT.begin() + insertPos,
						segment.begin(), segment.end());
}

void GeneticHGS::mutateToggleVisit(Individual & indiv)
{
	std::uniform_int_distribution<> distr(1, params.nbClients);
	int nFlips = 1;
	std::uniform_int_distribution<> flipDist(1, 3);
	nFlips = flipDist(params.ran);
	for (int f = 0; f < nFlips; f++)
	{
		int c = distr(params.ran);
		indiv.clientVisited[c] = !indiv.clientVisited[c];
	}
}

void GeneticHGS::mutate(Individual & indiv)
{
	if (params.ap.optionalVisit)
	{
		std::uniform_int_distribution<> branchDist(0, 9);
		if (branchDist(params.ran) < 3)
		{
			mutateToggleVisit(indiv);
			return;
		}
	}
	std::uniform_int_distribution<> typeDist(0, 2);
	switch (typeDist(params.ran))
	{
		case 0: mutateSegmentReversal(indiv); break;
		case 1: mutateSwap(indiv); break;
		case 2: mutateOrOpt(indiv); break;
	}
}

// ───────────────── Population management ─────────────────

double GeneticHGS::brokenPairsDistance(const Individual & a, const Individual & b)
{
	int differences = 0;
	for (int j = 1; j <= params.nbClients; j++)
	{
		if (a.successors[j] != b.successors[j] && a.successors[j] != b.predecessors[j])
			differences++;
		if (a.predecessors[j] == 0 && b.predecessors[j] != 0 && b.successors[j] != 0)
			differences++;
	}
	if (params.ap.optionalVisit)
	{
		for (int j = 1; j <= params.nbClients; j++)
			if (a.clientVisited[j] != b.clientVisited[j])
				differences++;
	}
	return (double)differences / (double)params.nbClients;
}

const Individual & GeneticHGS::binaryTournament()
{
	std::uniform_int_distribution<> distr(0, (int)population.size() - 1);
	int i = distr(params.ran);
	int j = distr(params.ran);
	while (j == i && population.size() > 1) j = distr(params.ran);

	if (population[i]->eval.penalizedCost < population[j]->eval.penalizedCost)
		return *population[i];
	else
		return *population[j];
}

bool GeneticHGS::addToPopulation(const Individual & indiv)
{
	Individual * newIndiv = new Individual(indiv);

	int place = (int)population.size();
	while (place > 0 && population[place - 1]->eval.penalizedCost > indiv.eval.penalizedCost - MY_EPSILON)
		place--;
	population.insert(population.begin() + place, newIndiv);

	bool isNewBest = false;
	if (indiv.eval.penalizedCost < bestSolution.eval.penalizedCost - MY_EPSILON)
	{
		bestSolution = indiv;
		isNewBest = true;
	}

	if ((int)population.size() > params.ap.mu + params.ap.lambda)
	{
		while ((int)population.size() > params.ap.mu)
			removeWorstBiasedFitness();
	}

	return isNewBest;
}

void GeneticHGS::removeWorstBiasedFitness()
{
	int n = (int)population.size();
	if (n <= 1) return;

	std::vector<double> avgDist(n, 0.0);
	for (int i = 0; i < n; i++)
	{
		std::vector<double> distances;
		distances.reserve(n - 1);
		for (int j = 0; j < n; j++)
			if (i != j)
				distances.push_back(brokenPairsDistance(*population[i], *population[j]));
		std::sort(distances.begin(), distances.end());
		int nbClose = std::min(params.ap.nbClose, (int)distances.size());
		double sum = 0.0;
		for (int k = 0; k < nbClose; k++) sum += distances[k];
		avgDist[i] = (nbClose > 0) ? sum / nbClose : 0.0;
	}

	std::vector<std::pair<double, int>> divRanking;
	divRanking.reserve(n);
	for (int i = 0; i < n; i++)
		divRanking.push_back({-avgDist[i], i});
	std::sort(divRanking.begin(), divRanking.end());
	std::vector<int> divRank(n);
	for (int i = 0; i < n; i++)
		divRank[divRanking[i].second] = i;

	double worstFitness = -1.e30;
	int worstIdx = -1;
	bool worstIsClone = false;

	for (int i = 1; i < n; i++)
	{
		double costR = (double)i / (double)(n - 1);
		double divR = (double)divRank[i] / (double)(n - 1);
		double bf;
		if (n <= params.ap.nbElite)
			bf = costR;
		else
			bf = costR + (1.0 - (double)params.ap.nbElite / (double)n) * divR;

		bool isClone = (avgDist[i] < MY_EPSILON);
		if ((isClone && !worstIsClone) || (isClone == worstIsClone && bf > worstFitness))
		{
			worstFitness = bf;
			worstIdx = i;
			worstIsClone = isClone;
		}
	}

	if (worstIdx >= 0)
	{
		delete population[worstIdx];
		population.erase(population.begin() + worstIdx);
	}
}

// ───────────────────── Main loop ─────────────────────

const Individual * GeneticHGS::getBestFound() const
{
	if (bestSolution.eval.penalizedCost < 1.e29) return &bestSolution;
	return nullptr;
}

void GeneticHGS::run(std::ostream * logStream)
{
	auto tStart = std::chrono::steady_clock::now();
	auto elapsed = [&]() -> double {
		return std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - tStart).count() / 1000.0;
	};

	// --- Initial population ---
	if (params.verbose) std::cout << "----- BUILDING INITIAL POPULATION" << std::endl;
	int initSize = 4 * params.ap.mu;
	for (int i = 0; i < initSize; i++)
	{
		Individual randomIndiv(params);
		if (params.ap.optionalVisit && i >= initSize * 4 / 5)
		{
			std::uniform_real_distribution<double> skipProb(0.1, 0.3);
			double pSkip = skipProb(params.ran);
			std::uniform_real_distribution<double> coin(0.0, 1.0);
			for (int c = 1; c <= params.nbClients; c++)
				if (coin(params.ran) < pSkip)
					randomIndiv.clientVisited[c] = false;
		}
		evaluateIndividual(randomIndiv);
		addToPopulation(randomIndiv);

		if (params.verbose && (i + 1) % std::max(1, initSize / 5) == 0)
			std::cout << "  Init " << i + 1 << "/" << initSize
					  << "  best=" << bestSolution.eval.penalizedCost
					  << "  T=" << elapsed() << "s" << std::endl;
	}
	if (params.verbose)
		std::cout << "----- INITIAL POPULATION COMPLETE  best=" << bestSolution.eval.penalizedCost
				  << "  pop=" << population.size() << std::endl;

	// --- Main genetic loop ---
	auto tMainStart = std::chrono::steady_clock::now();
	int nbIterNonProd = 0;
	Individual offspring(params);

	for (int iter = 1; ; iter++)
	{
		double t = elapsed();
		if (params.ap.timeLim > 0 && t >= params.ap.timeLim)
		{
			if (params.verbose) std::cout << "Time limit reached at iter " << iter << std::endl;
			break;
		}
		if (params.ap.iterLim > 0 && iter > params.ap.iterLim)
		{
			if (params.verbose) std::cout << "Iteration limit reached." << std::endl;
			break;
		}

		if (nbIterNonProd >= params.ap.nbIter)
		{
			if (params.ap.timeLim > 0)
			{
				if (params.verbose)
					std::cout << "----- RESTART at iter " << iter << "  T=" << t << "s" << std::endl;
				for (auto * p : population) delete p;
				population.clear();
				for (int i = 0; i < initSize; i++)
				{
					Individual ri(params);
					evaluateIndividual(ri);
					addToPopulation(ri);
				}
				nbIterNonProd = 0;
				continue;
			}
			else
			{
				if (params.verbose) std::cout << "Max non-improving iterations reached." << std::endl;
				break;
			}
		}

		crossoverOX(offspring, binaryTournament(), binaryTournament());
		mutate(offspring);
		evaluateIndividual(offspring);

		bool improved = addToPopulation(offspring);
		if (improved)
			nbIterNonProd = 0;
		else
			nbIterNonProd++;

		if (logStream)
		{
			*logStream << elapsed() << " " << iter << " " << bestSolution.eval.penalizedCost << "\n";
			logStream->flush();
		}

		if (params.verbose && iter % params.ap.freqPrint == 0)
		{
			std::cout << "Iter " << iter
					  << " | NoProd " << nbIterNonProd
					  << " | Pop " << population.size()
					  << " | Best " << bestSolution.eval.penalizedCost
					  << " | Dist " << bestSolution.eval.distance / params.n_scenarios
					  << " | CapEx " << bestSolution.eval.capacityExcess / params.n_scenarios
					  << " | Feas " << bestSolution.eval.isFeasible
					  << " | T " << elapsed() << "s"
					  << std::endl;
		}
	}

	double mainLoopTime = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - tMainStart).count() / 1000.0;

	if (params.verbose)
	{
		std::cout << "----- GENETIC ALGORITHM FINISHED" << std::endl;
		std::cout << "  Best penalizedCost: " << bestSolution.eval.penalizedCost << std::endl;
		std::cout << "  Avg distance:       " << bestSolution.eval.distance / params.n_scenarios << std::endl;
		std::cout << "  Avg capExcess:      " << bestSolution.eval.capacityExcess / params.n_scenarios << std::endl;
		std::cout << "  isFeasible:         " << bestSolution.eval.isFeasible << std::endl;
		std::cout << "  Total time:         " << elapsed() << "s" << std::endl;
		std::cout << "  Main loop time:     " << mainLoopTime << "s" << std::endl;
	}
}
