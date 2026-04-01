/*
 * mmap IPC Demo — userspace side
 *
 * Scenario: parent process and child process communicate through
 * a kernel buffer exposed via /dev/mmap_drv's .mmap fop.
 *
 * Shared region layout (must match driver_fops.c DUMP ioctl):
 *   [0..3]      int  seq          — monotonic write counter
 *   [4..259]    char msg[256]     — payload string
 *   [260..263]  int  _pad         — alignment to 8 bytes
 *   [264+]      pthread_mutex_t   — PTHREAD_PROCESS_SHARED mutex
 *
 * The mutex itself lives inside the shared buffer so both processes
 * reference the same physical memory — required for futex to work
 * across process boundaries.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>

#include "mmap_drv_uapi.h"

/* ---- shared region layout ---------------------------------------- */
typedef struct {
    int             seq;
    char            msg[256];
    int             _pad;
    pthread_mutex_t lock;   /* must be in shared memory, not private heap */
} SharedRegion;

/* ---- helpers ----------------------------------------------------- */

static void init_shared_mutex(SharedRegion *shm)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    /*
     * PTHREAD_PROCESS_SHARED: the mutex is keyed on the underlying
     * physical page address (via futex), not the virtual address.
     * Without this flag, a mutex in shared memory only works within
     * a single process.
     */
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&shm->lock, &attr);
    pthread_mutexattr_destroy(&attr);
}

/* ------------------------------------------------------------------ */

int main(void)
{
    int fd = open("/dev/mmap_drv", O_RDWR);
    if (fd < 0) { perror("open /dev/mmap_drv"); return 1; }

    /*
     * MAP_SHARED: writes are visible to all processes that map the
     * same region.  The kernel's remap_pfn_range wired this virtual
     * range to the driver's g_kbuf physical page.
     */
    SharedRegion *shm = mmap(NULL, MMAP_DRV_BUF_SIZE,
                              PROT_READ | PROT_WRITE, MAP_SHARED,
                              fd, 0);
    if (shm == MAP_FAILED) { perror("mmap"); close(fd); return 1; }

    /* Initialize shared state before fork so child inherits it */
    init_shared_mutex(shm);
    shm->seq = 0;
    strncpy(shm->msg, "(init)", sizeof(shm->msg));

    printf("[parent pid=%-6d] mmap OK at %p\n", getpid(), (void *)shm);

    pid_t child = fork();
    if (child < 0) { perror("fork"); return 1; }

    if (child == 0) {
        /* ---- CHILD: reads shared region 5 times ------------------ */
        for (int i = 0; i < 5; i++) {
            usleep(300 * 1000);   /* let parent write first each round */
            pthread_mutex_lock(&shm->lock);
            printf("[child  pid=%-6d] read  seq=%-2d msg='%s'\n",
                   getpid(), shm->seq, shm->msg);
            pthread_mutex_unlock(&shm->lock);
        }
        munmap(shm, MMAP_DRV_BUF_SIZE);
        close(fd);
        return 0;
    }

    /* ---- PARENT: writes shared region 5 times -------------------- */
    for (int i = 1; i <= 5; i++) {
        pthread_mutex_lock(&shm->lock);
        shm->seq = i;
        snprintf(shm->msg, sizeof(shm->msg),
                 "hello from parent, round %d", i);
        printf("[parent pid=%-6d] wrote seq=%-2d\n", getpid(), shm->seq);
        pthread_mutex_unlock(&shm->lock);
        usleep(500 * 1000);
    }

    /* Ask kernel to dump the shared buffer — proves zero-copy:
     * the kernel reads the exact bytes the userspace process wrote,
     * no read()/write() syscall was involved.                        */
    printf("[parent] triggering MMAP_DRV_IOC_DUMP → check dmesg\n");
    ioctl(fd, MMAP_DRV_IOC_DUMP);

    waitpid(child, NULL, 0);
    munmap(shm, MMAP_DRV_BUF_SIZE);
    close(fd);
    return 0;
}
