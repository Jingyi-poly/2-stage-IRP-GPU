
import os

# 设置目标文件夹路径
directory = '/home/zhajin/IRP-nov28/'

# 遍历目录中的文件
for filename in os.listdir(directory):
    if filename.startswith("arrayjob"):
        filepath = os.path.join(directory, filename)
        os.remove(filepath)
        print(f"Deleted {filepath}")
