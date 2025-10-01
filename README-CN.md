## 项目概述

OUIRP-GPU是一个复杂的C++库存路径规划问题(IRP)求解器实现，使用了混合遗传搜索算法与距离计算(HGSADC)。该项目通过C++客户端与Python服务器之间的gRPC通信集成GPU计算能力，实现了针对随机需求模式的库存路径规划场景的远程优化求解。

该系统解决了供应链网络中车辆路径规划和库存管理决策联合优化的挑战，需要协调配送计划与库存水平以最小化包括运输、库存持有和缺货惩罚在内的总成本。

## 快速开始
1.编译

1.1在rpc目录下：
mkdir build && cd build
cmake ..
make
cd ..

1.2在主目录下：
mkdir build && cd build
cmake ..
make
cd ..

2.运行方式
在run_scenarios.sh脚本中，scenarios=(1 10 100)时候，系统会开辟三个进程，三个进程分别处理1个场景的情况，10个场景的情况，100个场景的情况。

2.1调用本地资源运行
脚本当中的THREADS_PER_PROCESS等于n,n大于0时候，每个进程都会调用n个线程进行处理。

2.2调用GPU版本的算法
在rpc目录下运行server.py，脚本当中的THREADS_PER_PROCESS设置为0，此时每个进程自动调用GPU版本的算法。

3.注意事项
在超大规模场景下，才能充分发挥GPU版本算法的优势。当场景太小时候，请使用CPU资源进行计算。

### 主要构建命令
```bash
# 使用CMake构建项目（推荐）
mkdir build && cd build
cmake ..
make   # 使用所有可用核心
cd ..

# 替代方案：完全清理和重新构建
rm -vr build && mkdir build && cd build && cmake .. && make && cd ..

# 用于调试的快速单线程测试
THREADS_PER_PROCESS=1 ./irp Data/Small/Istanze0105h3/0.dat -seed 989 -type 38 -veh 1 -stock 100 -demandseed 989 -iter 1 -scenario 1 -control_day_1 1 -threads 1

# 标准多迭代运行
./irp Data/Small/Istanze0105h3/0.dat -seed 989 -type 38 -veh 1 -stock 100 -demandseed 989 -iter 1000 -scenario 5 -control_day_1 1 -threads 4

# 运行不同配置的并行场景
./run_scenarios.sh

# 清理构建产物
make clean_all  # 在build目录中
rm -vr build   # 完全清理
```

### 开发依赖设置
```bash
# Ubuntu/Debian - 完整依赖安装
sudo apt update && sudo apt install -y \
    build-essential g++ make cmake pkg-config \
    libgrpc++-dev libprotobuf-dev protobuf-compiler-grpc \
    libabsl-dev valgrind gdb clang-format

# macOS使用Homebrew
brew install grpc protobuf cmake pkg-config valgrind

# gRPC服务器和GPU加速的Python依赖
pip3 install grpcio grpcio-tools protobuf numpy torch

# 验证安装
pkg-config --modversion grpc++ protobuf
python3 -c "import grpc, torch, numpy; print('所有依赖已安装')"
```

### gRPC服务器管理
```bash
# 启动Python gRPC服务器（远程优化必需）
cd rpc && python3 server.py &
SERVER_PID=$!

# 测试gRPC连接
python3 rpc/test_grpc.py

# 停止服务器
kill $SERVER_PID

# 启动带调试跟踪的服务器
cd rpc && python3 -c "
import server
# 编辑server.py设置traces=True以启用调试
"
```

### 测试和验证
```bash
# 运行单线程性能测试
./test_threads.sh

# 使用Valgrind进行内存泄漏检测
valgrind --leak-check=full --track-origins=yes ./irp Data/Small/Istanze0105h3/0.dat -seed 989 -type 38 -veh 1 -stock 100 -iter 10

# 带日志的并行场景执行
THREADS_PER_PROCESS=4 ./run_scenarios.sh

# 查看日志输出
ls -la parallel_logs/
tail -f parallel_logs/scenario_1.log
```

