#!/usr/bin/env bash
#SBATCH --job-name=hpcsim-sim
#SBATCH --partition=cpu_96G
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --time=02:00:00
#SBATCH --output=results/slurm-sim-%j.out

# Confirmed on the teaching HPC cluster:
#   partition: cpu_96G
#   compiler : compiler/gnu/10.2.0
set -euo pipefail

cd "$SLURM_SUBMIT_DIR"

if command -v module >/dev/null 2>&1; then
    module purge
    module load compiler/gnu/10.2.0
fi

echo "job id: ${SLURM_JOB_ID:-local}"
echo "host  : $(hostname)"
echo "commit: $(git rev-parse --short HEAD)"

OUT_ROOT="results/report_experiments_${SLURM_JOB_ID:-local}"
bash scripts/run_report_experiments.sh "$OUT_ROOT"

echo "finished simulation experiments"
echo "output root: $OUT_ROOT"
