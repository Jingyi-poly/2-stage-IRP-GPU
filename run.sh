#FOLDERS=("/home/zhajin/IRP-nov28/Data/Small/Istanze001005h3"  "/home/zhajin/IRP-nov28/Data/Small/Istanze001005h6"
        #    "/home/zhajin/IRP-nov28/Data/Small/Istanze0105h3" "/home/zhajin/IRP-nov28/Data/Small/Istanze0105h6")
FOLDERS=("/home/zhajin/IRP-nov28/Data/Big/Istanze0105" "/home/zhajin/IRP-nov28/Data/Big/Istanze001005")

i=1
command_count=0 # 初始化命令计数器
for folder in "${FOLDERS[@]}"; do
    for file in "$folder"/*.dat; do
            let command_count+=1
            let command_count+=1
            let command_count+=1
            for seed in {0..6}; do
            	#./irp "$file" -seed $seed -type 38 -veh 2 -stock 100
                let command_count+=1
            done
    
        (( i = $i +1 ))
    done
done

echo "Total commands executed: $command_count" 

