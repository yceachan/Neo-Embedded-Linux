  IPC 机制
  ├── 消息传递（拷贝）：pipe、socket、msg_queue
  └── 共享内存（零拷贝）：
      ├── mmap/
      │   ├── 00-driver-mmap-and-sync.md  字符驱动 .mmap fop + remap_pfn_range + 跨进程 mutex
      │   ├── 01-api-matrix.md            mmap 家族 API 全览（mmap/munmap/msync/mprotect/mremap/madvise + shm_open/memfd_create）
      │   └── 02-kernel-mechanics.md      VMA 结构、缺页异常流程、MAP_SHARED vs MAP_PRIVATE COW 机制
      ├── shmget/shmat (SysV)
      └── memfd_create

  同步原语：
  └── semaphore/
      ├── 00-concept-and-model.md   信号量模型、POSIX vs SysV、futex 基石
      ├── 01-posix-api-matrix.md    POSIX sem API 全览（sem_init/sem_open）
      ├── 02-sysv-api-matrix.md     SysV semget/semop/semctl + SEM_UNDO
      └── 03-classic-patterns.md    四大经典范式：互斥/生产消费/读写锁/屏障
