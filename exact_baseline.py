#!/usr/bin/env python3
"""
Exact CVRP baseline solver using MIP (PuLP + CBC).

Two modes:
  1. Standalone: reads .vrp file, solves deterministic CVRP, evaluates heuristically
  2. With scenario file: reads binary scenarios exported by the C++ code for fair comparison

Usage:
  # Step 1: Export scenarios from C++ (run once)
  ./hgs_cuda instance.vrp sol.txt -seed 1 -nextrascen 99 -maxClient 20 -exportScenarios 1 -iterLim 1

  # Step 2: Solve MIP and evaluate with same scenarios
  python3 exact_baseline.py instance.vrp --scenarios scenarios_100_20.bin --timelimit 600
"""
import sys, math, struct, argparse, time
from multiprocessing import Pool, cpu_count
import numpy as np
import pulp


# ─────────────────────────────────────────────────────────────
# Instance parser (matches InstanceCVRPLIB.cpp)
# ─────────────────────────────────────────────────────────────
def read_vrp(path):
    with open(path) as f:
        lines = f.readlines()
    it = iter(lines)
    meta = {}
    next(it); next(it); next(it)
    line = next(it).strip()
    while line != "NODE_COORD_SECTION":
        parts = line.split()
        key = parts[0]
        if key == "DIMENSION":
            meta["n"] = int(parts[-1]) - 1
        elif key == "CAPACITY":
            meta["cap"] = float(parts[-1])
        elif key == "DISTANCE":
            meta["duration_limit"] = float(parts[-1])
        elif key == "SERVICE_TIME":
            meta["service_time_val"] = float(parts[-1])
        line = next(it).strip()

    n = meta["n"]
    xs, ys = [0.0]*(n+1), [0.0]*(n+1)
    for i in range(n+1):
        parts = next(it).split()
        xs[i] = float(parts[1])
        ys[i] = float(parts[2])

    line = next(it).strip()
    assert line == "DEMAND_SECTION", f"Expected DEMAND_SECTION, got {line}"
    demands = [0.0]*(n+1)
    for i in range(n+1):
        parts = next(it).split()
        demands[i] = float(parts[1])

    dist = [[0.0]*(n+1) for _ in range(n+1)]
    for i in range(n+1):
        for j in range(n+1):
            d = math.sqrt((xs[i]-xs[j])**2 + (ys[i]-ys[j])**2)
            dist[i][j] = round(d)

    svc = [0.0 if i == 0 else meta.get("service_time_val", 0.0) for i in range(n+1)]
    return n, meta["cap"], dist, demands, xs, ys, svc


# ─────────────────────────────────────────────────────────────
# Read binary scenario file exported by C++
# ─────────────────────────────────────────────────────────────
def read_scenario_bin(path):
    """Returns (n, n_scen, cap, penalty_cap, demands[n+1][n_scen], skip_pens[n+1], dist[n+1][n+1])"""
    with open(path, "rb") as f:
        nc = struct.unpack("i", f.read(4))[0]
        ns = struct.unpack("i", f.read(4))[0]
        cap = struct.unpack("d", f.read(8))[0]
        pen_cap = struct.unpack("d", f.read(8))[0]
        demands = np.zeros((nc+1, ns))
        for c in range(nc+1):
            demands[c, :] = np.frombuffer(f.read(ns * 8), dtype=np.float64)
        skip_pens = np.frombuffer(f.read((nc+1) * 8), dtype=np.float64).copy()
        dist_flat = np.frombuffer(f.read((nc+1)*(nc+1) * 8), dtype=np.float64)
        dist = dist_flat.reshape((nc+1, nc+1)).copy()
    return nc, ns, cap, pen_cap, demands, skip_pens, dist


