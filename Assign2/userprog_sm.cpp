#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/types.h>

#define PB2_SET_CAPACITY _IOW(0x10, 0x31, int32_t *)
#define PB2_INSERT_RIGHT _IOW(0x10, 0x32, int32_t *)
#define PB2_INSERT_LEFT _IOW(0x10, 0x33, int32_t *)
#define PB2_GET_INFO _IOR(0x10, 0x34, int32_t *)
#define PB2_POP_LEFT _IOR(0x10, 0x35, int32_t *)
#define PB2_POP_RIGHT _IOR(0x10, 0x36, int32_t *)

struct obj_info
{
    int32_t deque_size; //current number of elements in deque
    int32_t capacity;   //maximum capacity of the deque
};

int main()
{
    int32_t fd, fd2, pid;
    char filename[100] = "/proc/cs60038_a2_18CS30051";

    for (size_t i = 0; i < 2.; i++)
        fork();

    printf("%d Opening File\n", getpid());

    fd = open(filename, O_RDWR);
    if (fd < 0)
    {
        printf("%d Cannot open file...\n", getpid());
        printf("%d Error= %d\n", getpid(), errno);
        return 0;
    }

    fd2 = open(filename, O_RDWR);
    if (fd2 < 0)
    {
        printf("%d Cannot open file again...\n", getpid());
        printf("%d Error= %d\n", getpid(), errno);
        printf("%d fd2=%d\n", getpid(), fd2);
    }

    int sz = 4, x, a;
    int arr[] = {1, 2, 3, 4};

    if (ioctl(fd, PB2_SET_CAPACITY, (int32_t *)&sz))
    {
        printf("%d error %d\n", getpid(), errno);
        return 0;
    }

    x = ioctl(fd, PB2_INSERT_LEFT, (int32_t *)&arr[0]);
    x = ioctl(fd, PB2_INSERT_LEFT, (int32_t *)&arr[1]);
    x = ioctl(fd, PB2_INSERT_RIGHT, (int32_t *)&arr[2]);
    x = ioctl(fd, PB2_INSERT_RIGHT, (int32_t *)&arr[3]);
    x = ioctl(fd, PB2_INSERT_RIGHT, (int32_t *)&arr[0]);
    if (x < 0)
        printf("%d performed excess write\n", getpid());

    ioctl(fd, PB2_POP_LEFT, (int32_t *)&a);
    printf("%d read %d expected 2\n", getpid(), a);
    ioctl(fd, PB2_POP_RIGHT, (int32_t *)&a);
    printf("%d read %d expected 4\n", getpid(), a);
    ioctl(fd, PB2_POP_RIGHT, (int32_t *)&a);
    printf("%d read %d expected 3\n", getpid(), a);
    ioctl(fd, PB2_POP_LEFT, (int32_t *)&a);
    printf("%d read %d expected 1\n", getpid(), a);
    return 0;
}
