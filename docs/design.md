# 设计文档

## 1. 总体架构

```
                    ┌──────────────────────────────────────┐
 configs/*.ini ───► │ Config 解析                           │
                    └──────┬───────────────────────────────┘
                           ▼
 ┌─────────────┐    ┌─────────────┐     ┌─────────────────┐
 │ Workload    │───►│ Simulator   │◄───►│ Scheduler        │
 │ 泊松到达流  │    │ 离散事件引擎 │     │ fcfs/sjf/rr/    │
 │ 或 CSV trace│    │ (sim 模式)  │     │ backfill        │
 └─────────────┘    └──────┬──────┘     └─────────────────┘
                           │ 使用                ▲
                           ▼                    │ 只读上下文
                    ┌─────────────┐     ┌──────┴──────────┐
                    │ Cluster     │     │ MetricsCollector │──► results/*.csv
                    │ N 节点 x M 核│     └─────────────────┘
                    └─────────────┘

 exec 模式（MPI + OpenMP）：
   rank 0 (Master)：作业到达 + 调度决策 + 收发消息
   rank 1..N (Worker)：每个 rank 模拟一个节点，busy_kernel 用
   OpenMP num_threads(job.cores) 真实占核执行
```

## 2. 关键设计决策

| 决策 | 理由 |
|---|---|
| 事件三种：Arrival / Finish / TimeSliceExpire，调度在每个事件后触发 | TimeSliceExpire 支撑抢占式 RR：时间片到期→释放资源→扣减 `remaining`→回队尾 |
| 同一时刻按 Finish > Expire > Arrival 排序处理 | 先释放资源再接新作业，避免人为推迟可立即启动的作业 |
| `Scheduler::pick_next` 单步决策、只读上下文；`time_slice()>0` 声明抢占 | 资源分配/状态机统一由模拟器执行，策略实现最简且不会破坏不变量 |
| 核分配跨节点（`Cluster::try_allocate` first-fit 拆分到多节点） | "放得下"简化为 `cores <= free_cores()` 总量判断，Backfilling 影子时刻按全局核数累计即正确 |
| sim 与 exec 共用 Workload/Config | 两种模式下负载完全一致，加速比对比才有意义 |
| 全部代码 C++11 | 部署目标 CentOS 7 自带 GCC 4.8.5，免去 module 依赖，登录节点即可构建 sim 版 |

## 3. 指标定义（报告第五章口径）

设作业 j 的提交/开始/完成时刻为 s_j, b_j, f_j，集群共 C 个核，makespan = max f_j − min s_j：

- 平均等待时间 = mean(b_j − s_j)
- 平均周转时间 = mean(f_j − s_j)
- 吞吐量 = 作业数 / makespan
- 资源利用率 = Σ(busy core-seconds) / (C × makespan)
- 负载均衡度 = 各节点 busy core-seconds 的变异系数（标准差/均值，越小越均衡）

## 4. exec 模式消息协议

```
Master                                Worker (rank r)
  │ ──── TAG_JOB {id, cores, wall_s} ───► │  busy_kernel(wall_s, cores)
  │ ◄─── TAG_DONE {id, elapsed} ───────── │
  │ ──── TAG_STOP（全部完成后）─────────► │  退出
```

时间映射：`真实秒 = 模拟秒 × exec_time_scale`。Master 用墙钟驱动作业到达，
指标仍按模拟时间口径统计，便于与 sim 模式对照。

## 5. 路线图

- [ ] TODO(B)：SJF 防饥饿（aging）与语义确认；RR 时间片扫描实验（quantum 大小 vs 开销）
- [ ] TODO(B)：sjf 手算测试用例
- [ ] TODO(A)：exec 模式接入 Scheduler 接口；Worker 支持节点内多作业并发
- [ ] TODO(A)：增加独立的 runtime 估计字段（研究估计误差对 Backfilling 的影响）
- [ ] TODO(C)：参数扫描脚本、重复实验取均值、加速比/效率曲线
- [ ] TODO(C)：超算部署与 Slurm 模板适配（CentOS 7 / Slurm 20.11；
      模块用 compiler/gnu/10.2.0 + mpi/openmpi/4.1.6，分区名待 sinfo 确认）

## 6. 超算存储路径（报告 5.5 节，部署后填写）

- 代码路径：（待填）
- 实验数据路径：（待填）
