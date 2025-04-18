#!/bin/bash
#SBATCH --cpus-per-task=1
#SBATCH --mem=250G
#SBATCH --time=12:00:00
#SBATCH --partition=optimum
#SBATCH --array=1-22
#SBATCH --output=Output_norIRP_slurm/arrayjob_%A_%a.out

# Define an array of commands32-11+1
commands=(
"./irp Data/Big/Istanze0105/abs2n200_4.dat -seed $1 -type 38 -veh 4 -stock 10000000"
"./irp Data/Big/Istanze0105/abs2n200_5.dat -seed $1 -type 38 -veh 5 -stock 10000000"
"./irp Data/Big/Istanze0105/abs6n200_3.dat -seed $1 -type 38 -veh 3 -stock 10000000"
"./irp Data/Big/Istanze001005/abs1n100_5.dat -seed $1 -type 38 -veh 5 -stock 10000000"
"./irp Data/Big/Istanze001005/abs1n200_5.dat -seed $1 -type 38 -veh 5 -stock 10000000"
"./irp Data/Big/Istanze001005/abs2n100_5.dat -seed $1 -type 38 -veh 5 -stock 10000000"
"./irp Data/Big/Istanze001005/abs2n200_5.dat -seed $1 -type 38 -veh 5 -stock 10000000"
"./irp Data/Big/Istanze001005/abs3n200_3.dat -seed $1 -type 38 -veh 3 -stock 10000000"
"./irp Data/Big/Istanze001005/abs3n200_5.dat -seed $1 -type 38 -veh 5 -stock 10000000"
"./irp Data/Big/Istanze001005/abs4n200_5.dat -seed $1 -type 38 -veh 5 -stock 10000000"
"./irp Data/Big/Istanze001005/abs5n200_5.dat -seed $1 -type 38 -veh 5 -stock 10000000"
"./irp Data/Big/Istanze001005/abs5n200_5.dat -seed $1 -type 38 -veh 5 -stock 10000000"
"./irp Data/Big/Istanze001005/abs5n200_4.dat -seed $1 -type 38 -veh 4 -stock 10000000"
"./irp Data/Big/Istanze001005/abs6n200_5.dat -seed $1 -type 38 -veh 5 -stock 10000000"
"./irp Data/Big/Istanze001005/abs6n200_4.dat -seed $1 -type 38 -veh 4 -stock 10000000"
"./irp Data/Big/Istanze001005/abs7n200_5.dat -seed $1 -type 38 -veh 5 -stock 10000000"
"./irp Data/Big/Istanze001005/abs7n200_4.dat -seed $1 -type 38 -veh 4 -stock 10000000"
"./irp Data/Big/Istanze001005/abs7n200_2.dat -seed $1 -type 38 -veh 2 -stock 10000000"
"./irp Data/Big/Istanze001005/abs8n200_4.dat -seed $1 -type 38 -veh 4 -stock 10000000"
"./irp Data/Big/Istanze001005/abs9n200_5.dat -seed $1 -type 38 -veh 5 -stock 10000000"
"./irp Data/Big/Istanze001005/abs9n200_4.dat -seed $1 -type 38 -veh 4 -stock 10000000"
"./irp Data/Big/Istanze001005/abs10n200_5.dat -seed $1 -type 38 -veh 5 -stock 10000000"
)

# Execute the command corresponding to the SLURM_ARRAY_TASK_ID
${commands[$SLURM_ARRAY_TASK_ID-1]}
