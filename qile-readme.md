# 1.代码修改

## 1.1 修改LocalSearch

（1）模仿mutation11重写`mutation12`，在其中使用OUPolicySolver来计算，在LocalSearch.h中添加 `mutation12`的声明和 OUPolicySolver.h的包含。
（2）在mutation12中，设计`noeudTravails`来储存计算出的每天的noeudTravail，`noeudTravails`的类型为vector<Noeud*>，在OUPolicySolver被调用。
（3）模仿insertions和quantities，设计`places`来储存计算出的每天的place(OUPolicySolver的返回值)，后续根据`places`是否为空判断是否插入节点，不需要原先的breakpoint。
（4）其他代码和mutation11相似度高，详情见代码注释。

## 1.2 设计OUPolicySolver

（1）使用`memory`储存特定时间和库存的结果，加速计算。
（2）使用`OUPolicyDP`递归自身，计算特定时间和库存下 不同选择导致的成本，并选择成本最低的选项。

## 1.3 修改Individu

（1）改变 `OPTION 2 -- JUST IN TIME POLICY` 为 `OU Policy`，让整个过程都严格遵循OU Policy，防止默认解过优。

## 1.4 修改Makefile

（1）在Makefile中添加OUPolicySolver.o，直接复制就行。

# 2. 代码运行

## 2.1 编译和运行
（1）在命令行中输入 `make` 来编译代码。
（2）在命令行中输入 `./irp path-to-instance -seed 1000 -type 38 -veh <number-of-vehicle> -stock <Stockout Penalty>` 来运行代码。
- Example: stockout test4
  ./irp Data/Small/Istanze0105h3/abs4n5_2.dat -seed 1000 -type 38 -veh 3 -stock 1000000
  ./irp Data/Small/0.dat -seed 1000 -type 38 -veh 1 -stock 200


## 2.2 数据修改

（1）直接规定每一天的客户需求：Data/Small/Istanze0105h3/abs4n5_2.dat
    ```
    6	3	89
    1	119.0	168.0	372	179	0.30
    2	191.0	336.0	53	106	0	53	20 20 20
    3	69.0	349.0	21	42	0	21	19 19 19
    4	94.0	235.0	24	48	0	24	24 24 24
    5	422.0	279.0	67	134	0	67	22 22 22
    6	153.0	108.0	28	42	0	14	40 40 40
    ```
    其中，第一行表示客户数量，仓库数量，天数。
    第二行表示客户1的初始库存等信息，每天的需求(此处均为20)。
    第三行表示客户2的初始库存等信息，每天的需求(此处均为19)。
    以此类推...

（2）当然也可以使用另外的Param读取方式，但要对Params.cpp进行修改。



