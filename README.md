# 2-stage-IRP-GPU

## 介绍

当前版本为单个scenario的dp向量化。
核心代码的函数名称和公式推导中的符号比较对应，如有疑问欢迎联系我！

## 编译和运行

make

./irp Data/Small/0.dat -seed 1000 -type 38 -veh 1 -stock 200

生成的结果在：Data/Small/tradeoff

## 核心代码和说明

均在 MatrixSolver.cpp

后续会latex进行补充

## 结果验证

-stock 200 时： cost= 3599.1789
-stock 100 时： cost= 3277.3938