# ─────────────────────────────────────────────────────────────
# MIP formulation: two-index vehicle flow + MTZ
# ─────────────────────────────────────────────────────────────
def solve_cvrp_mip(n, cap, dist, demands, n_vehicles, optional_visit=False,
                    skip_penalties=None, time_limit=600):
    nodes = list(range(n+1))
    clients = list(range(1, n+1))
    K = n_vehicles

    prob = pulp.LpProblem("CVRP", pulp.LpMinimize)

    x = {}
    for i in nodes:
        for j in nodes:
            if i != j:
                x[i,j] = pulp.LpVariable(f"x_{i}_{j}", cat="Binary")

    u = {}
    for i in clients:
        u[i] = pulp.LpVariable(f"u_{i}", lowBound=demands[i], upBound=cap)

    y = {}
    if optional_visit:
        for i in clients:
            y[i] = pulp.LpVariable(f"y_{i}", cat="Binary")

    obj_expr = pulp.lpSum(dist[i][j] * x[i,j] for i in nodes for j in nodes if i != j)
    if optional_visit and skip_penalties is not None:
        obj_expr += pulp.lpSum(skip_penalties[i] * (1 - y[i]) for i in clients)
    prob += obj_expr

    for i in clients:
        in_arcs = pulp.lpSum(x[j,i] for j in nodes if j != i)
        out_arcs = pulp.lpSum(x[i,j] for j in nodes if j != i)
        if optional_visit:
            prob += in_arcs == y[i]
            prob += out_arcs == y[i]
        else:
            prob += in_arcs == 1
            prob += out_arcs == 1

    prob += pulp.lpSum(x[0,j] for j in clients) <= K
    prob += pulp.lpSum(x[j,0] for j in clients) <= K
    prob += pulp.lpSum(x[0,j] for j in clients) == pulp.lpSum(x[j,0] for j in clients)
    prob += pulp.lpSum(x[0,j] for j in clients) >= 1

    for i in clients:
        for j in clients:
            if i != j:
                if optional_visit:
                    prob += u[i] - u[j] + cap * x[i,j] <= cap - demands[j] + cap * (1 - y[j])
                else:
                    prob += u[i] - u[j] + cap * x[i,j] <= cap - demands[j]

    solver = pulp.PULP_CBC_CMD(msg=1, timeLimit=time_limit, threads=8)
    t0 = time.time()
    prob.solve(solver)
    solve_time = time.time() - t0
    status = pulp.LpStatus[prob.status]

    routes = []
    if status in ("Optimal", "Not Solved"):
        visited_from_depot = []
        for j in clients:
            if x[0,j].varValue is not None and x[0,j].varValue > 0.5:
                visited_from_depot.append(j)
        for start in visited_from_depot:
            route = [start]
            cur = start
            while True:
                nxt = None
                for j in nodes:
                    if j != cur and (cur,j) in x and x[cur,j].varValue is not None and x[cur,j].varValue > 0.5:
                        nxt = j
                        break
                if nxt is None or nxt == 0:
                    break
                route.append(nxt)
                cur = nxt
            routes.append(route)

    obj_val = pulp.value(prob.objective) if prob.objective is not None else None
    skipped = []
    if optional_visit:
        for i in clients:
            if y[i].varValue is not None and y[i].varValue < 0.5:
                skipped.append(i)

    return routes, obj_val, status, solve_time, skipped


# ─────────────────────────────────────────────────────────────
# Evaluate fixed routes under stochastic scenarios
# ─────────────────────────────────────────────────────────────
def evaluate_routes_stochastic(routes, n, dist, all_demands, n_scen, cap, penalty_cap,
                               skip_penalties=None, skipped_clients=None):
    """
    Evaluate FIXED routes (same routes for every scenario).
    all_demands shape: [n+1, n_scen] (client × scenario)
    """
    total_cost = 0.0
    total_dist = 0.0
    total_cap_ex = 0.0
    skipped_set = set(skipped_clients) if skipped_clients else set()
    skip_cost_total = 0.0
    if skip_penalties is not None:
        for c in skipped_set:
            skip_cost_total += skip_penalties[c]

    for s in range(n_scen):
        scen_dist = 0.0
        scen_cap_ex = 0.0
        for route in routes:
            if not route:
                continue
            distance = dist[0][route[0]]
            load = all_demands[route[0], s]
            for k in range(1, len(route)):
                distance += dist[route[k-1]][route[k]]
                load += all_demands[route[k], s]
            distance += dist[route[-1]][0]
            scen_dist += distance
            if load > cap:
                scen_cap_ex += penalty_cap * (load - cap)
        total_dist += scen_dist
        total_cap_ex += scen_cap_ex
        total_cost += scen_dist + scen_cap_ex + skip_cost_total

    avg_cost = total_cost / n_scen
    avg_dist = total_dist / n_scen
    avg_cap_ex = total_cap_ex / n_scen
    return avg_cost, avg_dist, avg_cap_ex


