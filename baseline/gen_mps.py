import gurobipy as gp
import numpy as np


def read_ins(fnm):
    ins_name = ''
    cap = 0
    n = 0
    dist_map = None
    demand_map = {}
    mode = 'INFO'
    coords = {}
    total_dem = 0
    maxDist = 0
    maxDemand = 0
    with open(fnm, 'r') as f:
        for line in f:
            if mode == 'INFO':
                if 'NAME :' in line:
                    name = line.split()[-1]
                elif 'DIMENSION :' in line:
                    n = int(line.split()[-1])
                    dist_map = np.zeros((n, n), dtype=np.float32)
                elif 'CAPACITY :' in line:
                    cap = int(line.split()[-1])
                elif 'NODE_COORD_SECTION' in line:
                    mode = 'COORD'
            elif mode == 'COORD':
                if 'DEMAND_SECTION' in line:
                    mode = 'DEMAND'
                else:
                    parts = line.split()
                    idx = int(parts[0]) - 1
                    x = float(parts[1])
                    y = float(parts[2])
                    coords[idx] = (x, y)
            elif mode == 'DEMAND':
                if 'DEPOT_SECTION' in line:
                    break
                else:
                    parts = line.split()
                    idx = int(parts[0]) - 1
                    demand = int(parts[1])
                    demand_map[idx] = demand
                    total_dem += demand
                    maxDemand = max(maxDemand, demand)

    for i in range(n):
        for j in range(n):
            if i != j:
                xi, yi = coords[i]
                xj, yj = coords[j]
                dist = round(np.hypot(xi - xj, yi - yj),0)
                dist_map[i, j] = dist
                maxDist = max(maxDist, dist)

        
    return cap, n, dist_map, demand_map, 2*int(round(total_dem/cap, 0)), maxDist


def read_scen(demand_map, nscen=10, max_client=5):
    with open(f'../baseline/scenarios{nscen}_{max_client}.txt', 'r') as f:
        for line in f:
            parts = line.split(':')
            idx = int(parts[0])
            demands = parts[1].strip().split(',')[:-1]
            demands = [int(d) for d in demands]
            demand_map[idx] = demands
    return demand_map   

def obtain_pen(demand_map, maxDist):
    print(demand_map)
    res = [0]*len(demand_map[0])
    for k in demand_map:
        if k == 0:
            continue
        for i in range(len(demand_map[k])):
            res[i] = max(res[i], demand_map[k][i])
    penalties = [max(0.1, min(maxDist/maxDemand, 1000)) for maxDemand in res]
    penalties = [max(0.1, min(maxDist/res[0], 1000)) for _ in res]
    print(maxDist, res[0])
    return penalties


def add_var(m,n,maxK, n_scen,dist_map,penalties):
    vmaps = {}
    for i in range(n):
        for j in range(n):
            if i == j:
                continue
            for k in range(maxK):
                for o in range(n_scen):
                    var_name = f"x_{i}_{j}_{k}_{o}"
                    vmaps[var_name] = m.addVar(vtype=gp.GRB.BINARY, name=var_name, obj=dist_map[i,j]/n_scen)

    for k in range(maxK):
        for o in range(n_scen):
            var_name = f"l_{k}_{o}"
            vmaps[var_name] = m.addVar(vtype=gp.GRB.CONTINUOUS, name=var_name, lb=0, obj=penalties[o]/n_scen)
    for i in range(n):
        for k in range(maxK):
            for o in range(n_scen):
                var_name = f'u_{i}_{k}_{o}'
                vmaps[var_name] = m.addVar(vtype=gp.GRB.CONTINUOUS, name=var_name, lb=0, ub = 2*n, obj=0)
    for i in range(n):
        for j in range(n):
            if i == j:
                continue
            var_name = f'z_{i}_{j}'
            vmaps[var_name] = m.addVar(vtype=gp.GRB.BINARY, name=var_name, obj=0)
            var_name = f'Y_{i}_{j}'
            vmaps[var_name] = m.addVar(vtype=gp.GRB.BINARY, name=var_name, obj=0)
            var_name = f'zeta_{i}_{j}'
            vmaps[var_name] = m.addVar(vtype=gp.GRB.CONTINUOUS, name=var_name, obj=0)

    for i in range(n):
        var_name = f'U_{i}'
        vmaps[var_name] = m.addVar(vtype=gp.GRB.CONTINUOUS, name=var_name, obj=0)
    return vmaps

