#!/bin/bash
set -e

echo "Cleaning..."
make clean

echo "Building..."
make

mkdir -p output/log

echo "Running select test..."
./output/bin/muxio select | tee output/log/run_select.log
grep -q "grep_marker_success" output/log/run_select.log || { echo "select test failed"; exit 1; }

echo "Running poll test..."
./output/bin/muxio poll | tee output/log/run_poll.log
grep -q "grep_marker_success" output/log/run_poll.log || { echo "poll test failed"; exit 1; }

echo "Running epoll test..."
./output/bin/muxio epoll | tee output/log/run_epoll.log
grep -q "grep_marker_success" output/log/run_epoll.log || { echo "epoll test failed"; exit 1; }

echo "All tests passed!"
