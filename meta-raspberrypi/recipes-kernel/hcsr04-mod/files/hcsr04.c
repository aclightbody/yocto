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

/* Declarations: Sensor */
static int __init hcsr04_module_init(void);
static void __exit hcsr04_module_cleanup(void);
int hcsr04_open(struct inode *inode, struct file *file);
int hcsr04_close(struct inode *inode, struct file *file);
ssize_t hcsr04_write(struct file *filp, const char *buffer, size_t length, loff_t *offset);
ssize_t hcsr04_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
static ssize_t hcsr04_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t hcsr04_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);

/* Declarations: circular buffer */ 
typedef struct circular_buf_t circular_buf_t;
typedef circular_buf_t* cbuf_handle_t; /* Pointer to circular buffer struct */
cbuf_handle_t circular_buf_init(int* buffer, size_t size); 
void circular_buf_free(cbuf_handle_t me); 
void circular_buf_reset(cbuf_handle_t me); 
void circular_buf_put(cbuf_handle_t me, int data); 
int circular_buf_get(cbuf_handle_t me, int * data);
bool circular_buf_empty(cbuf_handle_t me); 
bool circular_buf_full(cbuf_handle_t me); 
size_t circular_buf_capacity(cbuf_handle_t me); 
size_t circular_buf_size(cbuf_handle_t me); 

/* Definitions: Sensor */
// static int hcsr04 = 2; /* sys file variable */
static dev_t hcsr04_dev;
struct cdev hcsr04_cdev;    /* Data structure for character-communication-based device */
static int hcsr04_lock = 0; /* Access lock to hcsr04 device */
static struct kobject *hcsr04_kobject;
static ktime_t rising, falling; /* Data structures to record rising and falling edge of echo pulse of hcsr04 device */
static struct kobj_attribute hcsr04_attribute = __ATTR(hcsr04, 0660, hcsr04_show, hcsr04_store); /* Kernel object attribute: _ATTR(filename,file access right (0666 is read write execute access), invoked function when file read, invoked function when file written) */
#define CIRC_BUF_SIZE 5 /* Storing 5 echo values */

/* Definitions: Circular buffer */
struct circular_buf_t
{
	int* buffer; /* Data buffer */
	size_t head; /* Head pointer */
	size_t tail; /* Tail pointer */
	size_t max; /* Max size of the buffer in bytes */
	bool full; /* Is the buffer full or not */
};

static int fifoBufPulse[CIRC_BUF_SIZE] = {0};
static int fifoBufYear[CIRC_BUF_SIZE] = {0};
static int fifoBufMonth[CIRC_BUF_SIZE] = {0};
static int fifoBufDay[CIRC_BUF_SIZE] = {0};
static int fifoBufHour[CIRC_BUF_SIZE] = {0};
static int fifoBufMin[CIRC_BUF_SIZE] = {0};
static int fifoBufSec[CIRC_BUF_SIZE] = {0};
static cbuf_handle_t handlePulse = NULL;
static cbuf_handle_t handleYear = NULL;
static cbuf_handle_t handleMonth = NULL;
static cbuf_handle_t handleDay = NULL;
static cbuf_handle_t handleHour = NULL;
static cbuf_handle_t handleMin = NULL;
static cbuf_handle_t handleSec = NULL;

/* VFS file operation APIs */
struct file_operations hcsr04_fops =
{
    .owner = THIS_MODULE,
    .read = hcsr04_read,
    .write = hcsr04_write,
    .open = hcsr04_open,
    .release = hcsr04_close,
};

static inline size_t advance_headtail_value(size_t value, size_t max)
{
    // printk(KERN_INFO "Value in: %d\n", value);
	if(++value == max) /* pre-increment value */
	{
		// printk(KERN_INFO "Value if max: %d\n", value);
        value = 0;
	}
    // printk(KERN_INFO "Value out: %d\n", value);

	return value;
}

static void advance_head_pointer(cbuf_handle_t me)
{
	if(circular_buf_full(me))
	{
        // printk(KERN_INFO "tail index value pre-advance: %d, address pointed to by tail pointer: %p, address of tail pointer: %p\n", me->tail, me->tail, &(me->tail));
		me->tail = advance_headtail_value(me->tail, me->max); /* If the buffer is full the tail pointer is advanced by 1 as well as the head pointer below */
        // printk(KERN_INFO "tail index value post-advance: %d, address pointed to by tail pointer: %p, address of tail pointer: %p\n", me->tail, me->tail, &(me->tail));
    }

    // printk(KERN_INFO "head index value pre-advance: %d, address pointed to by tail pointer: %p, address of head pointer: %p\n", me->head, me->head, &(me->head));
	me->head = advance_headtail_value(me->head, me->max);
    // printk(KERN_INFO "head index value post-advance: %d, address pointed to by tail pointer: %p, address of head pointer: %p\n", me->head, me->head, &(me->head));
	me->full = (me->head == me->tail); /* Tests to see whether buffer is full after the head has been advanced by 1 */
    // printk(KERN_INFO "Full: %d\n", me->full);
}

