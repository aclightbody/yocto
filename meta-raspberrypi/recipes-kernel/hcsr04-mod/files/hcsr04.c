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

#define GPIO_OUT 20 /* GPIO20 */
#define GPIO_IN 21  /* GPIO21 */

static dev_t hcsr04_dev;
struct cdev hcsr04_cdev;    /* Data structure for character-communication-based device */
static int hcsr04_lock = 0; /* Access lock to hcsr04 device */
static struct kobject *hcsr04_kobject;
static ktime_t rising, falling;                                                                  /* Data structures to record rising and falling edge of echo pulse of hcsr04 device */
static struct kobj_attribute hcsr04_attribute = __ATTR(hcsr04, 0660, hcsr04_show, hcsr04_store); /* Kernel object attribute: _ATTR(filename,file access right, invoked function when file read, invoked function when file written) */

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

    /* sysfs */
    hcsr04_kobject = kobject_create_and_add("hcsr04", kernel_kobj); /* Add hcsr04 directory in /sys/kernel */
    sysfs_create_file(hcsr04_kobject, &hcsr04_attribute.attr);      /* Add hcsr04 flie in /sys/kernel/hcsr04 */

    return 0;
}

static void __exit hcsr04_module_cleanup(void)
{
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

    /* Module lock control */
    if (hcsr04_lock > 0)
        ret = -EBUSY;
    else
        hcsr04_lock++;
    return (ret);
}

int hcsr04_close(struct inode *inode, struct file *file)
{
    hcsr04_lock = 0;
    return (0);
}

ssize_t hcsr04_write(structfile *filp, constchar *buffer, size_tlength, loff_t *offset)
{
    /* Generate pulse on Trig output */
    gpio_set_value(GPIO_OUT, 0);
    gpio_set_value(GPIO_OUT, 1);
    udelay(10); /* Wait for 10 microseconds */
    gpio_set_value(GPIO_OUT, 0);

    while (gpio_get_value(GPIO_IN) == 0)
        ;                 /* Wait while input is 0 */
    rising = ktime_get(); /* When input is 1 (rising edge) get the time stamp */

    while (gpio_get_value(GPIO_IN) == 1)
        ;                  /* Wait while input is 1 */
    falling = ktime_get(); /* When input is 0 (falling edge) get the time stamp */

    return (1);
}

ssize_t hcsr04_read(structfile *filp, char __user *buf, size_tcount, loff_t *f_pos)
{
    int ret;
    int pulse;

    pulse = (int)ktime_to_us(ktime_sub(falling, rising)); /* Pulse duration (cast to int) = falling - rising */
    ret = copy_to_user(buf, &pulse, 4);                   /* Copy pulse duration to user space as 4 bytes */

    return 4; /* 4 bytes are returned */
}

/* Function executed when the user reads /sys/kernel/hcsr04/hcsr04 */
static ssize_t hcsr04_show(structkobject *kobj, structkobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", ktime_to_us(ktime_sub(falling, rising)));
}

/* Function executed when the user writes /sys/kernel/hcsr04/hcsr04 */
static ssize_t hcsr04_store(structkobject *kobj, structkobj_attribute *attr, char *buf, size_tcount)
{
    return 1;
}