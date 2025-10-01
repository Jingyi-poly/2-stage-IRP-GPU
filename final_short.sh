#!/bin/bash
#SBATCH --cpus-per-task=1
#SBATCH --mem=10G
#SBATCH --time=2:30:00
#SBATCH --partition=optimum
#SBATCH --array=1-50
#SBATCH --output=Output_norIRP_slurm/arrayjob_%A_%a.out

FOLDERS=( "Data/Small/Istanze001005h3" )

i=1
for folder in "${FOLDERS[@]}"; do

    #echo "Processing folder: $folder"  
    for file in "$folder"/*_1.dat; do	
    #echo "Processing files: $file"  
        	if [ $SLURM_ARRAY_TASK_ID -eq $i ]
    		then
			bash final_submit_short.sh "$file" 2
    		fi
    		(( i = $i +1 ))
    done
done