def evaluate_giant_tour_split(giant_tour, n, dist, all_demands, n_scen, cap, penalty_cap):
    """
    Evaluate a giant tour using the Split DP (same as C++ Split algorithm).
    For each scenario, Split finds the optimal partition into routes.
    This is what HGS actually does.
    """
    m = len(giant_tour)
    total_cost = 0.0
    total_dist = 0.0
    total_cap_ex = 0.0

    d0x = [0.0] * (m + 1)
    dx0 = [0.0] * (m + 1)
    sum_dist = [0.0] * (m + 1)
    for i in range(1, m + 1):
        d0x[i] = dist[0][giant_tour[i-1]]
        dx0[i] = dist[giant_tour[i-1]][0]
        if i < m:
            dnext = dist[giant_tour[i-1]][giant_tour[i]]
        else:
            dnext = -1e30
        sum_dist[i] = sum_dist[i-1] + (dist[giant_tour[i-2]][giant_tour[i-1]] if i >= 2 else 0.0)

    for s in range(n_scen):
        sum_load = [0.0] * (m + 1)
        for i in range(1, m + 1):
            sum_load[i] = sum_load[i-1] + all_demands[giant_tour[i-1], s]

        potential = [1e30] * (m + 1)
        pred = [0] * (m + 1)
        potential[0] = 0.0

        from collections import deque
        Q = deque([0])

        for j in range(1, m + 1):
            i = Q[0]
            load_excess = sum_load[j] - sum_load[i] - cap
            pen = penalty_cap * max(0.0, load_excess)
            cost = potential[i] + sum_dist[j] - sum_dist[i+1] + d0x[i+1] + dx0[j] + pen
            if cost < potential[j]:
                potential[j] = cost
                pred[j] = i

            if j < m:
                while len(Q) > 0:
                    back = Q[-1]
                    v1 = potential[j] + d0x[j+1]
                    v2 = potential[back] + d0x[back+1] + sum_dist[j+1] - sum_dist[back+1] + \
                         penalty_cap * (sum_load[j] - sum_load[back])
                    if v1 <= v2:
                        break
                    # j doesn't dominate back, insert j
                    # but first check dominatesRight
                    v1r = potential[j] + d0x[j+1]
                    v2r = potential[back] + d0x[back+1] + sum_dist[j+1] - sum_dist[back+1] + 1e-5
                    if v1r < v2r:
                        Q.pop()
                    else:
                        break
                Q.append(j)

                while len(Q) > 1:
                    fr = Q[0]
                    nfr = Q[1]
                    load_fr = sum_load[j+1] - sum_load[fr] - cap
                    pen_fr = penalty_cap * max(0.0, load_fr)
                    v1p = potential[fr] + sum_dist[j+1] - sum_dist[fr+1] + d0x[fr+1] + dx0[j+1] + pen_fr
                    load_nfr = sum_load[j+1] - sum_load[nfr] - cap
                    pen_nfr = penalty_cap * max(0.0, load_nfr)
                    v2p = potential[nfr] + sum_dist[j+1] - sum_dist[nfr+1] + d0x[nfr+1] + dx0[j+1] + pen_nfr
                    if v1p > v2p - 1e-5:
                        Q.popleft()
                    else:
                        break

        total_cost += potential[m]
        total_dist += potential[m]  # includes penalty

    avg_cost = total_cost / n_scen
    return avg_cost


