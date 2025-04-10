import subprocess
import itertools
import csv
from pathlib import Path
from datetime import datetime

# 参数配置范围
PARAM_RANGES = {
    "--gNbNum": [1, 7],          # 基站数量配置
    "--ueNum": [4, 12, 20],  # range(5, 31, 5)用户数量 5-30，步长5
    "--gnbPower": [50],   # 发射功率 (W)
    "--dlOnly": [0, 1], # 1:只有下行；0：只有上行
    "--allocate": [0, 1], # 0: ALL; 1: USED
    "--scheduler": [0, 1, 2], # 0: RR; 1: PF; 2: DP
    "--udpPacketSizeUl":[50, 100,200]
}

# 结果文件配置
OUTPUT_CSV = Path("./output/202502/test01/sim_results.csv")

def init_csv():
    """智能初始化CSV文件"""
    if not OUTPUT_CSV.exists():
        with OUTPUT_CSV.open('w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(['gNbNum', 'UeNum', 'gnbPower', 'dlOnly', 'allocate', 'scheduler', 'udpPacketSizeUl', 'Throughput', 'Delay', 'PacketLossRate', 'BLER','MCS','RB', 'Timestamp'])

def parse_output(output):
    """解析带有标记的输出"""
    for line in output.split('\n'):
        if "###RESULTS###" in line:
            data = line.split("###RESULTS###")[1].split("###END###")[0]
            return [float(x) for x in data.split(',')]
    raise ValueError("未找到有效结果数据")

def run_simulation(params):
    """执行单次仿真"""
    # 构建命令
    args = " ".join([f"{k}={v}" for k, v in params.items()])
    cmd = f'./ns3 run "contrib/nr/examples/test01.cc {args}"'
    print(f"运行: {cmd}")

    # 执行命令
    result = subprocess.run(
        cmd,
        shell=True,
        capture_output=True,
        text=True
    )

    try:
        throughput, delay, loss, bler, mcs, rb = parse_output(result.stdout)
    except ValueError as e:
        print(f"参数 {params} 失败: {str(e)}")
        return

    with OUTPUT_CSV.open('a', newline='') as f:
        writer = csv.writer(f)
        # 在写入数据时添加时间戳
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        base = [params['--gNbNum'], params['--ueNum'], params['--gnbPower'], params['--dlOnly'], params['--allocate'], params['--scheduler'],params['--udpPacketSizeUl'],
                f"{throughput:.3f}", f"{delay:.3f}", f"{loss:.3f}%", f"{bler:.3f}%",f"{mcs:.3f}",f"{rb:.3f}"]

        writer.writerow(base + [timestamp])

def generate_all_params(specified_params=None):
    """生成所有可能的参数组合"""
    if specified_params is None:
        specified_params = {}
    unspecified_params = {k: v for k, v in PARAM_RANGES.items() if k not in specified_params}
    param_names = list(unspecified_params.keys())
    param_values = list(unspecified_params.values())
    all_combinations = itertools.product(*param_values)

    for combination in all_combinations:
        params = specified_params.copy()
        for name, value in zip(param_names, combination):
            params[name] = value
        # 添加条件判断
        dlOnly = params.get('--dlOnly')
        allocate = params.get('--allocate')
        if dlOnly == 0 and allocate != 1:
            continue  # 当dlOnly为0且allocate不为1时，跳过该组合

        yield params

if __name__ == "__main__":
    init_csv()

    # 指定部分参数
    specified_params = {
        "--gNbNum": 1,
        #"--ueNum": 20,
        "--gnbPower": 50,
        #"--dlOnly":0,
        #"--allocate":1,
        "--udpPacketSizeUl":50000
    }

    # 生成所有可能的参数组合
    all_params = generate_all_params(specified_params)


    # 运行所有配置，每个配置运行n次
    for params in all_params:   
        for _ in range(5):     
            try:
                run_simulation(params)
            except subprocess.CalledProcessError as e:
                print(f"运行失败: {e.cmd}")
                print(f"错误输出: {e.stderr}")