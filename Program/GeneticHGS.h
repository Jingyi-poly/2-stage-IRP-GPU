#ifndef GENETIC_HGS_H
#define GENETIC_HGS_H

#include "Individual.h"
#include "LocalSearch.h"
#include "Split.h"
#include <vector>
#include <fstream>
#include <chrono>
#include <functional>

// Generic HGS genetic algorithm.
// The split/evaluation backend is injected via a std::function so that
// the same genetic logic can drive both CPU (OpenMP Split) and GPU (SplitCUDA).
class GeneticHGS
{
public:
	using EvalFunc = std::function<void(std::vector<Individual*>&)>;

	GeneticHGS(Params & params, EvalFunc evaluator, int batchSize = 1, bool gpuMode = false);
	~GeneticHGS();

	void run(std::ostream * logStream = nullptr);
	const Individual * getBestFound() const;

private:
	Params & params;
	EvalFunc evaluator;
	int batchSize;
	bool gpuMode;

	LocalSearch localSearch;
	Split splitLS;

	std::vector<Individual *> population;
	Individual bestSolution;

	void evaluateBatch(std::vector<Individual*>& batch);
	void evaluateIndividual(Individual & indiv);

	// OX Crossover (identical to classic HGS)
	void crossoverOX(Individual & result, const Individual & parent1, const Individual & parent2);

	// Mutation operators
	void mutateSegmentReversal(Individual & indiv);
	void mutateSwap(Individual & indiv);
	void mutateOrOpt(Individual & indiv);
	void mutateToggleVisit(Individual & indiv);
	void mutate(Individual & indiv);

	// Local search on routes (single-scenario mean demand, then GPU re-evaluates)
	void educate(Individual & indiv);

	// Population management
	bool addToPopulation(const Individual & indiv);
	void removeWorstBiasedFitness();
	const Individual & binaryTournament();
	double brokenPairsDistance(const Individual & a, const Individual & b);
};

#endif