/* 
circular_buf_init: Pass in a storage buffer and size. Returns a circular buffer handle. 
*/
cbuf_handle_t circular_buf_init(int* buffer, size_t size)
{
	cbuf_handle_t cbuf = kmalloc(sizeof(circular_buf_t),GFP_KERNEL);

	cbuf->buffer = buffer;
	cbuf->max = size;
	circular_buf_reset(cbuf);

	if(circular_buf_empty(cbuf))
    {
        // printk(KERN_INFO "Buffer empty\n");
    }

	return cbuf;
}

/* 
circular_buf_free: Free a circular buffer structure. Does not free data buffer; owner is responsible for that. 
*/
void circular_buf_free(cbuf_handle_t me)
{
	kfree(me);
}

/* 
circular_buf_reset: Reset the circular buffer to empty, head == tail 
*/
void circular_buf_reset(cbuf_handle_t me)
{
	me->head = 0;
	me->tail = 0;
	me->full = false;
}

/* 
circular_buf_size: Returns the current number of elements in the buffer 
*/
size_t circular_buf_size(cbuf_handle_t me)
{
	size_t size = me->max;

	if(!circular_buf_full(me))
	{
		if(me->head >= me->tail)
		{
			size = (me->head - me->tail);
		}
		else
		{
			size = (me->max + me->head - me->tail);
		}
	}

	return size;
}

/* 
circular_buf_capacity: Returns the maximum capacity of the buffer 
*/
size_t circular_buf_capacity(cbuf_handle_t me)
{
	return me->max;
}

/* 
circular_buf_put:Put continues to add data if the buffer is full. Old data is overwritten. 
*/
void circular_buf_put(cbuf_handle_t me, int data)
{
	me->buffer[me->head] = data;

	advance_head_pointer(me);
}

/*  
circular_buf_get: Retrieve a value from the buffer. Returns 0 on success, -1 if the buffer is empty 
*/
int circular_buf_get(cbuf_handle_t me, int* data)
{
    int r = -1;

	if(!circular_buf_empty(me))
	{
		*data = me->buffer[me->tail];
		me->tail = advance_headtail_value(me->tail, me->max);
		me->full = false;

		r = 0;
	}

	return r;
}

/* 
circular_buf_empty: Returns true if the buffer is empty 
*/
bool circular_buf_empty(cbuf_handle_t me)
{
	return (!circular_buf_full(me) && (me->head == me->tail)); /* Check with full flag and that the head and tail are at the same place */
}

/* 
circular_buf_full: Returns true if the buffer is full 
*/
bool circular_buf_full(cbuf_handle_t me)
{
	return me->full;
}

/* 
Look ahead at values in buffer without removing data (gets a copy).
look_ahead_counter: less than or equal to size of buffer (circular_buf_size())
Values from tail to look_ahead_counter are inserted into data array of size look_ahead_counter.
*/
int circular_buf_peek(cbuf_handle_t me, int* data, unsigned int look_ahead_counter)
{
	int r = -1;
	size_t pos;
    unsigned int i;

	// We can't look beyond the current buffer size
	if(circular_buf_empty(me) || look_ahead_counter > circular_buf_size(me))
	{
		return r;
	}

	pos = me->tail;
	for(i = 0; i < look_ahead_counter; i++)
	{
		data[i] = me->buffer[pos];
		pos = advance_headtail_value(pos, me->max);
	}

	return 0;
}

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

    /* sysfs */
    hcsr04_kobject = kobject_create_and_add("hcsr04", kernel_kobj); /* Add hcsr04 directory in /sys/kernel */
    sysfs_create_file(hcsr04_kobject, &hcsr04_attribute.attr);      /* Add hcsr04 file in /sys/kernel/hcsr04 */

    handlePulse = circular_buf_init(fifoBufPulse,CIRC_BUF_SIZE);
    handleYear = circular_buf_init(fifoBufYear,CIRC_BUF_SIZE);
    handleMonth = circular_buf_init(fifoBufMonth,CIRC_BUF_SIZE);
    handleDay = circular_buf_init(fifoBufDay,CIRC_BUF_SIZE);
    handleHour = circular_buf_init(fifoBufHour,CIRC_BUF_SIZE);
    handleMin = circular_buf_init(fifoBufMin,CIRC_BUF_SIZE);
    handleSec = circular_buf_init(fifoBufSec,CIRC_BUF_SIZE);


    return 0;
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

    circular_buf_free(handlePulse);
    circular_buf_free(handleYear);
    circular_buf_free(handleMonth);
    circular_buf_free(handleDay);
    circular_buf_free(handleHour);
    circular_buf_free(handleMin);
    circular_buf_free(handleSec);
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
    int duration = 0;
    int distance = 0;
    struct timespec t;
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int min = 0;
    int sec = 0;
    struct tm time;
    int userArray[7];

    // printk(KERN_INFO "Size of ssize_t is %d bytes in function %s\n", sizeof(ssize_t), __func__);

    duration = (int)ktime_to_us(ktime_sub(falling, rising)); /* Pulse duration (cast to int) = falling - rising */
    
    /* Date calculation */
    getnstimeofday(&t);
    time64_to_tm(t.tv_sec, 0, &time);
    year = 1900 + time.tm_year;
    month = 1 + time.tm_mon;
    day = time.tm_mday; /* day of the month */
    hour = time.tm_hour;
    min = time.tm_min;
    sec = time.tm_sec;

    userArray[0] = duration;
    userArray[1] = year;
    userArray[2] = month;
    userArray[3] = day;
    userArray[4] = hour;
    userArray[5] = min;
    userArray[6] = sec;

    // ret = copy_to_user(buf, &duration, 4);                   /* Copy pulse duration to user space as 4 bytes (32 bits) */
    ret = copy_to_user(buf, userArray, 4*7);
    
    // printk(KERN_INFO "hcsr04 read (count=%d, offset=%d)\n", (int)count, (int)*f_pos);

    /* FIFO circular buffer for last 5 values. Could do this with a linked list.*/
    circular_buf_put(handlePulse,duration);
    circular_buf_put(handleYear,year);
    circular_buf_put(handleMonth,month);
    circular_buf_put(handleDay,day);
    circular_buf_put(handleHour,hour);
    circular_buf_put(handleMin,min);
    circular_buf_put(handleSec,sec);

    // return 4; /* 4 bytes are returned */
    return 4*7;
}

