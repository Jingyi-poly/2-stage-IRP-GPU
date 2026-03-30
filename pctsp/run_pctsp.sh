#!/bin/bash
# =============================================================================
# run_pctsp.sh - 运行单个 PCTSP 实例
#
# 用法: ./run_pctsp.sh <instance_file> [time_limit] [output_dir]
#
# 参数:
#   instance_file  - 实例文件路径 (必需)
#   time_limit     - 求解时间限制，单位秒 (可选，默认: 600)
#   output_dir     - 输出目录 (可选，默认: results/当前时间戳)
#
# 示例:
#   ./run_pctsp.sh data/N20ft204.dat
#   ./run_pctsp.sh data/N40ft201.dat 120
#   ./run_pctsp.sh data/N60ft201.dat 300 results/my_experiment
# =============================================================================

set -e  # 遇到错误立即退出

# 获取脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="${SCRIPT_DIR}/src"

# 检查参数
if [ $# -lt 1 ]; then
    echo "用法: $0 <instance_file> [time_limit] [output_dir]"
    echo "示例: $0 data/N20ft204.dat 600"
    exit 1
fi

# 解析参数
INSTANCE_FILE="$1"
TIME_LIMIT="${2:-600}"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
OUTPUT_DIR="${3:-${SCRIPT_DIR}/results/${TIMESTAMP}}"

# 检查实例文件是否存在
if [ ! -f "$INSTANCE_FILE" ]; then
    echo "错误: 实例文件不存在: $INSTANCE_FILE"
    exit 1
fi

# 获取实例名称（不含路径和扩展名）
INSTANCE_NAME=$(basename "$INSTANCE_FILE" .dat)

# 创建输出目录
mkdir -p "$OUTPUT_DIR"

# 日志文件路径
LOG_FILE="${OUTPUT_DIR}/${INSTANCE_NAME}.log"

# 打印运行信息
echo "============================================================"
echo "PCTSP 求解器"
echo "============================================================"
echo "实例文件: $INSTANCE_FILE"
echo "实例名称: $INSTANCE_NAME"
echo "时间限制: ${TIME_LIMIT}s"
echo "输出目录: $OUTPUT_DIR"
echo "日志文件: $LOG_FILE"
echo "============================================================"

# 运行求解器，同时输出到终端和日志文件
echo "开始求解..."
echo ""

# 使用 tee 同时输出到终端和日志文件
cd "$SRC_DIR"
python pctsp.py "$SCRIPT_DIR/$INSTANCE_FILE" -t "$TIME_LIMIT" 2>&1 | tee "$LOG_FILE"

# 检查求解是否成功
if [ ${PIPESTATUS[0]} -eq 0 ]; then
    echo ""
    echo "============================================================"
    echo "求解完成！"
    echo "日志已保存到: $LOG_FILE"
    echo "============================================================"
else
    echo ""
    echo "============================================================"
    echo "求解失败！请检查日志文件: $LOG_FILE"
    echo "============================================================"
    exit 1
fi
