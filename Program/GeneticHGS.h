#ifndef GENETIC_HGS_H
#define GENETIC_HGS_H

#include "Individual.h"
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
	using EvalFunc = std::function<void(Individual &)>;

	GeneticHGS(Params & params, EvalFunc evaluator);
	~GeneticHGS();

	void run(std::ostream * logStream = nullptr);
	const Individual * getBestFound() const;

private:
	Params & params;
	EvalFunc evaluator;

	std::vector<Individual *> population;
	Individual bestSolution;

	void evaluateIndividual(Individual & indiv);

	// OX Crossover (identical to classic HGS)
	void crossoverOX(Individual & result, const Individual & parent1, const Individual & parent2);

	// Mutation operators (compensate for skipping LocalSearch)
	void mutateSegmentReversal(Individual & indiv);
	void mutateSwap(Individual & indiv);
	void mutateOrOpt(Individual & indiv);
	void mutateToggleVisit(Individual & indiv);
	void mutate(Individual & indiv);

	// Population management
	bool addToPopulation(const Individual & indiv);
	void removeWorstBiasedFitness();
	const Individual & binaryTournament();
	double brokenPairsDistance(const Individual & a, const Individual & b);
};

#endif