/* Function executed when the user reads /sys/kernel/hcsr04/hcsr04 */
static ssize_t hcsr04_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    ssize_t echo = 0;
    int i;
    int circBufDistInt[CIRC_BUF_SIZE]; /* Integer part */
    int circBufDistDec[CIRC_BUF_SIZE]; /* Decimal part */
    int dataPeekTest[CIRC_BUF_SIZE];
    int dataPeekPulse[CIRC_BUF_SIZE];
    int dataPeekYear[CIRC_BUF_SIZE];
    int dataPeekMonth[CIRC_BUF_SIZE];
    int dataPeekDay[CIRC_BUF_SIZE];
    int dataPeekHour[CIRC_BUF_SIZE];
    int dataPeekMin[CIRC_BUF_SIZE];
    int dataPeekSec[CIRC_BUF_SIZE];

    circular_buf_peek(handlePulse, dataPeekPulse, CIRC_BUF_SIZE);
    circular_buf_peek(handleYear, dataPeekYear, CIRC_BUF_SIZE);
    circular_buf_peek(handleMonth, dataPeekMonth, CIRC_BUF_SIZE);
    circular_buf_peek(handleDay, dataPeekDay, CIRC_BUF_SIZE);
    circular_buf_peek(handleHour, dataPeekHour, CIRC_BUF_SIZE);
    circular_buf_peek(handleMin, dataPeekMin, CIRC_BUF_SIZE);
    circular_buf_peek(handleSec, dataPeekSec, CIRC_BUF_SIZE);

    for (i=0;i<CIRC_BUF_SIZE;i++)
    {
        if (i == 0)
        {
            /* Distance done to 3 decimal places. Floating point operations are discouraged in Linux kernel. */
            echo = sprintf(buf, "[%d-%d-%d| %d:%d:%d] Pulse duration (us): %d, Distance (cm): %d.%d\n", dataPeekDay[i], dataPeekMonth[i],\
             dataPeekYear[i], dataPeekHour[i], dataPeekMin[i], dataPeekSec[i], dataPeekPulse[i], dataPeekPulse[i]/58, ((1000*dataPeekPulse[i])/58)%1000);
        }
        else
        {
            echo += sprintf(buf + echo, "[%d-%d-%d| %d:%d:%d] Pulse duration (us): %d, Distance (cm): %d.%d\n", dataPeekDay[i], dataPeekMonth[i],\
             dataPeekYear[i], dataPeekHour[i], dataPeekMin[i], dataPeekSec[i], dataPeekPulse[i], dataPeekPulse[i]/58, ((1000*dataPeekPulse[i])/58)%1000);
        }
    }


    // for(k1=0;k1<CIRC_BUF_SIZE;k1++)
    // {
    //     printk(KERN_INFO "peek [%d-%d-%d | %d:%d:%d] Duration (us): %d, Distance (cm): %d\n", dataPeekDay[k1], dataPeekMonth[k1],\
    //      dataPeekYear[k1], dataPeekHour[k1],  dataPeekMin[k1], dataPeekSec[k1], dataPeekPulse[k1], dataPeekPulse[k1]/58);
    // }

    return echo;
}

/* Function executed when the user writes /sys/kernel/hcsr04/hcsr04 */
static ssize_t hcsr04_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    // sscanf(buf, "%d\n", &hcsr04);

    return count;
}

module_init(hcsr04_module_init);
module_exit(hcsr04_module_cleanup);
MODULE_AUTHOR("Andrew Lightbody");
MODULE_LICENSE("GPL");