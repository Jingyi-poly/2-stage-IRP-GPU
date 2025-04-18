#!/bin/bash

for Seed in {0,11,15,70,240,500,1010,1536}; do
	sbatch final_slurm-batch-script-long.sh "$Seed" 
done