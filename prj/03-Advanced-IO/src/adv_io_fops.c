/*
 * src/adv_io_fops.c — File operations for advanced IO demo
 */

#include "adv_io_dev.h"

/* ── fops implementations ─────────────────────────────────────── */

static int adv_io_fasync(int fd, struct file *file, int on);

static int adv_io_open(struct inode *inode, struct file *file)
{
	struct adv_io_dev *d = container_of(inode->i_cdev,
					    struct adv_io_dev, cdev);
	file->private_data = d;

	mutex_lock(&d->open_mtx);
	d->open_cnt++;
	mutex_unlock(&d->open_mtx);
	return 0;
}

static int adv_io_release(struct inode *inode, struct file *file)
{
	/*
	 * MANDATORY fasync cleanup:
	 * If the application closes the fd or crashes without explicitly clearing O_ASYNC,
	 * its node remains in d->fasync_q. We MUST call fasync_helper(..., 0) to remove it.
	 * Failing to do so causes kill_fasync() to traverse a stale pointer (use-after-free) 
	 * on the next data arrival, triggering a kernel panic.
	 */
	adv_io_fasync(-1, file, 0);

	if (file->private_data) {
		struct adv_io_dev *d = file->private_data;
		mutex_lock(&d->open_mtx);
		d->open_cnt--;
		mutex_unlock(&d->open_mtx);
	}
	return 0;
}

static ssize_t adv_io_do_read(struct adv_io_dev *d, struct file *file,
			      char __user *ubuf, size_t len)
{
	u8 tmp[RING_SIZE];
	unsigned int got;
	unsigned long flags;
	int ret;

	if (len == 0)
		return 0;
	if (len > RING_SIZE)
		len = RING_SIZE;

	for (;;) {
		spin_lock_irqsave(&d->ring_lock, flags);
		got = ring_pop(d, tmp, len);
		spin_unlock_irqrestore(&d->ring_lock, flags);

		if (got)
			break;

		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;

		ret = wait_event_interruptible(d->read_wq,
					       READ_ONCE(d->count) > 0);
		if (ret)
			return -ERESTARTSYS;
	}

	if (copy_to_user(ubuf, tmp, got))
		return -EFAULT;

	/* a slot freed up — wake writers */
	wake_up_interruptible(&d->write_wq);
	kill_fasync(&d->fasync_q, SIGIO, POLL_OUT);
	return got;
}

static ssize_t adv_io_do_write(struct adv_io_dev *d, struct file *file,
			       const char __user *ubuf, size_t len)
{
	u8 tmp[RING_SIZE];
	unsigned int put;
	unsigned long flags;
	int ret;

	if (len == 0)
		return 0;
	if (len > RING_SIZE)
		len = RING_SIZE;

	if (copy_from_user(tmp, ubuf, len))
		return -EFAULT;

	for (;;) {
		spin_lock_irqsave(&d->ring_lock, flags);
		put = ring_push(d, tmp, len);
		spin_unlock_irqrestore(&d->ring_lock, flags);

		if (put)
			break;

		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;

		ret = wait_event_interruptible(d->write_wq,
					       READ_ONCE(d->count) < RING_SIZE);
		if (ret)
			return -ERESTARTSYS;
	}

	wake_up_interruptible(&d->read_wq);
	kill_fasync(&d->fasync_q, SIGIO, POLL_IN);
	return put;
}

static ssize_t adv_io_read(struct file *file, char __user *buf,
			   size_t len, loff_t *off)
{
	return adv_io_do_read(file->private_data, file, buf, len);
}

static ssize_t adv_io_write(struct file *file, const char __user *buf,
			    size_t len, loff_t *off)
{
	return adv_io_do_write(file->private_data, file, buf, len);
}

static ssize_t adv_io_read_iter(struct kiocb *iocb, struct iov_iter *to)
{
	struct file       *file = iocb->ki_filp;
	struct adv_io_dev *d    = file->private_data;
	ssize_t            total = 0;
	u8                 tmp[RING_SIZE];
	unsigned long      flags;
	int                ret;

	while (iov_iter_count(to)) {
		size_t want = min_t(size_t, iov_iter_count(to), RING_SIZE);
		unsigned int got;

		spin_lock_irqsave(&d->ring_lock, flags);
		got = ring_pop(d, tmp, want);
		spin_unlock_irqrestore(&d->ring_lock, flags);

		if (!got) {
			if (total)
				break;
			if (file->f_flags & O_NONBLOCK)
				return -EAGAIN;
			ret = wait_event_interruptible(d->read_wq,
						       READ_ONCE(d->count) > 0);
			if (ret)
				return -ERESTARTSYS;
			continue;
		}

		if (copy_to_iter(tmp, got, to) != got)
			return -EFAULT;
		total += got;

		wake_up_interruptible(&d->write_wq);
		kill_fasync(&d->fasync_q, SIGIO, POLL_OUT);
	}
	return total;
}

static ssize_t adv_io_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
	struct file       *file = iocb->ki_filp;
	struct adv_io_dev *d    = file->private_data;
	ssize_t            total = 0;
	u8                 tmp[RING_SIZE];
	unsigned long      flags;
	int                ret;

	while (iov_iter_count(from)) {
		size_t want = min_t(size_t, iov_iter_count(from), RING_SIZE);
		size_t copied;
		unsigned int put;

		copied = copy_from_iter(tmp, want, from);
		if (copied != want)
			return -EFAULT;

		for (;;) {
			spin_lock_irqsave(&d->ring_lock, flags);
			put = ring_push(d, tmp, copied);
			spin_unlock_irqrestore(&d->ring_lock, flags);

			if (put)
				break;
			if (total)
				return total;
			if (file->f_flags & O_NONBLOCK)
				return -EAGAIN;
			ret = wait_event_interruptible(d->write_wq,
						       READ_ONCE(d->count) < RING_SIZE);
			if (ret)
				return -ERESTARTSYS;
		}
		total += put;

		wake_up_interruptible(&d->read_wq);
		kill_fasync(&d->fasync_q, SIGIO, POLL_IN);

		if (put < copied) {
			break;
		}
	}
	return total;
}

static unsigned int adv_io_poll(struct file *file, poll_table *wait)
{
	struct adv_io_dev *d = file->private_data;
	unsigned int mask = 0;
	unsigned long flags;

	poll_wait(file, &d->read_wq,  wait);
	poll_wait(file, &d->write_wq, wait);

	spin_lock_irqsave(&d->ring_lock, flags);
	if (d->count > 0)
		mask |= POLLIN  | POLLRDNORM;
	if (d->count < RING_SIZE)
		mask |= POLLOUT | POLLWRNORM;
	spin_unlock_irqrestore(&d->ring_lock, flags);

	return mask;
}

static int adv_io_fasync(int fd, struct file *file, int on)
{
	struct adv_io_dev *d = file->private_data;
	return fasync_helper(fd, file, on, &d->fasync_q);
}

const struct file_operations adv_io_fops = {
	.owner      = THIS_MODULE,
	.open       = adv_io_open,
	.release    = adv_io_release,
	.read       = adv_io_read,
	.write      = adv_io_write,
	.read_iter  = adv_io_read_iter,
	.write_iter = adv_io_write_iter,
	.poll       = adv_io_poll,
	.fasync     = adv_io_fasync,
	.llseek     = no_llseek,
};
