# Update
Modified _getInsertionInfo_ in **OUPolicy.cpp**:
1. When computing cost, considered both detour computed by nodetravail when mutation starts, and the capacity-penalty induced by the corresponding load.
2. After the update, the objective value of Gurobi version is reduced. Solution is more flexible to use stockout, which tradeoff small amount of shortage for lower cost.
3. The cost for the ABS-version is not changed, and the cost of large scenarios still has gap from the lower bound.
