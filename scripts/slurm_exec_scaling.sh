#!/usr/bin/env bash
#SBATCH --job-name=hpcsim-exec
#SBATCH --partition=cpu_96G
#SBATCH --nodes=2
#SBATCH --ntasks=9
#SBATCH --cpus-per-task=4
#SBATCH --time=00:30:00
#SBATCH --output=results/slurm-exec-%j.out

# Confirmed on the teaching HPC cluster:
#   partition: cpu_96G
#   compiler : compiler/gnu/10.2.0
#   MPI      : mpi/openmpi/4.1.6
# The default NP_LIST=2,3,5,9 must not exceed --ntasks.
set -euo pipefail

cd "$SLURM_SUBMIT_DIR"

if command -v module >/dev/null 2>&1; then
    module purge
    module load compiler/gnu/10.2.0
    module load mpi/openmpi/4.1.6
fi

export OMP_NUM_THREADS="${SLURM_CPUS_PER_TASK:-4}"

echo "job id: ${SLURM_JOB_ID:-local}"
echo "host  : $(hostname)"
echo "commit: $(git rev-parse --short HEAD)"
echo "OMP_NUM_THREADS=$OMP_NUM_THREADS"

make mpi

OUT_ROOT="results/exec_scaling_${SLURM_JOB_ID:-local}"
NP_LIST=${NP_LIST:-2,3,5,9}
bash scripts/run_exec_scaling.sh configs/exec.ini "$OUT_ROOT" "$NP_LIST"

echo "finished exec scaling experiments"
echo "output root: $OUT_ROOT"
