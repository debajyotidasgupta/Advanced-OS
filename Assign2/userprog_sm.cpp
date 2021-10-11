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
    pid = fork();
    //fork(); //uncomment this to check for >2 processes
    if (pid == 0)
    {
        printf("%d Opening File\n", getpid());
        fd = open(filename, O_RDWR);
        if (fd < 0)
        {
            printf("%d Cannot open file...\n", getpid());
            printf("%d Error= %d\n", getpid(), errno);
            return 0;
        }
        fd2 = open(filename, O_RDWR);
        //check reopening for file
        if (fd2 < 0)
        {
            printf("%d Cannot open file again...\n", getpid());
            printf("%d Error= %d\n", getpid(), errno);
            printf("%d fd2=%d\n", getpid(), fd2);
        }
        //7 9 10 12-> write order
        //9 7 10 12-> expected deque structure
        int32_t a = 4;
        int32_t b = 4;
        int32_t x;
        if ((x = ioctl(fd, PB2_SET_CAPACITY, (int32_t *)&b)) < 0)
        {
            printf("%d error %d\n", getpid(), errno);
            return 0;
        }
        a = 7;
        x = ioctl(fd, PB2_INSERT_LEFT, (int32_t *)&a);
        a = 9;
        x = ioctl(fd, PB2_INSERT_LEFT, (int32_t *)&a);
        //sleep(2); //uncomment this for proper printing
        a = 10;
        x = ioctl(fd, PB2_INSERT_RIGHT, (int32_t *)&a);
        a = 12;
        x = ioctl(fd, PB2_INSERT_RIGHT, (int32_t *)&a);

        //to check for error on excess write
        x = ioctl(fd, PB2_INSERT_RIGHT, (int32_t *)&a);
        if (x < 0)
        {
            printf("%d performed excess write\n", getpid());
        }

        ioctl(fd, PB2_POP_LEFT, (int32_t *)&a);
        printf("%d read %d expected 9\n", getpid(), a); //expected 9
        ioctl(fd, PB2_POP_RIGHT, (int32_t *)&a);
        printf("%d read %d expected 12\n", getpid(), a); //expected 12
        //sleep(2); //uncomment this for proper printing
        ioctl(fd, PB2_POP_RIGHT, (int32_t *)&a);
        printf("%d read %d expected 10\n", getpid(), a); //expected 10
        ioctl(fd, PB2_POP_LEFT, (int32_t *)&a);
        printf("%d read %d expected 7\n", getpid(), a); //expected 7

        printf("%d Closing file\n", getpid());
        close(fd);

        //check if we can reopen after close
        fd = open(filename, O_RDWR);
        if (fd >= 0)
        {
            printf("%d successfully reopened after close\n", getpid());
            close(fd);
        }
        else
        {
            printf("%d could not reopen after close\n", getpid());
        }
        return 0;
    }
    else if (pid > 0)
    {
        printf("%d Opening File\n", getpid());
        fd = open(filename, O_RDWR);
        if (fd < 0)
        {
            printf("%d Cannot open file...\n", getpid());
            printf("%d Error= %d\n", getpid(), errno);
            return 0;
        }
        int32_t a = 4;
        int32_t b = 4;
        int32_t x;
        char str[4] = "abc";
        if ((x = ioctl(fd, PB2_SET_CAPACITY, (int32_t *)&b)) < 0)
        {
            printf("%d Size error %d\n", getpid(), errno);
            return 0;
        }
        //this should return error on writing 3 bytes
        if ((x = ioctl(fd, PB2_INSERT_LEFT, str)) < 0)
        {
            printf("%d %d write error on string of length 3\n", getpid(), errno);
        }
        //11 19 20 17 -> write order
        // //19 11 17 20 -> expected read order
        a = 11;
        x = ioctl(fd, PB2_INSERT_LEFT, (int32_t *)&a);
        a = 19;
        x = ioctl(fd, PB2_INSERT_LEFT, (int32_t *)&a);
        // //sleep(2); //uncomment this for proper printing
        a = 20;
        x = ioctl(fd, PB2_INSERT_RIGHT, (int32_t *)&a);

        struct obj_info obj_info;
        x = ioctl(fd, PB2_GET_INFO, &obj_info);
        printf("%d size %d expected 3\n", getpid(), obj_info.deque_size);
        printf("%d capacity %d expected 4\n", getpid(), obj_info.capacity);

        ioctl(fd, PB2_POP_LEFT, (int32_t *)&a);
        printf("%d read %d expected 19\n", getpid(), a);
        ioctl(fd, PB2_POP_RIGHT, (int32_t *)&a);
        printf("%d read %d expected 20\n", getpid(), a);

        a = 17;
        x = ioctl(fd, PB2_INSERT_LEFT, (int32_t *)&a);
        // //sleep(2); //uncomment this for proper printing
        x = ioctl(fd, PB2_POP_LEFT, (int32_t *)&a);
        printf("%d read %d expected 17\n", getpid(), a);
        x = ioctl(fd, PB2_POP_LEFT, (int32_t *)&a);
        printf("%d read %d expected 11\n", getpid(), a);

        b = 6;
        x = ioctl(fd, PB2_SET_CAPACITY, &b);
        x = ioctl(fd, PB2_GET_INFO, &obj_info);
        printf("%d size %d expected 0\n", getpid(), obj_info.deque_size);
        printf("%d capacity %d expected 6\n", getpid(), obj_info.capacity);

        //to check for error on excess read
        x = ioctl(fd, PB2_POP_LEFT, (int32_t *)&a);
        if (x < 0)
        {
            printf("%d performed excess read\n", getpid());
        }

        printf("%d Closing file\n", getpid());
        close(fd);

        return 0;
    }
    return 0;
}