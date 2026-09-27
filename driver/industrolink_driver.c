#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#define DEVICE_NAME "industrolink"
#define CLASS_NAME "industrolink_class"
#define BUFFER_SIZE 256

#define INDUSTROLINK_IOCTL_RESET \
    _IO('I', 1)

#define INDUSTROLINK_IOCTL_GET_SIZE \
    _IOR('I', 2, int)

/* Wait queue ioctl */
#define INDUSTROLINK_IOCTL_WAIT_EVENT \
    _IO('I', 3)

static dev_t dev_number;
static struct cdev industrolink_cdev;
static struct class *industrolink_class;
static struct device *industrolink_device;

static char device_buffer[BUFFER_SIZE];
static size_t buffer_size;

static DEFINE_MUTEX(buffer_mutex);

/* Telemetry wait queue */
static DECLARE_WAIT_QUEUE_HEAD(telemetry_wait_queue);
static bool telemetry_event;

/* Telemetry timer */
static struct timer_list telemetry_timer;


/*
 * Timer callback
 *
 * The timer fires every 5 seconds.
 * It sets telemetry_event and wakes processes
 * sleeping on the telemetry wait queue.
 */
static void industrolink_timer_callback(struct timer_list *timer)
{
    static int temperature = 70;
    static int pressure = 100;

    mutex_lock(&buffer_mutex);

    buffer_size = scnprintf(device_buffer,
                            BUFFER_SIZE,
                            "Temperature=%d C, Pressure=%d kPa\n",
                            temperature,
                            pressure);

    mutex_unlock(&buffer_mutex);

    pr_info("IndustroLink: telemetry timer event - %s",
            device_buffer);

    temperature += 2;
    pressure += 3;

    telemetry_event = true;

    wake_up_interruptible(&telemetry_wait_queue);

    mod_timer(&telemetry_timer,
              jiffies + msecs_to_jiffies(5000));
}

/*
 * Device open
 */
static int industrolink_open(struct inode *inode,
                             struct file *file)
{
    pr_info("IndustroLink: device opened\n");

    return 0;
}


/*
 * Device release
 */
static int industrolink_release(struct inode *inode,
                                struct file *file)
{
    pr_info("IndustroLink: device closed\n");

    return 0;
}


/*
 * Device read
 */
static ssize_t industrolink_read(struct file *file,
                                 char __user *user_buffer,
                                 size_t count,
                                 loff_t *offset)
{
    size_t bytes_to_copy;

    mutex_lock(&buffer_mutex);

    if (*offset >= buffer_size) {
        mutex_unlock(&buffer_mutex);
        return 0;
    }

    bytes_to_copy = min(count,
                        buffer_size - (size_t)*offset);

    if (copy_to_user(user_buffer,
                     device_buffer + *offset,
                     bytes_to_copy)) {
        mutex_unlock(&buffer_mutex);
        return -EFAULT;
    }

    *offset += bytes_to_copy;

    mutex_unlock(&buffer_mutex);

    pr_info("IndustroLink: read %zu bytes\n",
            bytes_to_copy);

    return bytes_to_copy;
}


/*
 * Device write
 */
static ssize_t industrolink_write(struct file *file,
                                  const char __user *user_buffer,
                                  size_t count,
                                  loff_t *offset)
{
    size_t bytes_to_copy;

    bytes_to_copy = min(count,
                        (size_t)(BUFFER_SIZE - 1));

    mutex_lock(&buffer_mutex);

    memset(device_buffer, 0, BUFFER_SIZE);

    if (copy_from_user(device_buffer,
                       user_buffer,
                       bytes_to_copy)) {
        mutex_unlock(&buffer_mutex);
        return -EFAULT;
    }

    device_buffer[bytes_to_copy] = '\0';
    buffer_size = bytes_to_copy;

    mutex_unlock(&buffer_mutex);

    pr_info("IndustroLink: wrote %zu bytes\n",
            bytes_to_copy);

    return bytes_to_copy;
}


/*
 * ioctl handler
 */