# ─────────────────────────────────────────────────────────────
# Main
# ─────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description="Exact CVRP baseline")
    parser.add_argument("instance", help="Path to .vrp file")
    parser.add_argument("--scenarios", help="Binary scenario file exported by C++")
    parser.add_argument("--maxclient", type=int, default=-1)
    parser.add_argument("--optionalVisit", action="store_true")
    parser.add_argument("--timelimit", type=int, default=600)
    args = parser.parse_args()

    print(f"=== Exact CVRP Baseline (MIP / CBC) ===")
    print(f"Instance: {args.instance}")

    # ── Read instance ──
    n_full, cap_full, dist_full, demands_full, xs, ys, svc = read_vrp(args.instance)

    # ── Read scenario file if provided ──
    scen_data = None
    if args.scenarios:
        nc, ns, cap_s, pen_cap, all_demands, skip_pens, dist_s = read_scenario_bin(args.scenarios)
        print(f"Loaded scenario file: {args.scenarios}")
        print(f"  Clients={nc}, Scenarios={ns}, Cap={cap_s}, PenCap={pen_cap:.4f}")
        n = nc
        cap = cap_s
        dist = dist_s.tolist()
        mean_demands = [float(all_demands[c, :].mean()) for c in range(n+1)]
        penalty_cap = pen_cap
        scen_data = (all_demands, ns, skip_pens)
    else:
        n = n_full
        if args.maxclient > 0 and n > args.maxclient:
            n = args.maxclient
        cap = cap_full
        dist = [row[:n+1] for row in dist_full[:n+1]]
        mean_demands = demands_full[:n+1]
        max_demand = max(mean_demands[1:n+1])
        max_dist_val = max(dist[i][j] for i in range(n+1) for j in range(n+1))
        penalty_cap = max(0.1, min(1000.0, max_dist_val / max_demand))
        scen_data = None

    print(f"Clients: {n}, Capacity: {cap}, TimeLimit: {args.timelimit}s")
    print(f"Penalty capacity: {penalty_cap:.4f}")

    total_demand = sum(mean_demands[1:n+1])
    n_vehicles = int(math.ceil(1.3 * total_demand / cap)) + 3
    print(f"Vehicles (upper bound): {n_vehicles}")
    print(f"Mean demands: depot={mean_demands[0]:.1f}, clients min={min(mean_demands[1:n+1]):.1f}, max={max(mean_demands[1:n+1]):.1f}, avg={sum(mean_demands[1:n+1])/n:.1f}")

    skip_pens_for_mip = None
    if args.optionalVisit and scen_data:
        skip_pens_for_mip = scen_data[2]
        print(f"Optional visit: skip penalty range [{skip_pens_for_mip[1:n+1].min():.1f}, {skip_pens_for_mip[1:n+1].max():.1f}]")

    # ── Solve deterministic CVRP ──
    print(f"\n--- Solving deterministic CVRP MIP (n={n}) ---")
    routes, obj_det, status, solve_time, skipped = solve_cvrp_mip(
        n, cap, dist, mean_demands, n_vehicles,
        optional_visit=args.optionalVisit,
        skip_penalties=skip_pens_for_mip,
        time_limit=args.timelimit
    )
    print(f"\nMIP status: {status}")
    print(f"MIP solve time: {solve_time:.2f}s")
    print(f"Deterministic objective: {obj_det}")
    print(f"Routes: {len(routes)}")
    if args.optionalVisit and skipped:
        print(f"Skipped clients: {len(skipped)}/{n}  ids={skipped}")
    for i, r in enumerate(routes):
        load = sum(mean_demands[c] for c in r)
        rdist = dist[0][r[0]] + sum(dist[r[k-1]][r[k]] for k in range(1, len(r))) + dist[r[-1]][0]
        print(f"  Route {i+1}: {r}  dist={rdist:.0f}  load={load:.0f}/{cap}")

    # ── Evaluate under stochastic scenarios ──
    if scen_data:
        all_demands, ns, skip_pens = scen_data

        print(f"\n--- [A] Fixed-route evaluation ({ns} scenarios) ---")
        print(f"    (Same routes for ALL scenarios, like standard CVRP)")
        avg_cost, avg_dist, avg_cap_ex = evaluate_routes_stochastic(
            routes, n, dist, all_demands, ns, cap, penalty_cap,
            skip_penalties=skip_pens if args.optionalVisit else None,
            skipped_clients=skipped if args.optionalVisit else None
        )
        print(f"    penalizedCost: {avg_cost:.4f}")
        print(f"    avg distance:  {avg_dist:.4f}")
        print(f"    avg capExcess: {avg_cap_ex:.4f}")

        print(f"\n--- [B] Split-adaptive evaluation ({ns} scenarios) ---")
        print(f"    (MIP route ORDER as giant tour, Split adapts per scenario)")
        giant_tour = []
        for r in routes:
            giant_tour.extend(r)
        print(f"    Giant tour: {giant_tour}")
        split_cost = evaluate_giant_tour_split(
            giant_tour, n, dist, all_demands, ns, cap, penalty_cap
        )
        print(f"    penalizedCost: {split_cost:.4f}")
        print(f"\n    VSS (A - B): {avg_cost - split_cost:.4f} ({(avg_cost - split_cost)/avg_cost*100:.1f}%)")

    print(f"\nTotal wall time: {solve_time:.2f}s")


