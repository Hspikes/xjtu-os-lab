#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/atomic.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/err.h>

#define M2M_DEVICE_NAME "m2mchardev"
#define M2M_CLASS_NAME "m2mchardev_class"
#define MAX_BYTES (64 * 1024)
#define MAX_MSG (4 * 1024)

static int dev_major = 0;

struct msg
{
    struct list_head list;
    size_t len;
    char *data;
};

struct mydev
{
    struct cdev devm;
    dev_t devt;
    struct class *class;
    struct device *device;

    struct mutex lock;
    spinlock_t stats_lock;

    struct list_head msg_list;
    size_t queued_bytes;

    wait_queue_head_t readq;
    wait_queue_head_t writeq;

    atomic_t readers;
    atomic_t writers;
};

static struct mydev m2m_chardev;

struct client_ctx
{
    struct mydev *dev;
    bool is_reader;
    bool is_writer;
};

static int m2m_open(struct inode *inode, struct file *filp);
static int m2m_release(struct inode *inode, struct file *filp);

static ssize_t m2m_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos);
static ssize_t m2m_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos);
static unsigned int m2m_poll(struct file *filp, poll_table *wait);

static const struct file_operations m2m_fops =
{
        .owner = THIS_MODULE,
        .open = m2m_open,
        .release = m2m_release,
        .read = m2m_read,
        .write = m2m_write,
        .poll = m2m_poll,
}; // 注册

static struct msg *msg_alloc(size_t len)
{
    struct msg *m;
    m = kmalloc(sizeof(*m), GFP_KERNEL);
    if (!m) return NULL;
    m->data = kmalloc(len, GFP_KERNEL);
    if (!m->data)
    {
        kfree(m);
        return NULL;
    }
    INIT_LIST_HEAD(&m->list);
    m->len = len;
    return m;
}

static void msg_free(struct msg *m)
{
    if (!m) return;
    kfree(m->data);
    kfree(m);
}

static int m2m_open(struct inode *inode, struct file *filp)
{
    struct client_ctx *ctx;
    ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
    if (!ctx) return -ENOMEM;

    ctx->dev = &m2m_chardev;
    ctx->is_reader = true;
    ctx->is_writer = true;
    filp->private_data = ctx;

    atomic_inc(&m2m_chardev.readers);
    atomic_inc(&m2m_chardev.writers);

    pr_info("%s: open: readers=%d writers=%d\n", M2M_DEVICE_NAME,
            atomic_read(&m2m_chardev.readers), atomic_read(&m2m_chardev.writers));
    return 0;
}

static int m2m_release(struct inode *inode, struct file *filp)
{
    struct client_ctx *ctx = filp->private_data;
    if (ctx)
    {
        atomic_dec(&m2m_chardev.readers);
        atomic_dec(&m2m_chardev.writers);
        kfree(ctx);
        filp->private_data = NULL;
    }
    pr_info("%s: release\n", M2M_DEVICE_NAME);
    return 0;
}

static ssize_t m2m_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos)
{
    struct client_ctx *ctx = filp->private_data;
    struct mydev *dev = ctx ? ctx->dev : &m2m_chardev;
    struct msg *m = NULL;
    ssize_t ret = 0;
    size_t to_copy;

    // 处理进程
    if ((filp->f_flags & O_NONBLOCK))
    {
        mutex_lock(&dev->lock);
        if (list_empty(&dev->msg_list))
        {
            mutex_unlock(&dev->lock);
            return -EAGAIN;
        }
        mutex_unlock(&dev->lock);
    }

    if (wait_event_interruptible(dev->readq, !list_empty(&dev->msg_list)))
        return -ERESTARTSYS;

    mutex_lock(&dev->lock);
    if (list_empty(&dev->msg_list))
    {
        mutex_unlock(&dev->lock);
        return -EAGAIN;
    }
    m = list_first_entry(&dev->msg_list, struct msg, list);
    list_del(&m->list);
    dev->queued_bytes -= m->len;
    mutex_unlock(&dev->lock);

    to_copy = min(count, m->len);
    if (copy_to_user(buf, m->data, to_copy))
    {
        ret = -EFAULT;
        msg_free(m);
        wake_up_interruptible(&dev->writeq);
        return ret;
    }

    ret = (ssize_t)to_copy;
    msg_free(m);

    /* wake writers that might be blocked due to full queue */
    wake_up_interruptible(&dev->writeq);
    return ret;
}

