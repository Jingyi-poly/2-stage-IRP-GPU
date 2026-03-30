#!/bin/bash
# =============================================================================
# run.sh - 批量运行所有 PCTSP 实例
#
# 用法: ./run.sh [time_limit] [data_dir] [output_dir]
#
# 参数:
#   time_limit  - 每个实例的求解时间限制，单位秒 (可选，默认: 600)
#   data_dir    - 数据目录 (可选，默认: data)
#   output_dir  - 输出目录 (可选，默认: results/当前时间戳)
#
# 示例:
#   ./run.sh                    # 使用默认参数运行所有实例
#   ./run.sh 120                # 每个实例限时120秒
#   ./run.sh 300 data results/exp1
# =============================================================================

set -e  # 遇到错误立即退出

# 获取脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="${SCRIPT_DIR}/src"

# 解析参数
TIME_LIMIT="${1:-600}"
DATA_DIR="${2:-${SCRIPT_DIR}/data}"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
OUTPUT_DIR="${3:-${SCRIPT_DIR}/results/${TIMESTAMP}}"

# 检查数据目录是否存在
if [ ! -d "$DATA_DIR" ]; then
    echo "错误: 数据目录不存在: $DATA_DIR"
    exit 1
fi

# 获取所有实例文件
INSTANCES=($(ls "$DATA_DIR"/*.dat 2>/dev/null | sort))
TOTAL_INSTANCES=${#INSTANCES[@]}

if [ $TOTAL_INSTANCES -eq 0 ]; then
    echo "错误: 在 $DATA_DIR 中未找到 .dat 文件"
    exit 1
fi

# 创建输出目录
mkdir -p "$OUTPUT_DIR"

# 汇总日志文件
SUMMARY_FILE="${OUTPUT_DIR}/summary.log"

# 打印运行信息
echo "============================================================"
echo "PCTSP 批量求解器"
echo "============================================================"
echo "数据目录: $DATA_DIR"
echo "实例数量: $TOTAL_INSTANCES"
echo "时间限制: ${TIME_LIMIT}s (每个实例)"
echo "输出目录: $OUTPUT_DIR"
echo "汇总文件: $SUMMARY_FILE"
echo "============================================================"
echo ""

# 初始化汇总文件
{
    echo "============================================================"
    echo "PCTSP 批量求解汇总报告"
    echo "============================================================"
    echo "运行时间: $(date)"
    echo "数据目录: $DATA_DIR"
    echo "实例数量: $TOTAL_INSTANCES"
    echo "时间限制: ${TIME_LIMIT}s"
    echo "============================================================"
    echo ""
    printf "%-20s %10s %10s %12s %10s %8s\n" \
        "实例" "节点数" "访问节点" "目标值" "距离" "时间(s)"
    echo "------------------------------------------------------------"
} > "$SUMMARY_FILE"

# 统计变量
SUCCESS_COUNT=0
FAIL_COUNT=0
START_TIME=$(date +%s)

# 遍历所有实例
for i in "${!INSTANCES[@]}"; do
    INSTANCE="${INSTANCES[$i]}"
    INSTANCE_NAME=$(basename "$INSTANCE" .dat)
    CURRENT=$((i + 1))

    echo "[$CURRENT/$TOTAL_INSTANCES] 正在求解: $INSTANCE_NAME"

    # 日志文件路径
    LOG_FILE="${OUTPUT_DIR}/${INSTANCE_NAME}.log"

    # 运行求解器
    cd "$SRC_DIR"
    if python pctsp.py "$INSTANCE" -t "$TIME_LIMIT" > "$LOG_FILE" 2>&1; then
        SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
        STATUS="✓"

        # 从日志中提取关键信息
        NODES=$(grep "Number of nodes:" "$LOG_FILE" | tail -1 | awk '{print $NF}')
        VISITED=$(grep "Nodes visited:" "$LOG_FILE" | awk '{print $3}')
        OBJ=$(grep "Objective value:" "$LOG_FILE" | awk '{print $NF}')
        DIST=$(grep "Total distance traveled:" "$LOG_FILE" | awk '{print $NF}')
        TIME_USED=$(grep "Computation time:" "$LOG_FILE" | awk '{print $3}')

        # 写入汇总
        printf "%-20s %10s %10s %12s %10s %8s\n" \
            "$INSTANCE_NAME" "$NODES" "$VISITED" "$OBJ" "$DIST" "$TIME_USED" >> "$SUMMARY_FILE"

        echo "  $STATUS 完成 - 目标值: $OBJ, 时间: ${TIME_USED}s"
    else
        FAIL_COUNT=$((FAIL_COUNT + 1))
        STATUS="✗"
        printf "%-20s %10s %10s %12s %10s %8s\n" \
            "$INSTANCE_NAME" "-" "-" "FAILED" "-" "-" >> "$SUMMARY_FILE"
        echo "  $STATUS 失败 - 详见日志: $LOG_FILE"
    fi
done

# 计算总运行时间
END_TIME=$(date +%s)
TOTAL_TIME=$((END_TIME - START_TIME))

# 写入汇总统计
{
    echo "------------------------------------------------------------"
    echo ""
    echo "============================================================"
    echo "统计信息"
    echo "============================================================"
    echo "成功: $SUCCESS_COUNT"
    echo "失败: $FAIL_COUNT"
    echo "总计: $TOTAL_INSTANCES"
    echo "总运行时间: ${TOTAL_TIME}s"
    echo "============================================================"
} >> "$SUMMARY_FILE"

# 打印最终统计
echo ""
echo "============================================================"
echo "批量求解完成！"
echo "============================================================"
echo "成功: $SUCCESS_COUNT / $TOTAL_INSTANCES"
echo "失败: $FAIL_COUNT / $TOTAL_INSTANCES"
echo "总运行时间: ${TOTAL_TIME}s"
echo "输出目录: $OUTPUT_DIR"
echo "汇总报告: $SUMMARY_FILE"
echo "============================================================"

# 显示汇总内容
echo ""
echo "汇总报告内容:"
echo ""
cat "$SUMMARY_FILE"
