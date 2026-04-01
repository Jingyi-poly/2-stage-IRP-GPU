#!/bin/bash
# bench_optimize.sh — 对比"未优化"与"优化后"的 GPU 评估性能
#
# 用法:
#   cd build && bash ../anpy/bench_optimize.sh            # 默认参数
#   cd build && bash ../anpy/bench_optimize.sh -r 3       # 每组跑 3 次取中位数
#   cd build && bash ../anpy/bench_optimize.sh -b 8       # 设置 batch size = 8
#
# 前置条件:
#   build_baseline/hgs_cuda  — 未优化版本
#   build/hgs_cuda           — 优化版本
# ---------------------------------------------------------------
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

BASELINE="$ROOT_DIR/build_baseline/hgs_cuda"
OPTIMIZED="$ROOT_DIR/build/hgs_cuda"
INST_DIR="$ROOT_DIR/Instances/CVRP"
LOG_DIR="$SCRIPT_DIR/logs"
mkdir -p "$LOG_DIR"

REPEATS=1
BATCH_SIZE=16
NTHREADS=4

while getopts "r:b:t:" opt; do
  case $opt in
    r) REPEATS=$OPTARG ;;
    b) BATCH_SIZE=$OPTARG ;;
    t) NTHREADS=$OPTARG ;;
    *) echo "Usage: $0 [-r repeats] [-b batchSize] [-t nthreads]"; exit 1 ;;
  esac
done

# ── 测试矩阵 ──────────────────────────────────────────────────
INSTANCES=("X-n106-k14.vrp" "X-n143-k7.vrp")
SCENARIOS=(1000 10000 50000)

get_iterlim() {
  case $1 in
    1000)  echo 100 ;;
    10000) echo 50 ;;
    50000) echo 10 ;;
    *)     echo 20 ;;
  esac
}

MU=25
INIT_SIZE=$((4 * MU))

# ── 辅助函数 ──────────────────────────────────────────────────
run_median() {
  local bin="$1" vrp="$2" nscen="$3" itlim="$4" extra="$5"
  local mains=() totals=()
  for r in $(seq 1 $REPEATS); do
    out=$("$bin" "$vrp" /dev/null -seed $r -nthreads $NTHREADS \
          -nextrascen "$nscen" -freqPrint 999999 -maxClient -1 \
          -iterLim "$itlim" $extra 2>&1)
    m=$(grep "Main loop time:" <<< "$out" | awk '{print $4}' | tr -d 's')
    t=$(grep "Total time:" <<< "$out" | awk '{print $3}' | tr -d 's')
    mains+=("$m"); totals+=("$t")
  done
  local idx=$(( (REPEATS - 1) / 2 ))
  local med_main=$(printf "%s\n" "${mains[@]}" | sort -g | sed -n "$((idx+1))p")
  local med_total=$(printf "%s\n" "${totals[@]}" | sort -g | sed -n "$((idx+1))p")
  echo "$med_main $med_total"
}

# ── 输出 ──────────────────────────────────────────────────────
OUTFILE="$LOG_DIR/bench_optimize_$(date +%Y%m%d_%H%M%S).txt"

