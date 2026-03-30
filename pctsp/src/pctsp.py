"""
Prize-Collecting Traveling Salesman Problem (PCTSP) Solver using Gurobi.

This module implements a complete PCTSP solver based on the formulation:
    max  Σ βⱼyⱼ - Σ cₑxₑ   (maximize prizes minus travel costs)
    s.t. Σ xₑ = 2yᵢ         (degree constraints)
         Subtour elimination constraints (lazy callbacks)
         yᵣ = 1              (depot must be visited)
         xₑ, yⱼ ∈ {0,1}      (binary variables)

The solver uses lazy constraints for subtour elimination (GSEC).
"""

import time
from typing import List, Tuple, Dict, Optional

import gurobipy as gp
from gurobipy import GRB

from data import PCTSPData


class PCTSPSolver:
    """
    Gurobi-based solver for the Prize-Collecting TSP.

    This solver builds and solves a MIP model for PCTSP with:
    - Edge selection variables (x)
    - Node selection variables (y)
    - Degree constraints
    - Lazy subtour elimination constraints
    """

    def __init__(self, data: PCTSPData, time_limit: float = 3600.0,
                 verbose: bool = True):
        """
        Initialize the PCTSP solver.

        Args:
            data: PCTSPData instance containing problem data
            time_limit: Maximum solving time in seconds (default: 1 hour)
            verbose: Whether to print Gurobi output (default: True)
        """
        self.data = data
        self.time_limit = time_limit
        self.verbose = verbose

        # Model and variables (populated in build_model)
        self.model: Optional[gp.Model] = None
        self.x: Dict[Tuple[int, int], gp.Var] = {}  # Edge variables
        self.y: Dict[int, gp.Var] = {}  # Node selection variables

        # Solution info
        self.solve_time: float = 0.0
        self.obj_value: Optional[float] = None
        self.selected_nodes: List[int] = []
        self.tour: List[int] = []
        self.total_prize: int = 0
        self.total_distance: float = 0.0

    def build_model(self) -> None:
        """Build the Gurobi optimization model."""
        n = self.data.n_nodes
        nodes = self.data.get_nodes()

        # Create model
        self.model = gp.Model("PCTSP")

        # Set parameters
        self.model.Params.TimeLimit = self.time_limit
        self.model.Params.LazyConstraints = 1  # Enable lazy constraints
        if not self.verbose:
            self.model.Params.OutputFlag = 0

        # Create edge variables x[i,j] for i < j (undirected edges)
        for i in range(n):
            for j in range(i + 1, n):
                self.x[i, j] = self.model.addVar(
                    vtype=GRB.BINARY,
                    name=f"x_{i}_{j}",
                    obj=-self.data.get_distance(i, j)  # Minimize distance
                )

        # Create node selection variables y[i]
        for i in nodes:
            self.y[i] = self.model.addVar(
                vtype=GRB.BINARY,
                name=f"y_{i}",
                obj=self.data.get_prize(i)  # Maximize prize
            )

        # Set objective to maximize
        self.model.ModelSense = GRB.MAXIMIZE

        # Constraint: depot must be visited (y_r = 1)
        self.model.addConstr(self.y[0] == 1, name="depot")

        # Degree constraints: sum of edges incident to i = 2*y[i]
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

        self.model.update()

    def _get_edge_value(self, i: int, j: int) -> float:
        """Get the value of edge variable x[i,j] in current solution."""
        edge = (min(i, j), max(i, j))
        return self.x[edge].X

    def _subtour_elimination_callback(self, model: gp.Model, where: int) -> None:
        """
        Lazy constraint callback for subtour elimination.

        Implements GSEC: for any subset V not containing depot,
        sum of edges in E(V) <= sum of y[i] for i in V\{k} for some k in V
        """
        if where == GRB.Callback.MIPSOL:
            # Get current solution values - cbGetSolution returns dict with same keys
            x_vals = model.cbGetSolution(self.x)
            y_vals = model.cbGetSolution(self.y)

            # Find selected nodes - y_vals keys match self.y keys (node indices)
            selected_nodes = [i for i in range(self.data.n_nodes)
                             if y_vals[i] > 0.5]

            # Build adjacency from current solution
            adj: Dict[int, List[int]] = {i: [] for i in selected_nodes}
            for (i, j), val in x_vals.items():
                if val > 0.5:
                    if i in adj:
                        adj[i].append(j)
                    if j in adj:
                        adj[j].append(i)

            # Find subtours not containing depot
            visited = set()
            subtours_without_depot = []

            for start in selected_nodes:
                if start in visited:
                    continue

                component = []
                queue = [start]
                contains_depot = False

                while queue:
                    node = queue.pop(0)
                    if node in visited:
                        continue
                    visited.add(node)
                    component.append(node)
                    if node == 0:
                        contains_depot = True

                    for neighbor in adj[node]:
                        if neighbor not in visited:
                            queue.append(neighbor)

                if not contains_depot and len(component) >= 2:
                    subtours_without_depot.append(component)

            # Add lazy constraints for each subtour not containing depot
            for subtour in subtours_without_depot:
                # GSEC: sum of edges within subtour <= |subtour| - 1
                # This prevents any isolated subtour
                edges_in_subtour = []
                for i in subtour:
                    for j in subtour:
                        if i < j:
                            edges_in_subtour.append(self.x[i, j])

                if edges_in_subtour:
                    # Add constraint: sum(x[e] for e in E(S)) <= |S| - 1
                    model.cbLazy(
                        gp.quicksum(edges_in_subtour) <= len(subtour) - 1
                    )

    def solve(self) -> bool:
        """
        Solve the PCTSP model.

        Returns:
            True if an optimal or feasible solution was found, False otherwise
        """
        if self.model is None:
            self.build_model()

        start_time = time.time()

        # Optimize with lazy constraint callback
        self.model.optimize(
            lambda model, where: self._subtour_elimination_callback(model, where)
        )

        self.solve_time = time.time() - start_time

        # Check solution status
        if self.model.Status == GRB.OPTIMAL:
            self._extract_solution()
            return True
        elif self.model.Status == GRB.TIME_LIMIT and self.model.SolCount > 0:
            self._extract_solution()
            return True
        elif self.model.SolCount > 0:
            self._extract_solution()
            return True
        else:
            return False

    def _extract_solution(self) -> None:
        """Extract solution information from the solved model."""
        self.obj_value = self.model.ObjVal

        # Get selected nodes
        self.selected_nodes = [i for i in range(self.data.n_nodes)
                               if self.y[i].X > 0.5]

        # Calculate total prize collected
        self.total_prize = sum(self.data.get_prize(i)
                               for i in self.selected_nodes)

        # Build tour from selected edges
        self._build_tour()

        # Calculate total distance
        self.total_distance = 0.0
        for i in range(len(self.tour) - 1):
            self.total_distance += self.data.get_distance(
                self.tour[i], self.tour[i + 1]
            )

    def _build_tour(self) -> None:
        """Build the tour sequence from selected edges."""
        if not self.selected_nodes:
            self.tour = []
            return

        # Build adjacency list for selected nodes
        adj: Dict[int, List[int]] = {i: [] for i in self.selected_nodes}

        for i in self.selected_nodes:
            for j in self.selected_nodes:
                if i < j and self.x[i, j].X > 0.5:
                    adj[i].append(j)
                    adj[j].append(i)

        # Build tour starting from depot
        tour = [0]
        visited = {0}
        current = 0

        while True:
            next_node = None
            for neighbor in adj[current]:
                if neighbor not in visited:
                    next_node = neighbor
                    break

            if next_node is None:
                break

            tour.append(next_node)
            visited.add(next_node)
            current = next_node

        # Close the tour by returning to depot
        tour.append(0)
        self.tour = tour

    def get_solution_summary(self) -> str:
        """
        Get a formatted summary of the solution.

        Returns:
            Multi-line string with solution details
        """
        if self.obj_value is None:
            return "No solution found."

        lines = [
            "=" * 60,
            "PCTSP SOLUTION SUMMARY",
            "=" * 60,
            f"Instance: {self.data.filename}",
            f"Number of nodes: {self.data.n_nodes}",
            "-" * 60,
            "SOLUTION STATISTICS:",
            f"  Objective value: {self.obj_value:.2f}",
            f"  Total prize collected: {self.total_prize}",
            f"  Total distance traveled: {self.total_distance:.2f}",
            f"  Nodes visited: {len(self.selected_nodes)} / {self.data.n_nodes}",
            f"  Computation time: {self.solve_time:.2f} seconds",
            "-" * 60,
            "SELECTED NODES AND PRIZES:",
        ]

        for i, node in enumerate(self.selected_nodes):
            prize = self.data.get_prize(node)
            node_type = "(depot)" if node == 0 else ""
            lines.append(f"  Node {node:3d}: prize = {prize:3d} {node_type}")

        lines.append("-" * 60)
        lines.append("TOUR SEQUENCE:")
        lines.append(f"  {' -> '.join(map(str, self.tour))}")
        lines.append("-" * 60)

        # Tour details with distances
        lines.append("TOUR DETAILS:")
        for i in range(len(self.tour) - 1):
            from_node = self.tour[i]
            to_node = self.tour[i + 1]
            dist = self.data.get_distance(from_node, to_node)
            lines.append(f"  {from_node:3d} -> {to_node:3d}: distance = {dist:.2f}")

        lines.append("=" * 60)

        return "\n".join(lines)

    def print_solution(self) -> None:
        """Print the solution summary to stdout."""
        print(self.get_solution_summary())



