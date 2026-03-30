#!/bin/bash
# =============================================================================
# compare.sh - 比较 Basic 和 Optimized PCTSP 求解器性能
# 
# 用法: ./compare.sh [time_limit] [data_dir] [output_dir]
#
# 参数:
#   time_limit  - 每个实例的求解时间限制，单位秒 (可选，默认: 60)
#   data_dir    - 数据目录 (可选，默认: data)
#   output_dir  - 输出目录 (可选，默认: results/compare_当前时间戳)
#
# 输出:
#   - 每个求解器的日志文件
#   - comparison.csv - 比较结果CSV文件
#   - comparison.xlsx - 比较结果Excel文件
# =============================================================================

set -e

# 获取脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="${SCRIPT_DIR}/src"

# 解析参数
TIME_LIMIT="${1:-60}"
DATA_DIR_INPUT="${2:-data}"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
OUTPUT_DIR_INPUT="${3:-results/compare_${TIMESTAMP}}"

# 转换为绝对路径
if [[ "$DATA_DIR_INPUT" = /* ]]; then
    DATA_DIR="$DATA_DIR_INPUT"
else
    DATA_DIR="${SCRIPT_DIR}/${DATA_DIR_INPUT}"
fi

if [[ "$OUTPUT_DIR_INPUT" = /* ]]; then
    OUTPUT_DIR="$OUTPUT_DIR_INPUT"
else
    OUTPUT_DIR="${SCRIPT_DIR}/${OUTPUT_DIR_INPUT}"
fi

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
mkdir -p "$OUTPUT_DIR/basic"
mkdir -p "$OUTPUT_DIR/optimized"

# CSV文件路径
CSV_FILE="${OUTPUT_DIR}/comparison.csv"
XLSX_FILE="${OUTPUT_DIR}/comparison.xlsx"

# 打印运行信息
echo "============================================================"
echo "PCTSP 求解器性能对比"
echo "============================================================"
echo "数据目录: $DATA_DIR"
echo "实例数量: $TOTAL_INSTANCES"
echo "时间限制: ${TIME_LIMIT}s (每个实例)"
echo "输出目录: $OUTPUT_DIR"
echo "============================================================"
echo ""

# 初始化CSV文件
echo "Instance,Nodes,Basic_Time,Basic_Obj,Basic_SEC_Count,Opt_Time,Opt_Obj,Speedup" > "$CSV_FILE"

# 统计变量
SUCCESS_COUNT=0
FAIL_COUNT=0
START_TIME=$(date +%s)

# 遍历所有实例
for i in "${!INSTANCES[@]}"; do
    INSTANCE="${INSTANCES[$i]}"
    INSTANCE_NAME=$(basename "$INSTANCE" .dat)
    CURRENT=$((i + 1))
    
    echo "[$CURRENT/$TOTAL_INSTANCES] 正在对比: $INSTANCE_NAME"

    # 日志文件路径（OUTPUT_DIR已是绝对路径）
    BASIC_LOG="${OUTPUT_DIR}/basic/${INSTANCE_NAME}.log"
    OPT_LOG="${OUTPUT_DIR}/optimized/${INSTANCE_NAME}.log"

    # 运行Basic求解器
    cd "$SRC_DIR"
    python pctsp_basic.py "$INSTANCE" -t "$TIME_LIMIT" > "$BASIC_LOG" 2>&1
    BASIC_STATUS=$?

    # 运行Optimized求解器
    python pctsp.py "$INSTANCE" -t "$TIME_LIMIT" > "$OPT_LOG" 2>&1
    OPT_STATUS=$?
    
    if [ $BASIC_STATUS -eq 0 ] && [ $OPT_STATUS -eq 0 ]; then
        SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
        
        # 从日志中提取信息
        NODES=$(grep "Number of nodes:" "$BASIC_LOG" | tail -1 | awk '{print $NF}')
        BASIC_TIME=$(grep "Computation time:" "$BASIC_LOG" | awk '{print $3}')
        BASIC_OBJ=$(grep "Objective value:" "$BASIC_LOG" | awk '{print $NF}')
        BASIC_SEC=$(grep "Pre-added SEC constraints:" "$BASIC_LOG" | awk '{print $NF}')
        
        OPT_TIME=$(grep "Computation time:" "$OPT_LOG" | awk '{print $3}')
        OPT_OBJ=$(grep "Objective value:" "$OPT_LOG" | awk '{print $NF}')
        
        # 计算加速比
        if [ -n "$OPT_TIME" ] && [ -n "$BASIC_TIME" ]; then
            SPEEDUP=$(echo "scale=2; $BASIC_TIME / $OPT_TIME" | bc 2>/dev/null || echo "N/A")
        else
            SPEEDUP="N/A"
        fi
        
        # 写入CSV
        echo "${INSTANCE_NAME},${NODES},${BASIC_TIME},${BASIC_OBJ},${BASIC_SEC},${OPT_TIME},${OPT_OBJ},${SPEEDUP}" >> "$CSV_FILE"
        
        echo "  Basic: ${BASIC_TIME}s, Obj=${BASIC_OBJ} | Opt: ${OPT_TIME}s, Obj=${OPT_OBJ} | Speedup: ${SPEEDUP}x"
    else
        FAIL_COUNT=$((FAIL_COUNT + 1))
        echo "${INSTANCE_NAME},-,FAILED,FAILED,FAILED,FAILED,FAILED,FAILED" >> "$CSV_FILE"
        echo "  ✗ 失败"
    fi
done

# 计算总运行时间
END_TIME=$(date +%s)
TOTAL_TIME=$((END_TIME - START_TIME))

# 生成Excel文件
echo ""
echo "正在生成Excel文件..."
cd "$SCRIPT_DIR"
python src/generate_comparison_xlsx.py "$CSV_FILE" "$XLSX_FILE"

# 打印最终统计
echo ""
echo "============================================================"
echo "对比完成！"
echo "============================================================"
echo "成功: $SUCCESS_COUNT / $TOTAL_INSTANCES"
echo "失败: $FAIL_COUNT / $TOTAL_INSTANCES"
echo "总运行时间: ${TOTAL_TIME}s"
echo "CSV文件: $CSV_FILE"
echo "Excel文件: $XLSX_FILE"
echo "============================================================"

# 显示CSV内容预览
echo ""
echo "结果预览:"
echo ""
head -20 "$CSV_FILE" | column -t -s ','

