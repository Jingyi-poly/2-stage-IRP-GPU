import matplotlib.pyplot as plt
from matplotlib.ticker import LogLocator,ScalarFormatter,LogFormatterMathtext,FuncFormatter
import numpy as np
f = open('rec1','r')
x = []
cuday = []
cpuy = []
cpupy = []
for line in f:
    l = line.replace('\n','').split(' ')
    l = [float(x) for x in l]
    x.append(l[0])
    cuday.append(l[1])
    cpupy.append(l[2])
    cpuy.append(l[3])
f.close()

plt.plot(x,cuday,label='cuda')
plt.plot(x,cpupy,label='cpu8')
plt.plot(x,cpuy,label='cpu1')
plt.xlabel('# scenario')
plt.ylabel('time (ms)')
plt.legend()
plt.grid()
plt.xscale('log')
# plt.scale('log')
plt.savefig('p1.png')
plt.savefig("p1.pdf", format="pdf", bbox_inches="tight")









plt.clf()
f = open('rec2','r')
x = []
cuda128 = []
cuda105 = []
cpupy = []
for line in f:
    l = line.replace('\n','').split(' ')
    l = [float(x) for x in l if x!='']
    x.append(l[0])
    cuda128.append(l[1])
    cuda105.append(l[2])
    # cpuy.append(l[3])
f.close()



def sci_formatter(val, pos):
    exponent = int(np.floor(np.log10(val)))
    coeff = val / 10**exponent
    # show only integer coefficients
    if abs(coeff - round(coeff)) < 1e-10:
        coeff = int(round(coeff))
        if coeff in [2,4,6]:
            return r"${0}\times 10^{{{1}}}$".format(coeff, exponent)
    return ""  # skip non-integers (keeps it clean)

fig, ax1 = plt.subplots()
color = 'tab:red'
ax1.set_xlabel('# Scenario')
ax1.set_ylabel('Evaluation cost N-128', color=color)
lns1 = ax1.plot(x, cuda128, color=color, label='X-n128')
ax1.tick_params(axis='y', labelcolor=color)
ax1.yaxis.set_major_formatter(ScalarFormatter())
ax1.ticklabel_format(style='plain', axis='y')
ax1.yaxis.get_major_formatter().set_scientific(False)
ax1.yaxis.get_major_formatter().set_useOffset(False)

ax1.set_xscale('log')

ax2 = ax1.twinx()

color = 'tab:blue'
ax2.set_ylabel('Evaluation cost N-105', color=color)  # we already handled the x-label with ax1
lns2 = ax2.plot(x, cuda105, color=color,label='X-n105')
ax2.tick_params(axis='y', labelcolor=color)
# ax1.ticklabel_format(useOffset=False)
# ax2.ticklabel_format(useOffset=False)
ax2.yaxis.set_major_formatter(ScalarFormatter())
ax2.ticklabel_format(style='plain', axis='y')

ax2.yaxis.get_major_formatter().set_scientific(False)
ax2.yaxis.get_major_formatter().set_useOffset(False)
ax2.set_xscale('log')

ax1.xaxis.set_major_locator(LogLocator(base=10.0, subs=[1.0], numticks=20))
# ax1.xaxis.set_minor_locator(LogLocator(base=10.0, subs=range(1, 11), numticks=20))
ax1.xaxis.set_minor_locator(LogLocator(base=10.0, subs=np.arange(1.0, 10.0), numticks=100))

ax1.xaxis.set_minor_formatter(FuncFormatter(sci_formatter))

# ax1.xaxis.set_minor_locator(LogLocator(base=10.0, subs=np.arange(1.0, 10.0, 0.2), numticks=50))
# formatter = LogFormatterMathtext(base=10.0, labelOnlyBase=False)
# ax1.xaxis.set_minor_formatter(formatter)
ax1.tick_params(axis='x', which='minor', labelsize=7)
ax1.tick_params(axis='x', which='major', labelsize=12)




ax1.grid(which='both', axis='x', linestyle='--', linewidth=0.7)
ax1.grid()

labs = [l.get_label() for l in lns1+lns2]
ax1.legend(lns1+lns2, labs, loc=0)



# plt.plot(x,cuday,label='cuda')
# # plt.plot(x,cpupy,label='cpu10')
# # plt.plot(x,cpuy,label='cpu1')
# plt.xlabel('# Scenario')
# plt.ylabel('Penalized cost')
# plt.legend()
# plt.yscale('log')
# plt.xscale('log')
plt.savefig('p2.png')
plt.savefig("p2.pdf", format="pdf", bbox_inches="tight")











import os


xlim = 50

plt.clf()
t1 = []
c1 = []
f = open('logs_old/cpu1.log','r')
for line in f:
    line = line.replace('\n','').split(' ')
    l = [float(x) for x in line]
    t1.append(l[0])
    c1.append(l[1])
f.close()
if t1[-1] < xlim:
    t1.append(xlim)
    c1.append(c1[-1])

t8 = []
c8 = []
f = open('logs_old/cpu8.log','r')
for line in f:
    line = line.replace('\n','').split(' ')
    l = [float(x) for x in line]
    t8.append(l[0])
    c8.append(l[1])
f.close()
if t8[-1] < xlim:
    t8.append(xlim)
    c8.append(c8[-1])

tc = []
cc = []
f = open('logs_old/cuda.log','r')
for line in f:
    line = line.replace('\n','').split(' ')
    l = [float(x) for x in line]
    tc.append(l[0])
    cc.append(l[1])
f.close()
if tc[-1] < xlim:
    tc.append(xlim)
    cc.append(cc[-1])

plt.plot(tc,cc,label='cuda')
plt.plot(t8,c8,label='cpu8')
plt.plot(t1,c1,label='cpu1')
plt.xlabel('time')
plt.ylabel('Evaluation cost')
plt.legend()
# plt.xscale('log')
# plt.scale('log')
plt.xlim(0,xlim)
plt.grid()

plt.savefig('p3.png')
plt.savefig("p3.pdf", format="pdf", bbox_inches="tight")