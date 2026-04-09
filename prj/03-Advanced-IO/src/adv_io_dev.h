/*
 * src/adv_io_dev.h — Shared header for advanced IO paradigms demo
 */

#ifndef __ADV_IO_DEV_H__
#define __ADV_IO_DEV_H__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/uio.h>

#define DRV_NAME       "adv_io"
#define DRV_CLASS      "adv_io_class"
#define RING_SIZE      256
#define ADV_IO_PRODUCE_MS 500

/* ── Device state ─────────────────────────────────────────────── */

struct adv_io_dev {
	/* ring buffer */
	u8                  ring[RING_SIZE];
	unsigned int        head;   /* producer index */
	unsigned int        tail;   /* consumer index */
	unsigned int        count;  /* bytes currently in ring */

	/* sync */
	spinlock_t          ring_lock;
	struct mutex        open_mtx;
	int                 open_cnt;

	/* wait queues — split read/write to avoid thundering herd */
	wait_queue_head_t   read_wq;
	wait_queue_head_t   write_wq;

	/* async/SIGIO subscribers */
	struct fasync_struct *fasync_q;

	/* periodic producer */
	struct delayed_work producer_work;

	/* cdev plumbing */
	dev_t               devno;
	struct cdev         cdev;
	struct class       *cls;
	struct device      *dev;
};

/* ── Shared logic ───────────────────────── */

/* Ring helpers (must hold ring_lock) */
static inline bool ring_empty(struct adv_io_dev *d) { return d->count == 0; }
static inline bool ring_full (struct adv_io_dev *d) { return d->count == RING_SIZE; }

unsigned int ring_pop(struct adv_io_dev *d, u8 *out, unsigned int len);
unsigned int ring_push(struct adv_io_dev *d, const u8 *in, unsigned int len);

/* Workqueue producer */
void adv_io_producer(struct work_struct *work);

/* File operations */
extern const struct file_operations adv_io_fops;

#endif /* __ADV_IO_DEV_H__ */