static long industrolink_ioctl(struct file *file,
                               unsigned int command,
                               unsigned long arg)
{
    int size;

    switch (command) {

    /*
     * Reset driver buffer
     */
    case INDUSTROLINK_IOCTL_RESET:

        mutex_lock(&buffer_mutex);

        memset(device_buffer, 0, BUFFER_SIZE);
        buffer_size = 0;

        mutex_unlock(&buffer_mutex);

        pr_info("IndustroLink: ioctl RESET successful\n");

        return 0;


    /*
     * Get current buffer size
     */
    case INDUSTROLINK_IOCTL_GET_SIZE:

        mutex_lock(&buffer_mutex);

        size = (int)buffer_size;

        mutex_unlock(&buffer_mutex);

        if (copy_to_user((int __user *)arg,
                         &size,
                         sizeof(size))) {
            return -EFAULT;
        }

        pr_info("IndustroLink: ioctl GET_SIZE = %d\n",
                size);

        return 0;


    /*
     * Wait for telemetry timer event
     *
     * The userspace process sleeps here until
     * the kernel timer wakes it.
     */
    case INDUSTROLINK_IOCTL_WAIT_EVENT:

        pr_info("IndustroLink: waiting for telemetry event\n");

        if (wait_event_interruptible(
                telemetry_wait_queue,
                telemetry_event)) {
            return -ERESTARTSYS;
        }

        telemetry_event = false;

        pr_info("IndustroLink: telemetry event received\n");

        return 0;


    default:

        return -ENOTTY;
    }
}


/*
 * File operations
 */
static const struct file_operations industrolink_fops = {
    .owner = THIS_MODULE,
    .open = industrolink_open,
    .release = industrolink_release,
    .read = industrolink_read,
    .write = industrolink_write,
    .unlocked_ioctl = industrolink_ioctl,
};


/*
 * Driver initialization
 */
static int __init industrolink_init(void)
{
    int ret;

    /*
     * Allocate major/minor number
     */
    ret = alloc_chrdev_region(&dev_number,
                              0,
                              1,
                              DEVICE_NAME);

    if (ret < 0) {
        pr_err("IndustroLink: failed to allocate device number\n");
        return ret;
    }


    /*
     * Initialize character device
     */
    cdev_init(&industrolink_cdev,
              &industrolink_fops);

    industrolink_cdev.owner = THIS_MODULE;


    /*
     * Add character device
     */
    ret = cdev_add(&industrolink_cdev,
                   dev_number,
                   1);

    if (ret < 0) {
        unregister_chrdev_region(dev_number, 1);

        pr_err("IndustroLink: failed to add cdev\n");

        return ret;
    }


    /*
     * Create device class
     */
    industrolink_class = class_create(CLASS_NAME);

    if (IS_ERR(industrolink_class)) {

        cdev_del(&industrolink_cdev);

        unregister_chrdev_region(dev_number, 1);

        return PTR_ERR(industrolink_class);
    }


    /*
     * Create /dev/industrolink
     */
    industrolink_device =
        device_create(industrolink_class,
                      NULL,
                      dev_number,
                      NULL,
                      DEVICE_NAME);

    if (IS_ERR(industrolink_device)) {

        class_destroy(industrolink_class);

        cdev_del(&industrolink_cdev);

        unregister_chrdev_region(dev_number, 1);

        return PTR_ERR(industrolink_device);
    }


    /*
     * Initialize telemetry timer
     */
    timer_setup(&telemetry_timer,
                industrolink_timer_callback,
                0);

    telemetry_event = false;


    /*
     * Start timer.
     *
     * First event occurs after 5 seconds.
     */
    mod_timer(&telemetry_timer,
              jiffies + msecs_to_jiffies(5000));


    pr_info("IndustroLink: driver loaded\n");

    pr_info("IndustroLink: major=%d minor=%d\n",
            MAJOR(dev_number),
            MINOR(dev_number));

    return 0;
}


/*
 * Driver cleanup
 */
static void __exit industrolink_exit(void)
{
    /*
     * Stop timer before removing driver.
     */
    timer_delete_sync(&telemetry_timer);


    device_destroy(industrolink_class,
                   dev_number);

    class_destroy(industrolink_class);

    cdev_del(&industrolink_cdev);

    unregister_chrdev_region(dev_number, 1);

    pr_info("IndustroLink: driver unloaded\n");
}


module_init(industrolink_init);
module_exit(industrolink_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nandini");
MODULE_DESCRIPTION(
    "IndustroLink Linux Character Device Driver with Telemetry Timer and Wait Queue"
);
MODULE_VERSION("1.2");
