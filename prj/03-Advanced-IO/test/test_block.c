/* test_block.c — blocking read demo for /dev/adv_io
 *
 * Opens the device WITHOUT O_NONBLOCK and issues a blocking read().
 * Driver workqueue produces 1 byte every 500 ms, so the read should
 * unblock as soon as the first byte lands.
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define DEV "/dev/adv_io"

int main(void)
{
	int fd = open(DEV, O_RDWR);
	if (fd < 0) { perror("open"); return 1; }

	printf("[block] waiting (blocking read)...\n");
	for (int i = 0; i < 5; i++) {
		unsigned char buf[16];
		ssize_t n = read(fd, buf, sizeof(buf));
		if (n < 0) { perror("read"); break; }
		printf("[block] got %zd byte(s):", n);
		for (ssize_t k = 0; k < n; k++) printf(" %02x", buf[k]);
		printf("\n");
	}
	close(fd);
	return 0;
}
