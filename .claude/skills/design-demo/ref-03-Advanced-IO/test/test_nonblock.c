/* test_nonblock.c — open /dev/adv_io with O_NONBLOCK, expect EAGAIN on empty ring */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

int main(void)
{
	char buf[16];
	int fd = open("/dev/adv_io", O_RDONLY | O_NONBLOCK);
	if (fd < 0) { perror("open"); return 2; }

	/* drain whatever the producer may have already queued */
	while (read(fd, buf, sizeof(buf)) > 0)
		;

	/* now ring should be empty → expect EAGAIN */
	ssize_t n = read(fd, buf, sizeof(buf));
	if (n < 0 && errno == EAGAIN) {
		printf("nonblock: got EAGAIN as expected\n");
		close(fd);
		return 0;
	}
	printf("nonblock: unexpected n=%zd errno=%d (%s)\n", n, errno, strerror(errno));
	close(fd);
	return 1;
}
