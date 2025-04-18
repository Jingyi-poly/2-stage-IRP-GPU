#ifndef MATRIXSOLVER_H
#define MATRIXSOLVER_H

#include <vector>
#include <map>
#include "Params.h"
#include "Noeud.h"
#include "LinearPiece.h"

struct Cost {
    double fromC;
    double fromL;
    double fromF;
    int pointer;
};

struct InsertionRes {
    double cost;
    double quantity;
    Noeud* place;
};

struct Solution {
    double totalCost;
    std::vector<bool> plans;
    std::vector<double> quantities;
    std::vector<Noeud*> places;
};

class MatrixSolver {
private:
    Params* params;
    Client client;
    int clientId;
    vector<Noeud*> noeudTravails;

    int i_max;
    int T;
    vector<double> d;
    vector<Cost> C_prev;
    vector<vector<InsertionRes>> insertionInfo;

public:
    MatrixSolver(Params* params, int clientId, vector<Noeud*> noeudTravails);

    InsertionRes getInsertionInfo(Client client, int day, vector<Noeud*> noeudTravails, double quantity);

    void preComputeAllInsertionInfo();
    
    Solution solve();
};

#endif