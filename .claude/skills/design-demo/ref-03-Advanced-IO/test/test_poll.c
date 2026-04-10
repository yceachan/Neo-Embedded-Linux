/* test_poll.c — IO multiplexing demo using poll(2)
 *
 * Demonstrates O_NONBLOCK + poll() against /dev/adv_io. The driver
 * implements .poll, returning POLLIN once the ring is non-empty.
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <string.h>

#define DEV "/dev/adv_io"

int main(void)
{
	int fd = open(DEV, O_RDWR | O_NONBLOCK);
	if (fd < 0) { perror("open"); return 1; }

	struct pollfd pfd = { .fd = fd, .events = POLLIN };

	for (int i = 0; i < 5; i++) {
		printf("[poll] waiting (timeout 2000 ms)...\n");
		int r = poll(&pfd, 1, 2000);
		if (r < 0) { perror("poll"); break; }
		if (r == 0) { printf("[poll] timeout\n"); continue; }
		if (pfd.revents & POLLIN) {
			unsigned char buf[32];
			ssize_t n = read(fd, buf, sizeof(buf));
			printf("[poll] POLLIN, read %zd byte(s):", n);
			for (ssize_t k = 0; k < n; k++) printf(" %02x", buf[k]);
			printf("\n");
		}
	}
	close(fd);
	return 0;
}