static ssize_t m2m_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
{
    struct client_ctx *ctx = filp->private_data;
    struct mydev *dev = ctx ? ctx->dev : &m2m_chardev;
    struct msg *m;
    ssize_t ret = 0;

    if (count == 0) return 0;
    if (count > MAX_MSG) return -EMSGSIZE;

    /* backpressure: if adding this message would exceed MAX_BYTES, wait (blocking) */
    while (1)
    {
        mutex_lock(&dev->lock);
        if (dev->queued_bytes + count <= MAX_BYTES)
        {
            mutex_unlock(&dev->lock);
            break;
        }
        mutex_unlock(&dev->lock);

        if (filp->f_flags & O_NONBLOCK) return -EAGAIN;

        if (wait_event_interruptible(dev->writeq, dev->queued_bytes + count <= MAX_BYTES))
            return -ERESTARTSYS;
    }

    m = msg_alloc(count);
    if (!m) return -ENOMEM;

    if (copy_from_user(m->data, buf, count))
    {
        msg_free(m);
        return -EFAULT;
    }

    mutex_lock(&dev->lock);
    list_add_tail(&m->list, &dev->msg_list);
    dev->queued_bytes += m->len;
    mutex_unlock(&dev->lock);

    wake_up_interruptible(&dev->readq);

    ret = (ssize_t)count;
    return ret;
}

static unsigned int m2m_poll(struct file *filp, poll_table *wait)
{
    struct client_ctx *ctx = filp->private_data;
    struct mydev *dev = ctx ? ctx->dev : &m2m_chardev;
    unsigned int mask = 0;

    poll_wait(filp, &dev->readq, wait);
    poll_wait(filp, &dev->writeq, wait);

    mutex_lock(&dev->lock);
    if (!list_empty(&dev->msg_list))
        mask |= POLLIN | POLLRDNORM;
    if (dev->queued_bytes < MAX_BYTES)
        mask |= POLLOUT | POLLWRNORM;
    mutex_unlock(&dev->lock);

    return mask;
}

static int __init m2m_init(void)
{
    int ret;
    dev_t devt;

    mutex_init(&m2m_chardev.lock);
    spin_lock_init(&m2m_chardev.stats_lock);
    INIT_LIST_HEAD(&m2m_chardev.msg_list);
    init_waitqueue_head(&m2m_chardev.readq);
    init_waitqueue_head(&m2m_chardev.writeq);
    atomic_set(&m2m_chardev.readers, 0);
    atomic_set(&m2m_chardev.writers, 0);
    m2m_chardev.queued_bytes = 0;

    if (dev_major)
    {
        devt = MKDEV(dev_major, 0);
        ret = register_chrdev_region(devt, 1, M2M_DEVICE_NAME);
    }
    else ret = alloc_chrdev_region(&devt, 0, 1, M2M_DEVICE_NAME);
    if (ret)
    {
        pr_err("%s: alloc/register chrdev failed: %d\n", M2M_DEVICE_NAME, ret);
        return ret;
    }

    m2m_chardev.devt = devt;
    cdev_init(&m2m_chardev.devm, &m2m_fops);
    m2m_chardev.devm.owner = THIS_MODULE;

    ret = cdev_add(&m2m_chardev.devm, devt, 1);
    if (ret)
    {
        pr_err("%s: cdev_add failed: %d\n", M2M_DEVICE_NAME, ret);
        unregister_chrdev_region(devt, 1);
        return ret;
    }

    m2m_chardev.class = class_create(THIS_MODULE, M2M_CLASS_NAME);
    if (IS_ERR(m2m_chardev.class))
    {
        pr_err("%s: class_create failed\n", M2M_DEVICE_NAME);
        cdev_del(&m2m_chardev.devm);
        unregister_chrdev_region(devt, 1);
        return PTR_ERR(m2m_chardev.class);
    }

    m2m_chardev.device = device_create(m2m_chardev.class, NULL, devt, NULL, M2M_DEVICE_NAME);
    if (IS_ERR(m2m_chardev.device))
    {
        pr_err("%s: device_create failed\n", M2M_DEVICE_NAME);
        class_destroy(m2m_chardev.class);
        cdev_del(&m2m_chardev.devm);
        unregister_chrdev_region(devt, 1);
        return PTR_ERR(m2m_chardev.device);
    }

    pr_info("%s: registered device %s (major=%d)\n", M2M_DEVICE_NAME, M2M_DEVICE_NAME, MAJOR(devt));
    return 0;
}

static void __exit m2m_exit(void)
{
    dev_t devt = m2m_chardev.devt;
    struct msg *m;
    struct list_head *pos, *n;

    if (!IS_ERR_OR_NULL(m2m_chardev.device))
        device_destroy(m2m_chardev.class, devt);
    if (!IS_ERR_OR_NULL(m2m_chardev.class))
        class_destroy(m2m_chardev.class);

    cdev_del(&m2m_chardev.devm);
    unregister_chrdev_region(devt, 1);

    /* free remaining messages */
    mutex_lock(&m2m_chardev.lock);
    list_for_each_safe(pos, n, &m2m_chardev.msg_list) 
    {
        m = list_entry(pos, struct msg, list);
        list_del(pos);
        msg_free(m);
    }
    mutex_unlock(&m2m_chardev.lock);

    pr_info("%s: unloaded\n", M2M_DEVICE_NAME);
}

module_init(m2m_init);
module_exit(m2m_exit);

MODULE_LICENSE("GPL");
// MODULE_AUTHOR("student");
// MODULE_DESCRIPTION("Simple m2m char device (distribute mode) for lab");