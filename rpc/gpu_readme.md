# 环境：安装pytorch,numpy
# 项目概览
Solver.py： OUIRP-DP求解器主体
data.py：
    input：JSON文件路径
    JSON文件格式：参考single.json的内容
    output：求解器需要的参数
main.py：
    调用data.py和Solver.py
    输入JSON文件的路径，就可以得到求解器返回的结果：
    cost：数组. 每个场景的最终成本.
    flags：二维数组. 每个场景每天的计划,内容是01序列，0就是不送，1就是送.
    quantities：二维数组. 每个场景每天对应的送货量。不送,那每日数量就是0;送,每日数量就是求解器得出来   的结果.

# 如何使用
直接在main.py的run中传入JSON文件路径即可

# 测试数据
single.json：单个场景的测试数据
mutil.json：多个场景的测试数据，最优结果同single.json
answer.json：场景，以及该场景下对应的答案