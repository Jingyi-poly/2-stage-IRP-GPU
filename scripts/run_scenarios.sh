#!/bin/bash

# Script:Serial execution of differentscenariovaluestests（optimized version）
# Usage：./run_scenarios.sh
#

# Description：
# - Changed to serial execution，to avoid resource contentionandGPU memory overflow
# - eachscenarioexclusive access to allCPUandGPUresources，optimal performance
# - suitable for large-scale scenarios（scenario >= 100）

# Set threads per process
THREADS_PER_PROCESS=${THREADS_PER_PROCESS:-0}

# Setscenariotimeout（seconds）
SCENARIO_TIMEOUT=${SCENARIO_TIMEOUT:-7200} # default2hours

# Checkpoint resume debug mode（set to1print detailed check info when set）
# Usage: DEBUG_CHECKPOINT=1 ./run_scenarios.sh
DEBUG_CHECKPOINT=${DEBUG_CHECKPOINT:-0}

# Base parameters
# Optimization notes：
# - iter 1000 → 150: Lower early stopping threshold，consecutive150non-improving iterations then stop，faster convergence
# - scenarios: 1000/4000/7000 → 100/300/500: medium scale，estimated30-60minutes to complete
# Force delivery clients (comma-separated, e.g., "1,2,3" or empty for none)
FORCE_DELIVERY_CLIENTS=${FORCE_DELIVERY_CLIENTS:-""}
# Build BASE_CMD with optional force_delivery_clients
BASE_CMD="./irp Data/Small/Istanze0105h3/0.dat -seed 100 -type 38 -veh 1 -stock 100 -demandseed 989 -iter 150 -control_day_1 1 -threads $THREADS_PER_PROCESS"
if [ -n "$FORCE_DELIVERY_CLIENTS" ]; then
  BASE_CMD="$BASE_CMD -force_delivery_clients $FORCE_DELIVERY_CLIENTS"
fi

# Run specificscenariovalues
# Note：Large-scale scenarios recommend serial execution，to avoid resource contention
# Optimized medium-scale scenarios，suitable to complete within2hoursfor testing
scenarios=(300 600)
#scenarios=(500 800 1100)
#scenarios=(15)

# Create log directory
TEMP_DIR="./parallel_logs"
mkdir -p "$TEMP_DIR"

# ========================================
# Dynamic resource detection and parallel strategy decision
# ========================================
ENABLE_PARALLEL=${ENABLE_PARALLEL:-auto} # auto | true | false
MAX_PARALLEL_JOBS=1 # Serial by default
EXECUTION_MODE="Serial（Serial）"

# Check if availablePythonandresource_manager
if [ "$ENABLE_PARALLEL" == "auto" ]; then
 if command -v python3 &> /dev/null && [ -f "./rpc/resource_manager.py" ]; then
 echo "🔍 Detecting system resources，deciding execution strategy..."

 # Callresource_managerdecision
 decision_json=$(python3 ./rpc/resource_manager.py decide \
 --scenarios ${scenarios[@]} \
 --threads $THREADS_PER_PROCESS 2>/dev/null)

 if [ $? -eq 0 ]; then
 # ParseJSONresult
 can_parallel=$(echo "$decision_json" | python3 -c "import sys, json; print(json.load(sys.stdin)['can_parallel'])" 2>/dev/null)
 max_jobs=$(echo "$decision_json" | python3 -c "import sys, json; print(json.load(sys.stdin)['max_parallel_jobs'])" 2>/dev/null)
 reason=$(echo "$decision_json" | python3 -c "import sys, json; print(json.load(sys.stdin)['reason'])" 2>/dev/null)

 if [ "$can_parallel" == "True" ]; then
 MAX_PARALLEL_JOBS=$max_jobs
 EXECUTION_MODE="parallel（Parallel, $MAX_PARALLEL_JOBS jobs）"
 echo "✅ Resources sufficient，using parallel mode: $MAX_PARALLEL_JOBS concurrent jobs"
 echo " Reason: $reason"
 else
 MAX_PARALLEL_JOBS=${max_jobs:-1}
 if [ "$MAX_PARALLEL_JOBS" -gt 1 ]; then
 EXECUTION_MODE="Partial Parallel（Partial Parallel, $MAX_PARALLEL_JOBS jobs）"
 echo "⚠️ Resources limited，using partial parallel: $MAX_PARALLEL_JOBS concurrent jobs"
 else
 EXECUTION_MODE="Serial（Serial）"
 echo "⚠️ Insufficient resources，using serial mode"
 fi
 echo " Reason: $reason"
 fi
 else
 echo "⚠️ Resource detection failed，using serial mode（safe default）"
 fi
 echo ""
 else
 echo "ℹ️ Not detectedresource_manager.py，using serial mode（default）"
 echo ""
 fi
