/*
 * Includes
 */
#include <linux/kernel.h>	/* printk() */
#include <linux/module.h>	/* modules */
#include <linux/fs.h>           /* file_operations */
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>

MODULE_DESCRIPTION("mydriver1");
MODULE_AUTHOR("Marc Chalain, Smile ECS");
MODULE_LICENSE("GPL");

/*
 * Arguments
 */
static struct miscdevice mymisc;

static int gpio_nr = 21;
module_param(gpio_nr, int, 0644);

static raw_spinlock_t myspinlock;
static DECLARE_WAIT_QUEUE_HEAD(irq_wait_queue);
static int clic = 0;
static irqreturn_t my_irq_handler(int irq, void * ident)
{
	unsigned long dummyflags;
	raw_spin_lock_irqsave(&myspinlock, dummyflags);
	if (!clic)
	{
		clic ++;
		wake_up(&irq_wait_queue);
	}
	raw_spin_unlock_irqrestore(&myspinlock, dummyflags);
	return IRQ_HANDLED;
}

/*
 * File operations
 */
static ssize_t my_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	unsigned long dummyflags;
	while (clic == 0)
		wait_event_interruptible(irq_wait_queue, clic);
	raw_spin_lock_irqsave(&myspinlock, dummyflags);
	count = sprintf(buf, "%d\n", clic);
	clic = 0;
	raw_spin_unlock_irqrestore(&myspinlock, dummyflags);
	*ppos += count;
	return count;
}

static ssize_t my_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	count = 0;
	return count;
}

static int my_open(struct inode *inode, struct file *file)
{
	int ret;
	printk(KERN_INFO "my char driver: open()\n");
	ret = request_irq(gpio_to_irq(gpio_nr), my_irq_handler, IRQF_SHARED | IRQF_TRIGGER_RISING, THIS_MODULE->name, THIS_MODULE->name);
	return ret;
}

static int my_release(struct inode *inode, struct file *file)
{
	printk(KERN_INFO "my char driver: release()\n");
	free_irq(gpio_to_irq(gpio_nr), THIS_MODULE->name);

	return 0;
}

static struct file_operations my_fops = {
	.owner =	THIS_MODULE,
	.read =		my_read,
	.write =	my_write,
	.open =		my_open,
	.release =	my_release,
};

static int __init my_init(void)
{
	int ret = 0;

	mymisc.minor = MISC_DYNAMIC_MINOR;
	mymisc.name = "mydriver";
	mymisc.fops = &my_fops;
	ret = misc_register(&mymisc);

	raw_spin_lock_init(&myspinlock);

	ret = gpio_request(gpio_nr, THIS_MODULE->name);
	ret = gpio_direction_input(gpio_nr);
	if (ret)
		pr_info("error\n");
	return 0;
}

static void __exit my_exit(void)
{
	gpio_free(gpio_nr);
	misc_deregister(&mymisc);
}

/*
 * Module entry points
 */
module_init(my_init);
module_exit(my_exit);
