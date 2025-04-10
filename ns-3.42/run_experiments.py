import subprocess
import random
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
    "--scheduler": [0, 1, 2] # 0: RR; 1: PF; 2: DP
}

# 结果文件配置
OUTPUT_CSV = Path("./output/test01/sim_results.csv")


def init_csv():
    """智能初始化CSV文件"""
    if not OUTPUT_CSV.exists():
        with OUTPUT_CSV.open('w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(['gNbNum', 'UeNum', 'gnbPower', 'dlOnly', 'allocate', 'scheduler', 'Throughput', 'Delay', 'PacketLossRate', 'BLER'])


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
    cmd = f'./ns3 run "contrib/nr/examples/test02.cc {args}"'
    print(f"运行: {cmd}")

    # 执行命令
    result = subprocess.run(
        cmd,
        shell=True,
        capture_output=True,
        text=True
    )

    try:
        throughput, delay, loss, bler = parse_output(result.stdout)
    except ValueError as e:
        print(f"参数 {params} 失败: {str(e)}")
        return

    with OUTPUT_CSV.open('a', newline='') as f:
        writer = csv.writer(f)
        # 在写入数据时添加时间戳
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        base = [params['--gNbNum'], params['--ueNum'], params['--gnbPower'], params['--dlOnly'], params['--allocate'], params['--scheduler'],
                f"{throughput:.3f}", f"{delay:.3f}", f"{loss:.3f}%", f"{bler:.3f}%"]

        writer.writerow(base + ['Timestamp', timestamp])


def generate_random_params():
    """生成随机参数组合"""
    return {
        "--gNbNum": random.choice(PARAM_RANGES["--gNbNum"]),
        "--ueNum": random.choice(PARAM_RANGES["--ueNum"]),
        "--gnbPower": random.choice(PARAM_RANGES["--gnbPower"]),
        "--dlOnly": random.choice(PARAM_RANGES["--dlOnly"]),
        "--allocate": random.choice(PARAM_RANGES["--allocate"]),
        "--scheduler": random.choice(PARAM_RANGES["--scheduler"]),

    }


if __name__ == "__main__":
    init_csv()

    # 运行3次随机配置
    for _ in range(20):
        params = generate_random_params()
        try:
            run_simulation(params)
        except subprocess.CalledProcessError as e:
            print(f"运行失败: {e.cmd}")
            print(f"错误输出: {e.stderr}")