elif [ "$ENABLE_PARALLEL" == "true" ]; then
 # forced parallel，use scenario count as max concurrency
 MAX_PARALLEL_JOBS=${#scenarios[@]}
 EXECUTION_MODE="parallel（Parallel, forced mode）"
fi

# Print configuration info
echo "========================================"
echo " IRP Dynamic Resource Allocation Mode"
echo "========================================"
echo "Execution mode: $EXECUTION_MODE"
echo "Thread config: $THREADS_PER_PROCESS"
echo "Timeout setting: $(($SCENARIO_TIMEOUT / 60)) minutes/scenario"
if [ "$THREADS_PER_PROCESS" -eq 0 ]; then
 echo " - Routing optimization: auto-detect CPU cores（parallel）"
 echo " - Inventory optimization: GPUacceleration（gRPCmode）"
else
 echo " - Routing optimization: auto-detect CPU cores（parallel）"
 echo " - Inventory optimization: $THREADS_PER_PROCESS threads（local）"
fi
echo "Scenariolist: ${scenarios[@]}"
echo "Log directory: $TEMP_DIR/"
echo "========================================"
echo ""

# checkexecutionfile
if [ ! -f "./irp" ]; then
 echo "❌ error: to ./irp executionfile"
 echo "pleasefirst: make"
 exit 1
fi

# gRPCcheck
check_grpc_health() {
 # check
 if netstat -tuln 2>/dev/null | grep -q ":50051 " || ss -tuln 2>/dev/null | grep -q ":50051 "; then
 # ，check（Pythonandgrpcurl）
 if command -v python3 &> /dev/null; then
 # TCPtest
 timeout 2 python3 -c "import socket; s=socket.socket(); s.settimeout(1); s.connect(('localhost', 50051)); s.close()" 2>/dev/null
 if [ $? -eq 0 ]; then
 return 0 # 
 fi
 else
 # Python，check
 return 0 # 
 fi
 fi
 return 1 # 
}

# usingGPUmode，checkgRPCserver
if [ "$THREADS_PER_PROCESS" -eq 0 ]; then
 echo "🔍 checkgRPCserverstatus..."
 if check_grpc_health; then
 echo "✅ gRPCserverRunning（50051）"
 else
 echo "⚠️ warning: gRPCservernotrunno"
 echo " usingGPUmode，pleaseinrun:"
 echo " cd rpc && python3 server.py"
 echo ""
 echo " usingCPUmoderun:"
 echo " export THREADS_PER_PROCESS=4 && ./run_scenarios.sh"
 echo ""
 read -p "continuerun？(y/n) " -n 1 -r
 echo
 if [[ ! $REPLY =~ ^[Yy]$ ]]; then
 exit 1
 fi
 fi
 echo ""
fi

# recordtotalstart time
total_start_time=$(date +%s)

# startresources（）
ENABLE_MONITORING=${ENABLE_MONITORING:-true}
MONITOR_PID=""

if [ "$ENABLE_MONITORING" = "true" ] && [ -f "./monitor_resources.sh" ]; then
 echo "📊 startresources..."
 ./monitor_resources.sh 30 "$TEMP_DIR/resource_monitor.log" "$TEMP_DIR/monitor.pid" &
 MONITOR_PID=$!
 sleep 1
 if ps -p $MONITOR_PID > /dev/null 2>&1; then
 echo "✅ resourcesalreadystart (PID: $MONITOR_PID)"
 echo " logfile: $TEMP_DIR/resource_monitor.log"
 else
 echo "⚠️ resourcesstartFailed"
 MONITOR_PID=""
 fi
 echo ""
fi

# storeeachscenarioandstart time
declare -A exit_codes
declare -A scenario_start_times
declare -A scenario_pids

# checkscenariocompleted（：checkoutputfile）
is_scenario_completed() {
 local scenario=$1
 local log_file="$TEMP_DIR/scenario_${scenario}.log"
 local exit_file="$TEMP_DIR/scenario_${scenario}.exit"

 # ：checkfile
 if [ ! -f "$exit_file" ]; then
 [ "$DEBUG_CHECKPOINT" == "1" ] && echo " [DEBUG] scenario $scenario: filestorein"
 return 1 # notComplete
 fi

 local saved_exit_code=$(cat "$exit_file" 2>/dev/null)
 if [ "$saved_exit_code" != "0" ]; then
 [ "$DEBUG_CHECKPOINT" == "1" ] && echo " [DEBUG] scenario $scenario: 0 ($saved_exit_code)"
 return 1 # executionFailed
 fi

 # ：checkoutputfilestorein
 # from BASE_CMD inparameter
 local instance_file=$(echo "$BASE_CMD" | grep -oP 'Data/\S+\.dat' | head -1)
 local veh=$(echo "$BASE_CMD" | grep -oP '\-veh\s+\K\d+')
 local rou=$(echo "$BASE_CMD" | grep -oP '\-stock\s+\K\d+') # -stock parameter C++ in rou
 local seed=$(echo "$BASE_CMD" | grep -oP '\-seed\s+\K\d+')
 local demandseed=$(echo "$BASE_CMD" | grep -oP '\-demandseed\s+\K\d+')

 # outputfile
 local instance_dir=$(dirname "$instance_file")
 local instance_name=$(basename "$instance_file" .dat)
 local output_file="${instance_dir}/tradeoff/STsol-${instance_name}_veh-${veh}_rou-${rou}_seed-${seed}_demandseed-${demandseed}_scenario-${scenario}"

 if [ "$DEBUG_CHECKPOINT" == "1" ]; then
 echo " [DEBUG] scenario $scenario: checkoutputfile"
 echo " file: $output_file"
 echo " parameter: veh=$veh, rou=$rou, seed=$seed, demandseed=$demandseed"
 fi

 # checkoutputfilestorein
 if [ -f "$output_file" ] && [ -s "$output_file" ]; then
 [ "$DEBUG_CHECKPOINT" == "1" ] && echo " result: filestorein ($(wc -c < "$output_file") ) ✅"
 return 0 # completed：outputfilestorein
 else
 # outputfilestoreinas，Descriptionparameteralreadybeforerundone
 if [ -f "$log_file" ]; then
 echo "⚠️ detectto scenario $scenario as0，outputfilestoreinas" | tee -a "$log_file"
 echo " outputfile: $output_file" | tee -a "$log_file"
 echo " willagainexecutionscenario" | tee -a "$log_file"
 fi
 [ "$DEBUG_CHECKPOINT" == "1" ] && echo " result: filestoreinas ❌"
 return 1 # againexecution
 fi
}

# runindividualscenario
run_scenario() {
 local scenario=$1
 local log_file="$TEMP_DIR/scenario_${scenario}.log"

 # checkcompleted（checkpointresumerun）
 if is_scenario_completed $scenario; then
 echo "⏩ scenario $scenario completed，skipping" >> "$TEMP_DIR/scenario_${scenario}.skip"
 # fromalreadysaveexitfileread
 local exit_code=$(cat "$TEMP_DIR/scenario_${scenario}.exit")
 echo "$exit_code" > "$TEMP_DIR/scenario_${scenario}.exit"
 return 0
 fi

 # recordstart timetofile（asafternoshell）
 echo "$(date +%s)" > "$TEMP_DIR/scenario_${scenario}.start"

 # run（Timeout）
 timeout $SCENARIO_TIMEOUT $BASE_CMD -scenario $scenario > "$log_file" 2>&1
 local exit_code=$?

 # checkTimeout（exit code 124timeout）
 if [ $exit_code -eq 124 ]; then
 echo "⏱️ TIMEOUT after ${SCENARIO_TIMEOUT}s" >> "$log_file"
 fi

 # recordtofile
 echo "$exit_code" > "$TEMP_DIR/scenario_${scenario}.exit"

 return $exit_code
}

total_scenarios=${#scenarios[@]}

# ========================================
# checkcheckpointresumerun
# ========================================
completed_count=0
pending_count=0

echo "🔍 checkcompletedscenario..."
if [ "$DEBUG_CHECKPOINT" == "1" ]; then
 echo " [DEBUG] Checkpoint resume debug modealready"
 echo ""
fi
for scenario in "${scenarios[@]}"; do
 if is_scenario_completed $scenario; then
 completed_count=$((completed_count + 1))
 else
 pending_count=$((pending_count + 1))
 fi
done

if [ $completed_count -gt 0 ]; then
 echo "✅ $completed_count completedscenario，willskipping"
 echo "⏳ run $pending_count scenario"
else
 echo "ℹ️ nocompletedscenario，willrun $total_scenarios scenario"
fi
echo ""

# ========================================
# parallelexecution
# ========================================
if [ "$MAX_PARALLEL_JOBS" -gt 1 ]; then
 echo "🚀 usingparallelExecution mode ($MAX_PARALLEL_JOBS concurrent jobs)"
 echo ""

 running_jobs=0
 scenario_idx=0

 for scenario in "${scenarios[@]}"; do
 # waittojob
 while [ $running_jobs -ge $MAX_PARALLEL_JOBS ]; do
 # waitaftertasksComplete
 wait -n
 running_jobs=$((running_jobs - 1))
 done

 # startnewscenario
 scenario_idx=$((scenario_idx + 1))
 echo "[$scenario_idx/$total_scenarios] start scenario=$scenario (concurrent: $((running_jobs + 1))/$MAX_PARALLEL_JOBS)"

 run_scenario $scenario &
 scenario_pids[$scenario]=$!
 running_jobs=$((running_jobs + 1))
 done

 # waitallscenariotasksComplete（waitscenario_pidsintasks，wait）
 echo ""
 echo "⏳ waitalltasksComplete..."
 for scenario in "${scenarios[@]}"; do
 if [ -n "${scenario_pids[$scenario]}" ]; then
 wait ${scenario_pids[$scenario]} 2>/dev/null
 fi
 done

 echo "✅ alltaskscompleted"
 echo ""

 # 
 for scenario in "${scenarios[@]}"; do
 if [ -f "$TEMP_DIR/scenario_${scenario}.exit" ]; then
 exit_codes[$scenario]=$(cat "$TEMP_DIR/scenario_${scenario}.exit")
 else
 exit_codes[$scenario]=255 # noterror
 fi
 done

# ========================================
# Serialexecution（）
# ========================================
else
 echo "🔄 usingSerialExecution mode"
 echo ""

 current_idx=0

 for scenario in "${scenarios[@]}"
 do
 current_idx=$((current_idx + 1))

 echo "========================================"
 echo " Progress: [$current_idx/$total_scenarios] scenario=$scenario"
 echo "========================================"

 # checkcompleted（checkpointresumerun）
 if is_scenario_completed $scenario; then
 echo "⏩ scenario $scenario completed，skipping"
 echo ""
 # savealready
 exit_codes[$scenario]=0
 continue
 fi

 echo "start time: $(date '+%Y-%m-%d %H:%M:%S')"
 echo ""

 # recordindividualscenariostart time
 scenario_start_time=$(date +%s)

 # run（Timeout，add&number，waitComplete）
 timeout $SCENARIO_TIMEOUT $BASE_CMD -scenario $scenario > "$TEMP_DIR/scenario_${scenario}.log" 2>&1
 exit_code=$?

 # checkTimeout（exit code 124timeout）
 if [ $exit_code -eq 124 ]; then
 echo "⏱️ TIMEOUT after ${SCENARIO_TIMEOUT}s ($(($SCENARIO_TIMEOUT / 60))minutes)" >> "$TEMP_DIR/scenario_${scenario}.log"
 fi

 # saveto
 exit_codes[$scenario]=$exit_code

 # recordindividualscenarioend time
 scenario_end_time=$(date +%s)
 scenario_duration=$((scenario_end_time - scenario_start_time))

 echo ""
 echo "end time: $(date '+%Y-%m-%d %H:%M:%S')"
 echo "elapsed: ${scenario_duration}seconds ($(($scenario_duration / 60))divide$(($scenario_duration % 60))seconds)"

 # checkexecutionresult
 if [ $exit_code -eq 0 ]; then
 echo "status: ✅ scenario $scenario runSuccess"
 elif [ $exit_code -eq 124 ]; then
 echo "status: ⏱️ scenario $scenario Timeout ( $(($SCENARIO_TIMEOUT / 60)) minutes)"
 echo ""
 echo "log (after20execute):"
 tail -n 20 "$TEMP_DIR/scenario_${scenario}.log"
 echo ""
 read -p "continuerunscenario？(y/n) " -n 1 -r
 echo
 if [[ ! $REPLY =~ ^[Yy]$ ]]; then
 echo "❌ inexecution"
 exit 1
 fi
 else
 echo "status: ❌ scenario $scenario runFailed (: $exit_code)"
 echo ""
 echo "errorlog (after20execute):"
 tail -n 20 "$TEMP_DIR/scenario_${scenario}.log"
 echo ""
 read -p "continuerunscenario？(y/n) " -n 1 -r
 echo
 if [[ ! $REPLY =~ ^[Yy]$ ]]; then
 echo "❌ inexecution"
 exit 1
 fi
 fi

 # log（after5execute）
 echo ""
 echo "log (after5execute):"
 tail -n 5 "$TEMP_DIR/scenario_${scenario}.log" | sed 's/^/ | /'
 echo ""

 # afterscenario，divide
 if [ $current_idx -lt $total_scenarios ]; then
 echo "----------------------------------------"
 echo ""
 fi
 done
fi

# stopresources
if [ -n "$MONITOR_PID" ] && ps -p $MONITOR_PID > /dev/null 2>&1; then
 echo "🛑 stopresources..."
 kill $MONITOR_PID 2>/dev/null
 wait $MONITOR_PID 2>/dev/null
 echo "✅ resourcesalreadystop"
 echo ""
fi

# recordtotalend time
total_end_time=$(date +%s)
total_duration=$((total_end_time - total_start_time))

# outputtotal
echo ""
echo "========================================"
echo " Execution completedtotal"
echo "========================================"
echo "Completetime: $(date '+%Y-%m-%d %H:%M:%S')"
echo "Total time: ${total_duration}seconds ($(($total_duration / 60))divide$(($total_duration % 60))seconds)"
echo ""
echo "scenarioelapsed:"
for scenario in "${scenarios[@]}"
do
 log_file="$TEMP_DIR/scenario_${scenario}.log"
 if [ -f "$log_file" ]; then
 # usingsaveSuccess/Failed/Timeout（greplog）
 if [ "${exit_codes[$scenario]}" -eq 0 ]; then
 echo " ✅ scenario $scenario - runSuccess，View logs: $log_file"
 elif [ "${exit_codes[$scenario]}" -eq 124 ]; then
 echo " ⏱️ scenario $scenario - Timeout ($(($SCENARIO_TIMEOUT / 60))minutes)，View logs: $log_file"
 else
 echo " ❌ scenario $scenario - Failed (: ${exit_codes[$scenario]})，View logs: $log_file"
 fi
 else
 echo " ❌ scenario $scenario - logfilestorein"
 fi
done
echo ""
echo "alllogfilesavein: $TEMP_DIR/"
echo "========================================"

# hintView logs
echo ""
echo "💡 hint:"
echo " - viewlog: cat $TEMP_DIR/scenario_<N>.log"
if [ -f "$TEMP_DIR/resource_monitor.log" ]; then
 echo " - viewresources: cat $TEMP_DIR/resource_monitor.log"
fi
echo " - GPUusing: watch -n 1 nvidia-smi"
echo " - : watch -n 1 'ps aux | grep irp'"
echo ""
