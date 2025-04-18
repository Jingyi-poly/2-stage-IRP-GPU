import os

def delete_file(file_path):
    os.remove(file_path)  # 删除文件

def delete_files_in_directory(directory):
    for root, dirs, files in os.walk(directory):
        for file in files:
            if file.startswith("STsol"):
                file_path = os.path.join(root, file)
                delete_file(file_path)
                print(f"Deleted {file_path}")

# 设置需要遍历的目标文件夹路径
target_directory = '/home/zhajin/IRP-nov28/'  # 替换为您的目标文件夹路径

# 删除所有匹配文件
delete_files_in_directory(target_directory)
