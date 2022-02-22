#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <asm/uaccess.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <asm/delay.h>
#include <linux/ktime.h>
#include <linux/timekeeping.h>
#include <linux/time.h>

#define GPIO_OUT 24 /* Trigger: GPIO24 */
#define GPIO_IN 23  /* Echo: GPIO23 */

/* Declarations */
static int __init hcsr04_module_init(void);
static void __exit hcsr04_module_cleanup(void);
int hcsr04_open(struct inode *inode, struct file *file);
int hcsr04_close(struct inode *inode, struct file *file);
ssize_t hcsr04_write(struct file *filp, const char *buffer, size_t length, loff_t *offset);
ssize_t hcsr04_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
static ssize_t hcsr04_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t hcsr04_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);

/* Definitions */
static int hcsr04 = 2; /* sys file variable */
static dev_t hcsr04_dev;
struct cdev hcsr04_cdev;    /* Data structure for character-communication-based device */
static int hcsr04_lock = 0; /* Access lock to hcsr04 device */
static struct kobject *hcsr04_kobject;
static ktime_t rising, falling; /* Data structures to record rising and falling edge of echo pulse of hcsr04 device */
static struct kobj_attribute hcsr04_attribute = __ATTR(hcsr04, 0660, hcsr04_show, hcsr04_store); /* Kernel object attribute: _ATTR(filename,file access right (0666 is read write execute access), invoked function when file read, invoked function when file written) */

/* VFS file operation APIs */
struct file_operations hcsr04_fops =
{
    .owner = THIS_MODULE,
    .read = hcsr04_read,
    .write = hcsr04_write,
    .open = hcsr04_open,
    .release = hcsr04_close,
};

static int __init hcsr04_module_init(void)
{
    char buffer[64];

    // printk(KERN_INFO "Loading hcsr04_module\n");

    /* Insert character device into Linux Kernel */
    alloc_chrdev_region(&hcsr04_dev, 0, 1, "hcsr04_dev");
    printk(KERN_INFO "%s\n", format_dev_t(buffer, hcsr04_dev));
    cdev_init(&hcsr04_cdev, &hcsr04_fops);
    hcsr04_cdev.owner = THIS_MODULE;
    cdev_add(&hcsr04_cdev, hcsr04_dev, 1);

    /* Reserve 2 GPIOs for character device */
    gpio_request(GPIO_OUT, "hcsr04_dev");
    gpio_request(GPIO_IN, "hcsr04_dev");
    gpio_direction_output(GPIO_OUT, 0); /* Trig signal */
    gpio_direction_input(GPIO_IN);      /* Echo signal */

    //  if (gpio_request(GPIO_OUT, "hcsr04_dev"))
    // {
    //     printk(KERN_INFO "hcsr04_dev: %s unable to get GPIO_OUT\n", __func__);
    //     ret = -EBUSY;
    //     goto Done;
    // }
    // if (gpio_request(GPIO_IN, "hcsr04_dev"))
    // {
    //     printk(KERN_INFO "hcsr04_dev: %s unable to get GPIO_IN\n", __func__);
    //     ret = -EBUSY;
    //     goto Done;
    // }
    // if (gpio_direction_output(GPIO_OUT, 0) < 0)
    // {
    //     printk(KERN_INFO "hcsr04_dev: %s unable to set GPIO_OUT as output\n", __func__);
    //     ret = -EBUSY;
    //     goto Done;
    // }
    // if (gpio_direction_input(GPIO_IN) < 0)
    // {
    //     printk(KERN_INFO "hcsr04_dev: %s unable to set GPIO_IN as input\n", __func__);
    //     ret = -EBUSY;
    //     goto Done;
    // }

    /* sysfs */
    hcsr04_kobject = kobject_create_and_add("hcsr04", kernel_kobj); /* Add hcsr04 directory in /sys/kernel */
    sysfs_create_file(hcsr04_kobject, &hcsr04_attribute.attr);      /* Add hcsr04 file in /sys/kernel/hcsr04 */

    return 0;

    // Done:
    //     return ret;
}

