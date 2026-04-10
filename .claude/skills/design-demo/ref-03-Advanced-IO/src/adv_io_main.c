/*
 * src/adv_io_main.c — Module init/exit for advanced IO demo
 */

#include "adv_io_dev.h"

static struct adv_io_dev *gdev;

static int __init adv_io_init(void)
{
	int ret;

	gdev = kzalloc(sizeof(*gdev), GFP_KERNEL);
	if (!gdev)
		return -ENOMEM;

	spin_lock_init(&gdev->ring_lock);
	mutex_init(&gdev->open_mtx);
	init_waitqueue_head(&gdev->read_wq);
	init_waitqueue_head(&gdev->write_wq);
	INIT_DELAYED_WORK(&gdev->producer_work, adv_io_producer);

	ret = alloc_chrdev_region(&gdev->devno, 0, 1, DRV_NAME);
	if (ret < 0)
		goto err_free;

	cdev_init(&gdev->cdev, &adv_io_fops);
	gdev->cdev.owner = THIS_MODULE;
	ret = cdev_add(&gdev->cdev, gdev->devno, 1);
	if (ret)
		goto err_region;

	gdev->cls = class_create(THIS_MODULE, DRV_CLASS);
	if (IS_ERR(gdev->cls)) {
		ret = PTR_ERR(gdev->cls);
		goto err_cdev;
	}

	gdev->dev = device_create(gdev->cls, NULL, gdev->devno, NULL, DRV_NAME);
	if (IS_ERR(gdev->dev)) {
		ret = PTR_ERR(gdev->dev);
		goto err_class;
	}

	schedule_delayed_work(&gdev->producer_work,
			      msecs_to_jiffies(ADV_IO_PRODUCE_MS));

	pr_info("%s: loaded, /dev/%s major=%d\n",
		DRV_NAME, DRV_NAME, MAJOR(gdev->devno));
	return 0;

err_class:
	class_destroy(gdev->cls);
err_cdev:
	cdev_del(&gdev->cdev);
err_region:
	unregister_chrdev_region(gdev->devno, 1);
err_free:
	kfree(gdev);
	gdev = NULL;
	return ret;
}

static void __exit adv_io_exit(void)
{
	if (gdev) {
		cancel_delayed_work_sync(&gdev->producer_work);
		device_destroy(gdev->cls, gdev->devno);
		class_destroy(gdev->cls);
		cdev_del(&gdev->cdev);
		unregister_chrdev_region(gdev->devno, 1);
		kfree(gdev);
	}
	pr_info("%s: unloaded\n", DRV_NAME);
}

module_init(adv_io_init);
module_exit(adv_io_exit);

MODULE_AUTHOR("Neo-Embedded-Linux");
MODULE_DESCRIPTION("Advanced IO paradigms demo char device (block/nonblock/poll/fasync/iter)");
MODULE_LICENSE("GPL");
