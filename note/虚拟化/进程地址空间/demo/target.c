/*
 * target.c — 进程地址空间观察靶子
 *
 * 在每一个常见段中都"种下"一个可观测地址，并打印出来；
 * 然后调用 vDSO（clock_gettime）两次给出耗时，最后 sleep 让分析器抓 maps。
 *
 * 编译: gcc -O0 -no-pie -o target target.c   (no-pie 让 .text 起始地址更直观)
 *       gcc -O0        -o target target.c   (默认 PIE，验证 ASLR)
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/syscall.h>

/* ---------- 各段的"种子" ---------- */
int           g_data_init = 0xC0DE;          /* .data  : 已初始化全局     */
int           g_bss_uninit;                  /* .bss   : 未初始化全局     */
static int    g_static_init = 0xBEEF;        /* .data  : 文件作用域 static */
const char    g_rodata_str[] = "RODATA-SEED"; /* .rodata: 只读数据         */
const int     g_rodata_int = 0xCAFE;         /* .rodata                   */

void plant_text(void) {                      /* .text  : 函数代码         */
    /* 防止被优化掉 */
    __asm__ volatile("" ::: "memory");
}

/* ---------- vDSO 加速观察 ---------- */
static long long ns_diff(struct timespec a, struct timespec b) {
    return (b.tv_sec - a.tv_sec) * 1000000000LL + (b.tv_nsec - a.tv_nsec);
}

static void bench_clock_gettime(int rounds) {
    struct timespec t0, t1, ts;

    /* (a) glibc 路径 → 走 vDSO */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < rounds; i++)
        clock_gettime(CLOCK_MONOTONIC, &ts);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    long long vdso_ns = ns_diff(t0, t1);

    /* (b) 强制走真正的 syscall (绕过 vDSO) */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < rounds; i++)
        syscall(SYS_clock_gettime, CLOCK_MONOTONIC, &ts);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    long long sys_ns = ns_diff(t0, t1);

    printf("[bench] %d rounds:\n", rounds);
    printf("  vDSO  clock_gettime    avg = %6.1f ns/call\n", (double)vdso_ns / rounds);
    printf("  raw   syscall(...)     avg = %6.1f ns/call   (×%.1f slower)\n",
           (double)sys_ns / rounds, (double)sys_ns / vdso_ns);
}

int main(void) {
    /* 局部变量 → 栈 */
    int  l_stack_var = 0xBABE;
    char l_stack_buf[64];
    strcpy(l_stack_buf, "STACK-SEED");

    /* 小块分配 → 走 brk(),  落在 [heap]  */
    char *small = malloc(64);
    strcpy(small, "HEAP-SEED");

    /* 大块分配 → glibc 走 mmap(MAP_ANON), 落在 mmap 区, 不在 [heap] */
    size_t big_sz = 2 * 1024 * 1024; /* 2MiB ≥ M_MMAP_THRESHOLD(128KiB) */
    char *big = mmap(NULL, big_sz, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (big == MAP_FAILED) { perror("mmap"); return 1; }
    strcpy(big, "MMAP-SEED");

    /* 函数指针 → libc .text */
    void *libc_fn = (void *)&printf;

    printf("=== target PID = %d ===\n", getpid());
    printf("  .text   plant_text     = %p\n", (void *)&plant_text);
    printf("  .text   main           = %p\n", (void *)&main);
    printf("  .rodata g_rodata_str   = %p  (\"%s\")\n", (void *)g_rodata_str, g_rodata_str);
    printf("  .rodata g_rodata_int   = %p\n", (void *)&g_rodata_int);
    printf("  .data   g_data_init    = %p\n", (void *)&g_data_init);
    printf("  .data   g_static_init  = %p\n", (void *)&g_static_init);
    printf("  .bss    g_bss_uninit   = %p\n", (void *)&g_bss_uninit);
    printf("  heap    malloc(64)     = %p\n", (void *)small);
    printf("  mmap    mmap(2MiB)     = %p\n", (void *)big);
    printf("  stack   l_stack_var    = %p\n", (void *)&l_stack_var);
    printf("  stack   l_stack_buf    = %p\n", (void *)l_stack_buf);
    printf("  libc    &printf        = %p\n", libc_fn);
    printf("\n");

    /* vDSO 性能观察 */
    bench_clock_gettime(1000000);

    /* 自己 dump [vdso] → /tmp/vdso.so，避免 analyzer 需要特权 */
    {
        FILE *fm = fopen("/proc/self/maps", "r");
        char line[512];
        unsigned long lo = 0, hi = 0;
        while (fm && fgets(line, sizeof line, fm)) {
            if (strstr(line, "[vdso]")) { sscanf(line, "%lx-%lx", &lo, &hi); break; }
        }
        if (fm) fclose(fm);
        if (lo) {
            size_t sz = hi - lo;
            unsigned char *buf = malloc(sz);
            FILE *fmem = fopen("/proc/self/mem", "rb");
            fseek(fmem, lo, SEEK_SET);
            size_t n = fread(buf, 1, sz, fmem);
            fclose(fmem);
            FILE *out = fopen("/tmp/vdso.so", "wb");
            fwrite(buf, 1, n, out);
            fclose(out);
            printf("[dump] vDSO -> /tmp/vdso.so  (%zu bytes, magic=%02x %02x %02x %02x \"%c%c%c\")\n",
                   n, buf[0], buf[1], buf[2], buf[3], buf[1], buf[2], buf[3]);
            free(buf);
        }
    }

    printf("\nsleeping 60s, run:  sudo ./as_analyzer.py %d\n", getpid());
    fflush(stdout);
    sleep(60);

    free(small);
    munmap(big, big_sz);
    return 0;
}
