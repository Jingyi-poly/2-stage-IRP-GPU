#!/bin/bash

for Seed in {11,4,900,21,30,70,120,80,1200,12000}; do
#echo "Processing folder: $folder"  
	./irp $1 -seed "$Seed" -type 38 -veh $2 -stock 0
done