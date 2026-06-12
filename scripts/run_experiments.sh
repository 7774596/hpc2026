#!/usr/bin/env bash
# 批量实验脚本：对每种调度策略跑一遍模拟，结果汇总到 results/summary.csv
#
# 用法：bash scripts/run_experiments.sh [config]
#
# TODO(C): 1) 扩展参数扫描：num_jobs（1e4/1e5/1e6）、arrival_rate、nodes；
#          2) 每组参数重复 >=3 次（不同 seed）取均值并记录方差；
#          3) exec 模式下扫描 MPI 进程数/OpenMP 线程数，采集加速比数据。
set -euo pipefail
cd "$(dirname "$0")/.."

BIN=${BIN:-build/hpcsim}
CONFIG=${1:-configs/small.ini}

if [[ ! -x "$BIN" ]]; then
    echo "binary $BIN not found, run 'make serial' first" >&2
    exit 1
fi

for sched in fcfs sjf rr backfill; do
    echo "=== scheduler: $sched ==="
    "$BIN" --config "$CONFIG" --scheduler "$sched" --tag "$sched"
    echo
done

echo "done. summary -> results/summary.csv"
