import pandas as pd

# 读取 CSV 文件
file_path = './output/202502/test01/sim_results.csv'
#file_path = './output/test01/sim_results.csv'
df = pd.read_csv(file_path)

# 移除可能包含的 Timestamp 列及其对应的数据行
if 'Timestamp' in df.columns:
    df = df.drop(columns='Timestamp')

# 将包含百分比的列转换为浮点数
percentage_columns = ['PacketLossRate', 'BLER']
for col in percentage_columns:
    df[col] = df[col].str.rstrip('%').astype('float') 


# 按照指定列进行分组，并计算平均值
grouped_columns = ['gNbNum', 'UeNum', 'gnbPower', 'dlOnly', 'allocate', 'scheduler','udpPacketSizeUl']
result = df.groupby(grouped_columns)[['Throughput', 'Delay', 'PacketLossRate', 'BLER','MCS','RB']].mean()

# 对所有数值列保留三位小数
numeric_columns = result.select_dtypes(include=['number']).columns
result[numeric_columns] = result[numeric_columns].round(3)

# 重置索引
result = result.reset_index()

# 当 gNbNum 等于 7 时，将 UeNum 列的值乘 3
result.loc[result['gNbNum'] == 7, 'UeNum'] = result.loc[result['gNbNum'] == 7, 'UeNum'] * 3

# 定义映射字典
dlOnly_mapping = {1: 'DL', 0: 'UL'}
allocate_mapping = {0: 'BW', 1: 'USED'}
scheduler_mapping = {0: 'RR', 1: 'PF', 2: 'DP'}
# 根据映射字典替换值
result['dlOnly'] = result['dlOnly'].map(dlOnly_mapping)
result['allocate'] = result['allocate'].map(allocate_mapping)
result['scheduler'] = result['scheduler'].map(scheduler_mapping)

# 输出结果到 CSV 文件
output_file_path = './output/202502/test01/processed_sim_results.csv'
#output_file_path = './output/test01/processed_sim_results.csv'
result.to_csv(output_file_path, index=False)

print(result)