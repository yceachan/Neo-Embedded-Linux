# Consult: muxIO 运行结果与现象分析

## 实际运行输出

执行 `./test.sh` 后的真实截取输出（截取自 `output/log/run_poll.log`）：

```text
Running poll test...
--- Using poll ---
[poll] Read from fd1: MSG1
[poll] Read from fd2: MSG2
[poll] Read from fd1: MSG1
[poll] Read from fd1: MSG1
[poll] Read from fd2: MSG2
[poll] Read from fd1: MSG1
[poll] Read from fd1: MSG1
[poll] Read from fd2: MSG2
[poll] Read from fd1: MSG1
[poll] Read from fd1: MSG1
[poll] Read from fd2: MSG2
[poll] grep_marker_success
```
*(注意：select 和 epoll 也输出了完全一致的事件响应时序。)*

## 现象分析

1. **时序交错 (Interleaving)**:
   我们期望得到 `MSG1` (每 400ms) 和 `MSG2` (每 700ms) 的交错打印。观察日志：`MSG1`, `MSG2`, `MSG1`, `MSG1`, `MSG2`…… 这与时间的最小公倍数规律吻合（例如 400ms 触发 MSG1，700ms 触发 MSG2，800ms 触发 MSG1，1200ms 触发 MSG1，1400ms 触发 MSG2 等等）。这证明了多路复用可以**单线程非阻塞地**处理多个输入源的时序到达。
2. **连接关闭处理 (Connection Closed)**:
   当子进程退出时，会关闭所有管道的写端。此时 `select` 会返回可读（`read` 进而返回 0），而 `poll` 和 `epoll` 会在 `.revents` 中置位 `POLLHUP`/`EPOLLHUP`。我们在代码中正确检测了这三种退出状态并退出监控循环，验证了内核的 EOF 投递机制。
3. **API 行为对比 (API Behavior)**:
   尽管底层机制（轮询复杂度、注册模型、内核结构）有差异，但在该用户态小规模并发下，三种多路复用技术达成了完全一致的事件响应能力。

## 结论

| 观察到的现象 | 证明的概念 |
|---|---|
| MSG1 与 MSG2 异步到达并被同一个 while 循环捕获 | I/O 多路复用 (I/O Multiplexing) 允许单线程并发处理多个 FD。 |
| 当子进程退出关闭写端时，循环立即退出 | 对端关闭也是一种就绪状态 (EOF)，各种复用 API 均提供机制报告这种状态变更。 |
| 三种模式输出相同 | `select`/`poll`/`epoll` 在小规模 FD 监控时具有功能上的等价性，差别主要在扩展性和 API 易用性。 |
