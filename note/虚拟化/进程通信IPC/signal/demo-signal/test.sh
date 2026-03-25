#!/bin/bash

# 确保在脚本所在目录执行
cd "$(dirname "$0")"

echo "=== 1. 编译程序 ==="
make clean && make

echo -e "\n=== 2. 启动后台进程 ==="
# 启动程序至后台，并将输出重定向至日志文件
./output/signal_demo > demo_output.log 2>&1 &

# 获取刚才放入后台的进程 PID
DEMO_PID=$!
echo "后台进程已启动，PID: $DEMO_PID"

# 给予进程 0.5 秒时间完成 sigaction 注册
sleep 0.5 

echo -e "\n=== 3. 发送测试信号 ==="
echo "向 PID $DEMO_PID 发送第一次 SIGUSR1 信号..."
kill -SIGUSR1 $DEMO_PID
sleep 0.5 # 等待进程处理完毕

echo "向 PID $DEMO_PID 发送第二次 SIGUSR1 信号..."
kill -SIGUSR1 $DEMO_PID
sleep 0.5 

echo -e "\n=== 4. 终止测试进程 ==="
echo "发送 SIGINT (Ctrl+C) 终止进程..."
kill -SIGINT $DEMO_PID
sleep 0.5

echo -e "\n=== 5. 验证进程输出日志 ==="
cat demo_output.log

# 清理日志
rm demo_output.log
echo -e "\n测试完成！"
