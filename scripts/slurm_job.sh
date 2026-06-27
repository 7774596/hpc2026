#!/usr/bin/env bash
# 超算 Slurm 作业模板：exec 模式（MPI + OpenMP 真实执行）
# 环境：教学超算 cpu_96G 分区，GNU 10.2.0 + OpenMPI 4.1.6
#
# 提交：sbatch scripts/slurm_job.sh
# 推荐正式实验使用 scripts/slurm_exec_scaling.sh；本文件保留为单次 exec smoke test。
#
#SBATCH --job-name=hpcsim
#SBATCH --partition=cpu_96G
#SBATCH --nodes=2
#SBATCH --ntasks=5                # 1 个 master + 4 个 worker
#SBATCH --cpus-per-task=8         # 每个 worker 可用的 OpenMP 核数
#SBATCH --time=00:30:00
#SBATCH --output=results/slurm-%j.out

module purge
module load compiler/gnu/10.2.0
module load mpi/openmpi/4.1.6

export OMP_NUM_THREADS=${SLURM_CPUS_PER_TASK}

cd "${SLURM_SUBMIT_DIR}"
make mpi
mpirun -np "${SLURM_NTASKS}" build/hpcsim_mpi \
    --config configs/exec.ini \
    --mode exec \
    --output results/exec_smoke_${SLURM_JOB_ID} \
    --tag smoke_np${SLURM_NTASKS}
