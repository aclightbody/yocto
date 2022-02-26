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
// static int hcsr04 = 2; /* sys file variable */
static dev_t hcsr04_dev;
struct cdev hcsr04_cdev;    /* Data structure for character-communication-based device */
static int hcsr04_lock = 0; /* Access lock to hcsr04 device */
static struct kobject *hcsr04_kobject;
static ktime_t rising, falling; /* Data structures to record rising and falling edge of echo pulse of hcsr04 device */
static struct kobj_attribute hcsr04_attribute = __ATTR(hcsr04, 0660, hcsr04_show, hcsr04_store); /* Kernel object attribute: _ATTR(filename,file access right (0666 is read write execute access), invoked function when file read, invoked function when file written) */
#define CIRC_BUF_SIZE 5 /* Storing 5 echo values */
static int circBufLen = 0; /* Indicates how many values are in buffer */
static int circBufWrtIdx = 0;
static int circBufPulse[CIRC_BUF_SIZE] = {0}; /* Circular buffer array */
static struct timespec circBufTime[CIRC_BUF_SIZE] = {0}; /* Circular buffer array */

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
    alloc_chrdev_region(&hcsr04_dev, 0, 1, "hcsr04_dev"); /* Allocating major number for device */
    printk(KERN_INFO "%s\n", format_dev_t(buffer, hcsr04_dev)); /* Print major and minor number */
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
    // int pulse;

    int duration = 0;
    int distance = 0;
    struct timespec t;
    // int userArray[];

    // static ssize_t echo;

    // printk(KERN_INFO "Size of ssize_t is %d bytes in function %s\n", sizeof(ssize_t), __func__);

    /* Date calculation */
    duration = (int)ktime_to_us(ktime_sub(falling, rising)); /* Pulse duration (cast to int) = falling - rising */
    // distance = duration / 58;

    ret = copy_to_user(buf, &duration, 4);                   /* Copy pulse duration to user space as 4 bytes (32 bits) */

    /* Date calculation */
    getnstimeofday(&t);

    // printk(KERN_INFO "hcsr04 read (count=%d, offset=%d)\n", (int)count, (int)*f_pos);
    // pulse = (int)ktime_to_us(ktime_sub(falling, rising)); /* Pulse duration (cast to int) = falling - rising */
    // printk(KERN_INFO "hcsr04_dev: %s pulse duration (us) %d, pulse distance (cm): %d\n", __func__, pulse, pulse/58);
    // ret = copy_to_user(buf, &pulse, 4);                   /* Copy pulse duration to user space as 4 bytes (32 bits) */

    /* Date and echo duration and distance output */
    // printk(KERN_INFO "%d:%d:%d:%ld\n", time.tm_hour, time.tm_min, time.tm_sec, t.tv_usec);
    // echo = sprintf(buf, "[%d-%d-%d, %d:%d:%d] Pulse duration (us): %d, Distance (cm): %d\n", day, month, year, hour, min, sec, duration, distance); /* Floating point operations are discouraged in Linux kernel */
    // printk(KERN_INFO "%s",buf);

    circBufPulse[circBufWrtIdx] = duration;
    circBufTime[circBufWrtIdx] = t;

    /* Circular buffer for last 5 values */

    // if (circBufLen >= CIRC_BUF_SIZE)
    // {
    //     circBufLen = CIRC_BUF_SIZE;/* Buffer full */
    // }
    // else
    // {
    //     circBufLen++;
    // }

    circBufWrtIdx++;
   
    if (circBufWrtIdx >= CIRC_BUF_SIZE)
    {
        circBufWrtIdx = 0;
    }

    return 4; /* 4 bytes are returned */
}

/* Function executed when the user reads /sys/kernel/hcsr04/hcsr04 */
static ssize_t hcsr04_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    // int duration = 0;
    // int distance = 0;
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int min = 0;
    int sec = 0;
    struct tm time;
    ssize_t echo = 0;
    int i;
    int j;
    int circBufDistInt[CIRC_BUF_SIZE]; /* Integer part */
    int circBufDistDec[CIRC_BUF_SIZE]; /* Decimal part */

    for (j=0;j<CIRC_BUF_SIZE;j++)
    {
        circBufDistInt[j] = circBufPulse[j]/58;
        circBufDistDec[j] = ((1000*circBufPulse[j])/58)%1000; /* 3 decimal places */

    }

    // // printk(KERN_INFO "Size of ssize_t is %d bytes in function %s\n", sizeof(ssize_t), __func__);

    // /* Date calculation */
    // duration = (int)ktime_to_us(ktime_sub(falling, rising));
    // distance = duration / 58;

    // /* Date and echo duration and distance output */
    // // printk(KERN_INFO "%d:%d:%d:%ld\n", time.tm_hour, time.tm_min, time.tm_sec, t.tv_usec);
    // echo = sprintf(buf, "[%d-%d-%d, %d:%d:%d] Pulse duration (us): %d, Distance (cm): %d\n", day, month, year, hour, min, sec, duration, distance); /* Floating point operations are discouraged in Linux kernel */
    // // printk(KERN_INFO "%s",buf);
   
    for (i=0;i<CIRC_BUF_SIZE;i++)
    {
        // printk(KERN_INFO "%d\n",circBufPulse[i]); /* Pulse duration */
        // printk(KERN_INFO "%d.%d\n", circBufDistInt[i],circBufDistDec[i]); /* Pulse distance */
        
        /* Time stamp */
        time64_to_tm(circBufTime[i].tv_sec, 0, &time);
        year = 1900 + time.tm_year;
        month = 1 + time.tm_mon;
        day = time.tm_mday; /* day of the month */
        hour = time.tm_hour;
        min = time.tm_min;
        sec = time.tm_sec;
        // printk(KERN_INFO "%d-%d-%d  %d:%d:%d\n", day, month, year, hour, min, sec);

        if (i == 0)
        {
            echo = sprintf(buf, "[%d-%d-%d, %d:%d:%d] Pulse duration (us): %d, Distance (cm): %d.%d\n", day, month, year, hour, min, sec, circBufPulse[i], circBufDistInt[i],circBufDistDec[i]);
        }
        else
        {
            echo += sprintf(buf + echo, "[%d-%d-%d, %d:%d:%d] Pulse duration (us): %d, Distance (cm): %d.%d\n", day, month, year, hour, min, sec, circBufPulse[i], circBufDistInt[i],circBufDistDec[i]);
        }
    }

// echo = sprintf(buf, "[%d-%d-%d, %d:%d:%d] Pulse duration (us): %d, Distance (cm): %d.%d \
//     [%d-%d-%d, %d:%d:%d]\n Pulse duration (us): %d, Distance (cm): %d.%d\n",\
//     10, 12, 2012, 11, 11, 11, 300, 5, 5,
//     23, 2, 2022, 12, 00, 00, 500, 4, 4);

    return echo;
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