## 架构概述

### 核心组件

1. **主算法引擎**：围绕`Genetic`类构建，实现了具有多样性管理和基于惩罚的可行性处理的混合遗传搜索算法
2. **个体表示**：`Individu`类通过染色体编码路径序列来表示解决方案，进行适应度评估和成本结构管理
3. **种群管理**：`Population`类处理具有多样性指标、精英保存和重启机制的解决方案集合
4. **局部搜索**：`LocalSearch`类提供复杂的邻域搜索改进，包括2-opt、交叉交换和专门的IRP算子
5. **策略优化**：`OUPolicySolver`使用带成本函数近似的动态规划处理库存优化
6. **gRPC集成**：通过protobuf序列化的Python服务器通信实现远程GPU计算
7. **线程管理**：`ThreadPool`类启用并行场景执行和并发遗传操作

### 关键类及其详细作用

#### 核心算法类
- **`Genetic`**：
  - 协调具有交叉（OX、POX变体）的进化算法
  - 管理变异算子和种群多样性
  - 实现约束处理的惩罚管理
  - 控制大型问题的分解策略

- **`Individu`**：
  - 将解决方案编码为染色体序列（不插入仓库的路径顺序）
  - 通过`split()`函数管理VRP解码的适应度评估
  - 处理包括容量/长度违规的成本结构（`coutSol`）
  - 与局部搜索集成以改进解决方案

- **`Population`**：
  - 维护可行和不可行解决方案池
  - 实现多样性距离计算
  - 管理精英解决方案保存和重启机制
  - 控制种群大小和多样性阈值

#### 问题特定类
- **`Params`**：
  - 加载和管理问题实例数据（客户、需求、成本）
  - 处理车辆容量和路径约束
  - 存储距离矩阵和时间窗
  - 管理随机问题参数

- **`OUPolicySolver`**：
  - 实现最优缺货策略的动态规划
  - 处理多场景随机优化
  - 管理库存成本、缺货惩罚和容量约束
  - 支持本地计算和远程gRPC调用

- **`LocalSearch`**：
  - 实现路径内改进（2-opt、Or-opt）
  - 提供路径间算子（重定位、交换、交叉交换）
  - 包括用于库存-路径协调的专门IRP算子
  - 使用高效的增量评估进行移动评估

#### 实用程序和支持类
- **`ScenarioUtils`**：
  - 管理多场景问题实例
  - 处理随机需求生成和场景采样
  - 协调并行场景执行
  - 聚合跨场景结果

- **`CommandLine`**：
  - 解析命令行参数包括`-seed`、`-type`、`-veh`、`-stock`、`-demandseed`、`-iter`、`-scenario`、`-control_day_1`、`-threads`
  - 验证输入参数并设置默认值
  - 处理实例和解决方案的文件路径解析

### 数据流架构

#### 1. 问题初始化
```
实例文件 → Params → 问题设置
           ↓
     CommandLine → 参数验证
           ↓
     Population → 初始解生成
```

#### 2. 主优化循环
```
遗传算法：
  ├── 父代选择（锦标赛/轮盘）
  ├── 交叉（OX/POX变体）
  ├── 变异（路径扰动）
  ├── 局部搜索改进
  ├── OUPolicySolver（本地或gRPC）
  ├── 适应度评估
  └── 种群更新

局部搜索：
  ├── 路径内：2-opt、Or-opt
  ├── 路径间：重定位、交换
  └── IRP专用：库存-路径协调
```

#### 3. 策略优化集成
```
对于每个个体：
  ├── 提取路径结构
  ├── 计算每个场景的配送成本
  ├── 调用OUPolicySolver：
  │   ├── 时间范围内的动态规划
  │   ├── 考虑配送vs不配送决策
  │   ├── 最小化：库存+缺货+配送成本
  │   └── 返回最优策略和成本
  └── 更新个体适应度
```

### gRPC集成架构

