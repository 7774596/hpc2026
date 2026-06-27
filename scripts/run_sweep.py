#!/usr/bin/env python
from __future__ import print_function

"""Run reproducible simulation sweeps and aggregate the results.

This script is dependency-free and supports both Python 2.7 and Python 3.
"""

import argparse
import csv
import itertools
import math
import os
import re
import shutil
import subprocess
import sys


SCHEDULERS = ["fcfs", "sjf", "rr", "backfill"]
METRICS = [
    "makespan",
    "avg_wait",
    "avg_turnaround",
    "throughput",
    "utilization",
    "load_balance_cv",
]


def makedirs(path):
    if path and not os.path.isdir(path):
        os.makedirs(path)


def parse_csv_list(value, cast=str):
    parts = [p.strip() for p in value.split(",") if p.strip()]
    if not parts:
        raise argparse.ArgumentTypeError("list must not be empty")
    try:
        return [cast(p) for p in parts]
    except ValueError as exc:
        raise argparse.ArgumentTypeError(str(exc))


def mean(values):
    return sum(values) / float(len(values)) if values else 0.0


def stdev(values):
    if len(values) <= 1:
        return 0.0
    m = mean(values)
    var = sum((v - m) * (v - m) for v in values) / float(len(values) - 1)
    return math.sqrt(var)


def read_config(path):
    values = {}
    key_re = re.compile(r"^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*([^#;]*)")
    with open(path, "r") as f:
        for line in f:
            m = key_re.match(line)
            if m:
                values[m.group(1)] = m.group(2).strip()
    return values


def write_config(base_path, out_path, overrides):
    seen = set()
    key_re = re.compile(r"^(\s*)([A-Za-z_][A-Za-z0-9_]*)(\s*=\s*)(.*)$")
    makedirs(os.path.dirname(out_path))
    with open(base_path, "r") as src, open(out_path, "w") as dst:
        for line in src:
            m = key_re.match(line)
            if m and m.group(2) in overrides:
                key = m.group(2)
                dst.write("%s%s%s%s\n" % (m.group(1), key, m.group(3), overrides[key]))
                seen.add(key)
            else:
                dst.write(line)
        missing = [k for k in overrides if k not in seen]
        if missing:
            dst.write("\n# Added by scripts/run_sweep.py\n")
            for key in missing:
                dst.write("%s = %s\n" % (key, overrides[key]))


def read_single_summary(path):
    with open(path, "r") as f:
        rows = list(csv.DictReader(f))
    if len(rows) != 1:
        raise RuntimeError("expected exactly one summary row in %s, found %d" % (path, len(rows)))
    return rows[0]


def format_tag(experiment, scheduler, seed, num_jobs, arrival_rate, nodes, rr_quantum):
    ar = str(arrival_rate).replace(".", "p")
    rq = str(rr_quantum).replace(".", "p")
    return "%s_%s_seed%s_jobs%s_arr%s_nodes%s_rr%s" % (
        experiment,
        scheduler,
        seed,
        num_jobs,
        ar,
        nodes,
        rq,
    )


def run_command(cmd, cwd):
    proc = subprocess.Popen(
        cmd,
        cwd=cwd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        universal_newlines=True,
    )
    out, _ = proc.communicate()
    return proc.returncode, out


def run_one(args, combo, base_values):
    scheduler, seed, num_jobs, arrival_rate, nodes, rr_quantum = combo
    tag = format_tag(args.experiment, scheduler, seed, num_jobs, arrival_rate, nodes, rr_quantum)
    run_dir = os.path.join(args.output, "runs", tag)
    cfg_path = os.path.join(args.output, "configs", tag + ".ini")
    makedirs(run_dir)

    overrides = {
        "seed": str(seed),
        "num_jobs": str(num_jobs),
        "arrival_rate": str(arrival_rate),
        "nodes": str(nodes),
        "rr_quantum": str(rr_quantum),
        "scheduler": scheduler,
    }
    write_config(args.base_config, cfg_path, overrides)

    cmd = [
        args.bin,
        "--config",
        cfg_path,
        "--scheduler",
        scheduler,
        "--output",
        run_dir,
        "--tag",
        tag,
    ]
    ret, stdout = run_command(cmd, args.repo_root)
    log_path = os.path.join(run_dir, "stdout.log")
    with open(log_path, "w") as f:
        f.write(stdout)
    if ret != 0:
        raise RuntimeError("run failed for %s; see %s" % (tag, log_path))

    row = read_single_summary(os.path.join(run_dir, "summary.csv"))
    if not args.save_jobs:
        for name in os.listdir(run_dir):
            if name.endswith("_jobs.csv"):
                os.unlink(os.path.join(run_dir, name))

    row.update(
        {
            "experiment": args.experiment,
            "tag": tag,
            "seed": str(seed),
            "num_jobs": str(num_jobs),
            "arrival_rate": str(arrival_rate),
            "nodes": str(nodes),
            "cores_per_node": base_values.get("cores_per_node", ""),
            "rr_quantum": str(rr_quantum),
            "run_dir": run_dir,
        }
    )
    return row


def write_csv(path, rows, fieldnames):
    makedirs(os.path.dirname(path))
    with open(path, "w") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for row in rows:
            writer.writerow(row)


