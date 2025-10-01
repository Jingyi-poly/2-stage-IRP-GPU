import os
import re

def rename_files_in_folder(folder_path, start_num, end_num):
    for num in range(end_num, start_num - 1, -1):
        for filename in os.listdir(folder_path):
            if filename.endswith(f"{num}.dat"):
                # 构造新文件名
                new_filename = re.sub(f"{num}.dat$", f"{num + 1}.dat", filename)
                old_file_path = os.path.join(folder_path, filename)
                new_file_path = os.path.join(folder_path, new_filename)
                # 重命名文件
                os.rename(old_file_path, new_file_path)
                print(f"Renamed '{filename}' to '{new_filename}'")

# 设置文件夹路径
folder_path = 'Small/Istanze001005h3'
# 调用函数，参数为文件夹路径、起始数字和结束数字
rename_files_in_folder(folder_path, 1, 4)
