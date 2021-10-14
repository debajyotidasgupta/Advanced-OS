#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

#include "../deque.h"

void normal_path()
{
    int fd = open("/proc/cs60038_a2_18CS10066", O_RDWR);
    int n = 150;
    write(fd, &n, sizeof(int));
    assert(errno == EACCES);
    int ret = ioctl(fd, PB2_SET_CAPACITY, &n);
    assert(ret == -1 && errno == EINVAL);

    errno = 0;
    n = 50;
    ret = ioctl(fd, PB2_SET_CAPACITY, &n);
    assert(ret == 0 && errno == 0);

    int x;
    ret = read(fd, &x, sizeof(int));
    assert(ret == -1 && errno == EACCES);
    ioctl(fd, PB2_INSERT_RIGHT, &n);

    errno = 0;
    read(fd, &x, sizeof(int));
    assert(x == n && errno == 0);

    n = 5;
    ioctl(fd, PB2_INSERT_RIGHT, &n);
    n = 4;
    ioctl(fd, PB2_INSERT_LEFT, &n);

    read(fd, &x, sizeof(int));
    assert(x == 4);
    read(fd, &x, sizeof(int));
    assert(x == 5);

    struct obj_info info;
    info.deque_size = -1;
    info.capacity = -1;
    ioctl(fd, PB2_GET_INFO, &info);
    assert(info.deque_size == 0 && info.capacity == 50);

    n = 5;
    ioctl(fd, PB2_INSERT_RIGHT, &n);
    n--;
    ioctl(fd, PB2_INSERT_RIGHT, &n);
    n--;
    ioctl(fd, PB2_GET_INFO, &info);
    assert(info.deque_size == 2 && info.capacity == 50);

    ioctl(fd, PB2_INSERT_LEFT, &n);
    n--;
    ioctl(fd, PB2_INSERT_LEFT, &n);
    n--;
    ioctl(fd, PB2_GET_INFO, &info);

    // 2 3 5 4
    assert(info.deque_size == 4 && info.capacity == 50);

    x = -1;
    ioctl(fd, PB2_EXTRACT_LEFT, &x);
    assert(x == 2);

    ioctl(fd, PB2_GET_INFO, &info);
    assert(info.deque_size == 3 && info.capacity == 50);

    ioctl(fd, PB2_EXTRACT_RIGHT, &x);
    assert(x == 4);

    ioctl(fd, PB2_EXTRACT_LEFT, &x);
    assert(x == 3);

    ioctl(fd, PB2_EXTRACT_LEFT, &x);
    assert(x == 5);

    errno = 0;
    n = 50;
    ret = ioctl(fd, PB2_SET_CAPACITY, &n);
    assert(ret == 0 && errno == 0);

    ret = read(fd, &x, sizeof(int));
    assert(ret == -1 && errno == EACCES);
    ioctl(fd, PB2_INSERT_RIGHT, &n);

    errno = 0;
    read(fd, &x, sizeof(int));
    assert(x == n && errno == 0);

    n = 5;
    ioctl(fd, PB2_INSERT_RIGHT, &n);
    n = 4;
    ioctl(fd, PB2_INSERT_LEFT, &n);

    read(fd, &x, sizeof(int));
    assert(x == 4);
    read(fd, &x, sizeof(int));
    assert(x == 5);

    info.deque_size = -1;
    info.capacity = -1;
    ioctl(fd, PB2_GET_INFO, &info);
    assert(info.deque_size == 0 && info.capacity == 50);

    n = 5;
    ioctl(fd, PB2_INSERT_RIGHT, &n);
    n--;
    ioctl(fd, PB2_INSERT_RIGHT, &n);
    n--;
    ioctl(fd, PB2_GET_INFO, &info);
    assert(info.deque_size == 2 && info.capacity == 50);

    ioctl(fd, PB2_INSERT_LEFT, &n);
    n--;
    ioctl(fd, PB2_INSERT_LEFT, &n);
    n--;
    ioctl(fd, PB2_GET_INFO, &info);

    // 2 3 5 4
    assert(info.deque_size == 4 && info.capacity == 50);

    x = -1;
    ioctl(fd, PB2_EXTRACT_LEFT, &x);
    assert(x == 2);

    ioctl(fd, PB2_GET_INFO, &info);
    assert(info.deque_size == 3 && info.capacity == 50);

    ioctl(fd, PB2_EXTRACT_RIGHT, &x);
    assert(x == 4);

    ioctl(fd, PB2_EXTRACT_LEFT, &x);
    assert(x == 3);

    ioctl(fd, PB2_EXTRACT_LEFT, &x);
    assert(x == 5);

    close(fd);

}

int main()
{
    normal_path();
    normal_path();
    printf("\033[0;32m✓ All checks passed\033[0m\n");

    return 0;
}