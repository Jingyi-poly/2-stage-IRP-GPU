#!/bin/bash
# IRP Solver Run Script

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_ROOT"

# Default parameters
INSTANCE="${1:-Data/Small/Istanze0105h3/abs1n10_1.dat}"
SCENARIOS="${2:-1}"
THREADS="${3:-4}"
TIME="${4:-60}"
SEED="${5:-42}"

echo "Running IRP solver..."
echo "  Instance: $INSTANCE"
echo "  Scenarios: $SCENARIOS"
echo "  Threads: $THREADS"
echo "  Time limit: $TIME seconds"
echo "  Seed: $SEED"
echo ""

./irp "$INSTANCE" -seed $SEED -type 38 -veh 1 -scenario $SCENARIOS -threads $THREADS -t $TIME
