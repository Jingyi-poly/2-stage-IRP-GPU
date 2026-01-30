#include "irp/model/Noeud.h"

Noeud::Noeud(void){}
/*
estUnDepot : Boolean value indicating whether this node is a depot.
cour : ,represent/indicateIndex or number of the node.
jour : ,Indicates the related day or time period.
estPresent : Boolean value indicating whether this node exists on a given day.
suiv and pred : Pointers to NoeudType, representing successor and predecessor nodes in the route.
route : Pointer to RouteType, representing the route associated with this node.
est : Double value representing service start time.
*/
Noeud::Noeud(bool estUnDepot, int cour,int jour, bool estPresent, Noeud * suiv , Noeud * pred, Route * route, double est) 
: estUnDepot(estUnDepot),cour(cour),jour(jour), estPresent(estPresent),suiv(suiv), pred(pred), route(route), est(est)
{
coutInsertion = 1.e30 ; // Initialize insertion cost to a very large value
placeInsertion = NULL ; // Initialize insertion place pointer to NULL
place = -1 ; //NodeinRoutein thePositiondefine
}

bool mySort (Insertion i, Insertion j) 
{ 
	if (i.detour < j.detour) return true ;
	else if (i.detour > j.detour) return false ;
	else return (i.load > j.load) ;
}
//Functionafter/back,allInsertionswillonlycontaindominationInsert,willhaveat/tosuccessorProcessefficiencyandeffect.
void Noeud::removeDominatedInsertions (double penalityCapa)
{
	// First order the elements by increasing detour
	std::sort (allInsertions.begin(), allInsertions.end(), mySort);

	// Then make a new structure that keeps the non-dominated elements.
	vector <Insertion> newVector ;
	newVector.push_back(allInsertions[0]);
	Insertion courInsertion = allInsertions[0];

	for (int i=1 ; i < (int)allInsertions.size() ; i++)
	{
		if (allInsertions[i].load > courInsertion.load + 0.0001 && 
			courInsertion.detour + penalityCapa * (allInsertions[i].load - courInsertion.load) > allInsertions[i].detour + 0.0001 )
		{
			newVector.push_back(allInsertions[i]);
			courInsertion = allInsertions[i];
		}
	}

	// and replace the old structure by the new one
	allInsertions = newVector;
}

Noeud::~Noeud(void){}