# ─────────────────────────────────────────────────────────────
# Wait-and-See (WS) bound: per-scenario optimal CVRP
# ─────────────────────────────────────────────────────────────
def _solve_one_scenario(args):
    """Solve a single-scenario soft-capacity CVRP with Gurobi."""
    import gurobipy as gp
    from gurobipy import GRB

    s, n, cap, dist_list, demands_s, penalty_cap, n_vehicles, tl = args
    dist = dist_list
    nodes = list(range(n+1))
    clients = list(range(1, n+1))
    K = n_vehicles
    total_demand = sum(demands_s[1:])
    M = total_demand + 1.0

    m = gp.Model(f"CVRP_s{s}")
    m.setParam("OutputFlag", 0)
    m.setParam("TimeLimit", tl)
    m.setParam("Threads", 1)

    x = {}
    for i in nodes:
        for j in nodes:
            if i != j:
                x[i,j] = m.addVar(vtype=GRB.BINARY, name=f"x_{i}_{j}")
    u = {}
    for i in clients:
        u[i] = m.addVar(lb=demands_s[i], ub=M, name=f"u_{i}")
    excess = {}
    for i in clients:
        excess[i] = m.addVar(lb=0, name=f"ex_{i}")

    m.setObjective(
        gp.quicksum(dist[i][j] * x[i,j] for i in nodes for j in nodes if i != j)
        + penalty_cap * gp.quicksum(excess[i] for i in clients),
        GRB.MINIMIZE
    )

    for i in clients:
        m.addConstr(gp.quicksum(x[j,i] for j in nodes if j != i) == 1)
        m.addConstr(gp.quicksum(x[i,j] for j in nodes if j != i) == 1)
    m.addConstr(gp.quicksum(x[0,j] for j in clients) <= K)
    m.addConstr(gp.quicksum(x[j,0] for j in clients) <= K)
    m.addConstr(gp.quicksum(x[0,j] for j in clients) == gp.quicksum(x[j,0] for j in clients))
    m.addConstr(gp.quicksum(x[0,j] for j in clients) >= 1)

    for i in clients:
        for j in clients:
            if i != j:
                m.addConstr(u[i] - u[j] + M * x[i,j] <= M - demands_s[j])

    for i in clients:
        m.addConstr(excess[i] >= u[i] - cap - M * (1 - x[i,0]))
        m.addConstr(excess[i] <= M * x[i,0])

    m.optimize()

    if m.SolCount > 0:
        obj = m.ObjVal
        lb = m.ObjBound
        gap = m.MIPGap
        status = "Optimal" if m.Status == GRB.OPTIMAL else f"Gap={gap:.4f}"
    else:
        obj = None
        lb = m.ObjBound if hasattr(m, 'ObjBound') else None
        status = "NoSolution"
    return s, obj, status, lb


def _solve_tsp_gurobi(n, dist):
    """Solve TSP on n+1 nodes (0=depot, 1..n=clients) using Gurobi + DFJ lazy."""
    import gurobipy as gp
    from gurobipy import GRB

    nodes = list(range(n+1))
    m = gp.Model("TSP")
    m.setParam("OutputFlag", 0)
    m.setParam("TimeLimit", 30)
    m.setParam("LazyConstraints", 1)

    x = {}
    for i in nodes:
        for j in nodes:
            if i != j:
                x[i,j] = m.addVar(vtype=GRB.BINARY, obj=dist[i][j])
    m.update()

    for i in nodes:
        m.addConstr(gp.quicksum(x[j,i] for j in nodes if j != i) == 1)
        m.addConstr(gp.quicksum(x[i,j] for j in nodes if j != i) == 1)

    def subtour_elim(model, where):
        if where == GRB.Callback.MIPSOL:
            vals = model.cbGetSolution(x)
            adj = {i: [] for i in nodes}
            for (i,j), v in vals.items():
                if v > 0.5:
                    adj[i].append(j)
            visited = set()
            tours = []
            for start in nodes:
                if start in visited:
                    continue
                tour = []
                cur = start
                while cur not in visited:
                    visited.add(cur)
                    tour.append(cur)
                    nxt = [j for j in adj[cur] if j not in visited]
                    if not nxt:
                        break
                    cur = nxt[0]
                if len(tour) > 1:
                    tours.append(tour)
            for tour in tours:
                if len(tour) < len(nodes):
                    s = tour
                    model.cbLazy(
                        gp.quicksum(x[i,j] for i in s for j in s if i != j) <= len(s) - 1
                    )

    m.optimize(subtour_elim)
    if m.SolCount == 0:
        return None
    tour_arcs = [(i,j) for (i,j), v in x.items() if v.X > 0.5]
    adj = {}
    for i,j in tour_arcs:
        adj[i] = j
    order = []
    cur = adj[0]
    while cur != 0:
        order.append(cur)
        cur = adj[cur]
    return order


