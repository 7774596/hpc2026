# hpc2026 作业调度模拟器构建脚本（面向 Linux / 超算环境）
#
#   make serial   构建纯模拟版 build/hpcsim（无 MPI 依赖）
#   make mpi      构建 MPI+OpenMP 执行版 build/hpcsim_mpi
#   make test     构建并运行单元测试
#   make demo     用 small 配置依次跑 4 种调度策略
#   make format   clang-format 全部源码
#   make clean    清理构建产物

# 注意：统一使用 C++11，保证 CentOS 7 自带的 GCC 4.8.5 可直接编译，
# 不要引入 C++14/17 特性（make_unique / optional / clamp / 结构化绑定等）
CXX      ?= g++
MPICXX   ?= mpic++
CXXFLAGS ?= -std=c++11 -O2 -Wall -Wextra -fopenmp
CPPFLAGS += -Iinclude -Isrc

CORE_SRCS := $(wildcard src/core/*.cpp) $(wildcard src/schedulers/*.cpp) src/parallel/kernel.cpp

.PHONY: all serial mpi test demo format clean

all: serial

serial:
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) src/main.cpp $(CORE_SRCS) -o build/hpcsim

mpi:
	@mkdir -p build
	$(MPICXX) $(CXXFLAGS) $(CPPFLAGS) -DHPCSIM_USE_MPI src/main.cpp src/parallel/mpi_runner.cpp $(CORE_SRCS) -o build/hpcsim_mpi

test:
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) tests/test_schedulers.cpp $(CORE_SRCS) -o build/test_schedulers
	./build/test_schedulers

demo: serial
	./build/hpcsim --config configs/small.ini --scheduler fcfs
	./build/hpcsim --config configs/small.ini --scheduler sjf
	./build/hpcsim --config configs/small.ini --scheduler rr
	./build/hpcsim --config configs/small.ini --scheduler backfill

format:
	clang-format -i $(shell find include src tests \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \))

clean:
	rm -rf build
