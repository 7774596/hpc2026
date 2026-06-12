#!/usr/bin/env python3
"""将 results/summary.csv 绘制成柱状图（每个指标一张），输出到 results/plots/。

用法：python scripts/plot_results.py [summary_csv]

TODO(C): 1) 增加加速比/效率曲线图（线程数、进程数为横轴）；
         2) 多次重复实验时绘制误差棒；
         3) 图表统一风格后插入报告第五章。
"""
import csv
import os
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

METRICS = [
    ("avg_wait", "Average Wait Time (s)"),
    ("avg_turnaround", "Average Turnaround Time (s)"),
    ("throughput", "Throughput (jobs/s)"),
    ("utilization", "Cluster Utilization"),
    ("load_balance_cv", "Load Balance CV (lower=better)"),
]


def main() -> int:
    path = sys.argv[1] if len(sys.argv) > 1 else "results/summary.csv"
    if not os.path.exists(path):
        print(f"{path} not found, run experiments first", file=sys.stderr)
        return 1

    with open(path, newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    if not rows:
        print("summary csv is empty", file=sys.stderr)
        return 1

    out_dir = os.path.join(os.path.dirname(path), "plots")
    os.makedirs(out_dir, exist_ok=True)

    schedulers = [r["scheduler"] for r in rows]
    for key, title in METRICS:
        values = [float(r[key]) for r in rows]
        fig, ax = plt.subplots(figsize=(6, 4))
        ax.bar(schedulers, values, color="steelblue")
        ax.set_title(title)
        ax.set_xlabel("scheduler")
        ax.grid(axis="y", alpha=0.3)
        fig.tight_layout()
        out = os.path.join(out_dir, f"{key}.png")
        fig.savefig(out, dpi=150)
        plt.close(fig)
        print(f"saved {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