def add_constr(m,n,maxK, n_scen,demand_map,vmaps,cap):
    expr = gp.LinExpr()
    for o in range(n_scen):
        for j in range(1, n):
            expr = gp.LinExpr()
            for i in range(n):
                if i == j:
                    continue
                for k in range(maxK):
                    var_name = f"x_{i}_{j}_{k}_{o}"
                    expr.addTerms(1.0, vmaps[var_name])
            m.addConstr(expr == 1, name=f"visit_once_j_{j}_{o}")
    for o in range(n_scen):
        for i in range(1, n):
            expr = gp.LinExpr()
            for j in range(n):
                if i == j:
                    continue
                for k in range(maxK):
                    var_name = f"x_{i}_{j}_{k}_{o}"
                    expr.addTerms(1.0, vmaps[var_name])
            m.addConstr(expr == 1, name=f"visit_once_i_{i}_{o}")

    for k in range(maxK):
        for o in range(n_scen):
            for j in range(1, n):
                expr = gp.LinExpr()
                for i in range(n):
                    if i == j:
                        continue
                    var_name = f"x_{i}_{j}_{k}_{o}"
                    expr.addTerms(1.0, vmaps[var_name])
                for i in range(n):
                    if i == j:
                        continue
                    var_name = f"x_{j}_{i}_{k}_{o}"
                    expr.addTerms(-1.0, vmaps[var_name])
                m.addConstr(expr == 0, name=f"same_car_{k}_{o}_{j}")

    
    for k in range(maxK):
        for o in range(n_scen):
            lhs = gp.LinExpr()
            rhs = gp.LinExpr()
            for j in range(1, n):
                var_name = f"x_0_{j}_{k}_{o}"
                lhs.addTerms(1.0, vmaps[var_name])
            for i in range(1, n):
                var_name = f"x_{i}_0_{k}_{o}"
                rhs.addTerms(1.0, vmaps[var_name])
            m.addConstr(lhs == rhs, name=f"depot_inout_{k}_{o}")
            m.addConstr(lhs <= 1, name=f"depot_out_{k}_{o}")
            m.addConstr(rhs <= 1, name=f"depot_in_{k}_{o}")


            
    for k in range(maxK):
        for o in range(n_scen):
            expr = gp.LinExpr()
            for i in range(n):
                for j in range(n):
                    if i == j:
                        continue
                    var_name = f"x_{i}_{j}_{k}_{o}"
                    expr.addTerms(demand_map[j][o], vmaps[var_name])
            var_name = f"l_{k}_{o}"
            expr.addTerms(-1.0, vmaps[var_name])
            m.addConstr(expr <= cap, name=f"load_cap_{k}_{o}")
    # quit()

    for k in range(maxK):
        for o in range(n_scen):
            for i in range(1, n):
                for j in range(1, n):
                    if i == j:
                        continue
                    u_i = f'u_{i}_{k}_{o}'
                    u_j = f'u_{j}_{k}_{o}'
                    x_ij = f"x_{i}_{j}_{k}_{o}"
                    expr = gp.LinExpr()
                    expr.addTerms(1.0, vmaps[u_i])
                    expr.addTerms(-1.0, vmaps[u_j])
                    expr.addTerms(n, vmaps[x_ij])
                    m.addConstr(expr <= n - 1, name=f"subtour_{i}_{j}_{k}_{o}")

    for j in range(1, n):
        expr = gp.LinExpr()
        for i in range(1,n):
            if i == j:
                continue
            var_name = f"z_{i}_{j}"
            expr.addTerms(1.0, vmaps[var_name])
            yij = f"Y_{i}_{j}"
            expr.addTerms(1.0, vmaps[yij])
        m.addConstr(expr == 1, name=f"first_stage_j_{j}")
    for i in range(1, n):
        expr = gp.LinExpr()
        for j in range(1,n):
            if i == j:
                continue
            var_name = f"z_{i}_{j}"
            expr.addTerms(1.0, vmaps[var_name])
            yij = f"Y_{i}_{j}"
            expr.addTerms(1.0, vmaps[yij])
        m.addConstr(expr == 1, name=f"first_stage_i_{i}")

    
    for i in range(1,n):
        for j in range(1,n):
            if i == j:
                continue
            for o in range(n_scen):
                lhs = gp.LinExpr()
                var_name = f"z_{i}_{j}"
                lhs.addTerms(1.0, vmaps[var_name])
                rhs1 = gp.LinExpr()
                rhs2 = gp.LinExpr()
                for k in range(maxK):
                    x_ij = f"x_{i}_{j}_{k}_{o}"
                    rhs1.addTerms(1.0, vmaps[x_ij])
                    if i!=0 and j!=0:
                        x_i0 = f"x_{i}_0_{k}_{o}"
                        rhs2.addTerms(1.0, vmaps[x_i0])
                        x_0j = f"x_0_{j}_{k}_{o}"
                        rhs2.addTerms(1.0, vmaps[x_0j])
                yij = f"Y_{i}_{j}"
                m.addConstr(lhs >= rhs1, name=f"link_first_second_{i}_{j}_{o}")
                zeta = f'zeta_{i}_{j}'
                if i!=0 and j!=0:
                    # rhs2.addTerms(-1.0, vmaps[yij])
                    rhs2.addTerms(-1.0, vmaps[zeta])
                    m.addConstr(lhs >= rhs2 - 1, name=f"link_first_seconds_{i}_{j}_{o}")


    for i in range(1, n):
        for j in range(1, n):
            if i == j:
                continue
            ui = vmaps[f'U_{i}']
            uj = vmaps[f'U_{j}']
            zij = vmaps[f'z_{i}_{j}']
            m.addConstr(ui - uj + n*zij <= n - 1, name=f"link_mtz_{i}_{j}")
            
            zeta = vmaps[f'zeta_{i}_{j}']
            m.addConstr(zeta >= uj - ui - 1, name=f"zeta_pos_{i}_{j}")
            m.addConstr(zeta >= ui - uj + 1, name=f"zeta_neg_{i}_{j}")


    expr = gp.LinExpr()
    for i in range(1, n):
        for j in range(1, n):
            if i == j:
                continue
            yi_j = vmaps[f'Y_{i}_{j}']
            expr.addTerms(1.0, yi_j)
    m.addConstr(expr == 1, name=f"final_link_{i}_{j}")
    
