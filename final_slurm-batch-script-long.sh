#!/bin/bash
#SBATCH --cpus-per-task=1
#SBATCH --mem=10G
#SBATCH --time=01:30:00
#SBATCH --partition=optimum
#SBATCH --array=1-100
#SBATCH --output=Output_norIRP_slurm/arrayjob_%A_%a.out

FOLDERS=( "/home/zhajin/norIRP/Data/Big/Istanze0105")

i=1
for folder in "${FOLDERS[@]}"; do
    for file in "$folder"/*_4.dat; do 
        for stock in {1..100}; do
            if [ $SLURM_ARRAY_TASK_ID -eq $i ]; then
                ./irp "$file" -seed $1 -type 38 -veh 5 -stock $stock
            fi
            (( i = $i + 1 ))
        done
    done
done
