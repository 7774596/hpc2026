#!/usr/bin/env python3
"""Compute speedup and efficiency from exec_summary.csv."""

import csv
import sys
from pathlib import Path


def read_rows(path):
    with open(path, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def main(argv=None):
    argv = argv or sys.argv[1:]
    in_path = Path(argv[0]) if argv else Path("results/exec_scaling/exec_summary.csv")
    out_path = Path(argv[1]) if len(argv) > 1 else in_path.with_name("exec_scaling.csv")
    if not in_path.exists():
        print(f"{in_path} not found", file=sys.stderr)
        return 1
    rows = read_rows(in_path)
    if not rows:
        print("exec summary is empty", file=sys.stderr)
        return 1

    rows = sorted(rows, key=lambda r: int(r["num_workers"]))
    base = rows[0]
    base_workers = int(base["num_workers"])
    base_wall = float(base["wall_time"])
    fields = list(rows[0].keys()) + ["speedup", "efficiency"]
    out_rows = []
    for row in rows:
        workers = int(row["num_workers"])
        wall = float(row["wall_time"])
        speedup = base_wall / wall if wall > 0 else 0.0
        efficiency = speedup / (float(workers) / base_workers) if workers > 0 else 0.0
        out = dict(row)
        out["speedup"] = f"{speedup:.10g}"
        out["efficiency"] = f"{efficiency:.10g}"
        out_rows.append(out)

    with open(out_path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        writer.writerows(out_rows)
    print(f"saved {out_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
