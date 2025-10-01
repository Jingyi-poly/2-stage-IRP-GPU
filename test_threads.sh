THREADS_PER_PROCESS=1
./irp Data/Small/Istanze0105h3/0.dat -seed 989 -type 38 -veh 1 -stock 100 -demandseed 989 -iter 1 -scenario 1 -control_day_1 1 -threads $THREADS_PER_PROCESS | head -5