def print_res(m,vmaps,n_scen,n,maxK):
    dist = 0
    vio = 0
    for i in range(n):
        for j in range(n):
            if i == j:
                continue
            for k in range(maxK):
                for o in range(n_scen):
                    var_name = f"x_{i}_{j}_{k}_{o}"
                    if vmaps[var_name].X > 0.5:
                        dist += vmaps[var_name].Obj
    
    for k in range(maxK):
        for o in range(n_scen):
            var_name = f"l_{k}_{o}"
            vio += vmaps[var_name].X 
    
    time_elp = m.Runtime

    print(f'Objective Value: {m.ObjVal}')
    print(f'Distance: {dist*n_scen}')
    print(f'     Vio: {vio}')
    print(f' Runtime: {round(time_elp,3)}s')

    return m.ObjVal, dist*n_scen, vio,time_elp


def fix_vars(vmaps, m):
    t = [
        # 'z_5_4', 
        # 'z_4_3', 
        'x_0_3_1_0', 
        'x_3_0_1_0', 
        'x_0_1_0_0', 
        'x_1_7_0_0', 
        'x_7_6_0_0', 
        'x_6_2_0_0', 
        'x_2_4_0_0', 
        'x_4_5_0_0', 
        'x_5_8_0_0', 
        'x_8_0_0_0', 
        # 
        'x_0_3_0_1', 
        'x_3_1_0_1', 
        'x_1_7_0_1', 
        'x_7_6_0_1', 
        'x_6_2_0_1', 
        'x_2_4_0_1', 
        'x_4_5_0_1', 
        'x_5_8_0_1', 
        'x_8_0_0_1', 
        # 'z_2_1',
        # 'x_4_0_0_6'
        ]
    for tt in t:
        m.addConstr(vmaps[tt] == 1, name=f'fix_{tt}')
import os
def generate_prob(fnm,nscen=10):
    flist = os.listdir('./')
    flist = [x for x in flist if 'scenarios' in x and '_' in x]

    ress = {}
    flist.sort(key=lambda x : int(x.replace('.txt','').replace('scenarios','').split('_')[0]))

    for scen_file in flist:
        tfile = scen_file.replace('.txt','').replace('scenarios','').split('_')
        nscen = int(tfile[0])
        max_client = int(tfile[1])
        # if nscen!=100:
        #     continue
        if max_client!=10:
            continue

        cap,n,dist_map,demand_map,maxK,maxdist = read_ins(fnm)
        demand_map = read_scen(demand_map, nscen, max_client)

        n = min(n, max_client + 1)
        demand_map = {k: demand_map[k][:nscen] for k in range(n)}
        dist_map = dist_map[:n, :n]

        maxdist = dist_map.max()
        penalties = obtain_pen(demand_map,maxdist)

        print(dist_map)


        m = gp.Model("CVRP")
        m.setParam('TimeLimit',3600)
        m.setParam('LogFile',f'./logs/{scen_file}.log')
        vmaps = add_var(m,n,maxK,nscen,dist_map,penalties)
        add_constr(m,n,maxK,nscen,demand_map,vmaps,cap)
        # fix_vars(vmaps, m)
        m.write('t.lp')
        m.optimize()
        if m.Status == gp.GRB.INFEASIBLE:
            m.computeIIS()
            m.write('./iis.ilp')
        else:
            m.write(f'sol/sol_{nscen}_{max_client}.sol')
            obj, tdis, vio,time_elp = print_res(m,vmaps,nscen,n,maxK)
            ress[scen_file] = [obj,tdis,vio,time_elp]

    return ress

run_rec = generate_prob('../Instances/CVRP/X-n129-k18.vrp')
print('  \n\n\n\n\n')
for ss in run_rec:
    print(ss)
    tb = run_rec[ss]
    print(f'    Objective Value: {tb[0]}')
    print(f'    Distance: {tb[1]}')
    print(f'         Vio: {tb[2]}')
    print(f'     Runtime: {round(tb[3],3)}s\n\n')
