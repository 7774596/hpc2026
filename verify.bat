@echo off
REM 本地验证脚本（Windows/MinGW 临时用，超算上请用 Makefile）
REM 与 Makefile 保持一致：-std=c++11（兼容服务器 GCC 4.8.5）
cd /d "%~dp0"
echo [1/4] build hpcsim.exe > verify_log.txt 2>&1
g++ -std=c++11 -O2 -Wall -Wextra -fopenmp -static -Iinclude -Isrc src/main.cpp src/core/cluster.cpp src/core/config.cpp src/core/workload.cpp src/core/metrics.cpp src/core/simulator.cpp src/schedulers/fcfs.cpp src/schedulers/sjf.cpp src/schedulers/round_robin.cpp src/schedulers/backfill.cpp src/schedulers/scheduler_factory.cpp src/parallel/kernel.cpp -o build/hpcsim.exe >> verify_log.txt 2>&1
if errorlevel 1 goto :fail
echo [2/4] build test_schedulers.exe >> verify_log.txt
g++ -std=c++11 -O2 -Wall -Wextra -fopenmp -static -Iinclude -Isrc tests/test_schedulers.cpp src/core/cluster.cpp src/core/config.cpp src/core/workload.cpp src/core/metrics.cpp src/core/simulator.cpp src/schedulers/fcfs.cpp src/schedulers/sjf.cpp src/schedulers/round_robin.cpp src/schedulers/backfill.cpp src/schedulers/scheduler_factory.cpp src/parallel/kernel.cpp -o build/test_schedulers.exe >> verify_log.txt 2>&1
if errorlevel 1 goto :fail
echo [3/4] run unit tests >> verify_log.txt
build\test_schedulers.exe >> verify_log.txt 2>&1
if errorlevel 1 goto :fail
echo [4/4] run demo (fcfs / sjf / rr / backfill) >> verify_log.txt
build\hpcsim.exe --config configs/small.ini --scheduler fcfs >> verify_log.txt 2>&1
build\hpcsim.exe --config configs/small.ini --scheduler sjf >> verify_log.txt 2>&1
build\hpcsim.exe --config configs/small.ini --scheduler rr >> verify_log.txt 2>&1
build\hpcsim.exe --config configs/small.ini --scheduler backfill >> verify_log.txt 2>&1
echo VERIFY_ALL_OK >> verify_log.txt
exit /b 0
:fail
echo VERIFY_FAILED >> verify_log.txt
exit /b 1