#### 协议定义（`rpc/irp.proto`）
- **`OuReq`**：包含客户ID、场景、库存参数、成本函数的请求消息
- **`OuResp`**：带有优化策略、成本和配送计划的响应
- **`ScenarioInfo`**：带有需求、成本和结果的单个场景数据
- **`CapaCostFunc`**：容量惩罚成本函数（阈值+线性）

#### 通信流程
```
C++客户端（Individu.cpp）：
  ├── 准备带有场景数据的OuReq
  ├── 向Python服务器发起gRPC调用
  ├── 接收带有最优策略的OuResp
  └── 更新个体成本和路径结构

Python服务器（rpc/server.py）：
  ├── 接收gRPC请求
  ├── 解析场景数据
  ├── 运行OURemoteSolver动态规划
  ├── 生成最优策略
  └── 通过gRPC响应返回结果
```

#### GPU加速支持
- Python服务器可以利用PyTorch张量进行并行场景处理
- NumPy数组启用向量化动态规划计算
- 大规模多场景问题的GPU内存管理

### 多场景处理架构

#### 场景生成
- 每个场景代表不同的需求实现
- 随机种子控制确保可重现的实验
- 需求种子创建相关的不确定性模式
- 场景可以在CPU核心间并行处理

#### 并行执行策略
```
主进程：
  ├── 启动多个场景工作器
  ├── 每个工作器：独立的IRP求解器实例
  ├── 共享：问题实例和基础参数
  ├── 不同：随机种子和需求模式
  └── 聚合：解决方案质量统计

run_scenarios.sh：
  ├── 为不同场景生成并行进程
  ├── 将输出记录到parallel_logs/目录
  ├── 管理进程同步和清理
  └── 报告完成状态和时间
```

## 关键参数和配置

### 命令行参数（详细）
- **`-seed`**：遗传算法可重现性的随机种子（例如989、1000）
- **`-type`**：问题变体标识符（38 = 带库存管理的标准IRP）
- **`-veh`**：可用车辆数量（单车问题通常为1）
- **`-stock`**：初始库存水平占最大容量的百分比（例如100 = 满库存）
- **`-demandseed`**：随机需求生成的单独种子（启用需求模式控制）
- **`-iter`**：最大遗传算法迭代次数（标准运行1000，密集型10000+）
- **`-scenario`**：随机问题的场景编号（典型范围1-20）
- **`-control_day_1`**：启用第一天配送控制优化（0/1标志）
- **`-threads`**：并行处理的CPU线程数（推荐：CPU核心数）

### 示例参数组合
```bash
# 快速调试运行
./irp Data/Small/Istanze0105h3/0.dat -seed 989 -type 38 -veh 1 -stock 100 -demandseed 989 -iter 100 -scenario 1 -control_day_1 1 -threads 1

# 标准优化运行
./irp Data/Small/Istanze0105h3/0.dat -seed 1000 -type 38 -veh 1 -stock 100 -demandseed 2000 -iter 1000 -scenario 5 -control_day_1 1 -threads 4

# 密集型多场景研究
for scenario in {1..20}; do
  ./irp Data/Small/Istanze0105h3/0.dat -seed 1001 -type 38 -veh 1 -stock 100 -demandseed 2000 -iter 5000 -scenario $scenario -control_day_1 1 -threads 8 &
done
```

### 关键文件结构
```
OUIRP-GPU/
├── Data/Small/Istanze0105h3/
│   └── 0.dat                    # 主问题实例（客户、需求、距离）
├── parallel_logs/               # 场景执行日志和结果
│   ├── scenario_1.log          # 单个场景输出
│   └── scenario_*.log          # 多个场景结果
├── rpc/                        # gRPC通信模块
│   ├── server.py               # Python优化服务器
│   ├── irp.proto              # 协议缓冲区定义
│   ├── gpu_solver.py          # GPU加速求解器组件
│   └── start_server.sh        # 服务器启动脚本
├── CMakeLists.txt             # 带依赖的构建配置
├── run_scenarios.sh           # 并行场景执行脚本
├── main.cpp                   # 应用程序入口点
├── Genetic.cpp/.h             # 遗传算法实现
├── Individu.cpp/.h            # 解决方案表示
├── OUPolicySolver.cpp/.h      # 库存策略优化
└── LocalSearch.cpp/.h         # 邻域搜索算子
```

