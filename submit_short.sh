#!/bin/bash

for Seed in {200..300}; do
#echo "Processing folder: $folder"  
	sbatch slurm-batch-script-short.sh "$Seed" 
done
sleep 60
