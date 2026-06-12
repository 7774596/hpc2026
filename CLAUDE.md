# CLAUDE.md — AI 助手工作指南

本文件供 Claude/Cursor 等 AI 编码助手在本仓库工作时参考。

## 项目背景

《高性能计算与并行计算》期末项目（候选题 6）：多节点/多线程作业调度模拟器。
三人小组：A（框架与并行化）、B（调度策略）、C（实验与分析）。
截止 2026-07-01，提交报告 + 超算上的代码与实验数据。

## 常用命令

```bash
make serial    # 构建纯模拟版 build/hpcsim
make mpi       # 构建 MPI+OpenMP 版 build/hpcsim_mpi（需 mpic++）
make test      # 构建并运行单元测试（改动后必须通过）
make demo      # small 配置跑 4 种调度策略
make format    # clang-format 格式化全部源码
```

Windows 本地无 make 时可直接用 g++ 编译（参考 Makefile 中的命令行）；
MPI 相关代码只能在超算/Linux 上验证。

## 架构速览

- `include/hpcsim/types.h`：核心数据结构 `Job`（`allocation` 支持跨节点占核、
  `remaining`/`last_start` 支持时间片抢占），**改动此文件需全员同步**
- `src/core/simulator.cpp`：串行离散事件引擎；事件为 JobArrival / JobFinish /
  TimeSliceExpire，同一时刻先处理释放资源的事件（Finish > Expire > Arrival）
- `include/hpcsim/scheduler.h`：调度策略接口。契约：`pick_next` 只读上下文、
  返回下一个要启动的作业（"放得下"= `cores <= cluster.free_cores()`，分配可跨节点）；
  资源分配、状态机和事件登记由模拟器统一执行；`time_slice() > 0` 表示抢占式策略
- `src/parallel/mpi_runner.cpp`：exec 模式 MPI 主从协议（TAG_JOB / TAG_DONE / TAG_STOP）

## 代码规范

- **C++11（硬性约束）**：部署目标是 CentOS 7 + GCC 4.8.5，禁止使用 C++14/17 特性
  （`std::make_unique`、`std::optional`、`std::clamp`、结构化绑定等）；
  含默认成员初始化器的结构体（Job/Event/Node 等）不能用聚合初始化，
  参考 `make_event` / 逐字段赋值的写法
- 4 空格缩进，行宽 100（见 .clang-format）
- 标识符用英文，注释可用中文；注释解释"为什么"而非"做了什么"
- 新增调度器：在 `src/schedulers/` 新建文件 → 实现 `Scheduler` 接口 →
  在 `scheduler_factory.cpp` 注册 → 在 `tests/test_schedulers.cpp` 添加手算用例
- 跨成员模块的 TODO 用 `TODO(A)` / `TODO(B)` / `TODO(C)` 标注归属

## 注意事项

- 不要改变 `Scheduler` 接口（`pick_next` / `time_slice`）的签名和契约
  （三人代码都依赖它）；确需修改时在 PR 中说明并通知全员
- 不要提交 `build/`、`results/` 下的产物
- 模拟器正确性以 `tests/` 中手算用例为准，性能优化不得改变这些用例的结果
- 报告对应章节的数据来自 `results/summary.csv`，修改指标计算公式前先确认
  `docs/design.md` 中的指标定义
