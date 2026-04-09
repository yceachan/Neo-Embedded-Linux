/*
 * src/adv_io_dev.c — Ring helpers and workqueue producer
 */

#include "adv_io_dev.h"

/* ── Ring helpers (must hold ring_lock) ───────────────────────── */

/* Pop up to len bytes into kernel temp buffer; returns bytes popped. */
unsigned int ring_pop(struct adv_io_dev *d, u8 *out, unsigned int len)
{
	unsigned int n = 0;
	while (n < len && d->count) {
		out[n++] = d->ring[d->tail];
		d->tail = (d->tail + 1) % RING_SIZE;
		d->count--;
	}
	return n;
}

/* Push up to len bytes from kernel temp buffer; returns bytes pushed. */
unsigned int ring_push(struct adv_io_dev *d, const u8 *in, unsigned int len)
{
	unsigned int n = 0;
	while (n < len && d->count < RING_SIZE) {
		d->ring[d->head] = in[n++];
		d->head = (d->head + 1) % RING_SIZE;
		d->count++;
	}
	return n;
}

/* ── Workqueue producer: simulate "hardware" producing data ───── */

void adv_io_producer(struct work_struct *work)
{
	struct delayed_work *dw = to_delayed_work(work);
	/*============================================================**
	*@READNME: container_of() 是 Linux 内核中一个非常常用的宏，用于从结构体成员的指针获取包含该成员的父结构体的指针。
	 1. ptr：已知结构体成员的指针（在我们的例子中是 dw，即 producer_work 的指针）。
     2. type：要反推出来的父结构体的类型（在我们的例子中是 struct adv_io_dev）。
     3. member：该成员在父结构体中的成员字段（在我们的例子中是 producer_work）。
	*=============================================================*/
	struct adv_io_dev   *d  = container_of(dw, struct adv_io_dev, producer_work);
	unsigned long flags;
	bool became_readable = false;
	static u8 seq;
	u8 byte = seq++;

	spin_lock_irqsave(&d->ring_lock, flags);
	if (!ring_full(d)) {
		bool was_empty = ring_empty(d);
		ring_push(d, &byte, 1);
		if (was_empty)
			became_readable = true;
	}
	spin_unlock_irqrestore(&d->ring_lock, flags);

	if (became_readable) {
		/* wake blocked readers / pollers */
		wake_up_interruptible(&d->read_wq);
		/* SIGIO to fasync subscribers — kill_fasync has its own
		 * locking, must NOT be called inside a spinlock. */
		kill_fasync(&d->fasync_q, SIGIO, POLL_IN);
	}

	/* re-arm */
	schedule_delayed_work(&d->producer_work,
			      msecs_to_jiffies(ADV_IO_PRODUCE_MS));
}
