# Update

改变 OUPolicy.cpp 的 getInsertionInfo 函数。具体来说：
1. 在计算cost时，既考虑mutation开始时nodetravail计算的detour，也考虑对应load可能产生的capacity-penalty，把两者加在一起。
```totalCost = detour + capaPenalty. (在所有insertions中找到totalCost最小的insertion)```
2. 修改之后，gurobi的部分scenario，最优解的成本减小。solution更灵活的使用了stockout的特点，用少量的缺货减小了总成本。
3. abs的scenario成本变化不大，大场景距离lower bound还有一定距离。
