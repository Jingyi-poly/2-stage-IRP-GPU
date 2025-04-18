#!/bin/bash
#SBATCH --cpus-per-task=1
#SBATCH --mem=10G
#SBATCH --time=02:20:00
#SBATCH --partition=optimum
#SBATCH --array=1-800
#SBATCH --output=arrayjob_%A_%a.out

FOLDERS=("/home/zhajin/norIRP/Data/Small/Istanze001005h3"
            "/home/zhajin/norIRP/Data/Small/Istanze001005h6"
            "/home/zhajin/norIRP/Data/Small/Istanze0105h3"
            "/home/zhajin/norIRP/Data/Small/Istanze0105h6")

i=1
for folder in "${FOLDERS[@]}"; do

    #echo "Processing folder: $folder"  
    for file in "$folder"/*.dat; do	
    #echo "Processing files: $file"  
#    	for N in {1..10}; do
        	if [ $SLURM_ARRAY_TASK_ID -eq $i ]
    		then
            		./irp "$file" -seed $1 -type 38 -veh 2 -stock 100
    		fi
    		(( i = $i +1 ))	
    done
done

sleep 60
