#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h> /*  INT_MAX, INT_MIN */
#include <stdlib.h> /* strtol */

int main(int argc, char **argv)
{
    char *app_name = argv[0];
    char *ptr_end; /* Iteration pointer */
    int iter; /* Iteration */
    char *dev_name = "/dev/hcsr04"; /* Assume module loaded and /dev/hcsr04 device file created */
    int fd = -1; /* Device file */
    char c;
    int d;
    int i;
    int iterConv;

    errno = 0;
    iterConv = strtol(argv[1], &ptr_end,10); /* Get argument at position 1 and convert string to int: base 10 */

    if(errno != 0 || *ptr_end != '\0' || iterConv > INT_MAX || iterConv < INT_MIN) /* End pointer = /0 */
    {
        goto err;
    } else 
    {
        iter = iterConv;
    }

    fd = open(dev_name, O_RDWR); /* Open device file as read write */
    c = 1; /* Write value */

    for (i=0; i<iter; i++)
    {
        write(fd, &c, 1); /* Write system call triggers sensor. Written value c here is meaningless. 1 byte */
        read(fd, &d, 4); /* Read system call stores echo pulse duration in 4 bytes */
        printf("Duration (us): %d,Distance (cm): %f\n", d, d/58.0); /* Display duration (microseconds) and corresponding distance (cm) */
    }

    close(fd); /* Close device file */

    err:
        printf("Incorrect argument input");

    return 0;
}