### 配置文件和设置
- **实例格式**：带有客户坐标、需求、车辆容量的`.dat`文件
- **输出格式**：带有路径、成本和配送计划的解决方案文件
- **日志格式**：带有迭代进度、最佳解决方案、时间信息的结构化日志
- **gRPC配置**：服务器端口50051（默认）、超时设置、消息大小限制

## 开发说明

### 线程安全和并发
- **ThreadPool实现**：`ThreadPool.cpp`中用于遗传操作的自定义线程池
- **场景并行性**：通过`run_scenarios.sh`的独立进程避免共享状态
- **gRPC线程安全**：Python服务器使用线程安全数据结构处理并发请求
- **内存同步**：种群更新和适应度跟踪的原子操作
- **竞态条件预防**：每个线程使用单独的随机数生成器

### 内存管理策略
- **RAII模式**：所有主要对象（Params、Population、Individu）使用构造函数/析构函数生命周期
- **智能指针**：种群个体和路径结构的共享所有权
- **内存池**：在局部搜索和遗传操作中重用临时对象
- **gRPC内存**：协议缓冲区对象由gRPC运行时自动管理
- **泄漏预防**：Valgrind集成用于内存调试和泄漏检测

### 调试和分析工具
```bash
# 启用全面跟踪
在main.cpp中#define TRACES

# 使用Valgrind进行内存调试
valgrind --tool=memcheck --leak-check=full --track-origins=yes ./irp [args]

# 使用gprof进行性能分析
g++ -pg [编译标志] && ./irp [args] && gprof ./irp gmon.out

# gRPC调试
export GRPC_VERBOSITY=DEBUG
export GRPC_TRACE=all
cd rpc && python3 server.py

# 并行执行监控
watch -n 1 'ps aux | grep irp | wc -l'  # 计算运行进程数
tail -f parallel_logs/scenario_*.log    # 监控场景进度
```

### 性能优化指南

#### 编译优化
```bash
# 带优化标志的CMake
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-O3 -march=native -flto" ..

# 最大优化的手动编译
g++ -O3 -march=native -flto -DNDEBUG -ffast-math [sources] -o irp
```

#### 运行时性能调优
- **线程数**：设置为CPU核心数减1以获得最佳性能
- **种群大小**：复杂实例的较大种群（50-100）
- **迭代预算**：根据问题大小和时间约束1000-5000次迭代
- **gRPC批大小**：每次gRPC调用分组多个策略优化
- **内存局部性**：一起处理具有相似需求模式的场景

#### 可扩展性考虑
- **多实例并行性**：运行多个独立的求解器实例
- **GPU内存管理**：在GPU上批处理场景以最大化利用率
- **网络优化**：使用本地gRPC通信避免网络开销
- **I/O优化**：在密集运行中对临时文件使用ramdisk

#### 瓶颈识别
- **遗传算法**：通常占总运行时间的60-80%
- **策略优化**：根据gRPC vs本地计算占15-30%
- **局部搜索**：解决方案微调占5-15%
- **文件I/O**：对标准实例影响最小

### 常见开发模式

#### 添加新的遗传算子
1. 在`Genetic.cpp`中按照现有交叉/变异模式实现算子
2. 在`Params`类中添加参数控制
3. 在`Individu::split()`中更新适应度评估
4. 在多场景评估之前先用单场景测试

#### 扩展gRPC协议
1. 用新消息字段修改`rpc/irp.proto`
2. 重新生成C++和Python绑定
3. 在`rpc/server.py`中更新服务器实现
4. 在`Individu.cpp`中更新客户端调用

#### 自定义实例创建
1. 遵循`Data/Small/Istanze0105h3/0.dat`中的`.dat`格式
2. 确保客户坐标和需求正确缩放
3. 验证车辆容量和时间范围参数
4. 在复杂场景之前用简单参数设置测试