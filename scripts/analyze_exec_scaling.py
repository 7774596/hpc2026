#!/usr/bin/env python
from __future__ import print_function

"""Compute speedup and efficiency from exec_summary.csv.

Compatible with Python 2.7 and Python 3.
"""

import csv
import os
import sys


def read_rows(path):
    with open(path, "r") as f:
        return list(csv.DictReader(f))


def main(argv=None):
    argv = argv or sys.argv[1:]
    in_path = argv[0] if argv else os.path.join("results", "exec_scaling", "exec_summary.csv")
    out_path = argv[1] if len(argv) > 1 else os.path.join(os.path.dirname(in_path), "exec_scaling.csv")
    if not os.path.exists(in_path):
        print("%s not found" % in_path, file=sys.stderr)
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
        out["speedup"] = "%.10g" % speedup
        out["efficiency"] = "%.10g" % efficiency
        out_rows.append(out)

    with open(out_path, "w") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        writer.writerows(out_rows)
    print("saved %s" % out_path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
