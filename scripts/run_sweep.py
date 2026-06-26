#!/usr/bin/env python3
"""Run reproducible simulation sweeps and aggregate the results.

This script is intentionally dependency-free so it can run on login nodes and
compute nodes without a Python package setup.
"""

import argparse
import csv
import itertools
import os
import re
import shutil
import statistics
import subprocess
import sys
from pathlib import Path


SCHEDULERS = ["fcfs", "sjf", "rr", "backfill"]
METRICS = [
    "makespan",
    "avg_wait",
    "avg_turnaround",
    "throughput",
    "utilization",
    "load_balance_cv",
]


def parse_csv_list(value, cast=str):
    parts = [p.strip() for p in value.split(",") if p.strip()]
    if not parts:
        raise argparse.ArgumentTypeError("list must not be empty")
    try:
        return [cast(p) for p in parts]
    except ValueError as exc:
        raise argparse.ArgumentTypeError(str(exc))


def read_config(path):
    values = {}
    key_re = re.compile(r"^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*([^#;]*)")
    with open(path, encoding="utf-8") as f:
        for line in f:
            m = key_re.match(line)
            if m:
                values[m.group(1)] = m.group(2).strip()
    return values


def write_config(base_path, out_path, overrides):
    seen = set()
    key_re = re.compile(r"^(\s*)([A-Za-z_][A-Za-z0-9_]*)(\s*=\s*)(.*)$")
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(base_path, encoding="utf-8") as src, open(out_path, "w", encoding="utf-8") as dst:
        for line in src:
            m = key_re.match(line)
            if m and m.group(2) in overrides:
                key = m.group(2)
                dst.write(f"{m.group(1)}{key}{m.group(3)}{overrides[key]}\n")
                seen.add(key)
            else:
                dst.write(line)
        missing = [k for k in overrides if k not in seen]
        if missing:
            dst.write("\n# Added by scripts/run_sweep.py\n")
            for key in missing:
                dst.write(f"{key} = {overrides[key]}\n")


def read_single_summary(path):
    with open(path, newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    if len(rows) != 1:
        raise RuntimeError(f"expected exactly one summary row in {path}, found {len(rows)}")
    return rows[0]


def format_tag(experiment, scheduler, seed, num_jobs, arrival_rate, nodes, rr_quantum):
    ar = str(arrival_rate).replace(".", "p")
    rq = str(rr_quantum).replace(".", "p")
    return (
        f"{experiment}_{scheduler}_seed{seed}_jobs{num_jobs}_"
        f"arr{ar}_nodes{nodes}_rr{rq}"
    )


def run_one(args, combo, base_values):
    scheduler, seed, num_jobs, arrival_rate, nodes, rr_quantum = combo
    tag = format_tag(args.experiment, scheduler, seed, num_jobs, arrival_rate, nodes, rr_quantum)
    run_dir = args.output / "runs" / tag
    cfg_path = args.output / "configs" / f"{tag}.ini"
    run_dir.mkdir(parents=True, exist_ok=True)

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
        str(args.bin),
        "--config",
        str(cfg_path),
        "--scheduler",
        scheduler,
        "--output",
        str(run_dir),
        "--tag",
        tag,
    ]
    proc = subprocess.run(
        cmd,
        cwd=args.repo_root,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    log_path = run_dir / "stdout.log"
    log_path.write_text(proc.stdout, encoding="utf-8")
    if proc.returncode != 0:
        raise RuntimeError(f"run failed for {tag}; see {log_path}")

    row = read_single_summary(run_dir / "summary.csv")
    if not args.save_jobs:
        for jobs_csv in run_dir.glob("*_jobs.csv"):
            jobs_csv.unlink()

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
            "run_dir": str(run_dir),
        }
    )
    return row


def write_csv(path, rows, fieldnames):
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", newline="", encoding="utf-8") as f:
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
            agg[f"{metric}_mean"] = f"{statistics.mean(values):.10g}"
            std = statistics.stdev(values) if len(values) > 1 else 0.0
            agg[f"{metric}_std"] = f"{std:.10g}"
        out.append(agg)
    return out


def parse_args(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-config", default="configs/medium.ini", type=Path)
    parser.add_argument("--bin", default="build/hpcsim", type=Path)
    parser.add_argument("--output", default="results/sim_sweep", type=Path)
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

    args.repo_root = Path(__file__).resolve().parents[1]
    args.base_config = (args.repo_root / args.base_config).resolve()
    args.bin = (args.repo_root / args.bin).resolve()
    args.output = (args.repo_root / args.output).resolve()
    return args


def main(argv=None):
    args = parse_args(argv or sys.argv[1:])
    if not args.base_config.exists():
        print(f"base config not found: {args.base_config}", file=sys.stderr)
        return 1
    if not args.bin.exists():
        print(f"binary not found: {args.bin}; run `make serial` first", file=sys.stderr)
        return 1

    base_values = read_config(args.base_config)
    schedulers = parse_csv_list(args.schedulers)
    invalid = [s for s in schedulers if s not in SCHEDULERS]
    if invalid:
        print(f"unknown scheduler(s): {', '.join(invalid)}", file=sys.stderr)
        return 1

    seeds = parse_csv_list(args.seeds, int)
    num_jobs = parse_csv_list(args.num_jobs or base_values.get("num_jobs", "1000"), int)
    arrival_rates = parse_csv_list(
        args.arrival_rates or base_values.get("arrival_rate", "1.0"), float
    )
    nodes = parse_csv_list(args.nodes or base_values.get("nodes", "4"), int)
    rr_quantums = parse_csv_list(args.rr_quantums or base_values.get("rr_quantum", "50"), float)

    if args.clean and args.output.exists():
        shutil.rmtree(args.output)
    args.output.mkdir(parents=True, exist_ok=True)

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
            f"[{index}/{total}] scheduler={scheduler} seed={seed} "
            f"num_jobs={nj} arrival_rate={ar} nodes={nn} rr_quantum={q}",
            flush=True,
        )
        try:
            raw_rows.append(run_one(args, combo, base_values))
        except Exception as exc:
            failures.append(str(exc))
            print(f"[error] {exc}", file=sys.stderr)
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
    write_csv(args.output / "raw_results.csv", raw_rows, raw_fields)
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
        agg_fields.extend([f"{metric}_mean", f"{metric}_std"])
    write_csv(args.output / "aggregate.csv", agg_rows, agg_fields)

    print(f"raw results      -> {args.output / 'raw_results.csv'}")
    print(f"aggregate results -> {args.output / 'aggregate.csv'}")
    if failures:
        print(f"{len(failures)} run(s) failed", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
