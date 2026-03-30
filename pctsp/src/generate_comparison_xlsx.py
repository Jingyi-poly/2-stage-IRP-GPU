"""
生成 PCTSP 求解器性能对比的 Excel 文件

用法: python generate_comparison_xlsx.py <input_csv> <output_xlsx>
"""

import sys
import csv
from typing import List, Dict, Any

try:
    import openpyxl
    from openpyxl.styles import Font, Alignment, Border, Side, PatternFill
    from openpyxl.utils import get_column_letter
    HAS_OPENPYXL = True
except ImportError:
    HAS_OPENPYXL = False

try:
    import pandas as pd
    HAS_PANDAS = True
except ImportError:
    HAS_PANDAS = False


def create_xlsx_with_openpyxl(csv_path: str, xlsx_path: str) -> None:
    """使用 openpyxl 创建 Excel 文件"""
    wb = openpyxl.Workbook()
    ws = wb.active
    ws.title = "Performance Comparison"
    
    # 定义样式
    header_font = Font(bold=True, color="FFFFFF")
    header_fill = PatternFill(start_color="4472C4", end_color="4472C4", fill_type="solid")
    header_alignment = Alignment(horizontal="center", vertical="center")
    thin_border = Border(
        left=Side(style='thin'),
        right=Side(style='thin'),
        top=Side(style='thin'),
        bottom=Side(style='thin')
    )
    
    # 读取CSV数据
    with open(csv_path, 'r', encoding='utf-8') as f:
        reader = csv.reader(f)
        rows = list(reader)
    
    # 写入数据
    for row_idx, row in enumerate(rows, 1):
        for col_idx, value in enumerate(row, 1):
            cell = ws.cell(row=row_idx, column=col_idx, value=value)
            cell.border = thin_border
            cell.alignment = Alignment(horizontal="center")
            
            # 尝试转换为数字
            if row_idx > 1:
                try:
                    cell.value = float(value)
                except (ValueError, TypeError):
                    pass
            
            # 表头样式
            if row_idx == 1:
                cell.font = header_font
                cell.fill = header_fill
                cell.alignment = header_alignment
    
    # 自动调整列宽
    for col_idx in range(1, len(rows[0]) + 1):
        column_letter = get_column_letter(col_idx)
        max_length = max(len(str(row[col_idx - 1])) for row in rows) + 2
        ws.column_dimensions[column_letter].width = max(max_length, 12)
    
    # 添加汇总信息
    summary_row = len(rows) + 3
    ws.cell(row=summary_row, column=1, value="统计信息").font = Font(bold=True)
    
    # 计算平均值（跳过表头和失败行）
    data_rows = [r for r in rows[1:] if r[2] != 'FAILED']
    if data_rows:
        avg_basic_time = sum(float(r[2]) for r in data_rows) / len(data_rows)
        avg_opt_time = sum(float(r[5]) for r in data_rows) / len(data_rows)
        avg_speedup = sum(float(r[7]) for r in data_rows if r[7] != 'N/A') / len(data_rows)
        
        ws.cell(row=summary_row + 1, column=1, value="实例数量")
        ws.cell(row=summary_row + 1, column=2, value=len(data_rows))
        
        ws.cell(row=summary_row + 2, column=1, value="Basic平均时间(s)")
        ws.cell(row=summary_row + 2, column=2, value=round(avg_basic_time, 4))
        
        ws.cell(row=summary_row + 3, column=1, value="Optimized平均时间(s)")
        ws.cell(row=summary_row + 3, column=2, value=round(avg_opt_time, 4))
        
        ws.cell(row=summary_row + 4, column=1, value="平均加速比")
        ws.cell(row=summary_row + 4, column=2, value=round(avg_speedup, 2))
    
    wb.save(xlsx_path)
    print(f"Excel文件已生成: {xlsx_path}")


def create_xlsx_with_pandas(csv_path: str, xlsx_path: str) -> None:
    """使用 pandas 创建 Excel 文件"""
    df = pd.read_csv(csv_path)
    
    with pd.ExcelWriter(xlsx_path, engine='openpyxl') as writer:
        df.to_excel(writer, sheet_name='Performance Comparison', index=False)
        
        # 添加汇总sheet
        summary_data = {
            '指标': ['实例数量', 'Basic平均时间(s)', 'Optimized平均时间(s)', '平均加速比'],
            '值': [
                len(df[df['Basic_Time'] != 'FAILED']),
                df[df['Basic_Time'] != 'FAILED']['Basic_Time'].astype(float).mean(),
                df[df['Opt_Time'] != 'FAILED']['Opt_Time'].astype(float).mean(),
                df[df['Speedup'] != 'N/A']['Speedup'].astype(float).mean()
            ]
        }
        summary_df = pd.DataFrame(summary_data)
        summary_df.to_excel(writer, sheet_name='Summary', index=False)
    
    print(f"Excel文件已生成: {xlsx_path}")


def main():
    if len(sys.argv) < 3:
        print("用法: python generate_comparison_xlsx.py <input_csv> <output_xlsx>")
        sys.exit(1)
    
    csv_path = sys.argv[1]
    xlsx_path = sys.argv[2]
    
    if HAS_OPENPYXL:
        create_xlsx_with_openpyxl(csv_path, xlsx_path)
    elif HAS_PANDAS:
        create_xlsx_with_pandas(csv_path, xlsx_path)
    else:
        print("警告: 未安装 openpyxl 或 pandas，无法生成 Excel 文件")
        print("请运行: pip install openpyxl")
        sys.exit(1)


if __name__ == "__main__":
    main()

