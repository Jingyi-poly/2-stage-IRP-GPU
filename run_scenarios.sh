#!/bin/bash

# 脚本：并行运行不同scenario值的测试
# 使用方法：./run_scenarios.sh

# 设置每个进程使用的线程数
THREADS_PER_PROCESS=${THREADS_PER_PROCESS:-4}

echo "开始并行运行不同scenario值的测试..."
echo "使用每进程线程数: $THREADS_PER_PROCESS"
echo "=================================="

# 基础参数
BASE_CMD="./irp Data/Small/Istanze0105h3/0.dat -seed 989 -type 38 -veh 1 -stock 100 -demandseed 989 -iter 1000 -threads $THREADS_PER_PROCESS"
#  ./irp Data/Small/Istanze0105h3/0.dat -seed 1001 -type 38 -veh 1 -stock 100 -demandseed 2000 -iter 100 -scenario 2 -control_day_1 1

# 运行特定的scenario值：1, 5, 10, 15, 20
scenarios=(1)

# 创建临时目录用于存放日志文件
TEMP_DIR="./parallel_logs"
mkdir -p "$TEMP_DIR"

# 记录开始时间
start_time=$(date +%s)

echo "启动并行任务..."

# 并行运行所有场景
for scenario in "${scenarios[@]}"
do
    echo "启动 scenario $scenario 在后台..."
    
    # 在后台运行命令，将输出重定向到临时文件
    $BASE_CMD -scenario $scenario -control_day_1 1 > "$TEMP_DIR/scenario_${scenario}.log" 2>&1 &
    
    # 记录进程ID
    pids[$scenario]=$!
    echo "scenario $scenario 进程ID: ${pids[$scenario]}"
done

echo ""
echo "所有任务已启动，等待完成..."
echo "=================================="

# 等待所有进程完成
for scenario in "${scenarios[@]}"
do
    if [ -n "${pids[$scenario]}" ]; then
        echo "等待 scenario $scenario (PID: ${pids[$scenario]}) 完成..."
        wait ${pids[$scenario]}
        exit_code=$?
        
        if [ $exit_code -eq 0 ]; then
            echo "✅ scenario $scenario 运行成功"
        else
            echo "❌ scenario $scenario 运行失败 (退出码: $exit_code)"
        fi
        
        # 显示日志文件的前几行
        echo "scenario $scenario 日志摘要:"
        head -5 "$TEMP_DIR/scenario_${scenario}.log"
        echo "----------------------------------------"
    fi
done

# 记录结束时间
end_time=$(date +%s)
duration=$((end_time - start_time))

echo ""
echo "=================================="
echo "所有scenario测试完成！"
echo "总耗时: ${duration} 秒"
echo "日志文件保存在: $TEMP_DIR/"

# 清理临时文件（可选）
# rm -rf "$TEMP_DIR" 