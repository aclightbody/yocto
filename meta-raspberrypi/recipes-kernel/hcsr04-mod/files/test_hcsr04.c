#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, char **argv)
{
    char *app_name = argv[0];
    char *dev_name = "/dev/hcsr04"; /* Assume module loaded and /dev/hcsr04 device file created */
    int fd = -1; /* Device file */
    char c;
    int d;

    fd = open(dev_name, O_RDWR); /* Open device file as read write */
    c = 1; /* Write value */
    write(fd, &c, 1); /* Write system call triggers sensor. Written value c here is meaningless. 1 byte */
    read(fd, &d, 4); /* Read system call stores echo pulse duration in 4 bytes */

    printf("%d: %f\n", d, d / 58.0); /* Display duration and corresponding distance */
    close(fd); /* Close device file */

    return 0;
}