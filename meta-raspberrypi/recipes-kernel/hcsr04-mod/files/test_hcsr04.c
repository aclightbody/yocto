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
    int d[7];
    int i;
    int iterConv;
    char* arg1 = "1"; /* Pointer to default string. Allows for dynamic memory allocation of input */

    if (argc > 1)
    {
        arg1 = (char*)malloc(strlen(argv[1] + 1)); /* Dynamically allocating memory depending on length of argv */
        strcpy(arg1,argv[1]); /* Copying string in argv[1] to arg1 */
    }

    errno = 0;
    iterConv = strtol(arg1, &ptr_end,10); /* Get argument at position 1 and convert string to int: base 10 */

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
        // read(fd, &d, 4); /* Read system call stores echo pulse duration in 4 bytes */
        read(fd, &d, 4*7);
        // printf("Duration (us): %d,Distance (cm): %f\n", d, d/58.0); /* Display duration (microseconds) and corresponding distance (cm) */
        printf("[%d-%d-%d | %d:%d:%d] Duration (us): %d, Distance (cm): %f\n", d[3], d[2], d[1], d[4], d[5], d[6], d[0], d[0]/58.0);
        usleep(500000); /* Delay inserted, as the echo pulses seem to interfere with each other if in quick succession. 5x10^5 microseconds = 0.5s */
    }

    close(fd); /* Close device file */

    return 0;

    err:
        printf("Incorrect argument input");
        return 0;
}