#!/usr/bin/env bash
# 超算 Slurm 作业模板：exec 模式（MPI + OpenMP 真实执行）
# 环境：CentOS 7 / Slurm 20.11，模块名已按本校超算实际环境填写
#
# 提交：sbatch scripts/slurm_job.sh
# TODO(C): 用 sinfo 确认分区名并修改 --partition；扫描 --nodes/--ntasks 采集加速比
#
#SBATCH --job-name=hpcsim
#SBATCH --partition=normal        # TODO(C): sinfo 查实际分区名
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
mpirun -np "${SLURM_NTASKS}" build/hpcsim_mpi --config configs/medium.ini --mode exec