def split_dp(perm, demands_s, cap, pen_cap, dist_mtx):
    """Split DP: partition permutation into routes minimizing distance + cap penalty."""
    n = len(perm)
    INF = 1e30
    cost = [INF] * (n+1)
    cost[0] = 0.0
    for j in range(1, n+1):
        load = 0.0
        route_dist = 0.0
        for i in range(j, 0, -1):
            cli = perm[i-1]
            load += demands_s[cli]
            if i == j:
                route_dist = dist_mtx[cli][0]
            else:
                prev_cli = perm[i]
                route_dist += dist_mtx[cli][prev_cli]
            full_dist = dist_mtx[0][cli] + route_dist
            excess = max(0.0, load - cap)
            segment_cost = full_dist + pen_cap * excess
            if cost[i-1] + segment_cost < cost[j]:
                cost[j] = cost[i-1] + segment_cost
    return cost[n]


def solve_ws_tsp_split(n, cap, dist, all_demands, n_scen, penalty_cap):
    """
    Approximate WS bound: for each scenario, solve TSP then Split DP.
    TSP+Split ≥ WS_true (since WS allows non-contiguous routes).
    TSP+Split ≤ HGS (since TSP is optimal per scenario).
    """
    dist_list = dist.tolist() if hasattr(dist, 'tolist') else dist

    print("  Solving per-scenario TSPs ...")
    t0 = time.time()
    scen_costs = []
    for s in range(n_scen):
        demands_s = [float(all_demands[c, s]) for c in range(n+1)]
        tsp_tour = _solve_tsp_gurobi(n, dist_list)
        if tsp_tour is None:
            scen_costs.append(float('inf'))
            continue
        cost = split_dp(tsp_tour, demands_s, cap, penalty_cap, dist_list)
        scen_costs.append(cost)
        if (s+1) % max(1, n_scen // 5) == 0 or s == n_scen - 1:
            elapsed = time.time() - t0
            print(f"    {s+1}/{n_scen} done  avg={np.mean(scen_costs):.1f}  ({elapsed:.1f}s)")

    total_time = time.time() - t0
    return np.mean(scen_costs), scen_costs, total_time


def main_ws():
    parser = argparse.ArgumentParser(description="Wait-and-See stochastic CVRP bound")
    parser.add_argument("instance", help="Path to .vrp file")
    parser.add_argument("--scenarios", required=True, help="Binary scenario file")
    parser.add_argument("--timelimit", type=int, default=30, help="Per-scenario solver time limit")
    parser.add_argument("--workers", type=int, default=0, help="Parallel workers (0=auto)")
    args = parser.parse_args()

    n_full, cap_full, dist_full, demands_full, xs, ys, svc = read_vrp(args.instance)
    nc, ns, cap, pen_cap, all_demands, skip_pens, dist_s = read_scenario_bin(args.scenarios)

    print(f"=== Wait-and-See Bound (TSP + Split DP) ===")
    print(f"Instance: {args.instance}")
    print(f"Clients: {nc}, Scenarios: {ns}, Cap: {cap}, PenCap: {pen_cap:.4f}")

    ws_avg, ws_per_scen, ws_time = solve_ws_tsp_split(
        nc, cap, dist_s, all_demands, ns, pen_cap
    )

    print(f"\n=== RESULTS ===")
    print(f"TSP+Split avg cost: {ws_avg:.4f}")
    print(f"Total wall time: {ws_time:.1f}s")
    print(f"\nInterpretation:")
    print(f"  This is an UPPER bound on WS (perfect info lower bound).")
    print(f"  WS_true <= TSP_Split = {ws_avg:.1f}")
    print(f"  If HGS > TSP_Split, there is room for HGS improvement.")


if __name__ == "__main__":
    if "--ws" in sys.argv:
        sys.argv.remove("--ws")
        main_ws()
    else:
        main()
