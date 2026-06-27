#!/usr/bin/env bash
# Run exec-mode MPI scaling experiments.
#
# Usage:
#   bash scripts/run_exec_scaling.sh [config] [output_dir] [np_list]
#
# np_list is total MPI ranks, including rank 0 master. For example, "2,3,5,9"
# means 1,2,4,8 workers.
set -euo pipefail
cd "$(dirname "$0")/.."

CONFIG=${1:-configs/exec.ini}
OUTDIR=${2:-results/exec_scaling}
NP_LIST=${3:-2,3,5,9}
BIN=${BIN:-build/hpcsim_mpi}
MPI_RUN=${MPI_RUN:-mpirun}
SCHEDULER=${SCHEDULER:-fcfs}
if [[ -z "${PYTHON:-}" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON=python3
    elif command -v python >/dev/null 2>&1; then
        PYTHON=python
    else
        echo "python interpreter not found; load a Python module or set PYTHON=/path/to/python" >&2
        exit 1
    fi
fi
echo "python: $("$PYTHON" --version 2>&1)"

if [[ ! -x "$BIN" ]]; then
    echo "binary $BIN not found, run 'make mpi' first" >&2
    exit 1
fi

mkdir -p "$OUTDIR/logs"
if [[ "${CLEAN:-1}" == "1" ]]; then
    rm -f "$OUTDIR/exec_summary.csv" "$OUTDIR/exec_scaling.csv"
    rm -f "$OUTDIR"/*_exec_jobs.csv
fi
IFS=',' read -r -a NPS <<< "$NP_LIST"

for raw_np in "${NPS[@]}"; do
    np=${raw_np//[[:space:]]/}
    if [[ -z "$np" ]]; then
        continue
    fi
    tag="np${np}_${SCHEDULER}"
    echo "=== exec scaling: np=${np} (${np}-1 workers) ==="
    if [[ "$(basename "$MPI_RUN")" == "srun" ]]; then
        RUN_PREFIX=("$MPI_RUN" -n "$np")
    else
        RUN_PREFIX=("$MPI_RUN" -np "$np")
    fi
    "${RUN_PREFIX[@]}" "$BIN" \
        --config "$CONFIG" \
        --mode exec \
        --scheduler "$SCHEDULER" \
        --output "$OUTDIR" \
        --tag "$tag" \
        >"$OUTDIR/logs/${tag}.log" 2>&1
    tail -n 12 "$OUTDIR/logs/${tag}.log"
    echo
done

"$PYTHON" scripts/analyze_exec_scaling.py "$OUTDIR/exec_summary.csv" "$OUTDIR/exec_scaling.csv"
"$PYTHON" scripts/plot_results.py "$OUTDIR/exec_scaling.csv" \
    --out-dir "$OUTDIR/plots" \
    --x num_workers \
    --series scheduler

echo "exec summary -> $OUTDIR/exec_summary.csv"
echo "exec scaling -> $OUTDIR/exec_scaling.csv"
