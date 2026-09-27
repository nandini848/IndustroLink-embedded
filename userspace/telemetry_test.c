#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <errno.h>

#define DEVICE "/dev/industrolink"

#define INDUSTROLINK_IOCTL_RESET \
    _IO('I', 1)

#define INDUSTROLINK_IOCTL_GET_SIZE \
    _IOR('I', 2, int)

#define INDUSTROLINK_IOCTL_WAIT_EVENT \
    _IO('I', 3)

int main(void)
{
    int fd;
    int size;

    printf("IndustroLink Telemetry Monitor\n");
    printf("--------------------------------\n");

    fd = open(DEVICE, O_RDWR);

    if (fd < 0) {
        perror("open");
        return 1;
    }

    printf("Device opened successfully\n\n");

    for (int i = 0; i < 5; i++) {

        printf("Waiting for telemetry event...\n");

        if (ioctl(fd, INDUSTROLINK_IOCTL_WAIT_EVENT) < 0) {
            perror("ioctl WAIT_EVENT");
            close(fd);
            return 1;
        }

        int temperature = 70 + (i * 2);
        int pressure = 100 + (i * 3);

        time_t now = time(NULL);

        printf("\nTelemetry #%d\n", i + 1);
        printf("Temperature : %d C\n", temperature);
        printf("Pressure    : %d kPa\n", pressure);
        printf("Timestamp   : %s", ctime(&now));

        if (ioctl(fd, INDUSTROLINK_IOCTL_GET_SIZE, &size) == 0)
            printf("Driver buffer: %d bytes\n", size);

        printf("-----------------------------\n\n");
    }

    if (ioctl(fd, INDUSTROLINK_IOCTL_RESET) == 0)
        printf("Driver buffer reset successfully\n");

    close(fd);

    printf("Device closed\n");

    return 0;
}
