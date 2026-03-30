"""
Prize-Collecting TSP (PCTSP) 基础求解器 - 无加速技巧版本

此版本使用标准的 MIP 建模方式：
- 预先添加所有可能的子回路消除约束（针对小规模子集）
- 不使用 Lazy Constraints 回调
- 用于与优化版本进行性能对比

注意：由于完整的 SEC 约束数量是指数级的，这里只添加小规模子集的约束。
"""

import time
import argparse
from typing import List, Tuple, Dict, Optional
from itertools import combinations

import gurobipy as gp
from gurobipy import GRB

from data import PCTSPData


class PCTSPBasicSolver:
    """
    基础版 PCTSP 求解器（无加速技巧）

    特点：
    - 预先添加小规模子回路消除约束
    - 不使用回调函数
    - 用于性能对比基准
    """

    def __init__(self, data: PCTSPData, time_limit: float = 3600.0,
                 verbose: bool = True, max_sec_size: int = 5):
        """
        初始化基础求解器

        Args:
            data: PCTSP 数据实例
            time_limit: 求解时间限制（秒）
            verbose: 是否输出求解日志
            max_sec_size: 预添加 SEC 约束的最大子集大小
        """
        self.data = data
        self.time_limit = time_limit
        self.verbose = verbose
        self.max_sec_size = max_sec_size

        self.model: Optional[gp.Model] = None
        self.x: Dict[Tuple[int, int], gp.Var] = {}
        self.y: Dict[int, gp.Var] = {}

        self.solve_time: float = 0.0
        self.obj_value: Optional[float] = None
        self.selected_nodes: List[int] = []
        self.tour: List[int] = []
        self.total_prize: int = 0
        self.total_distance: float = 0.0
        self.sec_count: int = 0  # 添加的 SEC 约束数量

    def build_model(self) -> None:
        """构建基础 Gurobi 模型（预添加 SEC 约束）"""
        n = self.data.n_nodes
        nodes = self.data.get_nodes()

        self.model = gp.Model("PCTSP_Basic")

        self.model.Params.TimeLimit = self.time_limit
        if not self.verbose:
            self.model.Params.OutputFlag = 0

        # 创建边变量 x[i,j]，i < j
        for i in range(n):
            for j in range(i + 1, n):
                self.x[i, j] = self.model.addVar(
                    vtype=GRB.BINARY,
                    name=f"x_{i}_{j}",
                    obj=-self.data.get_distance(i, j)
                )

        # 创建节点选择变量 y[i]
        for i in nodes:
            self.y[i] = self.model.addVar(
                vtype=GRB.BINARY,
                name=f"y_{i}",
                obj=self.data.get_prize(i)
            )

        self.model.ModelSense = GRB.MAXIMIZE

        # 约束：depot 必须访问
        self.model.addConstr(self.y[0] == 1, name="depot")

        # 度约束
        for i in nodes:
            edges = []
            for j in nodes:
                if i != j:
                    edge = (min(i, j), max(i, j))
                    edges.append(self.x[edge])
            self.model.addConstr(
                gp.quicksum(edges) == 2 * self.y[i],
                name=f"degree_{i}"
            )

        # 预添加小规模子回路消除约束 (SEC)
        self._add_sec_constraints()

        self.model.update()

    def _add_sec_constraints(self) -> None:
        """预先添加小规模子集的子回路消除约束"""
        n = self.data.n_nodes
        customers = list(range(1, n))  # 不包含 depot

        self.sec_count = 0

        # 对于大小为 2 到 max_sec_size 的所有不含 depot 的子集
        for size in range(2, min(self.max_sec_size + 1, n)):
            for subset in combinations(customers, size):
                # SEC: sum of edges in subset <= |subset| - 1
                edges_in_subset = []
                for i in range(len(subset)):
                    for j in range(i + 1, len(subset)):
                        node_i, node_j = subset[i], subset[j]
                        edge = (min(node_i, node_j), max(node_i, node_j))
                        edges_in_subset.append(self.x[edge])

                if edges_in_subset:
                    self.model.addConstr(
                        gp.quicksum(edges_in_subset) <= len(subset) - 1,
                        name=f"sec_{self.sec_count}"
                    )
                    self.sec_count += 1

    def solve(self) -> bool:
        """求解模型"""
        if self.model is None:
            self.build_model()

        start_time = time.time()
        self.model.optimize()
        self.solve_time = time.time() - start_time

        if self.model.Status == GRB.OPTIMAL:
            self._extract_solution()
            return True
        elif self.model.Status == GRB.TIME_LIMIT and self.model.SolCount > 0:
            self._extract_solution()
            return True
        elif self.model.SolCount > 0:
            self._extract_solution()
            return True
        return False


    def _extract_solution(self) -> None:
        """提取求解结果"""
        self.obj_value = self.model.ObjVal

        # 获取选中的节点
        self.selected_nodes = [
            i for i, var in self.y.items() if var.X > 0.5
        ]
        self.selected_nodes.sort()

        # 计算总奖励
        self.total_prize = sum(
            self.data.get_prize(i) for i in self.selected_nodes
        )

        # 构建路径
        self._build_tour()

        # 计算总距离
        self.total_distance = 0.0
        for k in range(len(self.tour) - 1):
            self.total_distance += self.data.get_distance(
                self.tour[k], self.tour[k + 1]
            )

    def _build_tour(self) -> None:
        """从解中构建路径"""
        if not self.selected_nodes:
            self.tour = []
            return

        # 构建邻接表
        adj: Dict[int, List[int]] = {i: [] for i in self.selected_nodes}
        for (i, j), var in self.x.items():
            if var.X > 0.5:
                adj[i].append(j)
                adj[j].append(i)

        # 从 depot 开始遍历
        self.tour = [0]
        visited = {0}
        current = 0

        max_iterations = len(self.selected_nodes) * 2  # 防止死循环
        iterations = 0

        while len(visited) < len(self.selected_nodes) and iterations < max_iterations:
            iterations += 1
            found_next = False
            for neighbor in adj.get(current, []):
                if neighbor not in visited:
                    self.tour.append(neighbor)
                    visited.add(neighbor)
                    current = neighbor
                    found_next = True
                    break

            # 如果当前节点没有未访问的邻居，说明存在子回路问题
            if not found_next:
                # 尝试找到任何未访问的节点继续
                for node in self.selected_nodes:
                    if node not in visited:
                        self.tour.append(node)
                        visited.add(node)
                        current = node
                        break
                else:
                    break  # 所有节点都已访问

        # 回到 depot
        self.tour.append(0)

    def print_solution(self) -> None:
        """打印求解结果"""
        print("=" * 60)
        print("PCTSP BASIC SOLUTION SUMMARY")
        print("=" * 60)
        print(f"Instance: {self.data.filename}")
        print(f"Number of nodes: {self.data.n_nodes}")
        print("-" * 60)
        print("MODEL STATISTICS:")
        print(f"  Pre-added SEC constraints: {self.sec_count}")
        print(f"  Max SEC subset size: {self.max_sec_size}")
        print("-" * 60)
        print("SOLUTION STATISTICS:")
        print(f"  Objective value: {self.obj_value:.2f}")
        print(f"  Total prize collected: {self.total_prize}")
        print(f"  Total distance traveled: {self.total_distance:.2f}")
        print(f"  Nodes visited: {len(self.selected_nodes)} / {self.data.n_nodes}")
        print(f"  Computation time: {self.solve_time:.2f} seconds")
        print("-" * 60)

        print("SELECTED NODES AND PRIZES:")
        for node in self.selected_nodes:
            prize = self.data.get_prize(node)
            depot_mark = "(depot)" if node == 0 else ""
            print(f"  Node {node:3d}: prize = {prize:3d} {depot_mark}")
        print("-" * 60)

        print("TOUR SEQUENCE:")
        tour_str = " -> ".join(str(n) for n in self.tour)
        print(f"  {tour_str}")
        print("-" * 60)

        print("TOUR DETAILS:")
        for k in range(len(self.tour) - 1):
            i, j = self.tour[k], self.tour[k + 1]
            dist = self.data.get_distance(i, j)
            print(f"  {i:4d} -> {j:4d}: distance = {dist:.2f}")
        print("=" * 60)


def main():
    """主函数"""
    parser = argparse.ArgumentParser(description="PCTSP Basic Solver")
    parser.add_argument("instance", help="实例文件路径")
    parser.add_argument("-t", "--time-limit", type=float, default=600,
                        help="时间限制（秒），默认600")
    parser.add_argument("-q", "--quiet", action="store_true",
                        help="静默模式")
    parser.add_argument("-s", "--max-sec-size", type=int, default=5,
                        help="最大SEC子集大小，默认5")
    args = parser.parse_args()

    print()
    print("=" * 60)
    print(f"Loading instance: {args.instance}")
    print("=" * 60)

    data = PCTSPData(args.instance)
    solver = PCTSPBasicSolver(
        data,
        time_limit=args.time_limit,
        verbose=not args.quiet,
        max_sec_size=args.max_sec_size
    )

    solver.build_model()

    if solver.solve():
        solver.print_solution()
    else:
        print("No solution found!")
        return 1

    return 0


if __name__ == "__main__":
    exit(main())