def solve_pctsp(filepath: str, time_limit: float = 3600.0,                verbose: bool = True) -> PCTSPSolver:
    """
    Convenience function to solve a PCTSP instance.

    Args:
        filepath: Path to the instance file
        time_limit: Maximum solving time in seconds
        verbose: Whether to print solver output

    Returns:
        PCTSPSolver instance with solution
    """
    data = PCTSPData(filepath)
    solver = PCTSPSolver(data, time_limit=time_limit, verbose=verbose)
    solver.build_model()
    solver.solve()
    return solver


def main():
    """Main entry point for command-line usage."""
    import argparse
    import os
    import glob

    parser = argparse.ArgumentParser(
        description="Solve Prize-Collecting TSP instances using Gurobi"
    )
    parser.add_argument(
        "instance",
        type=str,
        nargs="?",
        default=None,
        help="Path to instance file (default: run first instance in data/)"
    )
    parser.add_argument(
        "--time-limit", "-t",
        type=float,
        default=600.0,
        help="Time limit in seconds (default: 600)"
    )
    parser.add_argument(
        "--quiet", "-q",
        action="store_true",
        help="Suppress Gurobi solver output"
    )
    parser.add_argument(
        "--all", "-a",
        action="store_true",
        help="Solve all instances in data/ folder"
    )

    args = parser.parse_args()

    # Determine which instances to solve
    if args.all:
        # Find all .dat files in data folder
        data_dir = os.path.join(os.path.dirname(__file__), "..", "data")
        instances = sorted(glob.glob(os.path.join(data_dir, "*.dat")))
        if not instances:
            print("No instance files found in data/ folder")
            return
    elif args.instance:
        instances = [args.instance]
    else:
        # Default: use first instance in data folder
        data_dir = os.path.join(os.path.dirname(__file__), "..", "data")
        instances = sorted(glob.glob(os.path.join(data_dir, "*.dat")))
        if instances:
            instances = [instances[0]]
        else:
            print("No instance file specified and no files found in data/")
            return

    # Solve each instance
    for filepath in instances:
        print(f"\n{'='*60}")
        print(f"Loading instance: {filepath}")
        print(f"{'='*60}")

        try:
            solver = solve_pctsp(
                filepath,
                time_limit=args.time_limit,
                verbose=not args.quiet
            )
            solver.print_solution()
        except FileNotFoundError:
            print(f"Error: File not found: {filepath}")
        except Exception as e:
            print(f"Error solving instance: {e}")
            raise


if __name__ == "__main__":
    main()