def aggregate(raw_rows):
    group_keys = [
        "experiment",
        "scheduler",
        "num_jobs",
        "arrival_rate",
        "nodes",
        "cores_per_node",
        "rr_quantum",
    ]
    groups = {}
    for row in raw_rows:
        key = tuple(row[k] for k in group_keys)
        groups.setdefault(key, []).append(row)

    out = []
    for key, rows in sorted(groups.items()):
        agg = dict(zip(group_keys, key))
        agg["runs"] = str(len(rows))
        for metric in METRICS:
            values = [float(r[metric]) for r in rows]
            agg["%s_mean" % metric] = "%.10g" % mean(values)
            agg["%s_std" % metric] = "%.10g" % stdev(values)
        out.append(agg)
    return out


def parse_args(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-config", default="configs/medium.ini")
    parser.add_argument("--bin", default="build/hpcsim")
    parser.add_argument("--output", default="results/sim_sweep")
    parser.add_argument("--experiment", default="sim_sweep")
    parser.add_argument("--schedulers", default="fcfs,sjf,rr,backfill")
    parser.add_argument("--seeds", default="7,11,13")
    parser.add_argument("--num-jobs", default="")
    parser.add_argument("--arrival-rates", default="")
    parser.add_argument("--nodes", default="")
    parser.add_argument("--rr-quantums", default="")
    parser.add_argument("--clean", action="store_true", help="remove the output directory first")
    parser.add_argument("--save-jobs", action="store_true", help="keep per-job CSV files")
    parser.add_argument("--keep-going", action="store_true", help="continue after failed runs")
    args = parser.parse_args(argv)

    args.repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    args.base_config = os.path.abspath(os.path.join(args.repo_root, args.base_config))
    args.bin = os.path.abspath(os.path.join(args.repo_root, args.bin))
    args.output = os.path.abspath(os.path.join(args.repo_root, args.output))
    return args


def main(argv=None):
    args = parse_args(argv or sys.argv[1:])
    if not os.path.exists(args.base_config):
        print("base config not found: %s" % args.base_config, file=sys.stderr)
        return 1
    if not os.path.exists(args.bin):
        print("binary not found: %s; run `make serial` first" % args.bin, file=sys.stderr)
        return 1

    base_values = read_config(args.base_config)
    schedulers = parse_csv_list(args.schedulers)
    invalid = [s for s in schedulers if s not in SCHEDULERS]
    if invalid:
        print("unknown scheduler(s): %s" % ", ".join(invalid), file=sys.stderr)
        return 1

    seeds = parse_csv_list(args.seeds, int)
    num_jobs = parse_csv_list(args.num_jobs or base_values.get("num_jobs", "1000"), int)
    arrival_rates = parse_csv_list(
        args.arrival_rates or base_values.get("arrival_rate", "1.0"), float
    )
    nodes = parse_csv_list(args.nodes or base_values.get("nodes", "4"), int)
    rr_quantums = parse_csv_list(args.rr_quantums or base_values.get("rr_quantum", "50"), float)

    if args.clean and os.path.exists(args.output):
        shutil.rmtree(args.output)
    makedirs(args.output)

    combos = []
    for scheduler, seed, nj, ar, nn in itertools.product(
        schedulers, seeds, num_jobs, arrival_rates, nodes
    ):
        q_values = rr_quantums if scheduler == "rr" else [rr_quantums[0]]
        for q in q_values:
            combos.append((scheduler, seed, nj, ar, nn, q))

    raw_rows = []
    failures = []
    total = len(combos)
    for index, combo in enumerate(combos, 1):
        scheduler, seed, nj, ar, nn, q = combo
        print(
            "[%d/%d] scheduler=%s seed=%s num_jobs=%s arrival_rate=%s nodes=%s rr_quantum=%s"
            % (index, total, scheduler, seed, nj, ar, nn, q)
        )
        sys.stdout.flush()
        try:
            raw_rows.append(run_one(args, combo, base_values))
        except Exception as exc:
            failures.append(str(exc))
            print("[error] %s" % exc, file=sys.stderr)
            if not args.keep_going:
                return 1

    raw_fields = [
        "experiment",
        "tag",
        "scheduler",
        "seed",
        "num_jobs",
        "arrival_rate",
        "nodes",
        "cores_per_node",
        "rr_quantum",
        "makespan",
        "avg_wait",
        "avg_turnaround",
        "throughput",
        "utilization",
        "load_balance_cv",
        "run_dir",
    ]
    write_csv(os.path.join(args.output, "raw_results.csv"), raw_rows, raw_fields)
    agg_rows = aggregate(raw_rows)
    agg_fields = [
        "experiment",
        "scheduler",
        "num_jobs",
        "arrival_rate",
        "nodes",
        "cores_per_node",
        "rr_quantum",
        "runs",
    ]
    for metric in METRICS:
        agg_fields.extend(["%s_mean" % metric, "%s_std" % metric])
    write_csv(os.path.join(args.output, "aggregate.csv"), agg_rows, agg_fields)

    print("raw results       -> %s" % os.path.join(args.output, "raw_results.csv"))
    print("aggregate results -> %s" % os.path.join(args.output, "aggregate.csv"))
    if failures:
        print("%d run(s) failed" % len(failures), file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
