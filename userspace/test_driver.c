#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>

#define INDUSTROLINK_IOCTL_RESET _IO('I', 1)
#define INDUSTROLINK_IOCTL_GET_SIZE _IOR('I', 2, int)

int main(void)
{
    int fd;
    int size;
    char buffer[256];

    printf("Opening /dev/industrolink...\n");

    fd = open("/dev/industrolink", O_RDWR);

    if (fd < 0) {
        perror("open");
        return 1;
    }

    printf("open() successful. fd = %d\n", fd);

    const char *message = "IndustroLink userspace test";

    ssize_t written = write(fd, message, strlen(message));

    if (written < 0) {
        perror("write");
        close(fd);
        return 1;
    }

    printf("write() successful: %zd bytes\n", written);

    if (ioctl(fd, INDUSTROLINK_IOCTL_GET_SIZE, &size) < 0) {
        perror("ioctl GET_SIZE");
        close(fd);
        return 1;
    }

    printf("ioctl GET_SIZE successful: %d bytes\n", size);

    ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);

    if (bytes_read < 0) {
        perror("read");
        close(fd);
        return 1;
    }

    buffer[bytes_read] = '\0';

    printf("read() successful: %zd bytes\n", bytes_read);
    printf("Driver returned: %s\n", buffer);

    if (ioctl(fd, INDUSTROLINK_IOCTL_RESET) < 0) {
        perror("ioctl RESET");
        close(fd);
        return 1;
    }

    printf("ioctl RESET successful\n");

    if (ioctl(fd, INDUSTROLINK_IOCTL_GET_SIZE, &size) < 0) {
        perror("ioctl GET_SIZE after RESET");
        close(fd);
        return 1;
    }

    printf("ioctl GET_SIZE after RESET: %d bytes\n", size);

    close(fd);

    printf("close() successful\n");

    return 0;
}
