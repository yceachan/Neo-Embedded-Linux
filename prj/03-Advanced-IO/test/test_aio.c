/* test_aio.c — POSIX AIO demo using glibc librt
 *
 * Submits an aio_read() against /dev/adv_io. glibc POSIX AIO is
 * implemented via a user-space thread pool, but it ultimately calls
 * pread()/read(), which on our driver lands in adv_io_read (or
 * read_iter for io_submit-based paths). This is enough to validate
 * the iter / async submission story end-to-end.
 *
 * Link with -lrt.
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <aio.h>
#include <errno.h>
#include <string.h>

#define DEV "/dev/adv_io"

int main(void)
{
	int fd = open(DEV, O_RDWR);
	if (fd < 0) { perror("open"); return 1; }

	unsigned char buf[16] = {0};
	struct aiocb cb;
	memset(&cb, 0, sizeof(cb));
	cb.aio_fildes = fd;
	cb.aio_buf    = buf;
	cb.aio_nbytes = sizeof(buf);
	cb.aio_offset = 0;

	if (aio_read(&cb) < 0) { perror("aio_read"); return 1; }
	printf("[aio] submitted, doing other work...\n");

	/* poll completion (a real app would use SIGEV_SIGNAL/SIGEV_THREAD) */
	while (aio_error(&cb) == EINPROGRESS) {
		printf("[aio] ...still in progress\n");
		usleep(300 * 1000);
	}

	ssize_t n = aio_return(&cb);
	if (n < 0) { perror("aio_return"); return 1; }
	printf("[aio] completed, %zd byte(s):", n);
	for (ssize_t k = 0; k < n; k++) printf(" %02x", buf[k]);
	printf("\n");

	close(fd);
	return 0;
}
