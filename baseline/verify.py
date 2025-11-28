ns = 20
nc = 8

f = f'sol/sol_{ns}_{nc}.sol'
vmap = {}
with open(f, 'r') as fin:
    for l in fin:
        if '#' in l:
            continue
        l = l.replace('\n','').split(' ')
        vmap[l[0]] = float(l[1])

routes = {}
for v in vmap:
    if v.startswith('x_') and vmap[v] > 0:
        vv = v.split('_')
        s = vv[1]
        e = vv[2]
        c = vv[3]
        o = vv[-1]
        if o not in routes:
            routes[o] = {}
        if c not in routes[o]:
            routes[o][c] = []
        routes[o][c].append(f'{s}-{e}')
        # print(f"x {s} - {e} {o}")
for o in routes:
    print(f'SCEN {o}:')
    for c in routes[o]:
        print(f'  car {c}: ',end='')
        for k in routes[o][c]:
            print(k,end='  ')
        print()
    print('\n----------------------')


for v in vmap:
    if v.startswith('l_') and vmap[v] > 0:
        print(v, vmap[v])