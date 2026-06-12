# hpc2026 — 多节点/多线程作业调度模拟器

《高性能计算与并行计算》期末实践项目（候选题 6）。

构建一个 HPC 作业调度系统仿真器：模拟多核/多节点集群上的作业到达、排队、调度与执行，
支持 FCFS / SJF / Round-Robin / EASY Backfilling 四种调度策略对比，
并通过 MPI + OpenMP 在真实多节点环境中执行合成负载、评估并行加速效果。

## 功能

- **sim 模式**（串行离散事件模拟）：纯模拟，速度快，用于调度策略指标对比
- **exec 模式**（MPI + OpenMP 真实执行）：rank 0 为调度主控，其余 rank 各模拟一个计算节点，
  作业以多线程合成计算负载真实运行
- 作业可跨节点分配核数；RR 为真抢占（时间片到期回队尾，`rr_quantum` 可配）
- 任务动态生成（泊松到达流，运行时长/核数分布可配置），也支持从 CSV trace 加载
- 评估指标：平均等待时间、平均周转时间、吞吐量、资源利用率、负载均衡度

代码统一使用 **C++11**（兼容 CentOS 7 自带 GCC 4.8.5，零依赖直接部署）。

## 目录结构

```
hpc2026/
├── include/hpcsim/      # 公共头文件（types.h 为核心数据结构）
├── src/
│   ├── core/            # 集群模型、事件引擎、模拟器、负载生成、指标、配置
│   ├── schedulers/      # 四种调度策略 + 工厂（成员 B 负责）
│   └── parallel/        # OpenMP 合成负载内核 + MPI 主从执行器（成员 A 负责）
├── tests/               # 单元测试（手算用例核对调度正确性）
├── configs/             # 实验配置（small 调试用 / medium 实验用）
├── scripts/             # 实验脚本、绘图、Slurm 模板（成员 C 负责）
├── results/             # 实验输出（git 忽略）
└── docs/                # 设计文档
```

## 快速开始

```bash
# 本地 / 超算登录节点
make serial          # 构建 build/hpcsim（无 MPI 依赖）
make test            # 运行单元测试
make demo            # small 配置依次跑 4 种调度策略

# 单次运行
./build/hpcsim --config configs/small.ini --scheduler backfill

# 批量实验（结果追加到 results/summary.csv）
bash scripts/run_experiments.sh configs/medium.ini
python scripts/plot_results.py

# exec 模式（超算计算节点，需 MPI）
make mpi
mpirun -np 5 build/hpcsim_mpi --config configs/small.ini --mode exec
# 或提交 Slurm：sbatch scripts/slurm_job.sh
```

Windows 本地（MinGW，无 make）：双击或运行 `verify.bat`，
它会静态链接构建、跑单元测试并演示 4 种策略，输出在 `verify_log.txt`。

## 配置说明

| 键 | 含义 | 默认 |
|---|---|---|
| `nodes` / `cores_per_node` | 集群规模 | 4 / 8 |
| `num_jobs` | 作业数量 | 1000 |
| `arrival_rate` | 泊松到达率（作业/模拟秒） | 1.0 |
| `runtime_dist` | 运行时长分布 `uniform` 或 `exponential` | uniform |
| `runtime_min/max/mean` | 时长分布参数（秒） | 10 / 600 / 120 |
| `cores_min/max` | 作业核数范围 | 1 / 8 |
| `seed` | 随机种子 | 42 |
| `scheduler` | `fcfs` `sjf` `rr` `backfill` | fcfs |
| `rr_quantum` | RR 时间片（模拟秒） | 50 |
| `exec_time_scale` | exec 模式时间缩放（模拟秒×scale=真实秒） | 0.01 |
| `trace` | 可选：CSV trace 路径（id,submit,runtime,cores） | 无 |

## 部署到超算（CentOS 7 / Slurm）

```bash
# 1. 上传（在本机执行；服务器路径按需调整）
scp -r hpc2026 eduhpc1@<登录节点>:/public/home/eduhpc1/

# 2. 登录节点上构建与自检（系统 GCC 4.8.5 即可）
cd ~/hpc2026 && make serial && make test && make demo

# 3. exec 模式需要 MPI（模块名为本校超算实际环境）
module load compiler/gnu/10.2.0 mpi/openmpi/4.1.6
make mpi
sbatch scripts/slurm_job.sh     # 提交前用 sinfo 确认分区名并改脚本
```

## 团队分工

| 成员 | 职责 | 主要目录 |
|---|---|---|
| A（组长） | 框架、事件引擎、MPI/OpenMP 并行化、集成 | `src/core` `src/parallel` |
| B | 调度策略实现与正确性验证 | `src/schedulers` `tests` |
| C | 实验、数据采集、绘图、超算部署 | `scripts` `configs` `results` |

开发流程与代码规范见 [CONTRIBUTING.md](CONTRIBUTING.md)，架构设计见 [docs/design.md](docs/design.md)。

## 当前限制（按优先级迭代）

1. exec 模式下每个 Worker 同时只执行一个作业，且 Master 端暂为 FCFS 派发（TODO(A)）
2. `runtime` 兼作用户估计值（完美估计假设），估计误差研究需增加独立字段
3. 跨节点分配未计入跨节点通信开销，可作为报告中的模型简化说明
