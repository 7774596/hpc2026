# CONTRIBUTING — 开发协作规范

三人小组协作约定，目标是 7/1 前稳定交付，避免合并冲突和口径不一致。

## 分支模型

- `main`：稳定分支，保证随时可 `make serial && make test` 通过；只通过 PR 合入
- 功能分支：`feat/<area>-<desc>`，如 `feat/sched-backfill`、`feat/mpi-multi-job`
- 修复分支：`fix/<area>-<desc>`，如 `fix/core-event-order`
- 实验/报告材料分支：`exp/<desc>`，如 `exp/scaling-1m-jobs`

## 模块归属（动谁的模块先打招呼）

| 目录 | 负责人 |
|---|---|
| `src/core` `src/parallel` `include/hpcsim` | A |
| `src/schedulers` `tests` | B |
| `scripts` `configs` `results` | C |

`include/hpcsim/types.h` 和 `scheduler.h` 是公共契约，改动需三人都知情。
另：代码必须保持 C++11 兼容（服务器是 GCC 4.8.5），详见 CLAUDE.md。

## Commit 规范

格式：`type(scope): 摘要`（摘要中英文均可，不超过 50 字）

- type：`feat` `fix` `perf` `refactor` `test` `docs` `chore` `exp`
- scope：`core` `sched` `mpi` `omp` `scripts` `configs` `docs`

示例：

```
feat(sched): 实现 EASY Backfilling 节点级预留校验
fix(core): 同一时刻 Finish 事件先于 Arrival 处理
exp(scripts): 增加 1e6 作业规模扫描脚本
```

## PR 与评审

1. PR 描述写清：做了什么、为什么、如何验证（贴 `make test` 输出或实验数据）
2. 至少 1 名其他成员 approve 后合入；涉及公共契约（types.h / scheduler.h）需 2 人
3. 合入前 rebase 到最新 main，保持线性历史（`git pull --rebase`）

## 代码与测试要求

- 提交前：`make format && make test` 全部通过
- 新调度策略必须附带手算用例（见 `tests/test_schedulers.cpp` 的写法）
- 性能数据进 `results/`（git 忽略），但实验用的 config 和脚本必须入库，保证可复现
- 超算上的存储路径（报告 5.5 节要求）统一记录在 `docs/design.md` 末尾

## 报告写作分工（与代码模块对应）

| 章节 | 负责人 |
|---|---|
| 二、背景与意义；四.1 算法原理；四.3 环境 | B |
| 四.2 并行设计；四.4 实现过程；全文统稿 | A |
| 三、目标任务；五、实验分析；六、总结 | C |