{
GPU_NAME=$(nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null || echo "N/A")
echo "=================================================================="
echo "  性能对比: 未优化 vs 优化后 (Stream + Pinned Memory + Batch)"
echo "  GPU: $GPU_NAME"
echo "  Batch Size: $BATCH_SIZE   Threads: $NTHREADS   Repeats: $REPEATS"
echo "  $(date)"
echo "=================================================================="
echo ""

SEP="--------------------------------------------------------------------------------------------"

# ── 收集全部数据（只跑一次）──────────────────────────────────
declare -a R_INST R_NS R_ITLIM
declare -a R_TOT_B R_ML_B R_TOT_S R_ML_S R_TOT_16 R_ML_16
IDX=0

for inst in "${INSTANCES[@]}"; do
  vrp="$INST_DIR/$inst"
  [ ! -f "$vrp" ] && echo "SKIP: $vrp not found" && continue
  for ns in "${SCENARIOS[@]}"; do
    itlim=$(get_iterlim $ns)
    nscen_arg=$((ns - 1))
    batch_iter=$(( (itlim + BATCH_SIZE - 1) / BATCH_SIZE ))

    echo -ne "  Running $inst  scen=$ns ..." >&2

    read ml_b  tot_b  <<< $(run_median "$BASELINE"  "$vrp" $nscen_arg $itlim "")
    read ml_s  tot_s  <<< $(run_median "$OPTIMIZED" "$vrp" $nscen_arg $itlim "-gpuBatchSize 1")
    read ml_16 tot_16 <<< $(run_median "$OPTIMIZED" "$vrp" $nscen_arg $batch_iter "-gpuBatchSize $BATCH_SIZE")

    R_INST[$IDX]=$inst;  R_NS[$IDX]=$ns;   R_ITLIM[$IDX]=$itlim
    R_TOT_B[$IDX]=$tot_b;  R_ML_B[$IDX]=$ml_b
    R_TOT_S[$IDX]=$tot_s;  R_ML_S[$IDX]=$ml_s
    R_TOT_16[$IDX]=$tot_16; R_ML_16[$IDX]=$ml_16
    IDX=$((IDX + 1))

    echo " done" >&2
  done
done

# ── 表1: Wall-clock 总时间 & 加速比 ─────────────────────────
echo "[ 表1 ] Wall-clock 总时间 (秒) & 加速比"
echo "$SEP"
printf "%-18s %7s %5s | %9s %9s %11s | %7s %9s\n" \
       "Instance" "Scen" "Iter" "Baseline" "Opt(B=1)" "Opt(B=$BATCH_SIZE)" "B=1" "B=$BATCH_SIZE"
echo "$SEP"

prev_inst=""
for i in $(seq 0 $((IDX-1))); do
  inst=${R_INST[$i]}; ns=${R_NS[$i]}; itlim=${R_ITLIM[$i]}
  tot_b=${R_TOT_B[$i]}; tot_s=${R_TOT_S[$i]}; tot_16=${R_TOT_16[$i]}

  [ "$inst" != "$prev_inst" ] && [ -n "$prev_inst" ] && echo ""
  prev_inst=$inst

  sp_s=$(awk  "BEGIN{printf \"%.2f\", $tot_b/$tot_s}")
  sp_16=$(awk "BEGIN{printf \"%.2f\", $tot_b/$tot_16}")

  printf "%-18s %7d %5d | %8.2fs %8.2fs %9.2fs | %6sx %7sx\n" \
         "$inst" "$ns" "$itlim" "$tot_b" "$tot_s" "$tot_16" "$sp_s" "$sp_16"
done

echo ""
echo ""

# ── 表2: 吞吐量 (eval/s) & 提升百分比 ───────────────────────
echo "[ 表2 ] 吞吐量 (eval/s) & 提升百分比"
echo "$SEP"
printf "%-18s %7s | %10s %10s %12s | %8s %10s\n" \
       "Instance" "Scen" "Baseline" "Opt(B=1)" "Opt(B=$BATCH_SIZE)" "B=1" "B=$BATCH_SIZE"
echo "$SEP"

prev_inst=""
for i in $(seq 0 $((IDX-1))); do
  inst=${R_INST[$i]}; ns=${R_NS[$i]}; itlim=${R_ITLIM[$i]}
  tot_b=${R_TOT_B[$i]}; tot_s=${R_TOT_S[$i]}; tot_16=${R_TOT_16[$i]}

  [ "$inst" != "$prev_inst" ] && [ -n "$prev_inst" ] && echo ""
  prev_inst=$inst

  batch_iter=$(( (itlim + BATCH_SIZE - 1) / BATCH_SIZE ))
  init_batches=$(( (INIT_SIZE + BATCH_SIZE - 1) / BATCH_SIZE ))

  evals_b=$((INIT_SIZE + itlim))
  evals_s=$((INIT_SIZE + itlim))
  evals_16=$((init_batches * BATCH_SIZE + batch_iter * BATCH_SIZE))

  tp_b=$(awk  "BEGIN{printf \"%.1f\", $evals_b  / $tot_b}")
  tp_s=$(awk  "BEGIN{printf \"%.1f\", $evals_s  / $tot_s}")
  tp_16=$(awk "BEGIN{printf \"%.1f\", $evals_16 / $tot_16}")

  pct_s=$(awk  "BEGIN{printf \"%+.0f%%\", ($tp_s  / $tp_b - 1) * 100}")
  pct_16=$(awk "BEGIN{printf \"%+.0f%%\", ($tp_16 / $tp_b - 1) * 100}")

  printf "%-18s %7d | %8s/s %8s/s %10s/s | %7s %9s\n" \
         "$inst" "$ns" "$tp_b" "$tp_s" "$tp_16" "$pct_s" "$pct_16"
done

echo ""
echo "$SEP"
echo "注: B=1 仅含 Stream + Pinned Memory 优化"
echo "注: B=$BATCH_SIZE 含全部优化 (Stream + Pinned Memory + Batch)"
echo "注: 吞吐量 = 总评估个体数 / 总时间 (含初始种群 + 主循环)"

} 2>&1 | tee "$OUTFILE"

echo ""
echo "Results saved to: $OUTFILE"