static void __exit hcsr04_module_cleanup(void)
{
    // printk(KERN_INFO "Cleaning-up hcsr04_dev\n");

    /* Release GPIOs */
    gpio_free(GPIO_OUT);
    gpio_free(GPIO_IN);

    /* Unlock module */
    hcsr04_lock = 0;

    /* Remove character device from kernel */
    cdev_del(&hcsr04_cdev);
    unregister_chrdev_region(hcsr04_dev, 1);

    /* Remove hcsr04 directory from sysfs */
    kobject_put(hcsr04_kobject);
}

int hcsr04_open(struct inode *inode, struct file *file)
{
    int ret = 0;

    // printk(KERN_INFO "hcsr04_dev: %s\n", __func__);

    /* Module lock control */
    if (hcsr04_lock > 0)
        ret = -EBUSY;
    else
        hcsr04_lock++;

    return (ret);
}

int hcsr04_close(struct inode *inode, struct file *file)
{
    // printk(KERN_INFO "hcsr04_dev: %s\n", __func__);
    hcsr04_lock = 0;
    return (0);
}

ssize_t hcsr04_write(struct file *filp, const char *buffer, size_t length, loff_t *offset)
{
    /* Generate pulse on Trig output */
    gpio_set_value(GPIO_OUT, 0);
    gpio_set_value(GPIO_OUT, 1);
    udelay(10); /* Wait for 10 microseconds */
    gpio_set_value(GPIO_OUT, 0);

    while (gpio_get_value(GPIO_IN) == 0)
        ;                 /* Wait while input is 0 */
    rising = ktime_get(); /* When input is 1 (rising edge) get the time stamp */
    /* Can't put any code between rising and falling calls, as it delays function calls and therefore readings */

    while (gpio_get_value(GPIO_IN) == 1)
        ;                  /* Wait while input is 1 */
    falling = ktime_get(); /* When input is 0 (falling edge) get the time stamp */
    // printk(KERN_INFO "hcsr04_dev: %s got rising timestamp %lld from GPIO_IN\n", __func__, rising);
    // printk(KERN_INFO "hcsr04_dev: %s got falling timestamp %lld from GPIO_IN\n", __func__, falling);

    return (1);
}

ssize_t hcsr04_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    int ret;
    int pulse;

    // printk(KERN_INFO "hcsr04 read (count=%d, offset=%d)\n", (int)count, (int)*f_pos);

    pulse = (int)ktime_to_us(ktime_sub(falling, rising)); /* Pulse duration (cast to int) = falling - rising */
    // printk(KERN_INFO "hcsr04_dev: %s pulse duration (us) %d, pulse distance (cm): %d\n", __func__, pulse, pulse/58);
    ret = copy_to_user(buf, &pulse, 4);                   /* Copy pulse duration to user space as 4 bytes (32 bits) */

    return 4; /* 4 bytes are returned */
}

/* Function executed when the user reads /sys/kernel/hcsr04/hcsr04 */
static ssize_t hcsr04_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    int duration = 0;
    int distance = 0;
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int min = 0;
    int sec = 0;
    struct timespec t;
    struct tm time;
    static ssize_t ret;

    duration = (int)ktime_to_us(ktime_sub(falling, rising));
    distance = duration / 58;
    getnstimeofday(&t);
    time64_to_tm(t.tv_sec, 0, &time);
    year = 1900 + time.tm_year;
    month = 1 + time.tm_mon;
    day = time.tm_mday; /* day of the month */
    hour = time.tm_hour;
    min = time.tm_min;
    sec = time.tm_sec;
    // printk("%d:%d:%d:%ld\n", time.tm_hour, time.tm_min, time.tm_sec, t.tv_usec);
    ret = sprintf(buf, "[%d-%d-%d, %d:%d:%d] Pulse duration (us): %d, Distance (cm): %d\n", day, month, year, hour, min, sec, duration, distance); /* Floating point operations are discouraged in Linux kernel */

    return ret;
    // return sprintf(buf, "%d\n", hcsr04);
}

/* Function executed when the user writes /sys/kernel/hcsr04/hcsr04 */
static ssize_t hcsr04_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    // hcsr04 = (int)ktime_to_us(ktime_sub(falling, rising));
    // sscanf(buf, "%d\n", &hcsr04);

    return count;
}

module_init(hcsr04_module_init);
module_exit(hcsr04_module_cleanup);
MODULE_AUTHOR("Andrew Lightbody");
MODULE_LICENSE("GPL");