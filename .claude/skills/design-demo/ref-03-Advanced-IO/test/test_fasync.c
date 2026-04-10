/* test_fasync.c — signal-driven IO (SIGIO) demo
 *
 * Registers SIGIO handler, sets F_SETOWN + O_ASYNC, then sleeps in
 * pause(). The driver's workqueue producer triggers kill_fasync()
 * each time a byte transitions the ring from empty to non-empty.
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#define DEV "/dev/adv_io"

static int g_fd;
static volatile int g_count;

static void sigio_handler(int sig)
{
	unsigned char buf[32];
	ssize_t n = read(g_fd, buf, sizeof(buf));
	if (n > 0) {
		printf("[SIGIO] got %zd byte(s):", n);
		for (ssize_t k = 0; k < n; k++) printf(" %02x", buf[k]);
		printf("\n");
		
		g_count++;

		if (g_count >= 5) {
			int flags = fcntl(g_fd, F_GETFL);
			fcntl(g_fd, F_SETFL, flags & ~O_ASYNC);
		}
	}
}

int main(void)
{
	g_fd = open(DEV, O_RDWR | O_NONBLOCK);
	if (g_fd < 0) { perror("open"); return 1; }

	struct sigaction sa = { .sa_handler = sigio_handler };
	sigemptyset(&sa.sa_mask);
	sigaction(SIGIO, &sa, NULL);

	if (fcntl(g_fd, F_SETOWN, getpid()) < 0) { perror("F_SETOWN"); return 1; }
	int flags = fcntl(g_fd, F_GETFL);
	if (fcntl(g_fd, F_SETFL, flags | O_ASYNC) < 0) { perror("F_SETFL O_ASYNC"); return 1; }

	printf("[fasync] waiting for SIGIO (5 events)...\n");
	while (g_count < 5)
		pause();

	close(g_fd);
	return 0;
}
